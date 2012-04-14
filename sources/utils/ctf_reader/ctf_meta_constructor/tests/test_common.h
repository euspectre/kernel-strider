/* Common declarations and definitions for tests */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stddef.h> /* offsetof*/

#include "ctf_meta.h"

#ifndef container_of
/* Usefull macro for type convertion */
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif /* container_of */

/* 
 * Implementation for context which maps static memory region 
 * with constant size.
 */
struct test_context_info_static
{
    struct ctf_context_info base;
    const char* map_start;
    int map_start_shift;
    int map_size;
};

static inline int test_context_info_static_ops_extend_map(
    struct ctf_context_info* context_info,
    int new_size, const char** map_start_p, int* map_start_shift_p)
{
    struct test_context_info_static* context_info_static = container_of(
        context_info, typeof(*context_info_static), base);
    
    (void)new_size;
    *map_start_p = context_info_static->map_start;
    *map_start_shift_p = context_info_static->map_start_shift;

    return context_info_static->map_size;
}

static inline void test_context_info_static_init(
    struct test_context_info_static* context_info_static,
    const char* map_start, int map_start_shift, int map_size)
{
    context_info_static->map_start = map_start;
    context_info_static->map_start_shift = map_start_shift;
    context_info_static->map_size = map_size;
    
    context_info_static->base.destroy_info = NULL;
    context_info_static->base.extend_map = test_context_info_static_ops_extend_map;
}


#endif /* TEST_COMMON_H */