/*
 * Types definitions for CTF meta information. */

#ifndef CTF_META_TYPES_H
#define CTF_META_TYPES_H

/* Possible byte ordering for integer types */
enum ctf_int_byte_order
{
    ctf_int_byte_order_unknown = 0,
    ctf_int_byte_order_native,
    ctf_int_byte_order_be,
    ctf_int_byte_order_le
};

/* Base(pretty printing) for integer type*/
enum ctf_int_base
{
    ctf_int_base_unknown = 0,
    ctf_int_base_decimal,
    ctf_int_base_unsigned,
    ctf_int_base_hexadecimal,
    ctf_int_base_hexadecimal_upper,
    ctf_int_base_pointer,
    ctf_int_base_octal,
    ctf_int_base_binary
};

/* Encoding of integers */
enum ctf_int_encoding
{
    ctf_int_encoding_unknown = 0,
    ctf_int_encoding_none,
    ctf_int_encoding_utf8,
    ctf_int_encoding_ascii
};

#endif /* CTF_META_TYPES_H */