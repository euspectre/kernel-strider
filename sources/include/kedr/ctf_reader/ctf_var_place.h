/*
 * Class contained information about variable's relations with 'upper'
 * variables(in naming and layout sences).
 *
 * This class is responsible for name of the variable and for its visibility.
 * property.
 *
 * Normal usage of CTFVarPlace class:
 *
 * 0. Derive it, implementing isExistwithParent(), getParentVar() and
 * getNameImpl() methods.
 * 1. Create object of derived class on heap(new operator).
 * 2. Store pointer to created object somewhere.
 * 3. Call instantiateVar.
 *
 */

#ifndef CTF_VAR_PLACE_H_INCLUDED
#define CTF_VAR_PLACE_H_INCLUDED

class CTFVar;
class CTFType;
class CTFContext;

class CTFVarPlaceContext;

class CTFVarPlace
{
public:
    CTFVarPlace(void);
    virtual ~CTFVarPlace(void);
    /* Instantiate variable for this place using given type */
    void instantiateVar(const CTFType* type);
    /*
     * Connect variable to this place.
     * Place will be responded for variable lifetime.
     *
     * Return variable which was connected previously
     * (caller become responsible for its lifetime).
     *
     * Passing NULL means clearing connection.
     * NULL value returned means that no variable connected before.
     *
     * Used by CTFType method setVar().
     */
    CTFVar* setVar(CTFVar* var);
    /* Return variable for which this information is set */
    const CTFVar* getVar(void) const {return var;}
    
    CTFVarPlaceContext* getContextVar(void) const {return contextVar;}
    
    /* Return parent variable for given one */
    virtual const CTFVar* getParentVar(void) const = 0;
    /* Return container(in layout sence) variable for given one */
    virtual const CTFVar* getContainerVar(void) const = 0;
    /* Return previous(in layout sence) variable for given one */
    virtual const CTFVar* getPreviousVar(void) const = 0;

    /* Return full name of the variable. */
    std::string getName(void) const {return getNameImpl();}
    /*
     * Determine, whether variable is exist in given context.
     * Possible results:
     *
     *  1 - var exists in given context or in any context derived from it,
     *  0 - var doesn't exist in given context or in any context derived from it,
     * -1 - existence is undefined.
     */
    int isExist(CTFContext& context) const;
    int isExist(void) const;
    /* See description of same method in 'class CTFVar'. */
    CTFContext* adjustContext(CTFContext& context) const;
    const CTFContext* adjustContext(const CTFContext& context) const;

protected:
    virtual std::string getNameImpl(void) const = 0;
    /*
     * Functionality is same as in isExist() method, but in case when
     * parent variable is exist.
     *
     * More literally:
     *  1 - means that variable exists in given context or in any context
     *      derived from it, where parent variable exist.
     *  0 - means that variable doesn't exist in given context or in
     *      any context derived from it, where parent variable exist.
     *      (note, that condition about parent variable existence may be
     *      omitted here, because if parent does not exist, variable does
     *      not exist too in any case.)
     *  -1- means that existence is undefined.
     *
     * Default implementation always return 1.
     */
    virtual int isExistwithParent(CTFContext& context) const
        {(void)context; return 1;}
    virtual int isExistwithParent(void) const {return 1;}

    /*
     * Context created for contextVariable will maps variable at given
     * place.
     * 
     * NULL value means that variable at this place is not really mapped
     * (e.g., root variable).
     */
    CTFVarPlaceContext* contextVar;
private:
    CTFVarPlace(const CTFVarPlace& varInfo); /* not implemented */

    CTFVar* var;

    /*
     * The most 'upper' variable which has same existence property as this.
     *
     * Used for fast determination, whether variable is exist.
     */
    const CTFVarPlace* existenceVar;
};

/*
 * Specialization of variable place used as a place for context variable
 * 
 * The main feature of that place - it allows reservation of cache,
 * which is allocated in context for it.
 * 
 * Cache is organizes as array of elements.
 * When context is created, all elements are initialized with (-1).
 * Each element may be used only by object which reserve it via place
 * for context variable.
 * When context is flushed, cache is reinitialized.
 * 
 * Normal usage of cache is follows:
 * 1) when object is initialized, reserve cache element(s) via place
 *    for context variable, store returned element index;
 * 2) when need to return some value, which may be cached, firstly search
 *    context, which contain reserved element.
 * 3) then check value of the cache element in context.
 *    If it is (-1), value should be calculated and stored in cache.
 * 4) if cache element value is not (-1), it is cached value.
 * 5) when object is destroyed, cancel cache reservation(!!in reverse order)
 * 
 * NOTE: when context mapping moved, cache is not reinitialized.
 * So, pointers inside context mapping shouldn't be cached.
 */
class CTFVarPlaceContext : public CTFVarPlace
{
public:
    CTFVarPlaceContext(void);
    ~CTFVarPlaceContext(void);
    /* 
     * Reserve 'nElems' elements in the cache.
     * 
     * Return index of first element.
     */
    int reserveCache(int nElems = 1);
    /*
     * Cancel cache resevation.
     * 
     * 'elemIndex' is a value, returned by reserveCache(),
     * 'nElems' should be same as passed to reserveCache().
     */
    void cancelCacheReservation(int elemIndex, int nElems = 1);

    const CTFVar* getContainerVar(void) const {return NULL;}
    const CTFVar* getPreviousVar(void) const {return NULL;}
private:
    int cacheSize;
    
    friend class CTFContext;
};

#endif // CTF_VAR_PLACE_H_INCLUDED
