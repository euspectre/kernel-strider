/*
 * CTFContext - describe memory region, to which variables are mapped.
 */

#ifndef CTF_CONTEXT_H_INCLUDED
#define CTF_CONTEXT_H_INCLUDED

#include <cstddef> /* NULL */

class CTFVar;
class CTFVarPlaceContext;

class CTFContext
{
public:
    /* Create context with empty mapping. */
    CTFContext(const CTFVarPlaceContext* contextVar,
        CTFContext* baseContext = NULL);

    virtual ~CTFContext(void);
    /* Return variable to which context is bounded. */
    const CTFVarPlaceContext* getContextVar(void) const {return contextVar;}
    /* Return base context. */
    CTFContext* getBaseContext(void) const {return baseContext; }

    /* Return pointer to the start of the current mapping */
    const char* mapStart(void) const {return m_mapStart;}
    /* Return shift of the start of the current mapping (0-7 bits)*/
    int mapStartShift(void) const {return m_mapStartShift;}
    /* Return size of the mapping, in bits */
    int mapSize(void) const {return m_mapSize;}

    /*
     * Request mapping of first 'bits' bits.
     *
     * If needed, mapping will be extended.
     */
    void map(int bits);

    /* 
     * Return pointer to the cache element at given index.
     * 
     * See CTFVarPlaceContext class description for more information
     * about cache.
     */
    int* getCache(int elemIndex) const {return cache + elemIndex;}

protected:
    /*
     * Real implementation of the map extension.
     *
     * Set mapStart_p and mapStartShift_p and return new mapping size.
     * 
     * Returned value should be either not less than 'newSize',
     * or between 0 and newSize to indicate EOF.
     */
    virtual int extendMapImpl(int newSize, const char** mapStart_p,
                                int* mapStartShift_p) = 0;
    /*
     * Change context mapping.
     *
     * Tell to base class that context is recreated - previouse content
     * is destroyed and new one should be read.
     * 
     * May be called with size=0. In that case context will be expanded
     * via extendMapImpl when needed.
     */
    void setMap(int size, const char* mapStart, int mapStartShift);
    /*
     * Move map of the context.
     *
     * Tell to base class that mapping start address is changed, but
     * content of mapping remains.
     *
     * 'size' should be at least as current mapping size,
     * first bytes of the new map should be same as ones before call.
     */
    void moveMap(int size, const char* mapStart, int mapStartShift);

private:
    CTFContext(const CTFContext& context);/* not implemented */
    /* Cached values, so shouldn't be accessed even by derived class. */
    const char* m_mapStart;
    int m_mapStartShift;
    int m_mapSize;

    /* Variable for which context is created. May not be NULL. */
    const CTFVarPlaceContext* contextVar;
    /* Previous context in the chain. May be NULL. */
    CTFContext* baseContext;
    
    int* cache;
};

/* Context for element of array or sequence */
class CTFElemContext: public CTFContext
{
public:
    /* Return non-zero if map of element is not really exist. */
    virtual int isEnd(void) const = 0;
    /* Move context to the next element */
    virtual void next(void) = 0;
    /* Return index of current element */
    virtual int getElemIndex(void) const = 0;
    /*
     * Set context so it will point to the element with given index.
     *
     * If index out of range, iterator with (isEnd() == true) will be created.
     */
    virtual void setElemIndex(int index) = 0;
};

#endif // CTF_CONTEXT_H_INCLUDED
