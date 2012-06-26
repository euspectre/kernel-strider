#include <kedr/ctf_reader/ctf_reader.h>

#include <cassert>

#include <stdexcept>

CTFVar::CTFVar(void) : varPlace(NULL)
{
}

CTFVar::~CTFVar(void)
{
}

const CTFVar* CTFVar::findVar(const char* name) const
{
    const char* currentName;
    const CTFVar* currentVar = resolveNameImpl(name, &currentName, false);
    
    while((currentVar != NULL) && (*currentName != '\0'))
    {
        currentVar = currentVar->resolveNameImpl(currentName,
            &currentName, true);
    }
    return currentVar;
}

CTFContext& CTFVar::map(CTFContext& context) const
{
    CTFContext* contextAdjusted = adjustContext(context);
    assert(contextAdjusted);
    
    contextAdjusted->map(getEndOffset(*contextAdjusted));
    
    return *contextAdjusted;
}

const char* CTFVar::getMap(CTFContext& context, int* mapStartShift_p) const
{
    int varStartOffset = getStartOffset(context);
    assert(varStartOffset != -1);

    varStartOffset += context.mapStartShift();
    
    if(mapStartShift_p)
        *mapStartShift_p = varStartOffset % 8;

    return context.mapStart() + varStartOffset / 8;
}


#ifdef CTF_VAR_CHECK_LAYOUT
/*
 * Return (minimum) number which is greater or equal to val
 * and satisfy to alignment.
 */
static inline int align_val(int val, int align)
{
    int mask = align - 1;
    return (val + mask) & ~mask;
}

/* 
 * Return start offset of variable calculated using its prev-container
 * hierarchy.
 * 
 * -2 is a signal that start offset cannot be calculated using this method.
 */
int getStartOffsetReal(const CTFVar& var, CTFContext& context)
{
    const CTFVar* container = var.getVarPlace()->getContainerVar();
    const CTFVar* prev = var.getVarPlace()->getPreviousVar();
    
    if(prev)
    {
        int prevEndOffset = prev->getEndOffset(context);
        if(prevEndOffset == -1) return -2;
        
        int align = var.getAlignment(context);
        if(align == -1) return -2;

        return align_val(prevEndOffset, align);
    }
    else if(container)
    {
        int containerStartOffset = container->getStartOffset(context);
        if(containerStartOffset == -1) return -2;
        
        int align = var.getAlignment(context);
        if(align == -1) return -2;

        return align_val(containerStartOffset, align);
    }
    else
    {
        return 0;
    }
}

int CTFVar::getStartOffset(CTFContext& context) const
{
    int startOffsetImpl = getStartOffsetImpl(context);
    int startOffsetReal = getStartOffsetReal(*this, context);

    if(startOffsetReal != -2)
    {
        if(startOffsetImpl != startOffsetReal)
        {
            std::cerr << "Start offset of variable, returned by "
                "implementation, is " << startOffsetImpl <<
                ", but start offset of variable using prev-container hierarchy is "
                << startOffsetReal << ".\n";
            throw std::logic_error("Variable's start offset calculations are bugged");
        }
    }

    return startOffsetImpl;
}

int getStartOffsetReal(const CTFVar& var)
{
    const CTFVar* container = var.getVarPlace()->getContainerVar();
    const CTFVar* prev = var.getVarPlace()->getPreviousVar();
    
    if(prev)
    {
        int prevEndOffset = prev->getEndOffset();
        if(prevEndOffset == -1) return -2;
        
        int align = var.getAlignment();
        if(align == -1) return -2;

        return align_val(prevEndOffset, align);
    }
    else if(container)
    {
        int containerStartOffset = container->getStartOffset();
        if(containerStartOffset == -1) return -2;
        
        int align = var.getAlignment();
        if(align == -1) return -2;

        return align_val(containerStartOffset, align);
    }
    else
    {
        return 0;
    }
}

int CTFVar::getStartOffset(void) const
{
    int startOffsetImpl = getStartOffsetImpl();
    int startOffsetReal = getStartOffsetReal(*this);

    if(startOffsetReal != -2)
    {
        if(startOffsetImpl != startOffsetReal)
        {
            std::cerr << "Start offset of variable, returned by "
                "implementation, is " << startOffsetImpl <<
                ", but start offset of variable using prev-container hierarchy is "
                << startOffsetReal << ".\n";
            throw std::logic_error("Variable's start offset calculations are bugged");
        }
    }

    return startOffsetImpl;
}
#endif