#ifndef CTF_META_H
#define CTF_META_H

#include <kedr/ctf_reader/ctf_type.h>
#include <map>
#include <string>

class CTFVar;
class CTFContext;
/* Process name, visibility and other parent-specific properties of variable */
class CTFVarPlace;

/*
 * Layout information about variable.
 */
struct CTFVarLayoutInfo
{
    CTFVarLayoutInfo(const CTFVar* container, const CTFVar* prev)
        : container(container), prev(prev){};
    const CTFVar* container;
    const CTFVar* prev;
};

class CTFMeta
{
public:
    /* Find variable with given absolute name. */
    const CTFVar* findVar(const char* name) const;
    const CTFVar* findVar(const std::string& name) const;
protected:
    CTFMeta(void);
    ~CTFMeta(void);

    /*
     * Instantiate variable for given root type and all variables which
     * it chain.
     *
     * Return root variable.
     */
    const CTFVar* instantiate(const CTFType* rootType);
    /* Remove instantiation if it was. */
    void destroy(void);

    /* Factory for types */
    CTFTypeInt* createTypeInt(void) const;
    CTFTypeStruct* createTypeStruct(void) const;
    CTFTypeEnum* createTypeEnum(const CTFTypeInt* baseTypeInt) const;
    CTFTypeVariant* createTypeVariant(void) const;
    CTFTypeArray* createTypeArray(int size,
        const CTFType* elemType) const;
    CTFTypeSequence* createTypeSequence(CTFTag tagNElems,
        const CTFType* elemType) const;
private:
    CTFMeta(const CTFMeta& meta);/* not implemented */

    /* Place of root variable */
    CTFVarPlace* rootVarPlace;
};

#endif /* CTF_META_H */
