#include <kedr/ctf_reader/ctf_hash.h>

char IDHelpers::idTable[256];

IDHelpers::IDHelpers(void)
{
	for(int i = 0; i < 256; i++)
		IDHelpers::idTable[i] = isalnum(i) || (i == '_') ? 1 : 0;
}

/* Object needed only for fill table. */
IDHelpers IDHelperAUX;