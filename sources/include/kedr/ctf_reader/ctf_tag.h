/*
 * CTF tag - pointer to some place in type-field hierarchy.
 *
 * Variants and sequences use this pointer to refer to their base
 * enumeration and integer correspondingly.
 */

#ifndef CTF_TAG_H_INCLUDED
#define CTF_TAG_H_INCLUDED

class CTFType;
class CTFVar;
class CTFContext;

#include <vector>
#include <cstddef>

class CTFVarTag;

class CTFTag
{
public:
    /* Create disconnected tag. Used for error-reporting. */
    CTFTag(void);

    int isConnected(void) const {return baseType != NULL;}

    class Component
    {
    public:
        Component(const char* varName, const CTFType* varType)
            : varName(varName), varType(varType) {}
        const char* getVarName(void) const {return varName;}
        const CTFType* getVarType(void) const {return varType;}
    private:
        const char* varName;
        const CTFType* varType;
    };

    CTFTag(const CTFType* baseType, const char* varName,
        const CTFType* targetType);
    CTFTag(const CTFTag &tag);

    CTFTag& operator=(const CTFTag& tag);
    CTFTag& append(const CTFTag& tag);
    
    /* 
     * 'Instantiate' tag - create pointer to variable instead of
     * abstract place.
     * 
     * 'var' is variable, which use tag.
     */
    CTFVarTag instantiate(const CTFVar* var) const;

    const CTFType* getBaseType(void) const {return baseType;}
    const CTFType* getTargetType(void) const;
private:
    const CTFType* baseType;
    std::vector<Component> components;
};

/*
 * 'Instantiated' tag - now it points to variable.
 *
 * This class will be really needed instead of simple pointer to
 * target variable when tags will point to array elements.
 */
class CTFVarTag
{
public:
    CTFVarTag(void);
    CTFVarTag(const CTFVarTag& varTag);
    /* Return target variable of the tag. */
    const CTFVar* getVarTarget(void) const {return varTarget;}
    /* 
     * Return context for tag's target variable, based on given one.
     * 
     * If given context is insufficient, return NULL.
     */
    CTFContext* getContextTarget(CTFContext& contextVar) const;
    /*
     * Release all resources, allocated at getContextTarget() call.
     */
    void putContextTarget(CTFContext* contextTarget) const;
private:
    CTFVarTag(const CTFVar* varTarget);
    
    const CTFVar* varTarget;
    
    friend class CTFTag;
};

#endif // CTF_TAG_H_INCLUDED
