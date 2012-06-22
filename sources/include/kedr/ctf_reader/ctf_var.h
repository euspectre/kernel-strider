/*
 * Essence which cover some bits range in memory and may be interpreted
 * in some way.
 */

#ifndef CTF_VAR_H_INCLUDED
#define CTF_VAR_H_INCLUDED

#include <string>

#include <stdint.h>

#include <kedr/ctf_reader/ctf_meta.h>
#include <kedr/ctf_reader/ctf_var_place.h>
#include <kedr/ctf_reader/ctf_context.h>


/* 
 * For check correctness of variables layout calculations
 * compile with this macro:
 * 
 * CTF_VAR_CHECK_LAYOUT
 */

class CTFVar
{
public:
    /* Create variable without place */
    CTFVar(void);
    virtual ~CTFVar(void);
    /* Search variable relative to given one. */
    const CTFVar* findVar(const char* name) const;
    const CTFVar* findVar(const std::string& name) const
        {return findVar(name.c_str());}
    
    /*
     * Determine, whether variable exists in the given context.
     *
     * Possible results:
     *  1 - variable definitely exist in the given context or any context
     *      based(may be, indirectly) on it.
     *  0 - variable definitly does not exist in the given context or any
     *      context based on it.
     * -1 - variable may exists in some contexts based on given, but may do not.
     */
    int isExist(CTFContext& context) const
        {return varPlace->isExist(context);}
    
    int isExist(void) const {return varPlace->isExist();}

    /*
     * Determine layout of variable.
     *
     * If function returns non-negative value, corresponded layout parameter is
     * same in all contexts based on given one, in which variable exists(!).
     *
     * -1 may be returned for any context which is not contain variable. This
     * means that corresponded parameter is not constant, or probably not
     * constant.
     *
     * But it is better to return (non-negative) parameter as soon as it
     * constant - this optimize layout calculations for other variables.
     */

#ifndef CTF_VAR_CHECK_LAYOUT
    int getAlignment(CTFContext& context) const
        {return getAlignmentImpl(context);}
    int getStartOffset(CTFContext& context) const
        {return getStartOffsetImpl(context);}
    int getSize(CTFContext& context) const
        {return getSizeImpl(context);}
    int getEndOffset(CTFContext& context) const
        {return getEndOffsetImpl(context);}
#else
    int getAlignment(CTFContext& context) const
        {return getAlignmentImpl(context);}
    int getStartOffset(CTFContext& context) const;
    int getSize(CTFContext& context) const
        {return getSizeImpl(context);}
    int getEndOffset(CTFContext& context) const
        {return getEndOffsetImpl(context);}
#endif
    
    /* Same methods but without concrete context. */
#ifndef CTF_VAR_CHECK_LAYOUT
    int getAlignment(void) const {return getAlignmentImpl();}
    int getStartOffset(void) const {return getStartOffsetImpl();}
    int getSize(void) const {return getSizeImpl();}
    int getEndOffset(void) const {return getEndOffsetImpl();}
#else
    int getAlignment(void) const {return getAlignmentImpl();}
    int getStartOffset(void) const;
    int getSize(void) const {return getSizeImpl();}
    int getEndOffset(void) const {return getEndOffsetImpl();}
#endif

    /* 
     * Return context in chain, based on 'context' argument, which
     * cover given variable.
     * 
     * If no context in chain cover given variable, return NULL.
     * 
     * Using adjusted context instead of lower context in the same chain
     * make variable interpretation slightly faster.
     */
    CTFContext* adjustContext(CTFContext& context) const
        {return varPlace->adjustContext(context);}
    const CTFContext* adjustContext(const CTFContext& context) const
        {return varPlace->adjustContext(context);}

    /*
     * Make variable mapped in the context.
     * 
     * Return context, which really contains mapped variable
     * (input context may contain variable via chain).
     *
     * After this call mapping of the variable may be requested
     * or variable may be interpreted in other ways.
     *
     * Should be called only when exists() returns 1.
     */
    CTFContext& map(CTFContext& context) const;

    /*
     * Return mapping of variable into memory.
     *
     * mapStartShift_p, if not NULL, will contain shift (0-7 bits) of start
     * mapping.
     *
     * Should be called only after map() return success.
     * (Or when it is known that variable is already mapped.)
     *
     * Function may be used for user-defined interpretation of variable.
     */
    const char* getMap(CTFContext& context, int* mapStartShift_p) const;
    /*
     * Return name of the variable.
     */
    std::string name(void) const {return varPlace->getName();}

    /*
     * Return type which create given variable.
     * 
     * Internal variables may safetly return NULL.
     */
    const CTFType* getType(void) const {return getTypeImpl();}

    /*
     * Return parent variable for this one.
     */
    const CTFVar* getParent(void) const
        {return varPlace->getParentVar();}
    
    const CTFVar* getContainer(void) const
        {return varPlace->getContainerVar();}
    
    const CTFVar* getPrevious(void) const
        {return varPlace->getPreviousVar();}

    /* Return place for which variable is set. */
    const CTFVarPlace* getVarPlace(void) const {return varPlace;}
    /* Variable classification */
    virtual int isInt(void) const {return 0; }
    virtual int isEnum(void) const {return 0; }
    virtual int isVariant(void) const {return 0; }
    virtual int isArray(void) const {return 0; }
    //TODO: Other variable types
protected:
    /* Callback called after place of variable is changed */
    virtual void onPlaceChanged(const CTFVarPlace* placeOld)
        {(void)placeOld;}

    virtual int getAlignmentImpl(CTFContext& context) const = 0;
    virtual int getStartOffsetImpl(CTFContext& context) const = 0;
    virtual int getEndOffsetImpl(CTFContext& context) const = 0;
    virtual int getSizeImpl(CTFContext& context) const = 0;
    
    virtual int getAlignmentImpl(void) const = 0;
    virtual int getStartOffsetImpl(void) const = 0;
    virtual int getEndOffsetImpl(void) const = 0;
    virtual int getSizeImpl(void) const = 0;

    /* 
     * Resolve (partially) name of variable relative to given one.
     * 
     * Return variable which has name resolved, set nameEnd to
     * the first unresolved character in the name.
     * 
     * 'isContinued' flag is true when given component is not first,
     *  so delimited may be required.
     */
    virtual const CTFVar* resolveNameImpl(const char* name,
        const char** nameEnd, bool isContinued) const
        {(void)name; (void)nameEnd; (void)isContinued ;return NULL;}

    virtual const CTFType* getTypeImpl(void) const = 0;
private:
    CTFVar(const CTFVar& var); /* not implemented */

    CTFVarPlace* varPlace;

friend class CTFVarPlace;
};

/* Integer specialization of variable. */
class CTFVarInt: public CTFVar
{
public:
    int isInt(void) const {return 1;}
    const CTFTypeInt* getType(void) const
        {return static_cast<const CTFTypeInt*>(CTFVar::getType());}

    int32_t getInt32(CTFContext& context) const
        {return getInt32Impl(context);}
    int64_t getInt64(CTFContext& context) const
        {return getInt64Impl(context);}
    uint32_t getUInt32(CTFContext& context) const
        {return getUInt32Impl(context);}
    uint64_t getUInt64(CTFContext& context) const
        {return getUInt64Impl(context);}
protected:
    virtual int32_t getInt32Impl(CTFContext& context) const = 0;
    virtual int64_t getInt64Impl(CTFContext& context) const = 0;
    virtual uint32_t getUInt32Impl(CTFContext& context) const = 0;
    virtual uint64_t getUInt64Impl(CTFContext& context) const = 0;
};

/* Enumeration specialization of variable. */
class CTFVarEnum: public CTFVarInt
{
public:
    int isEnum(void) const {return 1;}
    const CTFTypeEnum* getType(void) const
        {return static_cast<const CTFTypeEnum*>(CTFVar::getType());}
    
    /* 
     * Return index of the enumeration value, corresponded to the
     * integer value.
     */
    int getValue(CTFContext& context) const
        {return getValueImpl(context);}
    /* 
     * Return string corresponded to the integer value.
     * 
     * Return empty string if integer value of variable has no corresponded string.
     */
    std::string getEnum(CTFContext& context) const
        {return static_cast<const CTFTypeEnum*>(getType())->valueToStr(getValueImpl(context));}
protected:    
    virtual int getValueImpl(CTFContext& context) const = 0;
};

class CTFVarVariant: public CTFVar
{
public:
    int isVariant(void) const {return 1;}
    const CTFTypeVariant* getType(void) const
        {return static_cast<const CTFTypeVariant*>(CTFVar::getType());}
        
    /*
     * Return field of the variant variable corresponded to given
     * selection index(see CTFTypeVariant).
     * 
     * NULL corresponds to 0 index.
     */
    const CTFVar* getSelection(int index) const
        {return getSelectionImpl(index);}
    /*
     * Return index of the selection which is currently active.
     * 
     * If no field corresponds to the tag string value, or no tag string
     * value corresponds to tag integer value, return 0.
     * 
     * NOTE: This function is interpretation, so it may be called
     * only when variant is mapped. In that case tag is mapped too
     * (or tag is definitly not exist) and its integer value is defined.
     */
    int getActiveIndex(CTFContext& context) const
        {return getActiveIndexImpl(context);}
    /*
     * Return currently active field.
     * 
     * If no field corresponds to the tag string value, or no tag string
     * value corresponds to tag integer value, return 0.
     * 
     * See also getActiveIndex().
     */
    const CTFVar* getActiveField(CTFContext& context) const
        {return getSelection(getActiveIndex(context));}
protected:
    virtual const CTFVar* getSelectionImpl(int index) const = 0;
    virtual int getActiveIndexImpl(CTFContext& context) const = 0;
};

/* 
 * Array or sequence variable as sequence of variables with same type
 * but different interpretation.
 */
class CTFVarArray: public CTFVar
{
public:
    int isArray(void) const {return 1;}
    
    /* 
     * Return number of elements in the array/sequence.
     * 
     * For sequence, if context insufficient, return -1.
     */
    int getNElems(CTFContext& context) const
        {return getNElemsImpl(context);}
    /* Variant for arrays. For sequences always return -1. */
    int getNElems(void) const {return getNElemsImpl();}
    
    /* Context for one element in the array */
    class Elem;
    /* Iterator through elements in the array */
    class ElemIterator;
    
    /* 
     * Create context for the first element in the sequence.
     * 
     * 'arrayContext' is context for array variable itself.
     * 
     * If array doesn't have any element, return NULL.
     * 
     * Note, that even this method behaves like variable interpretation,
     * array variable isn't required to be fully mapped inside this
     * context.
     * 
     * First element is garanteed to be mapped by context created only
     * when array context already maps it. Same for contexts obtained
     * from created one via next() method.
     */
    Elem* begin(CTFContext& arrayContext) const
        {return beginImpl(arrayContext);}

protected:
    virtual int getNElemsImpl(CTFContext& context) const = 0;
    virtual int getNElemsImpl(void) const = 0;
    virtual Elem* beginImpl(CTFContext& arrayContext) const = 0;
};

/* Element of the array-like variable */
class CTFVarArray::Elem: public CTFContext
{
public:
    Elem(const CTFVarPlaceContext* contextVar,
        CTFContext* baseContext = NULL)
        : CTFContext(contextVar, baseContext), refs(1) {}
    /* 
     * Return context for the next element in the array/sequence.
     * 
     * Current context is destroyed.
     * 
     * If current element is last in the array/sequence, return NULL.
     */
    Elem* next(void) {return nextImpl();}
    
    void ref() {++refs;}
    void unref() {if(--refs == 0) delete this;}
protected:
    virtual Elem* nextImpl(void) = 0;
private:
    int refs;
};

class CTFVarArray::ElemIterator
{
public:
    /* Create past-the-end iterator*/
    ElemIterator() : elem(NULL) {}
    /* Create iterator pointed to the first element in the array */
    ElemIterator(const CTFVarArray& varArray, CTFContext& context)
        : elem(varArray.begin(context)) {}
    /* Copy iterator */
    ElemIterator(const ElemIterator& elemIterator)
        : elem(elemIterator.elem) {if(elem) elem->ref();}
    
    ~ElemIterator(void) {if(elem) elem->unref();}
    
    ElemIterator& operator=(const ElemIterator& iter)
    {
        if(iter.elem) iter.elem->ref();
        if(elem) elem->unref();
        elem = iter.elem;
        return *this;
    }

    /* Common iterator declarations and methods */
    typedef int                         difference_type;
    typedef std::forward_iterator_tag   iterator_category;
    typedef Elem                        value_type;
    typedef Elem&                       reference_type;
    typedef Elem*                       pointer_type;

    /* Iterators are compared via their bool representation */
    operator bool(void) const {return elem != NULL;}

    reference_type operator*(void) const
        { return *elem;}
    pointer_type operator->(void) const
        { return elem;}

    ElemIterator& operator++(void)
        {elem = elem->next(); return *this;}
private:
    Elem* elem;
};
#endif // CTF_VAR_H_INCLUDED
