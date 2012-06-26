#include <kedr/ctf_reader/ctf_reader.h>

#include <stdexcept>

#include <cassert>

#include <endian.h>

#include <limits>

#include <typeinfo>

#include <kedr/ctf_reader/ctf_hash.h>
//#include <kedr/ctf_reader/ctf_list.h>

#include <iostream>

#include <algorithm>

/*
 * Similar to (v < 0) expression, but do not trigger compiler warnings
 * when v is really unsigned.
 */
#define is_negative(v) (!((v) > 0) && ((v) != 0))

/* 
 * Clear array of pointers to objects, also deleting objects.
 * 
 * Such arrays are common here, e.g. for store fields of structure type
 * of subvariables of structure variable.
 * 
 * Because all such arrays are filled from the first element to the last,
 * clearing should delete objects in reverse order: from last element
 * to the first.
 */

template<class T> static void deleteT(T* t) {delete t;}
template<class T>
void clearPtrVector(std::vector<T*>& v)
{
    std::for_each(v.rbegin(), v.rend(), deleteT<T>);
    v.clear();
}

/************************* Alignment helpers **************************/

/*
 * Return (minimum) number which is greater or equal to val
 * and satisfy to alignment.
 */
static inline int align_val(int val, int align)
{
    int mask = align - 1;
    return (val + mask) & ~mask;
}

/* Return alignment of the type */
#define ALIGN_OF(TYPE) ({struct {char c; TYPE m;} v; offsetof(typeof(v), m);})

/******** Layout operations for variable with fixed alignment**********/
class VarLayoutFixedAlign
{
public:
    VarLayoutFixedAlign(int align): align(align) {}
    
    int getAlignment(CTFContext& context) const
    {
        (void)context;
        return align;
    }
    int getAlignment(void) const {return align;}
    
    virtual int getStartOffset(CTFContext& context) const = 0;
    virtual int getStartOffset(void) const = 0;

    /*
     * Fast variants for layout methods used in interpret methods.
     * (do not use virtual operations in this object,
     *  take into account that method cannot return -1).
     */
    virtual int getStartOffsetInterpret(CTFContext& context) const = 0;
/* For simplify class reusing, fields are made public. */
    int align;
};

/*** Layout operations for variable with fixed size and alignment******/
class VarLayoutFixed
{
public:
    VarLayoutFixed(int align, int size): align(align), size(size) {}

    int getAlignment(CTFContext& context) const
    {
        (void)context;
        return align;
    }
    int getAlignment(void) const {return align;}
    
    int getSize(CTFContext& context) const
    {
        (void)context;
        return size;
    }
    int getSize(void) const {return size;}


    virtual int getStartOffset(CTFContext& context) const = 0;
    virtual int getStartOffset(void) const = 0;

    virtual int getEndOffset(CTFContext& context) const = 0;
    virtual int getEndOffset(void) const = 0;

    /*
     * Fast variants for layout methods used in interpret methods.
     * (do not use virtual operations in this object,
     *  take into account that method cannot return -1).
     */
    virtual int getStartOffsetInterpret(CTFContext& context) const = 0;
    
    int align;
    int size;
};

/*************************Layout specializations **********************/
/* Fixed align layout implementation which use base variable */
class VarLayoutFixedAlignUseBase : public VarLayoutFixedAlign
{
public:
    VarLayoutFixedAlignUseBase(int align, const CTFVar* varBase,
        int relativeOffset): VarLayoutFixedAlign(align),
        varBase(varBase), relativeOffset(relativeOffset) {}

    int getStartOffset(CTFContext& context) const
    {
        int baseStartOffset = varBase->getStartOffset(context);

        if(baseStartOffset == -1) return -1;
        return baseStartOffset + relativeOffset;
    }
    int getStartOffset(void) const {return -1;}

    int getStartOffsetInterpret(CTFContext& context) const
    {
        int baseStartOffset = varBase->getStartOffset(context);
        return baseStartOffset + relativeOffset;
    }

    const CTFVar* varBase;
    int relativeOffset;
};

/* Fixed align layout implementation which use absolute offset */
class VarLayoutFixedAlignAbsolute : public VarLayoutFixedAlign
{
public:
    VarLayoutFixedAlignAbsolute(int align, int offset):
        VarLayoutFixedAlign(align), offset(offset) {}

    int getStartOffset(CTFContext& context) const
    {
        (void)context;
        return offset;
    }
    int getStartOffset(void) const {return offset;}


    int getStartOffsetInterpret(CTFContext& context) const
    {
        (void)context;
        return offset;
    }
/* For simplify class reusing, fields are made public. */
    int offset;
};

/* Fixed align layout implementation which use previous variable */
class VarLayoutFixedAlignUsePrev : public VarLayoutFixedAlign
{
public:
    VarLayoutFixedAlignUsePrev(int align, const CTFVar* varPrev):
        VarLayoutFixedAlign(align), varPrev(varPrev) {}

    int getStartOffset(CTFContext& context) const
    {
        int prevEndOffset = varPrev->getEndOffset(context);

        if(prevEndOffset == -1) return -1;
        return align_val(prevEndOffset, align);
    }
    int getStartOffset(void) const {return -1;}

    int getStartOffsetInterpret(CTFContext& context) const
    {
        int prevEndOffset = varPrev->getEndOffset(context);
        return align_val(prevEndOffset, align);
    }

    const CTFVar* varPrev;
};

/* Fixed align layout implementation which use container variable */
class VarLayoutFixedAlignUseContainer : public VarLayoutFixedAlign
{
public:
    VarLayoutFixedAlignUseContainer(int align, const CTFVar* varContainer):
        VarLayoutFixedAlign(align), varContainer(varContainer) {}

    int getStartOffset(CTFContext& context) const
    {
        int containerStartOffset = varContainer->getStartOffset(context);

        if(containerStartOffset == -1) return -1;
        return align_val(containerStartOffset, align);
    }
    int getStartOffset(void) const {return -1;}

    int getStartOffsetInterpret(CTFContext& context) const
    {
        int containerStartOffset = varContainer->getStartOffset(context);
        return align_val(containerStartOffset, align);
    }

    const CTFVar* varContainer;
};

/* Fixed layout implementation which use base variable */
class VarLayoutFixedUseBase : public VarLayoutFixed
{
public:
    VarLayoutFixedUseBase(int align, int size, const CTFVar* varBase,
        int relativeOffset): VarLayoutFixed(align, size),
        varBase(varBase), relativeOffset(relativeOffset) {}

    int getStartOffset(CTFContext& context) const
    {
        int baseStartOffset = varBase->getStartOffset(context);

        if(baseStartOffset == -1) return -1;
        return baseStartOffset + relativeOffset;
    }
    int getStartOffset(void) const {return -1;}

    int getEndOffset(CTFContext& context) const
    {
        int baseStartOffset = varBase->getStartOffset(context);

        if(baseStartOffset == -1) return -1;
        return baseStartOffset + relativeOffset + size;
    }
    int getEndOffset(void) const {return -1;}


    int getStartOffsetInterpret(CTFContext& context) const
    {
        int baseStartOffset = varBase->getStartOffset(context);

        return baseStartOffset + relativeOffset;
    }

    const CTFVar* varBase;
    int relativeOffset;
};

/* Fixed layout implementation which use absolute offset */
class VarLayoutFixedAbsolute : public VarLayoutFixed
{
public:
    VarLayoutFixedAbsolute(int align, int size, int offset):
        VarLayoutFixed(align, size), offset(offset) {}

    int getStartOffset(CTFContext& context) const
    {
        (void)context;
        return offset;
    }
    int getStartOffset(void) const
    {
        return offset;
    }

    int getEndOffset(CTFContext& context) const
    {
        (void)context;
        return offset + size;
    }
    int getEndOffset(void) const
    {
        return offset + size;
    }

    int getStartOffsetInterpret(CTFContext& context) const
    {
        (void)context;
        return offset;
    }

    int offset;
};


/* Fixed layout implementation which use previous variable */
class VarLayoutFixedUsePrev : public VarLayoutFixed
{
public:
    VarLayoutFixedUsePrev(int align, int size, const CTFVar* varPrev):
        VarLayoutFixed(align, size), varPrev(varPrev) {}

    int getStartOffset(CTFContext& context) const
    {
        int prevEndOffset = varPrev->getEndOffset(context);

        if(prevEndOffset == -1) return -1;
        return align_val(prevEndOffset, align);
    }
    int getStartOffset(void) const {return -1;}

    int getEndOffset(CTFContext& context) const
    {
        int prevEndOffset = varPrev->getEndOffset(context);

        if(prevEndOffset == -1) return -1;
        return align_val(prevEndOffset, align) + size;
    }
    int getEndOffset(void) const {return -1;}

    int getStartOffsetInterpret(CTFContext& context) const
    {
        int prevEndOffset = varPrev->getEndOffset(context);
        return align_val(prevEndOffset, align);
    }

    const CTFVar* varPrev;
};


/* Fixed layout implementation which use container variable */
class VarLayoutFixedUseContainer : public VarLayoutFixed
{
public:
    VarLayoutFixedUseContainer(int align, int size,
        const CTFVar* varContainer):
        VarLayoutFixed(align, size), varContainer(varContainer) {}

    int getStartOffset(CTFContext& context) const
    {
        int containerStartOffset = varContainer->getStartOffset(context);

        if(containerStartOffset == -1) return -1;
        return align_val(containerStartOffset, align);
    }
    int getStartOffset(void) const {return -1;}

    int getEndOffset(CTFContext& context) const
    {
        int containerStartOffset = varContainer->getStartOffset(context);

        if(containerStartOffset == -1) return -1;
        return align_val(containerStartOffset, align) + size;
    }
    int getEndOffset(void) const {return -1;}


    int getStartOffsetInterpret(CTFContext& context) const
    {
        int containerStartOffset = varContainer->getStartOffset(context);
        return align_val(containerStartOffset, align);
    }

    const CTFVar* varContainer;
};


/*********************** Integer variable *****************************/

/*
 * Base class for integer variable implementation.
 *
 * Note, that it is derive from CTFVarEnum instead of CTFVarInt.
 * This is done for reuse this class and its subclassed for enumerations.
 */
class VarInt : public CTFVarEnum
{
public:
    VarInt(const CTFType* type, int align, int size)
        : type(type), align(align), size(size), layoutFixed(NULL) {}

    void setLayout(VarLayoutFixed* layoutFixed) {this->layoutFixed = layoutFixed;}
    VarLayoutFixed* getLayout(void) {return layoutFixed;}

    virtual ~VarInt(void) {delete layoutFixed;}
    /* Really, this is not enum */
    int isEnum(void) const {return 0;}
protected:
    int getAlignmentImpl(CTFContext& context) const
        {(void)context; return align;}
    int getSizeImpl(CTFContext& context) const
        {(void)context; return size;}
    int getStartOffsetImpl(CTFContext& context) const
        {return layoutFixed->getStartOffset(context);}
#ifndef CTF_VAR_CHECK_LAYOUT
    int getEndOffsetImpl(CTFContext& context) const
        {return layoutFixed->getEndOffset(context);}
#else
    int getEndOffsetImpl(CTFContext& context) const
        {return getStartOffset(context) + size;}
#endif
    int getAlignmentImpl(void) const {return align;}
    int getSizeImpl(void) const {return size;}
    int getStartOffsetImpl(void) const
        {return layoutFixed->getStartOffset();}
#ifndef CTF_VAR_CHECK_LAYOUT
    int getEndOffsetImpl(void) const
        {return layoutFixed->getEndOffset();}
#else
    int getEndOffsetImpl(void) const
        {return getStartOffset() + size;}
#endif
    /*
     * Fast variants for layout methods used in interpret methods.
     * (do not use virtual operations in this object,
     *  take into account that method cannot return -1).
     */
#ifndef CTF_VAR_CHECK_LAYOUT
    int getStartOffsetInterpret(CTFContext& context) const
        {return layoutFixed->getStartOffsetInterpret(context);}
#else
    int getStartOffsetInterpret(CTFContext& context) const
        {return getStartOffset(context);}
#endif

    const CTFType* getTypeImpl(void) const {return type;}
    /* Really, it is not enum, so enumeration method do nothing. */
    int getValueImpl(CTFContext& context) const {(void)context; return 0;}

protected:
    const CTFType* type;/* for reuse by enumeration */
private:
    int align;
    int size;

    VarLayoutFixed* layoutFixed;
};


/* Start offset of byte-aligned integer variable for use in interpret operation. */
#define INT_START ({ \
    CTFContext* contextTmp = adjustContext(context); assert(contextTmp); \
    contextTmp->mapStart() + getStartOffsetInterpret(*contextTmp) / 8; })


/* Variable implementation for internal C++ types */
template <class T, bool isBE>
class VarIntInternal : public VarInt
{
public:
    VarIntInternal(const CTFType* type, int align)
        : VarInt(type, align, sizeof(T) * 8) {}
    static int minAlign(void) {return ALIGN_OF(T);}

    template<class TV>
    TV getInt(CTFContext& context) const
        {return getInt_internal<TV,
            std::numeric_limits<T>::is_signed,
            std::numeric_limits<TV>::is_signed>(context);}
protected:
    int32_t getInt32Impl(CTFContext& context) const
        {return getInt<int32_t>(context);}
    uint32_t getUInt32Impl(CTFContext& context) const
        {return getInt<uint32_t>(context);}
    int64_t getInt64Impl(CTFContext& context) const
        {return getInt<int64_t>(context);}
    uint64_t getUInt64Impl(CTFContext& context) const
        {return getInt<uint64_t>(context);}
private:
    template<class TV, bool t_is_signed, bool tv_is_signed>
    TV getInt_internal(CTFContext& context) const;

};

#ifndef _BSD_SOURCE
#error _BSD_SOURCE feature needs for correct processing byte order.
#endif

/* Process byte order in value */
#define toHost(val, type, isBE, result) \
    if(sizeof(type) == 1) {result = val;}    \
    else if(sizeof(type) == 2) {result = (type) (isBE ? be16toh((uint16_t)val) : le16toh((uint16_t)val));} \
    else if(sizeof(type) == 4) {result = (type) (isBE ? be32toh((uint32_t)val) : le32toh((uint32_t)val));} \
    else if(sizeof(type) == 8) {result = (type) (isBE ? be64toh((uint64_t)val) : le64toh((uint64_t)val));}


template<class T, bool isBE>
template<class TV, bool t_is_signed, bool tv_is_signed>
TV VarIntInternal<T, isBE>::getInt_internal(CTFContext& context) const
{
    T result_unordered = *(const T*)INT_START;
    T result;
    toHost(result_unordered, T, isBE, result);
    if(t_is_signed)
    {
        if(tv_is_signed)
        {
            /* Both T and TV - signed */
            if(sizeof(T) > sizeof(TV))
            {
                /* Check result's signed range */
            }
        }
        else
        {
            /*T - signed, TV - unsigned */
            if(is_negative(result)) throw std::overflow_error
                ("Overflow when read negative integer as unsigned");
            if(sizeof(T) > sizeof(TV))
            {
                /* Check result's unsigned range */
            }
        }
    }
    else
    {
        if(tv_is_signed)
        {
            /* T - unsigned, TV - signed */
            if(sizeof(T) >= sizeof(TV))
            {
                /* Check result's unsigned range */
            }
        }
        else
        {
            /* Both T and TV - unsigned */
            if(sizeof(T) > sizeof(TV))
            {
                /* Check result's unsigned range */
            }
        }
    }
    return (TV)result;
}


class TypeInt : public CTFTypeInt
{
public:
    TypeInt(void);
    virtual ~TypeInt(void) {};
protected:
    CTFType* cloneImpl(void) const;
    int getAlignmentImpl(void) const {return align;}
    int getAlignmentMaxImpl(void) const {return align;}
    void setVarImpl(CTFVarPlace& varPlace) const;

    int getSizeImpl(void) const {return size;}
    int isSignedImpl(void) const {return isSigned;}
    enum ByteOrder getByteOrderImpl(void) const {return byteOrder;}

    void setSizeImpl(int size);
    void setAlignmentImpl(int align);
    void setSignedImpl(int isSigned);
    void setByteOrderImpl(enum ByteOrder byteOrder);

    void fixParamsImpl(void);

    CTFTypeEnum* createEnumImpl(void) const;
private:
    TypeInt(const TypeInt& typeInt);

    int size;
    int align;
    int isSigned;
    enum ByteOrder byteOrder;
    int byteOrderIsSet : 1;
};

static inline int isPower2(int val)
{
    int tmp = val;
    while((tmp > 1) && ((tmp & 1) == 0))
    {
        tmp >>= 1;
    }
    return tmp == 1;
}

TypeInt::TypeInt(void) : size(-1), align(-1), isSigned(-1),
    byteOrder(be), byteOrderIsSet(0) {}

TypeInt::TypeInt(const TypeInt& typeInt) : size(typeInt.size),
    align(typeInt.align), isSigned(typeInt.isSigned),
    byteOrder(typeInt.byteOrder), byteOrderIsSet(typeInt.byteOrderIsSet){}

CTFType* TypeInt::cloneImpl(void) const
{
    return new TypeInt(*this);
}

void TypeInt::setSizeImpl(int size)
{
    if(this->size != -1)
        throw std::logic_error("Attempt to set size for integer type, for "
                               "which it is already set");
    assert(size > 0);

    this->size = size;
}

void TypeInt::setAlignmentImpl(int align)
{
    if(this->align != -1)
        throw std::logic_error("Attempt to set align for integer type, for "
                               "which it is already set");
    assert(isPower2((align)));

    this->align = align;
}

void TypeInt::setSignedImpl(int isSigned)
{
    if(this->isSigned != -1)
        throw std::logic_error("Attempt to set signedness for integer type, for "
                               "which it is already set");
    this->isSigned = isSigned ? 1 : 0;
}

void TypeInt::setByteOrderImpl(enum ByteOrder byteOrder)
{
    if(this->byteOrderIsSet)
        throw std::logic_error("Attempt to set byte order for integer type, for "
                               "which it is already set");

    this->byteOrder = byteOrder;
    byteOrderIsSet = 1;
}

void TypeInt::fixParamsImpl(void)
{
    if(size == -1)
        throw std::logic_error("Size parameter of integer type should be set.");
    if(!byteOrderIsSet)
        throw std::logic_error("Byte order parameter of integer type should be set.");
    if(isSigned == -1)
        throw std::logic_error("Signedness parameter of integer type should "
                               "be set.");
    if(align == -1)
    {
        if(size < 8) align = 1;
        else align = 8;
    }
}

template <bool isBE>
VarInt* createVarInt(const CTFType* type, int size, int isSigned, int align)
{
    VarInt* varInt;

    if(size < 8)
        throw std::logic_error(
            "Sub-bytes integers are currently not supported.");

    if(size % 8)
        throw std::logic_error(
            "Integers with size more than 8 but not multiple to 8 are"
            "not supported.");

    switch(size / 8)
    {
/* Create integer with given type */
#define CREATE_INT(intType) do{                                \
if(align < VarIntInternal<intType, isBE>::minAlign())                    \
throw std::logic_error                                          \
("Too little alignment for interpret integer as " #intType);   \
else varInt = new VarIntInternal<intType, isBE>(type, align);}while(0)
/* Create integer with given size(in bytes)*/
#define CASE_CREATE(sizeBytes, signedType, unsignedType) case sizeBytes: \
if(isSigned) CREATE_INT(signedType); else CREATE_INT(unsignedType); break

    CASE_CREATE(1, int8_t, uint8_t);
    CASE_CREATE(2, int16_t, uint16_t);
    CASE_CREATE(4, int32_t, uint32_t);
    CASE_CREATE(8, int64_t, uint64_t);

#undef CASE_CREATE
#undef CREATE_INT
    default:
        throw std::logic_error(
            "Non-standard integer type sizes currently are not supported");
    }

    return varInt;
}

void TypeInt::setVarImpl(CTFVarPlace& varPlace) const
{
    VarInt* varInt;

    struct CTFVarStartOffsetParams layoutParams;

    layoutParams.fill(varPlace, align);

    VarLayoutFixed* layoutFixed = NULL;

    switch(layoutParams.layoutType)
    {
    case CTFVarStartOffsetParams::LayoutTypeAbsolute:
        layoutFixed = new VarLayoutFixedAbsolute(
            layoutParams.align, size, layoutParams.info.absolute.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseBase:
        layoutFixed = new VarLayoutFixedUseBase(
            layoutParams.align, size,
            layoutParams.info.useBase.var, layoutParams.info.useBase.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUsePrev:
        layoutFixed = new VarLayoutFixedUsePrev(
            layoutParams.align, size, layoutParams.info.usePrev.var);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseContainer:
        layoutFixed = new VarLayoutFixedUseContainer(
            layoutParams.align, size, layoutParams.info.useContainer.var);
    break;
    }

    try
    {
        if(byteOrder == be)
            varInt = createVarInt<true>(this, size, isSigned, layoutParams.align);
        else
            varInt = createVarInt<false>(this, size, isSigned, layoutParams.align);
    }catch(...)
    {
        delete layoutFixed;
        throw;
    }

    varInt->setLayout(layoutFixed);
    varPlace.setVar(varInt);
}

CTFTypeInt* CTFMeta::createTypeInt(void) const
{
    return new TypeInt();
}

/********************** Fields *************************/

/* Field of the structure or of the variant*/
struct Field
{
    std::string name;
    const CTFType* type;

    Field(const std::string& name, const CTFType* type)
        : name(name), type(type) {}

    Field(const Field& field)
        : name(field.name), type(field.type) {}
};

/* Wrapper around strings for fast search*/
struct FieldKey
{
    const char* fieldName;

    FieldKey(const char* fieldName) : fieldName(fieldName) {}

    unsigned int hash() const {return IDHelpers::hash(fieldName);}
    bool operator<(const FieldKey& fieldKey) const
    {
        return IDHelpers::less(fieldName, fieldKey.fieldName);
    }
};

/************************** Structure *********************************/
/* Structure type */
class StructType : public CTFTypeStruct
{
public:
    StructType(void);

protected:
    void addFieldImpl(const std::string& fieldName, const CTFType* fieldType);

    CTFType* cloneImpl(void) const {return new StructType(*this);}
    int getAlignmentImpl(void) const {return align;}
    int getAlignmentMaxImpl(void) const {return maxAlign;};
    void setVarImpl(CTFVarPlace& varPlace) const;

    CTFTag resolveTagImpl(const char* tagStr,
        const char** tagStrEnd, bool isContinued) const;
private:
    StructType(const StructType& structType);
    /* Ordered list of structure fields */
    typedef std::vector<Field> fields_t;
    fields_t fields;
    /* Key->index hash table, for search fields by name*/
    typedef HashTable<FieldKey, int, unsigned>
        fieldsTable_t;
    fieldsTable_t fieldsTable;
    /* alignment(constant) of the structure */
    int align;
    /* Maximum alignment of structure variable and all of its subvariables */
    int maxAlign;

    friend class StructVar;
};

StructType::StructType(void) : align(1), maxAlign(1) {}

StructType::StructType(const StructType& structType)
    : fields(structType.fields), align(structType.align),
    maxAlign(structType.maxAlign)
{
    int n_fields = structType.fields.size();
    for(int i = 0; i < n_fields; i++)
    {
        fieldsTable.insert(
            FieldKey(structType.fields[i].name.c_str()), i);
    }
}

void StructType::addFieldImpl(const std::string& fieldName,
    const CTFType* fieldType)
{
    fields.push_back(Field(fieldName, fieldType));

    std::pair<fieldsTable_t::iterator, bool> result =
        fieldsTable.insert(FieldKey(fields.back().name.c_str()),
            fields.size() - 1);
    if(!result.second)
    {
        fields.pop_back();
        throw std::logic_error(std::string("Attempt to add field with "
            "name ") + fieldName + " which already exists in the structure.");
    }

    int fieldAlign = fieldType->getAlignment();
    if(align < fieldAlign)
        align = fieldAlign;

    int fieldMaxAlign = fieldType->getAlignmentMax();
    if(maxAlign < fieldMaxAlign)
        maxAlign = fieldMaxAlign;
}

CTFTag StructType::resolveTagImpl(const char* tagStr,
    const char** tagStrEnd, bool isContinued) const
{
    if(isContinued)
    {
        if(*tagStr != '.') return CTFTag();
        tagStr++;
    }

    fieldsTable_t::const_iterator iter = fieldsTable.find(tagStr);
    if(iter == fieldsTable.end()) return CTFTag();

    const Field& field = fields[iter->second];

    if(tagStrEnd) *tagStrEnd = tagStr + field.name.size();

    return CTFTag(this, field.name.c_str(), field.type);
}

/*
 * Base class for structure variable.
 */
class StructVar : public CTFVar
{
public:
    StructVar(const StructType* structType);
    /* virtual is needed here, see setVarImpl() */
    virtual ~StructVar(void);

    /* 
     * Setup fields of structure variable.
     * 
     * Return true on success and false if cannot set fields for the given
     * specialization of the structure.
     */
    virtual bool setFields(void) = 0;
protected:
    /* 
     * If all fields of the structure has total fixed size,
     * instantiate fields and return that size.
     * 
     * Otherwise return -1.
     */
    int setFieldsFixedSize(void);
    
    /* 
     * Instantiate all fields and return variable corresponded to the last
     * one.
     */
    const CTFVar* setFieldsCommon(void);
    /* Return last field of the structure. Used for debugging. */
    const CTFVar* getLastField(void) const {return fields.back()->getVar();}

    const CTFVar* resolveNameImpl(const char* name,
        const char** nameEnd, bool isContinued) const;

    const CTFType* getTypeImpl(void) const {return structType;}
private:
    /* Variable place for structure field. */
    class StructFieldPlace: public CTFVarPlace
    {
    public:
        StructFieldPlace(const StructVar* structParent, int index)
            : structParent(structParent), index(index) {}
    protected:
        std::string getNameImpl(void) const;
        const CTFVar* getParentVar(void) const {return structParent;}
        const CTFVar* getContainerVar(void) const {return structParent;}
        const CTFVar* getPreviousVar(void) const
            {return index > 0 ? structParent->fields[index - 1]->getVar() : NULL;}
    private:
        const StructVar* structParent;
        int index;
    };

    typedef std::vector<StructFieldPlace*> fields_t;
    fields_t fields;

    const StructType* structType;
};

StructVar::StructVar(const StructType* structType): structType(structType)
{
}


StructVar::~StructVar(void)
{
    clearPtrVector(fields);
}

std::string StructVar::StructFieldPlace::getNameImpl(void) const
{
    const Field& field = structParent->structType->fields[index];
    return structParent->name() + "." + field.name;
}

const CTFVar* StructVar::resolveNameImpl(const char* name,
    const char** nameEnd, bool isContinued) const
{
    if(isContinued)
    {
        if(*name != '.') return NULL;
        name++;
    }
    StructType::fieldsTable_t::const_iterator iter =
        structType->fieldsTable.find(name);
    if(iter == structType->fieldsTable.end()) return NULL;

    int index = iter->second;
    int n_fields = fields.size();
    if(index >= n_fields)
        throw std::logic_error("Attempt to resolve name of structure field, "
            "which has not instantiated yet");

    *nameEnd = name + structType->fields[index].name.size();

    return fields[index]->getVar();
}


const CTFVar* StructVar::setFieldsCommon(void)
{
    int n_fields = structType->fields.size();
    fields.reserve(n_fields);

    const CTFVar* lastField = NULL;

    for(int i = 0; i < n_fields; i++)
    {
        StructFieldPlace* fieldPlace = new StructFieldPlace(this, i);
        fields.push_back(fieldPlace);
        fieldPlace->instantiateVar(structType->fields[i].type);
        lastField = fieldPlace->getVar();
    }
    return lastField;
}


int StructVar::setFieldsFixedSize(void)
{
    int n_fields = structType->fields.size();
    fields.reserve(n_fields);

    int size = 0;

    for(int i = 0; i < n_fields; i++)
    {
        StructFieldPlace* fieldPlace = new StructFieldPlace(this, i);
        fields.push_back(fieldPlace);
        fieldPlace->instantiateVar(structType->fields[i].type);
        
        const CTFVar* lastField = fieldPlace->getVar();

        int fieldAlign = lastField->getAlignment();
        if(fieldAlign != -1)
        {
            int fieldSize = lastField->getSize();
            if(fieldSize != -1)
            {
                size = align_val(size, fieldAlign);
                size += fieldSize;
                continue;
            }
        }
        /* Rollback */
        clearPtrVector(fields);
        return -1;
    }
    return size;
}

/*
 * Specializations of structure variable.
 */

/* Absolute layout, non-constant size */
class StructVarAbsolute : public StructVar,
    public VarLayoutFixedAlignAbsolute
{
public:
    StructVarAbsolute(const StructType* structType, int offset)
        : StructVar(structType),
        VarLayoutFixedAlignAbsolute(structType->getAlignment(), offset) {}

    bool setFields(void)
    {
        lastField = setFieldsCommon();
        return true;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixedAlign::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixedAlign::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset();}

    int getEndOffsetImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        return endOffset;
    }
    int getEndOffsetImpl(void) const {return -1;}
    
    int getSizeImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        return endOffset - offset;
    }
    int getSizeImpl(void) const {return -1;}
private:
    const CTFVar* lastField;
};

/* Absolute layout, constant size */
class StructVarAbsoluteConst : public StructVar,
    public VarLayoutFixedAbsolute
{
public:
    StructVarAbsoluteConst(const StructType* structType, int offset)
        : StructVar(structType),
        VarLayoutFixedAbsolute(structType->getAlignment(), 0, offset) {}

    bool setFields(void)
    {
        size = setFieldsFixedSize();
        return size != -1;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixed::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixed::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAbsolute::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAbsolute::getStartOffset();}

#ifndef CTF_VAR_CHECK_LAYOUT
    int getEndOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAbsolute::getEndOffset(context);}
    int getEndOffsetImpl(void) const
        {return VarLayoutFixedAbsolute::getEndOffset();}
#else
    int getEndOffsetImpl(CTFContext& context) const
        {return getLastField() ? getLastField()->getEndOffset(context) : 0;}
    int getEndOffsetImpl(void) const
        {return getLastField() ? getLastField()->getEndOffset() : 0;}
#endif
    int getSizeImpl(CTFContext& context) const
        {return VarLayoutFixedAbsolute::getSize(context);}
    int getSizeImpl(void) const
        {return VarLayoutFixedAbsolute::getSize();}
};


/* Use base layout, non-constant size */
class StructVarUseBase : public StructVar,
    public VarLayoutFixedAlignUseBase
{
public:
    StructVarUseBase(const StructType* structType,
        const CTFVar* varBase, int relativeOffset) :
        StructVar(structType), VarLayoutFixedAlignUseBase(
            structType->getAlignment(), varBase, relativeOffset) {}

    bool setFields(void)
    {
        lastField = setFieldsCommon();
        return true;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixedAlign::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixedAlign::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseBase::getStartOffset(context);}
    int getStartOffsetImpl(void) const {return -1;}

    int getEndOffsetImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        return endOffset;
    }
    int getEndOffsetImpl(void) const {return -1;}
    
    int getSizeImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        int startOffset = VarLayoutFixedAlignUseBase::getStartOffset(context);
        if(startOffset == -1) return -1;
        
        return endOffset - startOffset;
    }
    int getSizeImpl(void) const {return -1;}
private:
    const CTFVar* lastField;
};


/* Use base layout, constant size */
class StructVarUseBaseConst : public StructVar,
    public VarLayoutFixedUseBase
{
public:
    StructVarUseBaseConst(const StructType* structType,
        const CTFVar* varBase, int relativeOffset) :
        StructVar(structType), VarLayoutFixedUseBase(
            structType->getAlignment(), 0, varBase, relativeOffset) {}

    bool setFields(void)
    {
        size = setFieldsFixedSize();
        return size != -1;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixed::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixed::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedUseBase::getStartOffset(context);}
    int getStartOffsetImpl(void) const {return -1;}
#ifndef CTF_VAR_CHECK_LAYOUT
    int getEndOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedUseBase::getEndOffset(context);}
    int getEndOffsetImpl(void) const {return -1;}
#else
    int getEndOffsetImpl(CTFContext& context) const
        {return getLastField() ? getLastField()->getEndOffset(context) : 0;}
    int getEndOffsetImpl(void) const
        {return getLastField() ? getLastField()->getEndOffset() : 0;}
#endif

    int getSizeImpl(CTFContext& context) const
        {return VarLayoutFixedUseBase::getSize(context);}
    int getSizeImpl(void) const
        {return VarLayoutFixedUseBase::getSize();}
};

/* Use prev layout, non-constant size */
class StructVarUsePrev : public StructVar,
    public VarLayoutFixedAlignUsePrev
{
public:
    StructVarUsePrev(const StructType* structType,
        const CTFVar* varPrev) :
        StructVar(structType), VarLayoutFixedAlignUsePrev(
            structType->getAlignment(), varPrev) {}

    bool setFields(void)
    {
        lastField = setFieldsCommon();
        return true;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixedAlign::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixedAlign::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUsePrev::getStartOffset(context);}
    int getStartOffsetImpl(void) const {return -1;}

    int getEndOffsetImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        return endOffset;
    }
    int getEndOffsetImpl(void) const {return -1;}
    
    int getSizeImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        int startOffset = VarLayoutFixedAlignUsePrev::getStartOffset(context);
        if(startOffset == -1) return -1;
        
        return endOffset - startOffset;
    }
    int getSizeImpl(void) const {return -1;}
private:
    const CTFVar* lastField;
};

/* Use prev layout, constant size */
class StructVarUsePrevConst : public StructVar,
    public VarLayoutFixedUsePrev
{
public:
    StructVarUsePrevConst(const StructType* structType,
        const CTFVar* varPrev) :
        StructVar(structType), VarLayoutFixedUsePrev(
            structType->getAlignment(), 0, varPrev) {}

    bool setFields(void)
    {
        size = setFieldsFixedSize();
        return size != -1;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixed::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixed::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedUsePrev::getStartOffset(context);}
    int getStartOffsetImpl(void) const {return -1;}

#ifndef CTF_VAR_CHECK_LAYOUT
    int getEndOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedUsePrev::getEndOffset(context);}
    int getEndOffsetImpl(void) const {return -1;}
#else
    int getEndOffsetImpl(CTFContext& context) const
        {return getLastField() ? getLastField()->getEndOffset(context) : 0;}
    int getEndOffsetImpl(void) const
        {return getLastField() ? getLastField()->getEndOffset() : 0;}
#endif

    int getSizeImpl(CTFContext& context) const
        {return VarLayoutFixed::getSize(context);}
    int getSizeImpl(void) const
        {return VarLayoutFixed::getSize();}
};


/* Use container layout, non-constant size */
class StructVarUseContainer : public StructVar,
    public VarLayoutFixedAlignUseContainer
{
public:
    StructVarUseContainer(const StructType* structType,
        const CTFVar* varContainer) :
        StructVar(structType), VarLayoutFixedAlignUseContainer(
            structType->getAlignment(), varContainer) {}

    bool setFields(void)
    {
        lastField = setFieldsCommon();
        return true;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixedAlign::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixedAlign::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseContainer::getStartOffset(context);}
    int getStartOffsetImpl(void) const {return -1;}

    int getEndOffsetImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        return endOffset;
    }
    int getEndOffsetImpl(void) const {return -1;}
    
    int getSizeImpl(CTFContext& context) const
    {
        int endOffset = lastField->getEndOffset(context);
        if(endOffset == -1) return -1;
        
        int startOffset = VarLayoutFixedAlignUseContainer::getStartOffset(context);
        if(startOffset == -1) return -1;
        
        return endOffset - startOffset;
    }
    int getSizeImpl(void) const {return -1;}
private:
    const CTFVar* lastField;
};

/* Use container layout, constant size */
class StructVarUseContainerConst : public StructVar,
    public VarLayoutFixedUseContainer
{
public:
    StructVarUseContainerConst(const StructType* structType,
        const CTFVar* varContainer) :
        StructVar(structType), VarLayoutFixedUseContainer(
            structType->getAlignment(), 0, varContainer) {}

    bool setFields(void)
    {
        size = setFieldsFixedSize();
        return size != -1;
    }
protected:
    int getAlignmentImpl(CTFContext& context) const
        {return VarLayoutFixed::getAlignment(context);}
    int getAlignmentImpl(void) const
        {return VarLayoutFixed::getAlignment();}
    
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedUseContainer::getStartOffset(context);}
    int getStartOffsetImpl(void) const {return -1;}
#ifndef CTF_VAR_CHECK_LAYOUT
    int getEndOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedUseContainer::getEndOffset(context);}
    int getEndOffsetImpl(void) const {return -1;}
#else
    int getEndOffsetImpl(CTFContext& context) const
        {return getLastField() ? getLastField()->getEndOffset(context) : 0;}
    int getEndOffsetImpl(void) const
        {return getLastField() ? getLastField()->getEndOffset() : 0;}
#endif
    int getSizeImpl(CTFContext& context) const
        {return VarLayoutFixed::getSize(context);}
    int getSizeImpl(void) const
        {return VarLayoutFixed::getSize();}
};

/* Setup structure variable */

void StructType::setVarImpl(CTFVarPlace& varPlace) const
{
    CTFVarStartOffsetParams startOffsetParams;
    startOffsetParams.fill(varPlace, align);

    StructVar* structVar;

    switch(startOffsetParams.layoutType)
    {
    case CTFVarStartOffsetParams::LayoutTypeAbsolute:
        structVar = new StructVarAbsoluteConst(this,
            startOffsetParams.info.absolute.offset);
        varPlace.setVar(structVar);
        
        if(!structVar->setFields())
        {
            structVar = new StructVarAbsolute(this,
                startOffsetParams.info.absolute.offset);
            delete varPlace.setVar(structVar);
            
            structVar->setFields();
        }
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseBase:
        structVar = new StructVarUseBaseConst(this,
            startOffsetParams.info.useBase.var,
            startOffsetParams.info.useBase.offset);
        varPlace.setVar(structVar);
        
        if(!structVar->setFields())
        {
            structVar = new StructVarUseBase(this,
                startOffsetParams.info.useBase.var,
                startOffsetParams.info.useBase.offset);
            delete varPlace.setVar(structVar);
            
            structVar->setFields();
        }
    break;
    case CTFVarStartOffsetParams::LayoutTypeUsePrev:
        structVar = new StructVarUsePrevConst(this,
            startOffsetParams.info.usePrev.var);
        varPlace.setVar(structVar);
        
        if(!structVar->setFields())
        {
            structVar = new StructVarUsePrev(this,
                startOffsetParams.info.usePrev.var);
            delete varPlace.setVar(structVar);
            
            structVar->setFields();
        }
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseContainer:
        structVar = new StructVarUseContainerConst(this,
            startOffsetParams.info.useContainer.var);
        varPlace.setVar(structVar);
        
        if(!structVar->setFields())
        {
            structVar = new StructVarUseContainer(this,
                startOffsetParams.info.useContainer.var);
            delete varPlace.setVar(structVar);
            
            structVar->setFields();
        }
    break;
    default:
        assert(0);
    }
}

CTFTypeStruct* CTFMeta::createTypeStruct(void) const
{
    return new StructType();
}

/******************** Enumeration *************************/
/* Range of integer values and its using as key for map. */
template<class T>
struct Range
{
    /* Inclusive range [start, end]*/
    T start;
    T end;

    Range(T start, T end)
        : start(start), end(end) {assert(start <= end);}

    Range(const Range& range) : start(range.start), end(range.end) {}
    Range& operator =(const Range& range)
    {
        start = range.start;
        end = range.end;
    }

    bool operator<(const Range& range) const
    {
        return end < range.start;
    }
};

/* Base type */
class TypeEnumBase : public CTFTypeEnum
{
public:
    TypeEnumBase(const TypeInt* typeInt) : valueStrings(1, ""),
        typeInt(typeInt) {}
protected:
    int getAlignmentImpl(void) const
        {return typeInt->getAlignment();}
    int getAlignmentMaxImpl(void) const
        {return typeInt->getAlignmentMax();}

    std::string valueToStrImpl(int index) const {return valueStrings[index];}
    int getNValuesImpl(void) const {return valueStrings.size();}

    std::vector<std::string> valueStrings;
    const TypeInt* typeInt;
};

/* Templated enumeration type.*/
template<class T>
class TypeEnum : public TypeEnumBase
{
public:
    TypeEnum(const TypeInt* typeInt) : TypeEnumBase(typeInt) {}
    /* Whether given integer is represented by base integer of enumeration */
    template<class TV>
    bool isRepresented(TV v) const;
    /* Resolve integer value, returning its index(or 0) */
    int resolveInt (T v) const;
protected:
    CTFType* cloneImpl() const {return new TypeEnum<T>(*this);}

    void setVarImpl(CTFVarPlace& varPlace) const;

    template<class TV>
    void addValueImpl(const char* valueName, TV start, TV end);

    void addValue64Impl(const char* valueName,
        int64_t start, int64_t end)
        {return addValueImpl<int64_t>(valueName, start, end);}
    void addValueU64Impl(const char* valueName,
        uint64_t start, uint64_t end)
        {return addValueImpl<uint64_t>(valueName, start, end);}
    void addValue32Impl(const char* valueName,
        int32_t start, int32_t end)
        {return addValueImpl<int32_t>(valueName, start, end);}
    void addValueU32Impl(const char* valueName,
        uint32_t start, uint32_t end)
        {return addValueImpl<uint32_t>(valueName, start, end);}
private:
    /* Range->index map */
    typedef std::map<Range<T>, int> valuesMap_t;
    valuesMap_t valuesMap;
};

template<class T> template<class TV>
bool TypeEnum<T>::isRepresented(TV v) const
{
    int size = TypeEnumBase::typeInt->getSize();

    if(std::numeric_limits<TV>::is_signed)
    {
        if(std::numeric_limits<T>::is_signed)
        {
            /* Mask for all unsigned integers, which may be represented */
            T umask = (((T)1) << (size - 1)) - 1;
            /* Extract sign bit */
            T sign = ((T)v) & (umask + 1);
            /*
             * If 'v' doesn't use non-represented bits, except in sign-extension,
             * (v + sign) gives representable unsigned integer.
             */
            return ((((T)v) + sign) & ~umask) == 0;
        }
        else
        {
            if(is_negative(v)) return false;
            /* Mask for all unsigned integers, which may be represented */
            T mask = (((T)1) << size) - 1;

            return (((T)v) & ~mask) == 0;

        }
    }
    else
    {
        if(std::numeric_limits<T>::is_signed)
        {
            /* Mask for all unsigned integers, which may be represented */
            T umask = (((T)1) << (size - 1)) - 1;

            return (((T)v) & ~umask) == 0;
        }
        else
        {
            /* Mask for all unsigned integers, which may be represented */
            T umask = (((T)1) << size) - 1;

            return (((T)v) & ~umask) == 0;
        }
    }
}


template<class T> template<class TV>
void TypeEnum<T>::addValueImpl(const char* valueName,
    TV start, TV end)
{
    if(!isRepresented<TV>(start) || !isRepresented<TV>(end))
        throw std::invalid_argument("Attempt to add value to "
            "enumeration, which cannot be represented with underline "
            "integer type.");
    typename valuesMap_t::value_type v(Range<T>((T)start, (T)end),
        valueStrings.size());
    std::pair<typename valuesMap_t::iterator, bool> result = valuesMap.insert(v);

    if(!result.second)
        throw std::invalid_argument("Attempt to add value to "
            "enumeration, which overlaps with already existed value.");

    valueStrings.push_back(valueName);
}

template<class T>
int TypeEnum<T>::resolveInt(T v) const
{
    typename valuesMap_t::const_iterator iter = valuesMap.find(Range<T>(v,v));
    if(iter != valuesMap.end())
        return iter->second;
    else
        return 0;
}


/* Templated enumeration variable for internal integer base types */
template<class TEnum, class T, bool isBE>
class VarEnumInternal: public VarIntInternal<T, isBE>
{
public:
    VarEnumInternal(const TypeEnum<TEnum>* typeEnum, int align)
        : VarIntInternal<T, isBE>(typeEnum, align) {};
protected:
    int isEnum(void) const {return 1;}
    int getValueImpl(CTFContext& context) const
    {
        const TypeEnum<TEnum>* typeEnum =
            static_cast<const TypeEnum<TEnum>*>(VarInt::type);
        TEnum v = VarIntInternal<T, isBE>::template getInt<TEnum>(context);
        return typeEnum->resolveInt(v);
    }
};


template <class TEnum, bool isBE>
VarInt* createVarEnum(const TypeEnum<TEnum>* typeEnum,
    int size, int align)
{
    VarInt* varInt;

    if(size < 8)
        throw std::logic_error(
            "Sub-bytes integers are currently not supported.");

    if(size % 8)
        throw std::logic_error(
            "Integers with size more than 8 but not multiple to 8 are"
            "not supported.");

    switch(size / 8)
    {
    case 1:
        if(std::numeric_limits<TEnum>::is_signed)
            varInt = new VarEnumInternal<TEnum, int8_t, isBE>(typeEnum, align);
        else
            varInt = new VarEnumInternal<TEnum, uint8_t, isBE>(typeEnum, align);
    break;
    case 2:
        if(std::numeric_limits<TEnum>::is_signed)
            varInt = new VarEnumInternal<TEnum, int16_t, isBE>(typeEnum, align);
        else
            varInt = new VarEnumInternal<TEnum, uint16_t, isBE>(typeEnum, align);
    break;
    case 4:
        if(std::numeric_limits<TEnum>::is_signed)
            varInt = new VarEnumInternal<TEnum, int32_t, isBE>(typeEnum, align);
        else
            varInt = new VarEnumInternal<TEnum, uint32_t, isBE>(typeEnum, align);
    break;
    case 8:
        if(std::numeric_limits<TEnum>::is_signed)
            varInt = new VarEnumInternal<TEnum, int64_t, isBE>(typeEnum, align);
        else
            varInt = new VarEnumInternal<TEnum, uint64_t, isBE>(typeEnum, align);
    break;
    default:
        throw std::logic_error(
            "Non-standard integer types currently are not supported");
    }

    return varInt;
}


template<class T>
void TypeEnum<T>::setVarImpl(CTFVarPlace& varPlace) const
{
    VarInt* varEnum;

    int align = typeInt->getAlignment();
    int size = typeInt->getSize();
    CTFTypeInt::ByteOrder byteOrder = typeInt->getByteOrder();

    if(byteOrder == CTFTypeInt::be)
        varEnum = createVarEnum<T, true>(this, size, align);
    else
        varEnum = createVarEnum<T, false>(this, size, align);

    struct CTFVarStartOffsetParams layoutParams;

    layoutParams.fill(varPlace, align);

    VarLayoutFixed* layoutFixed = NULL;

    switch(layoutParams.layoutType)
    {
    case CTFVarStartOffsetParams::LayoutTypeAbsolute:
        layoutFixed = new VarLayoutFixedAbsolute(
            layoutParams.align, size, layoutParams.info.absolute.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseBase:
        layoutFixed = new VarLayoutFixedUseBase(
            layoutParams.align, size,
            layoutParams.info.useBase.var, layoutParams.info.useBase.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUsePrev:
        layoutFixed = new VarLayoutFixedUsePrev(
            layoutParams.align, size, layoutParams.info.usePrev.var);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseContainer:
        layoutFixed = new VarLayoutFixedUseContainer(
            layoutParams.align, size, layoutParams.info.useContainer.var);
    break;
    }

    varEnum->setLayout(layoutFixed);
    varPlace.setVar(varEnum);
}

CTFTypeEnum* TypeInt::createEnumImpl(void) const
{
    TypeEnumBase* typeEnum;

    if(size <= 32)
    {
        if(isSigned)
            typeEnum = new TypeEnum<int32_t>(this);
        else
            typeEnum = new TypeEnum<uint32_t>(this);
    }
    else
    {
        if(isSigned)
            typeEnum = new TypeEnum<int64_t>(this);
        else
            typeEnum = new TypeEnum<uint64_t>(this);
    }
    return typeEnum;
}

CTFTypeEnum* CTFMeta::createTypeEnum(const CTFTypeInt* typeInt)const
{
    return typeInt->createEnum();
}

/********************** Variant type ******************************/
class TypeVariant: public CTFTypeVariant
{
public:
    TypeVariant(void);
protected:
    CTFType* cloneImpl(void) const;
    int getAlignmentMaxImpl(void) const {return maxAlign;}
    void setVarImpl(CTFVarPlace& varPlace) const;

    CTFTag resolveTagImpl(const char* tagStr,
        const char** tagStrEnd, bool isContinued) const;

    int getNSelectionsImpl(void) const {return fields.size();}
    std::string selectionToStrImpl(int index) const
        {return index > 0 ? fields[index - 1].name : std::string("");}

    void setTagImpl(CTFTag tag);
    void addFieldImpl(const std::string& fieldName,
        const CTFType* fieldType);
private:
    CTFTag tag;

    TypeVariant(const TypeVariant& TypeVariant);
    /* Ordered list of variant fields */
    typedef std::vector<Field> fields_t;
    fields_t fields;
    /* Key->index hash table, for search fields by name*/
    typedef HashTable<FieldKey, int, unsigned>
        fieldsTable_t;
    fieldsTable_t fieldsTable;
    /* Maximum alignment of variant variable and all of its subvariables */
    int maxAlign;

    /*
     * Mapping from enumeration indices into selection indices.
     *
     * Has a sence only when tag is set.
     */
    typedef std::vector<int> selectionMap_t;
    selectionMap_t selectionMap;

    /*
     * Mapping from enumeration string values to values.
     *
     * Really, this mapping depends on enumeration type only,
     * but enumeration type hasn't method for extract this mapping.
     *
     * Again, this mapping has a sence only when tag is set.
     */
    typedef std::map<std::string, int> enumMap_t;
    enumMap_t enumMap;

    friend class VarVariant;
};

TypeVariant::TypeVariant(void) : maxAlign(1)
{
}

TypeVariant::TypeVariant(const TypeVariant& typeVariant)
    : tag(typeVariant.tag), fields(typeVariant.fields),
    fieldsTable(typeVariant.fieldsTable), maxAlign(typeVariant.maxAlign)
{
}


CTFType* TypeVariant::cloneImpl(void) const
{
    return new TypeVariant(*this);
}

CTFTag TypeVariant::resolveTagImpl(const char* tagStr,
    const char** tagStrEnd, bool isContinued) const
{
    if(isContinued)
    {
        if(*tagStr != '.') return CTFTag();
        tagStr++;
    }

    fieldsTable_t::const_iterator iter = fieldsTable.find(tagStr);
    if(iter == fieldsTable.end()) return CTFTag();

    const Field& field = fields[iter->second];

    if(tagStrEnd) *tagStrEnd = tagStr + field.name.size();

    return CTFTag(this, field.name.c_str(), field.type);
}

void TypeVariant::setTagImpl(CTFTag tag)
{
    assert(tag.isConnected());
    if(this->tag.isConnected())
        throw std::logic_error("Attempt to set tag for variant, which "
            "already has tag.");

    if(!tag.getTargetType()->isEnum())
        throw std::logic_error("Attempt to set non-enumeration tag for "
            "variant");

    this->tag = tag;

    const CTFTypeEnum* typeTarget = static_cast<const CTFTypeEnum*>
        (tag.getTargetType());

    /* Create enumeration string->value mapping*/
    for(int i = 1; i < typeTarget->getNValues(); i++)
    {
        std::pair<std::string, int> v(typeTarget->valueToStr(i), i);
        enumMap.insert(v);
    }

    /*
     * Create enumeration->selection mapping and fill it for current
     * variant's fields.
     */
    selectionMap.resize(typeTarget->getNValues(), 0);

    int n_fields = fields.size();
    for(int i = 0; i < n_fields; i++)
    {
        enumMap_t::const_iterator iter = enumMap.find(fields[i].name);
        if(iter != enumMap.end())
        {
            selectionMap[iter->second] = i + 1;
        }
        //TODO: Warning about variant field, which doesn't corresponds
        // to any enumeration string?
    }
}

void TypeVariant::addFieldImpl(const std::string& fieldName,
    const CTFType* fieldType)
{
    fields.push_back(Field(fieldName, fieldType));

    std::pair<fieldsTable_t::iterator, bool> result =
        fieldsTable.insert(FieldKey(fields.back().name.c_str()),
            fields.size() - 1);
    if(!result.second)
    {
        fields.pop_back();
        throw std::logic_error(std::string("Attempt to add field with "
            "name ") + fieldName + " which already exists in the variant.");
    }

    int fieldMaxAlign = fieldType->getAlignmentMax();
    if(maxAlign < fieldMaxAlign)
        maxAlign = fieldMaxAlign;

    /* Update enumeration->selection mapping */
    enumMap_t::const_iterator iter = enumMap.find(fieldName);
    if(iter != enumMap.end())
    {
        selectionMap[iter->second] = fields.size();
    }
    //TODO: Warning about variant field, which doesn't corresponds
    // to any enumeration string?

}

/* Base class for variant variable */
class VarVariant : public CTFVarVariant
{
public:
    VarVariant(const TypeVariant* typeVariant);
    ~VarVariant(void);

    void setFields(void);
protected:
    int getAlignmentImpl(CTFContext& context) const
        {(void) context; return 1;}
    int getAlignmentImpl(void) const {return 1;}
    
    int getEndOffsetImpl(CTFContext& context) const;
    int getEndOffsetImpl(void) const {return -1;}
    
    int getSizeImpl(CTFContext& context) const;
    int getSizeImpl(void) const {return -1;}

    const CTFVar* resolveNameImpl(const char* name,
        const char** nameEnd, bool isContinued) const;

    const CTFType* getTypeImpl(void) const {return typeVariant;}

    const CTFVar* getSelectionImpl(int index) const;
    int getActiveIndexImpl(CTFContext& context) const;

private:
    class VariantFieldPlace: public CTFVarPlace
    {
    public:
        VariantFieldPlace(const VarVariant* varVariant,
            int selectionIndex) : varVariant(varVariant),
            selectionIndex(selectionIndex) {}

        const CTFVar* getParentVar(void) const {return varVariant;}
        const CTFVar* getContainerVar(void) const {return varVariant;}
        const CTFVar* getPreviousVar(void) const {return NULL;}
    protected:
        std::string getNameImpl(void) const
            {return varVariant->typeVariant->fields[selectionIndex - 1].name;}
        int isExistwithParent(CTFContext& context) const;
        int isExistwithParent(void) const {return -1;}
    private:
        const VarVariant* varVariant;
        int selectionIndex;
    };

    typedef std::vector<VariantFieldPlace*> fields_t;
    fields_t fields;

    CTFVarTag varTag;

    const TypeVariant* typeVariant;
};

int VarVariant::VariantFieldPlace::isExistwithParent
    (CTFContext& context) const
{
    int activeIndex = varVariant->getActiveIndex(context);
    if(activeIndex == -1) return -1;
    return activeIndex == selectionIndex ? 1 : 0;
}

VarVariant::VarVariant(const TypeVariant* typeVariant)
    : typeVariant(typeVariant) {}

void VarVariant::setFields(void)
{
    varTag = typeVariant->tag.instantiate(this);

    assert(varTag.getVarTarget()->isEnum());
    assert(varTag.getVarTarget()->getType()
        == typeVariant->tag.getTargetType());

    int n_fields = typeVariant->fields.size();

    for(int i = 0; i < n_fields; i++)
    {
        VariantFieldPlace* fieldPlace = new VariantFieldPlace(this, i + 1);
        fieldPlace->instantiateVar(typeVariant->fields[i].type);
        fields.push_back(fieldPlace);
    }
}

VarVariant::~VarVariant(void)
{
    clearPtrVector(fields);
}

int VarVariant::getEndOffsetImpl(CTFContext& context) const
{
    int activeIndex = getActiveIndex(context);
    if(activeIndex == -1) return -1;
    else if(activeIndex == 0) return getStartOffset(context);
    else return fields[activeIndex - 1]->getVar()->getEndOffset(context);
}

int VarVariant::getSizeImpl(CTFContext& context) const
{
    int activeIndex = getActiveIndex(context);
    if(activeIndex == -1) return -1;
    else if(activeIndex == 0) return 0;

    int startOffset = getStartOffset(context);
    if(startOffset == -1) return -1;

    return fields[activeIndex - 1]->getVar()->getEndOffset(context)
        - startOffset;
}

const CTFVar* VarVariant::resolveNameImpl(const char* name,
    const char** nameEnd, bool isContinued) const
{
    if(isContinued)
    {
        if(*name != '.') return NULL;
        name++;
    }

    TypeVariant::fieldsTable_t::const_iterator iter
        = typeVariant->fieldsTable.find(name);
    if(iter == typeVariant->fieldsTable.end()) return NULL;

    const Field& field = typeVariant->fields[iter->second];

    if(nameEnd) *nameEnd = name + field.name.size();

    return fields[iter->second]->getVar();
}

const CTFVar* VarVariant::getSelectionImpl(int index) const
{
    int n_fields = fields.size();
    if(index > n_fields)
        throw std::logic_error("Request variant's variable selection "
            "which has not been instantiated yet.");
    return index > 0 ? fields[index - 1]->getVar() : NULL;
}

int VarVariant::getActiveIndexImpl(CTFContext& context) const
{
    const CTFVarEnum* varTarget = static_cast<const CTFVarEnum*>
        (varTag.getVarTarget());

    switch(varTarget->isExist(context))
    {
    case 1:
    break;
    case 0:
        /* If tag isn't exist, selection index is always 0. */
        return 0;
    break;
    case -1:
        /* Without tag we can nothing to say about active selection. */
        return -1;
    break;
    }

    CTFContext* contextTarget = varTag.getContextTarget(context);
    if(contextTarget == NULL) return -1;

    varTarget->map(*contextTarget);

    int enumValue = varTarget->getValue(*contextTarget);

    varTag.putContextTarget(contextTarget);

    return typeVariant->selectionMap[enumValue];
}

/* Specializations for variant variable */
class VarVariantAbsolute: public VarVariant,
    public VarLayoutFixedAlignAbsolute
{
public:
    VarVariantAbsolute(const TypeVariant* typeVariant, int offset)
        : VarVariant(typeVariant),
        VarLayoutFixedAlignAbsolute(1, offset) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset(context); }
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset(); }
};

class VarVariantUseBase: public VarVariant,
    public VarLayoutFixedAlignUseBase
{
public:
    VarVariantUseBase(const TypeVariant* typeVariant,
        const CTFVar* varBase, int relativeOffset):
        VarVariant(typeVariant),
        VarLayoutFixedAlignUseBase(1, varBase, relativeOffset) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseBase::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUseBase::getStartOffset();}
};

class VarVariantUsePrev: public VarVariant,
    public VarLayoutFixedAlignUsePrev
{
public:
    VarVariantUsePrev(const TypeVariant* typeVariant,
        const CTFVar* varPrev):
        VarVariant(typeVariant),
        VarLayoutFixedAlignUsePrev(1, varPrev) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUsePrev::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUsePrev::getStartOffset();}
};

class VarVariantUseContainer: public VarVariant,
    VarLayoutFixedAlignUseContainer
{
public:
    VarVariantUseContainer(const TypeVariant* typeVariant,
        const CTFVar* varContainer):
        VarVariant(typeVariant),
        VarLayoutFixedAlignUseContainer(1, varContainer) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseContainer::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUseContainer::getStartOffset();}
};

void TypeVariant::setVarImpl(CTFVarPlace& varPlace) const
{
    CTFVarStartOffsetParams startOffsetParams;
    startOffsetParams.fill(varPlace, 1);

    VarVariant* varVariant = NULL;

    switch(startOffsetParams.layoutType)
    {
    case CTFVarStartOffsetParams::LayoutTypeAbsolute:
        varVariant = new VarVariantAbsolute(this,
            startOffsetParams.info.absolute.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseBase:
        varVariant = new VarVariantUseBase(this,
            startOffsetParams.info.useBase.var,
            startOffsetParams.info.useBase.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUsePrev:
        varVariant = new VarVariantUsePrev(this,
            startOffsetParams.info.usePrev.var);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseContainer:
        varVariant = new VarVariantUseContainer(this,
            startOffsetParams.info.useContainer.var);
    break;
    }

    varPlace.setVar(varVariant);

    varVariant->setFields();
}

CTFTypeVariant* CTFMeta::createTypeVariant(void) const
{
    return new TypeVariant();
}

/******************** Element context(common)**************************/
/*
 * This type is used by arrays and sequence as type of context for elements.
 */

class VarFlexer;
class ElemContext: public CTFVarArray::Elem
{
public:
    ElemContext(const VarFlexer* flexer, CTFContext* arrayContext,
        const CTFVar* elemVar, int nElems);
    
    void setStartOffset(int startOffset);
protected:
    int extendMapImpl(int newSize, const char** mapStart_p,
                                int* mapStartShift_p);
    Elem* nextImpl(void);
private:
    const VarFlexer* flexer;

    const CTFVar* elemVar;
    int nElems;

    int index;
};

int ElemContext::extendMapImpl(int newSize, const char** mapStart_p,
    int* mapStartShift_p)
{
    CTFContext* baseContext = getBaseContext();
    baseContext->map(newSize);

    *mapStart_p = baseContext->mapStart();
    *mapStartShift_p = baseContext->mapStartShift();

    return baseContext->mapSize();
}

/*
 * Flexer - variable which has assignable size for different contexts.
 *
 * This variable is used for prepend array element in its layout.
 */
class VarFlexer: public CTFVar
{
public:
    VarFlexer() {}

    void setStartOffset(int startOffset, CTFContext& context) const;
protected:
    void onPlaceChanged(const CTFVarPlace* placeOld);

    int getAlignmentImpl(CTFContext& context) const
		{(void) context; return 1;}
    int getAlignmentImpl(void) const
		{return 1;}
    
    int getSizeImpl(CTFContext& context) const;
    int getSizeImpl(void) const {return -1;}
    
	int getStartOffsetImpl(CTFContext& context) const
        {(void) context; return 0;}
	int getStartOffsetImpl(void) const
        {return 0;}
    
    int getEndOffsetImpl(CTFContext& context) const;
	int getEndOffsetImpl() const {return -1;}

    const CTFType* getTypeImpl(void) const {return NULL;}
private:
    int startOffsetIndex;
};

ElemContext::ElemContext(const VarFlexer* flexer, CTFContext* arrayContext,
    const CTFVar* elemVar, int nElems)
        : Elem(
            static_cast<const CTFVarPlaceContext*>(flexer->getVarPlace()),
            arrayContext),
        flexer(flexer), elemVar(elemVar), nElems(nElems), index(0)
{
   moveMap(arrayContext->mapSize(), arrayContext->mapStart(),
        arrayContext->mapStartShift());
}

void ElemContext::setStartOffset(int startOffset)
{
    flexer->setStartOffset(startOffset, *this);
}

CTFVarArray::Elem* ElemContext::nextImpl(void)
{
    ++index;

    if(index == nElems)
    {
        delete this;
        return NULL;
    }

    setStartOffset(elemVar->getEndOffset(*this));
    return this;
}



void VarFlexer::setStartOffset(int startOffset, CTFContext& context) const
{
    assert(context.getContextVar() == getVarPlace()->getContextVar());
    
    CTFContext* contextAdjusted = adjustContext(context);
    assert(contextAdjusted);
    
    *contextAdjusted->getCache(startOffsetIndex) = startOffset;
}

void VarFlexer::onPlaceChanged(const CTFVarPlace* placeOld)
{
    if(placeOld)
    {
        placeOld->getContextVar()->cancelCacheReservation(startOffsetIndex);
    }
    const CTFVarPlace* varPlace = getVarPlace();
    if(varPlace)
    {
        startOffsetIndex = varPlace->getContextVar()->reserveCache();
    }
}

int VarFlexer::getSizeImpl(CTFContext& context) const
{
    const CTFContext* contextAdjusted = adjustContext(context);
    if(contextAdjusted == NULL) return -1;
    int startOffset = *contextAdjusted->getCache(startOffsetIndex);
    assert(startOffset != -1);

    return startOffset;
}

int VarFlexer::getEndOffsetImpl(CTFContext& context) const
{
    return VarFlexer::getSizeImpl(context);
}

class VarPlaceFlexer: public CTFVarPlaceContext
{
public:
    VarPlaceFlexer(const CTFVar* parent, std::string varName)
        :parent(parent), varName(varName) {}

    const VarFlexer* getVar(void) const
        {return static_cast<const VarFlexer*>(CTFVarPlace::getVar());}

    const CTFVar* getParentVar(void) const {return parent;}
protected:
    std::string getNameImpl(void) const
        {return parent->name() + "." + varName;}
private:
    const CTFVar* parent;
    std::string varName;
};

/* Type which create flexer variable. */
class TypeFlexer : public CTFType
{
protected:
	CTFType* cloneImpl() const {return new TypeFlexer();}
	int getAlignmentMaxImpl(void) const {return 1;}

	void setVarImpl(CTFVarPlace& varPlace) const
        {varPlace.setVar(new VarFlexer());}
};

/*********************** Array type *******************************/
class TypeArray: public CTFTypeArray
{
public:
    TypeArray(int nElems, const CTFType* elemType)
        : nElems(nElems), elemType(elemType) {}
protected:
    CTFType* cloneImpl(void) const
        {return new TypeArray(nElems, elemType);}
    int getAlignmentImpl(void) const
        {return elemType->getAlignment();}
    int getAlignmentMaxImpl(void) const
        {return elemType->getAlignmentMax();}
    void setVarImpl(CTFVarPlace& varPlace) const;
private:
    int nElems;
    const CTFType* elemType;

    friend class VarArray;
};


/* Common part for variables of array and sequence types */
class VarArrayBase: public CTFVarArray
{
public:
    VarArrayBase(const CTFType* type, const CTFType* elemType);

    void setElems(void);
protected:
    int getAlignmentImpl(CTFContext& context) const
        {(void)context; return align;}
    int getAlignmentImpl(void) const
        {return align;}
    
    int getSizeImpl(CTFContext& context) const;
    int getSizeImpl(void) const;
    
    int getEndOffsetImpl(CTFContext& context) const;
    int getEndOffsetImpl(void) const;

    const CTFVar* resolveNameImpl(const char* name,
        const char** nameEnd, bool isContinued) const;

    const CTFType* getTypeImpl(void) const {return type;}

    Elem* beginImpl(CTFContext& arrayContext) const;

protected:
    class ArrayElemPlace: public CTFVarPlace
    {
    public:
        ArrayElemPlace(const VarArrayBase* varArrayBase)
            : varArrayBase(varArrayBase) {}

        const CTFVar* getParentVar(void) const {return varArrayBase;}
        const CTFVar* getPreviousVar(void) const
            {return varArrayBase->varPlaceFlexer.getVar();}
        const CTFVar* getContainerVar(void) const {return NULL;}
    protected:
        virtual std::string getNameImpl(void) const
            {return varArrayBase->name() + "[]";}
    private:
        const VarArrayBase* varArrayBase;
    };

    const CTFType* elemType;
    int align;
private:
    const CTFType* type;

    VarPlaceFlexer varPlaceFlexer;
    ArrayElemPlace arrayElemPlace;
};

VarArrayBase::VarArrayBase(const CTFType* type, const CTFType* elemType)
    : elemType(elemType), align(elemType->getAlignment()), type(type),
    varPlaceFlexer(this, "<flexer>"), arrayElemPlace(this)
{}

void VarArrayBase::setElems(void)
{
    TypeFlexer typeFlexer;
    varPlaceFlexer.instantiateVar(&typeFlexer);

    arrayElemPlace.instantiateVar(elemType);
}

int VarArrayBase::getSizeImpl(CTFContext& context) const
{
    int nElems = getNElems(context);
    if(nElems == -1) return -1;
    else if(nElems == 0) return 0;

    const CTFVar* varElem = arrayElemPlace.getVar();
    int elemAlign = varElem->getAlignment(context);
    if((elemAlign != -1) && (elemAlign <= align))
    {
        int elemSize = varElem->getSize(context);
        if(elemSize != -1)
        {
            return align_val(elemSize, elemAlign)
                * (nElems - 1) + elemSize;
        }
    }
    CTFContext* contextAdjusted = adjustContext(context);
    if(contextAdjusted == NULL) return -1;

    int startOffset = getStartOffset(*contextAdjusted);
    int endOffset = startOffset;

    for(Elem* elemContext = begin(*contextAdjusted);
        elemContext != NULL;
        elemContext = elemContext->next())
    {
        endOffset = varElem->getEndOffset(*elemContext);
    }

    return endOffset - startOffset;
}

int VarArrayBase::getSizeImpl(void) const
{
    int nElems = getNElems();
    if(nElems == -1) return -1;
    else if(nElems == 0) return 0;

    const CTFVar* varElem = arrayElemPlace.getVar();
    int elemAlign = varElem->getAlignment();
    if((elemAlign != -1) && (elemAlign <= align))
    {
        int elemSize = varElem->getSize();
        if(elemSize != -1)
        {
            return align_val(elemSize, elemAlign)
                * (nElems - 1) + elemSize;
        }
    }
    return -1;
}

int VarArrayBase::getEndOffsetImpl(CTFContext& context) const
{
    CTFContext* contextAdjusted = adjustContext(context);
    if(contextAdjusted == NULL) return -1;

    int startOffset = getStartOffset(*contextAdjusted);

    int nElems = getNElems(context);
    if(nElems == -1) return -1;
    else if(nElems == 0) return startOffset;

    const CTFVar* varElem = arrayElemPlace.getVar();
    int elemAlign = varElem->getAlignment(context);
    if((elemAlign != -1) && (elemAlign <= align))
    {
        int elemSize = varElem->getSize(context);
        if(elemSize != -1)
        {
            return startOffset + align_val(elemSize, elemAlign)
                    * (nElems - 1) + elemSize;
        }
    }

    int endOffset = startOffset;

    for(Elem* elemContext = begin(*contextAdjusted);
        elemContext != NULL;
        elemContext = elemContext->next())
    {
        endOffset = varElem->getEndOffset(*elemContext);
    }

    return endOffset;
}

int VarArrayBase::getEndOffsetImpl(void) const
{
    int startOffset = getStartOffset();
    if(startOffset == -1) return -1;
    
    int size = getSize();
    if(size == -1) return -1;
    
    return startOffset + size;
}


const CTFVar* VarArrayBase::resolveNameImpl(const char* name,
    const char** nameEnd, bool isContinued) const
{
    (void)isContinued;
    if((name[0] != '[') || name[1] != ']') return NULL;
    if(nameEnd) *nameEnd = name + 2;

    return arrayElemPlace.getVar();
}

CTFVarArray::Elem* VarArrayBase::beginImpl(CTFContext& arrayContext) const
{
    CTFContext* contextAdjusted = adjustContext(arrayContext);
    assert(contextAdjusted);

    ElemContext* elemContext = new ElemContext(varPlaceFlexer.getVar(),
        contextAdjusted,
        arrayElemPlace.getVar(), getNElems(*contextAdjusted));

    elemContext->setStartOffset(getStartOffset(*contextAdjusted));
    
    return elemContext;
}

/* Really array variable. */
class VarArray: public VarArrayBase
{
public:
    VarArray(const TypeArray* typeArray)
        : VarArrayBase(typeArray, typeArray->elemType),
        nElems(typeArray->nElems)  {}
protected:
    int getNElemsImpl(CTFContext& context) const
    {
        (void)context;
        return nElems;
    }
    
    int getNElemsImpl(void) const {return nElems;}
private:
    int nElems;
};

/* Layout specializations for array variable */
class VarArrayAbsolute: public VarArray,
    public VarLayoutFixedAlignAbsolute
{
public:
    VarArrayAbsolute(const TypeArray* typeArray, int offset)
        : VarArray(typeArray),
        VarLayoutFixedAlignAbsolute(typeArray->getAlignment(), offset) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset();}
};

class VarArrayUseBase: public VarArray,
    public VarLayoutFixedAlignUseBase
{
public:
    VarArrayUseBase(const TypeArray* typeArray, const CTFVar* varBase,
        int relativeOffset) : VarArray(typeArray),
        VarLayoutFixedAlignUseBase(typeArray->getAlignment(), varBase,
            relativeOffset) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseBase::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUseBase::getStartOffset();}
};

class VarArrayUsePrev: public VarArray,
    public VarLayoutFixedAlignUsePrev
{
public:
    VarArrayUsePrev(const TypeArray* typeArray, const CTFVar* varPrev)
        : VarArray(typeArray),
        VarLayoutFixedAlignUsePrev(typeArray->getAlignment(), varPrev) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUsePrev::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUsePrev::getStartOffset();}
};

class VarArrayUseContainer: public VarArray,
    public VarLayoutFixedAlignUseContainer
{
public:
    VarArrayUseContainer(const TypeArray* typeArray,
        const CTFVar* varContainer)
        : VarArray(typeArray),
        VarLayoutFixedAlignUseContainer(typeArray->getAlignment(), varContainer) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseContainer::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUseContainer::getStartOffset();}
};

void TypeArray::setVarImpl(CTFVarPlace& varPlace) const
{
    CTFVarStartOffsetParams startOffsetParams;
    startOffsetParams.fill(varPlace, elemType->getAlignment());

    VarArray* varArray = NULL;

    switch(startOffsetParams.layoutType)
    {
    case CTFVarStartOffsetParams::LayoutTypeAbsolute:
        varArray = new VarArrayAbsolute(this,
            startOffsetParams.info.absolute.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseBase:
        varArray = new VarArrayUseBase(this,
            startOffsetParams.info.useBase.var,
            startOffsetParams.info.useBase.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUsePrev:
        varArray = new VarArrayUsePrev(this,
            startOffsetParams.info.usePrev.var);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseContainer:
        varArray = new VarArrayUseContainer(this,
            startOffsetParams.info.useContainer.var);
    break;
    }

    varPlace.setVar(varArray);

    varArray->setElems();
}

CTFTypeArray* CTFMeta::createTypeArray(int size,
    const CTFType* elemType) const
{
    return new TypeArray(size, elemType);
}

/*********************** Sequence type *******************************/
class TypeSequence: public CTFTypeSequence
{
public:
    TypeSequence(CTFTag tagNElems, const CTFType* elemType);
protected:
    CTFType* cloneImpl(void) const
        {return new TypeSequence(tagNElems, elemType);}
    int getAlignmentImpl(void) const
        {return elemType->getAlignment();}
    int getAlignmentMaxImpl(void) const
        {return elemType->getAlignmentMax();}
    void setVarImpl(CTFVarPlace& varPlace) const;
private:
    CTFTag tagNElems;
    const CTFType* elemType;

    friend class VarSequence;
};

TypeSequence::TypeSequence(CTFTag tagNElems, const CTFType* elemType)
    : tagNElems(tagNElems), elemType(elemType)
{
    const CTFType* typeNElems = tagNElems.getTargetType();
    if(!typeNElems->isInt())
        throw std::logic_error("Attempt to create sequence which tagged "
            "type is not integer.");
}

/* Sequence variable. */
class VarSequence: public VarArrayBase
{
public:
    VarSequence(const TypeSequence* typeSequence);

    void setElems(void);
protected:
    int getNElemsImpl(CTFContext& context) const;
    int getNElemsImpl(void) const {return -1;}
private:
    CTFVarTag tagNElems;
};

VarSequence::VarSequence(const TypeSequence* typeSequence)
    : VarArrayBase(typeSequence, typeSequence->elemType) {}

void VarSequence::setElems(void)
{
    const TypeSequence* typeSequence = static_cast<const TypeSequence*>
        (getType());

    tagNElems = typeSequence->tagNElems.instantiate(this);
    assert(tagNElems.getVarTarget()->isInt());
    assert(tagNElems.getVarTarget()->getType()
        == typeSequence->tagNElems.getTargetType());

    VarArrayBase::setElems();
}

int VarSequence::getNElemsImpl(CTFContext& context) const
{
    CTFContext* tagContext = tagNElems.getContextTarget(context);
    if(tagContext == NULL) return -1;

    const CTFVarInt* varNElems = static_cast<const CTFVarInt*>
        (tagNElems.getVarTarget());

    int nElems = varNElems->getInt32(*tagContext);

    tagNElems.putContextTarget(tagContext);

    return nElems >= 0 ? nElems : 0;
}

/* Layout specializations for sequence variable */
class VarSequenceAbsolute: public VarSequence,
    public VarLayoutFixedAlignAbsolute
{
public:
    VarSequenceAbsolute(const TypeSequence* typeSequence, int offset)
        : VarSequence(typeSequence),
        VarLayoutFixedAlignAbsolute(typeSequence->getAlignment(), offset) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignAbsolute::getStartOffset();}
};

class VarSequenceUseBase: public VarSequence,
    public VarLayoutFixedAlignUseBase
{
public:
    VarSequenceUseBase(const TypeSequence* typeSequence,
        const CTFVar* varBase, int relativeOffset):
        VarSequence(typeSequence), VarLayoutFixedAlignUseBase(
            typeSequence->getAlignment(), varBase, relativeOffset) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseBase::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUseBase::getStartOffset();}
};

class VarSequenceUsePrev: public VarSequence,
    public VarLayoutFixedAlignUsePrev
{
public:
    VarSequenceUsePrev(const TypeSequence* typeSequence, const CTFVar* varPrev)
        : VarSequence(typeSequence), VarLayoutFixedAlignUsePrev(
            typeSequence->getAlignment(), varPrev) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUsePrev::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUsePrev::getStartOffset();}
};

class VarSequenceUseContainer: public VarSequence,
    public VarLayoutFixedAlignUseContainer
{
public:
    VarSequenceUseContainer(const TypeSequence* typeSequence,
        const CTFVar* varContainer)
        : VarSequence(typeSequence), VarLayoutFixedAlignUseContainer(
            typeSequence->getAlignment(), varContainer) {}
protected:
    int getStartOffsetImpl(CTFContext& context) const
        {return VarLayoutFixedAlignUseContainer::getStartOffset(context);}
    int getStartOffsetImpl(void) const
        {return VarLayoutFixedAlignUseContainer::getStartOffset();}
};

void TypeSequence::setVarImpl(CTFVarPlace& varPlace) const
{
    CTFVarStartOffsetParams startOffsetParams;
    startOffsetParams.fill(varPlace, elemType->getAlignment());

    VarSequence* varSequence = NULL;

    switch(startOffsetParams.layoutType)
    {
    case CTFVarStartOffsetParams::LayoutTypeAbsolute:
        varSequence = new VarSequenceAbsolute(this,
            startOffsetParams.info.absolute.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseBase:
        varSequence = new VarSequenceUseBase(this,
            startOffsetParams.info.useBase.var,
            startOffsetParams.info.useBase.offset);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUsePrev:
        varSequence = new VarSequenceUsePrev(this,
            startOffsetParams.info.usePrev.var);
    break;
    case CTFVarStartOffsetParams::LayoutTypeUseContainer:
        varSequence = new VarSequenceUseContainer(this,
            startOffsetParams.info.useContainer.var);
    break;
    }

    varPlace.setVar(varSequence);

    varSequence->setElems();
}

CTFTypeSequence* CTFMeta::createTypeSequence(CTFTag tagNElems,
    const CTFType* elemType) const
{
    return new TypeSequence(tagNElems, elemType);
}
