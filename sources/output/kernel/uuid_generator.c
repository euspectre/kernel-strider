/*
 * Implementation of uuid generator.
 * 
 * Generate random uuid's (version 4 in RFC 4122).
 * 
 * Taken from "/lib/uuid.c" (big-endian version)
 */
#include <linux/random.h> /* random32 */
#include <linux/string.h> /* memcpy */

void generate_uuid(unsigned char uuid[16])
{
    int i;
    u32 r;

    for (i = 0; i < 4; i++)
    {
        r = random32();
        memcpy(uuid + i * 4, &r, 4);
    }
    /* reversion 0b10 */
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    /* version 4 : random generation */
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
}