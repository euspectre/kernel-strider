#include <kedr/ctf_reader/ctf_reader.h>

#include <cassert>

CTFTag CTFType::resolveTag(const char* tagStr) const
{
    const char* tagStrEnd;
    CTFTag tag = resolveTagImpl(tagStr, &tagStrEnd, false);

    if(!tag.isConnected()) return tag;/* fail */

    while(*tagStrEnd != '\0')
    {
        CTFTag tagAppended =
            tag.getTargetType()->resolveTagImpl(tagStrEnd, &tagStrEnd, true);
        if(!tagAppended.isConnected()) return tagAppended; /* fail */
        tag.append(tagAppended);
    }

    return tag;
}

CTFTag CTFType::resolveTag(const std::string& tagStr) const
{
    return resolveTag(tagStr.c_str());
}