#include <kedr/ctf_reader/ctf_reader.h>

#include <cassert>

CTFVarPlace::CTFVarPlace(void) : contextVar(NULL),
    var(NULL), existenceVar(NULL)  {}

CTFVarPlace::~CTFVarPlace()
{
    /* 
     * Disconnect variable before its deleting for trigger
     * onPlaceChanged() callback.
     */
    delete setVar(NULL);
}


void CTFVarPlace::instantiateVar(const CTFType* type)
{
    /* Update 'existenceVar' field */
    const CTFVar* parent = getParentVar();
    if(parent != NULL)
    {
        switch(isExistwithParent())
        {
        case 1:
            existenceVar = parent->varPlace->existenceVar;
        break;
        case 0: /*variable is never exist */
        case -1:
            existenceVar = this;
        break;
        default:
            assert(0);
        }
    }
    /* Update 'contextVar' field */
    if(getPreviousVar())
        contextVar = getPreviousVar()->varPlace->contextVar;
    else if(getContainerVar())
        contextVar = getContainerVar()->varPlace->contextVar;

    /* Everything is prepared for instantiate variable */
    type->setVar(*this);
}

CTFVar* CTFVarPlace::setVar(CTFVar* var)
{
    CTFVar* varOld = this->var;
    if(varOld)
    {
        varOld->varPlace = NULL;
        varOld->onPlaceChanged(this);
    }

    this->var = var;
    if(var)
    {
        const CTFVarPlace* placeOld = var->getVarPlace();
        var->varPlace = this;
        var->onPlaceChanged(placeOld);
    }
    return varOld;
}

int CTFVarPlace::isExist(CTFContext& context) const
{
    const CTFVarPlace* currentVarPlace = this;
    while(currentVarPlace->existenceVar != NULL)
    {
        currentVarPlace = currentVarPlace->existenceVar;
        switch(currentVarPlace->isExistwithParent(context))
        {
        case -1:
            return -1;
        case 0:
            return 0;
        case 1:
            currentVarPlace = currentVarPlace->getParentVar()->varPlace;
        break;
        default:
            assert(0);
        }
    }
    return 1;
}

int CTFVarPlace::isExist(void) const
{
    return existenceVar == NULL ? 1 : isExistwithParent();
}


CTFContext* CTFVarPlace::adjustContext(CTFContext& context) const
{    
    CTFContext* contextTmp;
    for(contextTmp = &context;
        contextTmp != NULL;
        contextTmp = contextTmp->getBaseContext())
    {
        if(contextTmp->getContextVar() == contextVar) break;
    }
    return contextTmp;
}

const CTFContext* CTFVarPlace::adjustContext(const CTFContext& context) const
{    
    const CTFContext* contextTmp;
    for(contextTmp = &context;
        contextTmp != NULL;
        contextTmp = contextTmp->getBaseContext())
    {
        if(contextTmp->getContextVar() == contextVar) break;
    }
    return contextTmp;
}


CTFVarPlaceContext::CTFVarPlaceContext(void) : cacheSize(0)
{
    contextVar = this;
}

CTFVarPlaceContext::~CTFVarPlaceContext(void)
{
    /* 
     * Need explicitly disconnect variable before check that no cache
     * reservation is active.
     */
    delete setVar(NULL);
    assert(cacheSize == 0);
}

int CTFVarPlaceContext::reserveCache(int nElems)
{
    int result = cacheSize;
    cacheSize += nElems;
    return result;
}

void CTFVarPlaceContext::cancelCacheReservation(int elemIndex, int nElems)
{
    assert(cacheSize >= nElems);
    cacheSize -= nElems;

    (void)elemIndex;
    assert(cacheSize == elemIndex);
}
