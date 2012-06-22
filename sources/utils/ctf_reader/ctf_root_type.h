/* Definition of root type and root variable for CTFReader */

#ifndef CTF_ROOT_TYPE_H_INCLUDED
#define CTF_ROOT_TYPE_H_INCLUDED

#include <kedr/ctf_reader/ctf_reader.h>
#include <cstdlib> /*malloc */

/* Maximum number of event-related top-variables. */
#define N_EVENT_VARS 4
/* Maximum number of packet-related top-variables. */
#define N_PACKET_VARS 2
/* Maximum number of top variables. */
#define N_TOP_VARS (N_PACKET_VARS + N_EVENT_VARS)

extern std::string topVarNames[N_TOP_VARS];

class CTFReader::RootType: public CTFType
{
public:
    RootType(void);

    void assignType(const std::string& position, const CTFType* type);
protected:
    CTFType* cloneImpl(void) const {return new RootType(*this);}
    int getAlignmentImpl(void) const;
    int getAlignmentMaxImpl(void) const;
    void setVarImpl(CTFVarPlace& varPlace) const;

    CTFTag resolveTagImpl(const char* tagStr,
        const char** tagStrEnd, bool isContinued) const;
private:
    const CTFType* topTypes[N_TOP_VARS];

    friend class RootVar;
};

/* Root variable */
class EventStartVar;
class EventStartVarPlace;
class CTFReader::RootVar: public CTFVar
{
public:
    RootVar(const RootType* rootType);
    ~RootVar(void);

    void setTopVars(void);

protected:
    int getAlignmentImpl(CTFContext& context) const;
    int getAlignmentImpl(void) const;
    int getStartOffsetImpl(CTFContext& context) const;
    int getStartOffsetImpl(void) const;
    int getEndOffsetImpl(CTFContext& context) const;
    int getEndOffsetImpl(void) const;
    int getSizeImpl(CTFContext& context) const;
    int getSizeImpl(void) const;
    
    const CTFVar* resolveNameImpl(const char* name,
        const char** nameEnd, bool isContinued) const;
    const CTFType* getTypeImpl(void) const {return rootType;}

private:
    class TopVarPlace: public CTFVarPlace
    {
    public:
        TopVarPlace(const RootVar* rootVar, int index)
            : rootVar(rootVar), index(index) {}

        const CTFVar* getParentVar(void) const {return rootVar;}
        const CTFVar* getPreviousVar(void) const;
        const CTFVar* getContainerVar(void) const {return NULL;}
    protected:
        std::string getNameImpl(void) const
            {return topVarNames[index];}
    private:
        const RootVar* rootVar;
        int index;
    };
    
    class TopVarPlaceContext: public CTFVarPlaceContext
    {
    public:
        TopVarPlaceContext(const RootVar* rootVar, int index)
            : rootVar(rootVar), index(index) {}

        const CTFVar* getParentVar(void) const {return rootVar;}
    protected:
        std::string getNameImpl(void) const
            {return topVarNames[index];}
    private:
        const RootVar* rootVar;
        int index;
    };

    const RootType* rootType;

    CTFVarPlace* topVarPlaces[N_TOP_VARS];
    EventStartVarPlace* eventStartVarPlace;

    /*   Cached values   */
    
    /* Variable, for which packet context is created */
    const CTFVarPlaceContext* packetContextVar;
    /*
     * Last variable in the packet context.
     *
     * Its endOffset is used for determine packet size.
     */
    const CTFVar* packetLastVar;
    /* Variable, for which event context is created */
    const CTFVarPlaceContext* eventContextVar;
    /* Flexible variable(its size depends on context) */
    const EventStartVar* eventStartVar;
    /*
     * Last variable in the event context.
     *
     * Its endOffset is used for determine event size.
     */
    const CTFVar* eventLastVar;

    /* Alignment of the whole packet */
    int packetAlign;
    /* Alignment of the event start is taken from eventContextVar. */
    
    friend class Event;
    friend class Packet;
};

/****************** Event start variable **********************/
/*
 * Flexible variable which is used as start of the event.
 *
 * Size of that variable depend on the context.
 */
class EventStartVar: public CTFVar
{
public:
    /* Set start of the event in given context */
    void setEventStart(int eventStartOffset, CTFContext& context) const;

    int getEventStart(const CTFContext& context) const;
protected:
    void onPlaceChanged(const CTFVarPlace* placeOld);

    int getAlignmentImpl(CTFContext& context) const
		{(void) context; return 1;}
    int getAlignmentImpl(void) const {return 1;}
    int getSizeImpl(CTFContext& context) const
        {return getEventStart(context);}
    int getSizeImpl(void) const {return -1;}
	int getStartOffsetImpl(CTFContext& context) const
        {(void) context; return 0;}
    int getStartOffsetImpl(void) const {return 0;}
	int getEndOffsetImpl(CTFContext& context) const
        {return getEventStart(context);}
    int getEndOffsetImpl(void) const {return -1;}

    const CTFType* getTypeImpl(void) const {return NULL;}
private:
    /* Index in cache of event start offset */
    int eventStartIndex;
};


class EventStartVarPlace: public CTFVarPlaceContext
{
public:
    EventStartVarPlace(const CTFVar* parent, std::string varName)
        :parent(parent), varName(varName) {}

    const EventStartVar* getVar(void) const
        {return static_cast<const EventStartVar*>(CTFVarPlace::getVar());}

    const CTFVar* getParentVar(void) const {return parent;}
protected:
    std::string getNameImpl(void) const
        {return parent->name() + "." + varName;}
private:
    const CTFVar* parent;
    std::string varName;
};

/* Type which create flexer variable. */
class EventStartType : public CTFType
{
protected:
	CTFType* cloneImpl() const {return new EventStartType();}
	int getAlignmentMaxImpl(void) const {return 1;}

	void setVarImpl(CTFVarPlace& varPlace) const
        {varPlace.setVar(new EventStartVar());}
};

#endif /* CTF_ROOT_TYPE_H_INCLUDED */
