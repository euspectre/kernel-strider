#include <kedr/ctf_reader/ctf_context.h>

#include <cassert>

#include <cstddef> /* NULL */

#include <algorithm> /* fill_n() */
#include <cstdlib> /* malloc, free */
#include <new> /* bad_alloc exception */
#include <stdexcept> /* runtime_error and other exceptions */

#include <iostream>

#include <kedr/ctf_reader/ctf_var_place.h>

/****************** Context implementation ****************************/
CTFContext::CTFContext(const CTFVarPlaceContext* contextVar,
        CTFContext* baseContext)
	: m_mapStart(NULL), m_mapStartShift(0), m_mapSize(0),
	contextVar(contextVar), baseContext(baseContext)
{
    if(contextVar->cacheSize > 0)
    {
        cache = (int*)malloc(sizeof(int) * contextVar->cacheSize);
        if(!cache) throw std::bad_alloc();
        std::fill_n(cache, contextVar->cacheSize, -1);
    }
    else
    {
        cache = NULL;
    }
}

CTFContext::~CTFContext(void)
{
    free(cache);
}

void CTFContext::setMap(int size, const char* mapStart, int mapStartShift)
{
	m_mapSize = size;
	m_mapStart = mapStart;
	m_mapStartShift = mapStartShift;

	if(cache) std::fill_n(cache, contextVar->cacheSize, -1);
}

void CTFContext::moveMap(int size, const char* mapStart, int mapStartShift)
{
	assert(size >= m_mapSize);
	
	m_mapSize = size;
	m_mapStart = mapStart;
	m_mapStartShift = mapStartShift;
}

void CTFContext::map(int bits)
{
	if(m_mapSize >= bits) return;

	m_mapSize = extendMapImpl(bits, &m_mapStart, &m_mapStartShift);
    
    if(m_mapSize < bits)
    {
        std::cerr << "Context has beed extended to " << m_mapSize
            << ", while extension to " << bits << " was requested.\n";
        throw std::runtime_error("Failed to extend context.");
    }
}
