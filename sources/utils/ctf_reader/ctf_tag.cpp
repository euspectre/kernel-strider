#include <kedr/ctf_reader/ctf_reader.h>

#include <stdexcept>

typedef std::vector<CTFTag::Component> componentsVector;

CTFTag::CTFTag(void) : baseType(NULL), components() {}

CTFTag::CTFTag(const CTFType* baseType, const char* varName,
    const CTFType* varType) : baseType(baseType),
    components(1, Component(varName, varType)) {}

CTFTag::CTFTag(const CTFTag& tag) : baseType(tag.baseType),
    components(tag.components) {}

CTFTag& CTFTag::append(const CTFTag& tag)
{
    components.insert(components.end(), tag.components.begin(), tag.components.end());
    return *this;
}

CTFTag& CTFTag::operator=(const CTFTag& tag)
{
    baseType = tag.baseType;
    components = tag.components;
    return *this;
}

const CTFType* CTFTag::getTargetType(void) const
{
    int lastIndex = components.size() - 1;
    return components[lastIndex].getVarType();
}

CTFVarTag::CTFVarTag(void) : varTarget(NULL) {}
CTFVarTag::CTFVarTag(const CTFVarTag& varTag)
    : varTarget(varTag.varTarget) {}

CTFVarTag::CTFVarTag(const CTFVar* varTarget) : varTarget(varTarget) {}

CTFContext* CTFVarTag::getContextTarget(CTFContext& context) const
{
    return varTarget->adjustContext(context);
}

void CTFVarTag::putContextTarget(CTFContext* contextTarget) const
{
    /* Without indices in tag - nothing to do*/
    (void)contextTarget;
}

CTFVarTag CTFTag::instantiate(const CTFVar* var) const
{
    const CTFVar* varBase = var->getParent();
    /* Determine base variable, which corresponds to tag's base type. */
    for(;varBase != NULL; varBase = varBase->getParent())
    {
        if(varBase->getType() == baseType) break;
    }
    if(varBase == NULL) throw std::invalid_argument(
        "Attempt to instantiate tag with variable which cannot use this tag.");
    
    const CTFVar* varTarget = varBase;
    /* Determine target variable */
    std::vector<Component>::const_iterator iter = components.begin();
    std::vector<Component>::const_iterator iter_end = components.end();
    for(; iter != iter_end; ++iter)
    {
        varTarget = varTarget->findVar(iter->getVarName());
        if(varTarget == NULL) throw std::logic_error(
            std::string("Failed to instantiate tag component '")
            + iter->getVarName() + "'.");
        if(varTarget->getType() != iter->getVarType())
            throw std::logic_error(std::string("Instantiated tag component '")
                + iter->getVarName() + "' has incorrect type.");
    }
    
    return CTFVarTag(varTarget);
}
