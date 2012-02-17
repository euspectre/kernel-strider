/*
 * Generator of uuid for CTF trace.
 * 
 * Even in newest kernels(>2.6.35) there are special functions for
 * generate uuid, distinct implementation remain useful for oldest
 * kernels and for choosing version of uuid(see RFC 4122)
 */

#ifndef UUID_GENERATOR_H
#define UUID_GENERATOR_H

void generate_uuid(unsigned char uuid[16]);

#endif /* UUID_GENERATOR_H*/
