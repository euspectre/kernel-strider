#include <kedr/ctf_reader/ctf_reader.h>

#include <vector>

#include <stdexcept>

#include <cassert>

/*
 * Return (minimum) number which is greater or equal to val
 * and satisfy to alignment.
 */
static inline int align_val(int val, int align)
{
    int mask = align - 1;
    return (val + mask) & ~mask;
}

/* Temporary variable used for determine layout parameters */
class VarLayoutProbe : public CTFVar
{
public:
	VarLayoutProbe(int align) : align(align) {};
protected:
	int getAlignmentImpl(CTFContext& context) const
		{(void) context; return align;}
    int getAlignmentImpl(void) const
		{return align;}
	int getSizeImpl(CTFContext& context) const
        {(void) context; throw std::logic_error("Probe size shouldn't be requested");}
    int getSizeImpl(void) const
        {throw std::logic_error("Probe size shouldn't be requested");}
    int getStartOffsetImpl(CTFContext& context) const
        {(void) context; throw std::logic_error("Probe start offset shouldn't be requested");}
    int getStartOffsetImpl(void) const
        {throw std::logic_error("Probe start offset shouldn't be requested");}
    int getEndOffsetImpl(CTFContext& context) const
        {(void) context; throw std::logic_error("Probe end offset shouldn't be requested");}
    int getEndOffsetImpl(void) const
        {throw std::logic_error("Probe end offset shouldn't be requested");}
    
    const CTFType* getTypeImpl(void) const {return NULL;}
private:
	int align;
};

/* One element in layout chain */
struct LayoutChainElem
{
	/*
	 * Variable corresponded to this element.
	 */
	const CTFVar* var;
	/*
	 * Whether this variable is container for next variable in chain
	 * (oterwise - previous).
	 */
	int isContainer;
	/*
	 * Alignment of variable.
	 */
	int align;
	/*
	 * Size of variable.
	 *
	 * Has a sence only when isContainer is 0.
	 */
	int size;
	/*
	 * Fill stucture for given variable in given role.
	 */
	void fill(const CTFVar* var, int isContainer = 1);
};
/*
 * Types of layout chain elements:
 *
 * {var, isContainer = 1, align} - container
 * {var, isContainer = 0, align, size} - prev
 */
void LayoutChainElem::fill(const CTFVar* var, int isContainer)
{
	this->var = var;
	this->isContainer = isContainer;
	this->align = var->getAlignment();

	if(!isContainer)
	this->size = var->getSize();
}

/*
 * Sequence of variables with relations ->prev or ->container.
 *
 * E.g., for given metadata
 *
 * enum e{....} i;
 * int j;
 * struct
 * {
 *     int field1;
 *     variant <e>
 *     {
 *         char var1;
 *         int var2;
 *     }var;
 * }s;
 *
 * next sequence is possible:
 * var2->var->field1->s
 */
typedef std::vector<struct LayoutChainElem> LayoutChain;

/* Return size of chain.
 * (Offset from the start of base variable to the start/end of the first
 * variable, depenging on it isContainer property.)
 */
static int getChainSize(const LayoutChain& layoutChain,
    int baseIndex = -1);
/*
 *  If given variable has zero offset inside its context, return
 * alignment of the rest of variables chain(as maximum alignments of
 * variables in this chain.)
 * 
 * Otherwise return -1.
 */
static int getZeroOffsetAlignment(const CTFVar* var);
/* Fill usePrev or useContainer layout for variable */
static void fillNearestLayout(struct CTFVarStartOffsetParams& params,
    const CTFVar* var);
/*
 * Return maximum alignment to which offset meets.
 * maxAlign bounds this value.
 */
static int getOffsetAlign(int offset, int maxAlign = 0);

static void fillInternal(struct CTFVarStartOffsetParams& params,
    CTFVar* var)
{
	int align = var->getAlignment();
	if(align == -1)
	{
	    int zeroOffsetAlign = getZeroOffsetAlignment(var);
	    if(zeroOffsetAlign != -1)
	    {
	        /* Absolute zero offset */
	        params.layoutType = CTFVarStartOffsetParams::LayoutTypeAbsolute;
	        params.info.absolute.offset = 0;

	        params.align = zeroOffsetAlign;
	        return;
	    }
	    /*
	     * Otherwize variable with non-constant alignment has usePrev or
	     * useContainer layout.
	     */
	     fillNearestLayout(params, var);
	     return;
	}

    /*
     * Chain of variables with constant total size.
     * First variable is prev/container for probed one.
     */
	LayoutChain layoutChain;
	layoutChain.reserve(20);
    /* Maximum alignment of all variables in chain and probe variable */
	int chainAlign = align;
	struct LayoutChainElem constructedElem;

	const CTFVar* currentVar = var;

	/* Look for the uppest base variable and create chain up to it*/
	int baseIndex = -1;

	while(1)
	{
        const CTFVar* prev
            = currentVar->getVarPlace()->getPreviousVar();
        const CTFVar* container
            = currentVar->getVarPlace()->getContainerVar();
		if(prev)
		{
			currentVar = prev;
			constructedElem.fill(currentVar, 0);
			if(constructedElem.size == -1)
			{
			    /* Prev variable with non-constant size cannot be based. */
			    break;
			}
		}
		else if(container)
		{
			currentVar = container;
			constructedElem.fill(currentVar, 1);
		}
        else
        {
            /* Found context variable - absolute layout. */
			int offset = getChainSize(layoutChain);
			offset = align_val(offset, align);

            params.layoutType = CTFVarStartOffsetParams::LayoutTypeAbsolute;
            params.info.absolute.offset = offset;
            params.align = getOffsetAlign(offset, chainAlign);
            return;
        }

        if(constructedElem.align == -1)
        {
			int zeroOffsetAlign = getZeroOffsetAlignment(var);
			if(zeroOffsetAlign != -1)
            {
                /* Absolute offset */
                int offset = getChainSize(layoutChain);
                offset = align_val(offset, align);

                params.layoutType = CTFVarStartOffsetParams::LayoutTypeAbsolute;
				params.info.absolute.offset = offset;
                params.align = getOffsetAlign(offset,
					std::max(zeroOffsetAlign, chainAlign));
                return;
            }
            /* Variable with non-constant alignment cannot be based. */
            break;
        }

		if(constructedElem.align >= chainAlign)
		{
		    /* Update base variable and chain alignment*/
		    baseIndex = layoutChain.size();
		    chainAlign = constructedElem.align;
		}

		layoutChain.push_back(constructedElem);
	}
    if(baseIndex != -1)
    {
        int offset = getChainSize(layoutChain, baseIndex);
        offset = align_val(offset, align);
        
        params.layoutType = CTFVarStartOffsetParams::LayoutTypeUseBase;
        params.info.useBase.offset = offset;
        params.info.useBase.var = layoutChain[baseIndex].var;
        params.align = getOffsetAlign(offset, chainAlign);
    }
    else
    {
        fillNearestLayout(params, var);
    }
}

void CTFVarStartOffsetParams::fill(CTFVarPlace& varPlace,
    int align)
{
	VarLayoutProbe* varProbe = new VarLayoutProbe(align);

	CTFVar* varOld = varPlace.setVar(varProbe);

    fillInternal(*this, varProbe);

	varPlace.setVar(varOld);

	delete varProbe;

	return;
}

int getChainSize(const LayoutChain& layoutChain,
    int baseIndex)
{
    int size = 0;
    if(baseIndex == -1) baseIndex = layoutChain.size() - 1;
    for(int i = baseIndex; i >= 0; i--)
    {
        const struct LayoutChainElem& layoutElem = layoutChain[i];
        size = align_val(size, layoutElem.align);
        if(!layoutElem.isContainer) size += layoutElem.size;
    }
    return size;
}

int getZeroOffsetAlignment(const CTFVar* var)
{
    const CTFVar* currentVar = var;
    int align = 1;
    while(1)
    {
        const CTFVar* prev
            = currentVar->getVarPlace()->getPreviousVar();
        const CTFVar* container
            = currentVar->getVarPlace()->getContainerVar();
		if(prev)
		{
			currentVar = prev;
			if(currentVar->getSize() != 0) return 0;
		}
		else if(container)
		{
			currentVar = container;
		}
        else
        {
            return align;
        }
		int currentAlign = currentVar->getAlignment();
		if(currentAlign > align)
			align = currentAlign;
    }
}

void fillNearestLayout(struct CTFVarStartOffsetParams& params,
    const CTFVar* var)
{
    const CTFVar* prev = var->getVarPlace()->getPreviousVar();
    const CTFVar* container = var->getVarPlace()->getContainerVar();

    if(prev)
    {
        params.layoutType = CTFVarStartOffsetParams::LayoutTypeUsePrev;
        params.info.usePrev.var = prev;
    }
    else if(container)
    {
        params.layoutType = CTFVarStartOffsetParams::LayoutTypeUseContainer;
        params.info.useContainer.var = container;
    }
    else
    {
        assert(0);
    }
    params.align = var->getAlignment();
}

int getOffsetAlign(int offset, int maxAlign)
{
    int align;
    for(align = maxAlign; align != 1; align >>= 1)
    {
        if((offset & (align - 1)) == 0) break;
    }
    return align;
}
