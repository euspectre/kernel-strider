/* Universal unique identifier */

#ifndef KEDR_UUID_H
#define KEDR_UUID_H

typedef unsigned char uuid_t[16];

static inline void uuid_to_str(const uuid_t uuid, char str[36])
{
#define out_quadr(buf, quadr) *(buf) = (quadr) > 9 ? 'A' + (quadr) - 10 : '0' + (quadr)
#define out_byte(out_index, in_index) \
	out_quadr(str + out_index, uuid[in_index] >> 4); \
	out_quadr(str + out_index + 1, uuid[in_index] & 0xf)

	out_byte(0,0);
	out_byte(2,1);
	out_byte(4,2);
	out_byte(6,3);
	str[8] = '-';
	out_byte(9,4);
	out_byte(11,5);
	str[13] = '-';
	out_byte(14,6);
	out_byte(16,7);
	str[18] = '-';
	out_byte(19,8);
	out_byte(21,9);
	str[23] = '-';
	out_byte(24,10);
	out_byte(26,11);
	out_byte(28,12);
	out_byte(30,13);
	out_byte(32,14);
	out_byte(34,15);

#undef out_byte
#undef out_quadr
}

#endif /* KEDR_UUID_H */