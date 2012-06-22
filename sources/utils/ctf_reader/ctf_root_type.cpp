/* Implementation of root type for CTFReader */

#include "ctf_root_type.h"

#include <cstring> /* strerror, strcmp */

#include <stdexcept> /* logic_error and other exceptions */

#include <cassert> /* assert() macro */

std::string topVarNames[N_TOP_VARS] =
{
    "trace.packet.header",
    "stream.packet.context",
    "stream.event.header",
    "stream.event.context",
    "event.context",
    "event.fields"
};

/******************************* Root type ****************************/

/* Root type and variable */
CTFReader::RootType::RootType(void)
{
    for(int i = 0; i < N_TOP_VARS; i++)
        topTypes[i] = NULL;
}

CTFTag CTFReader::RootType::resolveTagImpl(const char* tagStr,
    const char** tagStrEnd, bool isContinued) const
{
    if(isContinued)
        throw std::logic_error("Resolving tag of root type in contuinue mode.");
    
    for(int i = 0; i < N_TOP_VARS; i++)
    {
        if(topTypes[i] == NULL) continue;
        if(strncmp(tagStr, topVarNames[i].c_str(), topVarNames[i].size()) == 0)
        {
            *tagStrEnd = tagStr + topVarNames[i].size();
            return CTFTag(this, topVarNames[i].c_str(), topTypes[i]);
        }
    }
    return CTFTag();
}

void CTFReader::RootType::assignType(const std::string& position,
	const CTFType* type)
{
    for(int i = 0; i < N_TOP_VARS; i++)
    {
        if(position == topVarNames[i])
        {
            if(topTypes[i] != NULL)
                throw std::invalid_argument("Attempt to assign type to "
                    "position '" + topVarNames[i]
                    + "', which is already assigned.");

            topTypes[i] = type;
            return;
        }
    }
    throw std::invalid_argument("Attempt to assign type to unknown "
        "position '" + position + "'");
}

int CTFReader::RootType::getAlignmentMaxImpl(void) const
{
    int alignCurrent = 1;

    for(int i = 0; i < N_TOP_VARS; i++)
    {
        const CTFType* topType = topTypes[i];

        if(topType == NULL) continue;
        int align = topType->getAlignmentMax();
        if(align > alignCurrent)
            alignCurrent = align;
    }
    return alignCurrent;
}

int CTFReader::RootType::getAlignmentImpl(void) const
{
    return RootType::getAlignmentImpl();
}

void CTFReader::RootType::setVarImpl(CTFVarPlace& varPlace) const
{
	/* Check that type can be instantiated. */
	int i;
	for(i = 0; i < N_PACKET_VARS; i++)
	{
		if(topTypes[i]) break;
	}
	if(i == N_PACKET_VARS) throw std::logic_error
		("For instantiation at least one packet-related type should be assigned");
	
	for(i = 0; i < N_EVENT_VARS; i++)
	{
		if(topTypes[N_PACKET_VARS + i]) break;
	}
	if(i == N_EVENT_VARS) throw std::logic_error
		("For instantiation at least one event-related type should be assigned");
	
	RootVar* rootVar = new RootVar(this);
	varPlace.setVar(rootVar);
	
	rootVar->setTopVars();
}

CTFReader::RootVar::RootVar(const RootType* rootType) : rootType(rootType),
    eventStartVarPlace(NULL),
    
    packetContextVar(NULL),
    packetLastVar(NULL),
    eventContextVar(NULL),
    eventStartVar(NULL),
    eventLastVar(NULL),
    packetAlign(rootType->getAlignmentMax())
{
    for(int i = 0; i < N_TOP_VARS; i++)
        topVarPlaces[i] = NULL;
}

CTFReader::RootVar::~RootVar(void)
{
    for(int i = 0; i < N_TOP_VARS; i++)
        delete topVarPlaces[i];
    delete eventStartVarPlace;
}

const CTFVar* CTFReader::RootVar::TopVarPlace::getPreviousVar(void) const
{
    int minIndex = index < N_PACKET_VARS ? 0 : N_PACKET_VARS;
    
    for(int i = index - 1; i >= minIndex; i--)
    {
        if(rootVar->topVarPlaces[i] != NULL)
            return rootVar->topVarPlaces[i]->getVar();
    }
    
    return minIndex == 0 ? NULL : rootVar->eventStartVar;
}

void CTFReader::RootVar::setTopVars(void)
{
    packetContextVar = NULL;
    for(int i = 0; i < N_PACKET_VARS; i++)
    {
        if(rootType->topTypes[i] == NULL) continue;
        if(!packetContextVar)
        {
            CTFVarPlaceContext* contextVar = 
                new TopVarPlaceContext(this, i);
            packetContextVar = contextVar;
            topVarPlaces[i] = contextVar;
        }
        else
        {
            topVarPlaces[i] = new TopVarPlace(this, i);
        }
        
        topVarPlaces[i]->instantiateVar(rootType->topTypes[i]);
			
        packetLastVar = topVarPlaces[i]->getVar();
    }

    assert(packetContextVar);
    
    eventStartVarPlace = new EventStartVarPlace(this, "EventStarter");
    EventStartType eventStartType;

    eventStartVarPlace->instantiateVar(&eventStartType);
    eventStartVar = static_cast<const EventStartVar*>
        (eventStartVarPlace->getVar());

    eventContextVar = eventStartVarPlace;

    for(int i = N_PACKET_VARS; i < N_TOP_VARS; i++)
    {
        if(rootType->topTypes[i] == NULL) continue;
        topVarPlaces[i]	= new TopVarPlace(this, i);
        topVarPlaces[i]->instantiateVar(rootType->topTypes[i]);

        eventLastVar = topVarPlaces[i]->getVar();
    }
    
    assert(eventContextVar);
}

int CTFReader::RootVar::getAlignmentImpl(CTFContext& context) const
{
    (void)context;
    return rootType->getAlignmentMax();
}

int CTFReader::RootVar::getAlignmentImpl(void) const
{
    return rootType->getAlignmentMax();
}

int CTFReader::RootVar::getStartOffsetImpl(CTFContext& context) const
{
    (void)context;
    return 0;
}

int CTFReader::RootVar::getStartOffsetImpl(void) const
{
    return 0;
}

int CTFReader::RootVar::getSizeImpl(CTFContext& context) const
{
    (void)context;
    throw std::logic_error("Size of the root variable shouldn't "
        "be requested.");
}

int CTFReader::RootVar::getSizeImpl(void) const
{
    throw std::logic_error("Size of the root variable shouldn't "
        "be requested.");
}


int CTFReader::RootVar::getEndOffsetImpl(CTFContext& context) const
{
    (void)context;
    throw std::logic_error("End offset of the root variable shouldn't "
        "be requested.");
}

int CTFReader::RootVar::getEndOffsetImpl(void) const
{
    throw std::logic_error("End offset of the root variable shouldn't "
        "be requested.");
}


const CTFVar* CTFReader::RootVar::resolveNameImpl(const char* name,
	const char** nameEnd, bool isContinued) const
{
	(void)isContinued;
	for(int i = 0; i < N_TOP_VARS; i++)
	{
		if(rootType->topTypes[i] == NULL) continue;
		
		if(strncmp(name, topVarNames[i].c_str(), topVarNames[i].size()) != 0)
			continue;
		
		if((topVarPlaces[i] == NULL) || (topVarPlaces[i]->getVar() == NULL))
			throw std::logic_error("Request for variable '"
				+ std::string(name) + "' which has not instantiated yet.");
		
		*nameEnd = name + topVarNames[i].size();
		
		return topVarPlaces[i]->getVar();
	}
	return NULL;
}


