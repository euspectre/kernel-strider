#include <kedr/ctf_reader/ctf_reader.h>
#include "ctf_scope.h"

#include <sstream>

#include <kedr/utils/uuid.h>

UUID::UUID(void): val(buf), is_const(false) {}
UUID::UUID(unsigned char* val): val(val), is_const(false) {}
UUID::UUID(const unsigned char* val): val((unsigned char*)val), is_const(true) {}

std::ostream& operator<< (std::ostream& os, const UUID& uuid)
{
    char str[36];
    uuid_to_str(uuid.bytes(), str);
    
    os.write(str, sizeof(str));
    
    return os;
}

std::istream& operator>> (std::istream& is, UUID& uuid)
{
    int i = 0;
    while(i < 16)
    {
        int ch = is.get();
        if(!is) break; /* Unexpected eof */

        if(ch == '-') continue;
        if(!isxdigit(ch)) break; /* Unexpected character */

        int ch2 = is.get();
        if(!is) break; /* Unexpected eof */
        if(!isxdigit(ch2)) break; /* Unexpected character */
        
#define XDIGIT_TO_QUADR(xdigit) ((xdigit) >= '0') && ((xdigit) <= '9') ? (xdigit) - '0' : \
    (((xdigit) >= 'a') && ((xdigit) <= 'f') ? (xdigit) - 'a' + 10 : \
    (xdigit) - 'A' + 10)

        unsigned char quadr1 = XDIGIT_TO_QUADR(ch), quadr2 = XDIGIT_TO_QUADR(ch2);
#undef XDIGIT_TO_QUADR

        uuid.bytes()[i] = (quadr1 << 4) | quadr2;
        i++;
    }
    
    if(i != 16) is.setstate(is.rdstate() | std::ios_base::failbit);
    return is;
}

const std::string* CTFReader::findParameter(const std::string& paramName) const
{
    return scopeRoot->findParameter(paramName.c_str());
}

