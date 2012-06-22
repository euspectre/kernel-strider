#include <kedr/ctf_reader/ctf_meta.h>

#include <kedr/ctf_reader/ctf_var.h>
#include <kedr/ctf_reader/ctf_var_place.h>

#include <cassert>

#include <memory> /* auto_ptr */

typedef std::map<const CTFVarPlace*, struct CTFVarLayoutInfo> mapType;

CTFMeta::CTFMeta(void) : rootVarPlace(NULL)
{

}

CTFMeta::~CTFMeta(void)
{
    destroy();
}

class CTFVarPlaceRoot : public CTFVarPlace
{
public:
    const CTFVar* getParentVar(void) const {return NULL;}
    const CTFVar* getContainerVar(void) const {return NULL;}
    const CTFVar* getPreviousVar(void) const {return NULL;}
protected:
    std::string getNameImpl(void) const {return std::string("ROOT");};
};

const CTFVar* CTFMeta::instantiate(const CTFType* rootType)
{
    assert(rootVarPlace == NULL);

    rootVarPlace = new CTFVarPlaceRoot();
    rootVarPlace->instantiateVar(rootType);
    
    return rootVarPlace->getVar();
}

void CTFMeta::destroy(void)
{
    if(rootVarPlace)
    {   
        delete rootVarPlace;
        rootVarPlace = NULL;
    }
}

const CTFVar* CTFMeta::findVar(const char* name) const
{
    assert(rootVarPlace);
    const CTFVar* rootVar = rootVarPlace->getVar();
    assert(rootVar);
    return rootVar->findVar(name);
}

const CTFVar* CTFMeta::findVar(const std::string& name) const
{
    assert(rootVarPlace);
    const CTFVar* rootVar = rootVarPlace->getVar();
    assert(rootVar);
    return rootVar->findVar(name);
}
