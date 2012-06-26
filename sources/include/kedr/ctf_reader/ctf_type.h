#ifndef CTF_TYPE_H_INCLUDED
#define CTF_TYPE_H_INCLUDED

#include <cstddef>

#include <stdint.h> /*int32_t and co.*/

#include <kedr/ctf_reader/ctf_tag.h>

#include <string>

class CTFMeta;
class CTFVar;
class CTFVarPlace;

class CTFType
{
public:
    virtual ~CTFType(void) {};
    /* Create type equal to the given one */
    CTFType* clone(void) const {return cloneImpl();}
    /*
     * Return alignment of the type, this value is used by containers
     * such as structure or arrays for align themselves.
     * 
     * Return -1 if alignment is not defined for the type
     */
    int getAlignment(void) const {return getAlignmentImpl();}
    /*
     * Return maximum alignment for variable, created by the type,
     * and all its sub-variables.
     * 
     * This value may be used by root type for align context variables.
     */
    int getAlignmentMax(void) const {return getAlignmentMaxImpl();}
    /* Create variable corresponded to the type and connect it with given place */
    void setVar(CTFVarPlace& varPlace) const {setVarImpl(varPlace);}
    /*
     * Return tag, which based on given type, and which points to place
     * corresponded to tagStr.
     *
     * If fail to resolve tag, return disconnected tag.
     */
    CTFTag resolveTag(const char* tagStr) const;
    CTFTag resolveTag(const std::string& tagStr) const;
    /* RTTI */
    virtual int isInt(void) const {return 0;}
    virtual int isStruct(void) const {return 0;}
    virtual int isEnum(void) const {return 0;}
    virtual int isArray(void) const {return 0;}
    virtual int isSequence(void) const {return 0;}
protected:
    virtual CTFType* cloneImpl(void) const = 0;
    virtual int getAlignmentImpl(void) const {return -1;}
    virtual int getAlignmentMaxImpl(void) const = 0;
    virtual void setVarImpl(CTFVarPlace& varPlace) const = 0;
    /*
     * Resolve(partially) tagStr as tag based on given type.
     *
     * After successfull call, tagStrEnd contains pointer to the end
     * of resolved part.
     *
     * If failed to resolve, return disconnected tag.
     * 
     * 'isContinued' flag is true when given component is not first,
     *  so delimited may be required.
     */
    virtual CTFTag resolveTagImpl(const char* tagStr,
        const char** tagStrEnd, bool isContinued) const
        {(void)tagStr; (void)tagStrEnd; (void)isContinued; return CTFTag();}
};




class CTFTypeEnum;
/* Type represented integer with fixed align and size. */
class CTFTypeInt : public CTFType
{
public:
    int isInt(void) const {return 1;}
    CTFTypeInt* clone(void) const
        {return static_cast<CTFTypeInt*>(CTFType::clone());}
    
    /* Parameters of integer type */
    int getSize(void) const {return getSizeImpl();}
    /* Alignment is returned by base type */
    int isSigned(void) const {return isSignedImpl();}

    enum ByteOrder {be = 0, le};
    enum ByteOrder getByteOrder(void) const {return getByteOrderImpl();}
    /* Set parameters for just constructed type */
    void setSize(int size) {return setSizeImpl(size);}
    void setAlignment(int align) {return setAlignmentImpl(align);}
    void setSigned(int isSigned) {return setSignedImpl(isSigned);}
    void setByteOrder(enum ByteOrder byteOrder)
        {return setByteOrderImpl(byteOrder);}
    /*
     * Fix parameters of the type.
     *
     * Throw exception if insufficient or incorrect parameters.
     */
    void fixParams(void) {return fixParamsImpl();}
    /*
     * Create enumeration type based on given interger type.
     */
    CTFTypeEnum* createEnum(void) const {return createEnumImpl();}
protected:
    virtual int getSizeImpl(void) const = 0;
    /* Alignment is returned by base type */
    virtual int isSignedImpl(void) const = 0;
    virtual enum ByteOrder getByteOrderImpl(void) const = 0;

    virtual void setSizeImpl(int size) = 0;
    virtual void setAlignmentImpl(int align) = 0;
    virtual void setSignedImpl(int isSigned) = 0;
    virtual void setByteOrderImpl(enum ByteOrder byteOrder) = 0;

    virtual void fixParamsImpl(void) = 0;

    virtual CTFTypeEnum* createEnumImpl(void) const = 0;
};

class CTFTypeStruct : public CTFType
{
public:
    CTFTypeStruct* clone(void) const
        {return static_cast<CTFTypeStruct*>(CTFType::clone());}
    /* Modify parameters for structure type */
    void addField(const std::string& fieldName, const CTFType* fieldType)
        {addFieldImpl(fieldName, fieldType);}

protected:
    virtual void addFieldImpl(const std::string& fieldName, const CTFType* fieldType) = 0;
};


class CTFTypeEnum : public CTFType
{
public:
    int isEnum(void) const {return 1;}
    /* 
     * Convert value with given index into string.
     * 
     * Value with 0 index always corresponds to empty string.
     */
    std::string valueToStr(int index) const
        {return valueToStrImpl(index);}
    /* 
     * Return number of values for given enumeration.
     * 
     * Each enumeration has at least one value - empty string.
     */
    int getNValues(void) const
        {return getNValuesImpl();}

    /* Modify parameters for enumeration type */
    void addValue32(const char* valueName, int32_t start, int32_t end)
        {return addValue32Impl(valueName, start, end);}
    void addValueU32(const char* valueName, uint32_t start, uint32_t end)
        {return addValueU32Impl(valueName, start, end);}
    void addValue64(const char* valueName, int64_t start, int64_t end)
        {return addValue64Impl(valueName, start, end);}
    void addValueU64(const char* valueName, uint64_t start, uint64_t end)
        {return addValueU64Impl(valueName, start, end);}
protected:
    virtual std::string valueToStrImpl(int index) const = 0;
    virtual int getNValuesImpl(void) const = 0;
    
    virtual void addValue32Impl(const char* valueName,
        int32_t start, int32_t end) = 0;
    virtual void addValueU32Impl(const char* valueName,
        uint32_t start, uint32_t end) = 0;
    virtual void addValue64Impl(const char* valueName,
        int64_t start, int64_t end) = 0;
    virtual void addValueU64Impl(const char* valueName,
        uint64_t start, uint64_t end) = 0;
};

class CTFTypeVariant : public CTFType
{
public:
    CTFTypeVariant* clone(void) const
        {return static_cast<CTFTypeVariant*>(CTFType::clone());}

    /* 
     * Return number of possibles selections for given variant.
     * 
     * Each selection corresponds either variant field or nothing,
     * so any variant has at least 1 possible selection.
     */
    int getNSelections(void) const
        {return getNSelectionsImpl();}

    /* 
     * Convert selection with given index into string.
     * 
     * For selection corresponded to variant field string contains
     * name(relative) of that field.
     * 
     * Selection with 0 index always corresponds to empty string.
     */
    std::string selectionToStr(int index) const
        {return selectionToStrImpl(index);}


    /* Modify parameters for variant type */
    void setTag(CTFTag tag) {setTagImpl(tag);}
    
    void addField(const std::string& fieldName, const CTFType* fieldType)
        {addFieldImpl(fieldName, fieldType);}

protected:
    virtual int getNSelectionsImpl(void) const = 0;
    virtual std::string selectionToStrImpl(int index) const = 0;
    
    virtual void setTagImpl(CTFTag tag) = 0;
    virtual void addFieldImpl(const std::string& fieldName,
        const CTFType* fieldType) = 0;
};


class CTFTypeArray: public CTFType
{
public:
    int isArray(void) const {return 1;}
    
    CTFTypeArray* clone(void) const
        {return static_cast<CTFTypeArray*>(CTFType::clone());}

};

class CTFTypeSequence: public CTFType
{
public:
    int isSequence(void) const {return 1;}
    CTFTypeSequence* clone(void) const
        {return static_cast<CTFTypeSequence*>(CTFType::clone());}

};

/********************** Layout Support ********************************/
/*
 * One of the key factors of such classes hierarchy
 * is support for optimization layout functions per variable.
 *
 * Using context information, layout functions tell what alignment and
 * size variable has, and where variable starts and ends.
 *
 * Mapping ranges of all variables are arranged according to types
 * interpretation. E.g., variable corresponded to the first field of the
 * structure lays just before variable, corresponded to the second field
 * of the structure.
 *
 * So, the simplest way for implement layout functions for given variable
 * is to use result of layout function for another variable, which is
 * lays just before given one('previous' variable).
 * Then, with knowledge of own alignemnt and size, layout functions
 * may be implemented as follows:
 *
 * int getAlignment() {return alignment;}
 * int getStartOffset() {return align_val(prev->getEndOffset(), alignment);}
 * int getSize() {return size;}
 * int getEndOffset() {return align_val(prev->getEndOffset(), alignment) + size;}
 *
 * But such implementation take a long time for execute:
 * e.g., calculation of the start offset of the 20th variable requires
 * calls of layout functions for all previous 19 variables.
 *
 * Now consider situation, when previous variable has constant size and
 * that alignment for all variables is 1bit. In that case, size of
 * previous variable may be stored for current variable,
 * and its getStartOffset() function may be implemented such:
 *
 * int getStartOffset() {return prev2->getEndOffset() + prev_size;}
 *
 * where prev2 is variable before previous one, and prev_size - stored
 * size of previous variable. Note here, that we do not call any layout
 * function for previous variable ('prev_size' is determined at variable
 * construction stage).
 *
 * If prev2 (previous of previous) variable has constant size too, layout
 * function become
 *
 * int getStartOffset() {return prev3->getEndOffset() + prevs_size;}
 *
 * where prevs_size = prev_size + prev2_size. And so on.
 *
 *
 * We devide all layout situation into 4 categories according to
 * implementation of layout function getStartOffset():
 *
 * 1) "absolute" (offset):
 *  {return offset;}
 * 2) "use base" (variable):
 *  {return varBase->getStartOffset() + relative_offset;}
 * 3) "use prev" (variable):
 *  {return align_val(varPrev->getEndOffset(), align);}
 * 4) "use container" (variable);
 *  {return align_val(varContainer->getStartOffset(), align);}
 */



/* Parameters for calculate start offset of variable */
struct CTFVarStartOffsetParams
{
    int align;
    enum LayoutType
    {
        LayoutTypeAbsolute,
        LayoutTypeUseBase,
        LayoutTypeUsePrev,
        LayoutTypeUseContainer
    } layoutType;
    union
    {
        struct {int offset;} absolute;
        struct {const CTFVar* var; int offset;} useBase;
        struct {const CTFVar* var;} usePrev;
        struct {const CTFVar* var;} useContainer;
    } info;
    /*
     * Fill parameters for variable.
     *
     * 'varPlace' is place, to which variable will be connected;
     * 'align' is an alignment of variable(may be -1).
     *
     * Note, that 'align' field in resuled structure may exceed
     * corresponded parameter of the function.
     */
    void fill(CTFVarPlace& varPlace, int align);
};

#endif // CTF_TYPE_H_INCLUDED
