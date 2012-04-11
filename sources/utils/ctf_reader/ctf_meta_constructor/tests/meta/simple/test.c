#include "test_common.h"

#include <stdio.h>
#include <assert.h>
#include "ctf_meta.h"

#define checked(op) assert(op)

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    int result;
    
    struct ctf_meta* meta = ctf_meta_create();
    assert(meta);
    /* Declare uint32_t type*/
    checked(ctf_meta_int_begin(meta, "uint32_t") == 0);

    checked(ctf_meta_int_begin_scope(meta) == 0);
    
    checked(ctf_meta_int_set_size(meta, 32) == 0);
    checked(ctf_meta_int_set_align(meta, 32) == 0);
    checked(ctf_meta_int_set_byte_order(meta, ctf_int_byte_order_le) == 0);
    
    ctf_meta_int_end_scope(meta);
    
    struct ctf_type* type_int = ctf_meta_int_end(meta);
    assert(type_int);
    /* Declare structure contained two uint32_t fields */
    checked(ctf_meta_struct_begin(meta, "two_ints", 0) == 0);
    
    checked(ctf_meta_struct_begin_scope(meta) == 0);
    checked(ctf_meta_struct_add_field(meta, "first", type_int) == 0);
    checked(ctf_meta_struct_add_field(meta, "second", type_int) == 0);
    
    ctf_meta_struct_end_scope(meta);
    
    struct ctf_type* type_struct = ctf_meta_struct_end(meta);
    assert(type_struct);
    
    /* Create top-level scope */
    checked(ctf_meta_top_scope_begin(meta, "trace") == 0);
    /* Assign dynamic type */
    checked(ctf_meta_assign_type(meta, "packet.header", type_struct) == 0);
    checked(ctf_meta_top_scope_end(meta) == 0);
    
    result = ctf_meta_instantiate(meta);
    assert(result == 0);

    struct ctf_var* context_var = ctf_meta_find_var(meta, "trace.packet.header");
    assert(context_var != NULL);

    struct ctf_var* var = ctf_var_find_var(context_var, "second");
    assert(var != NULL);
    assert(ctf_var_contains_int(var));

    
    struct test_context_info_static context_info_static;
    
    uint32_t values[2] = {106, 107};
    test_context_info_static_init(&context_info_static,
        (const char*)&values, 0, sizeof(values) * 8);
    
    struct ctf_context* context_test = ctf_meta_create_context(meta,
        context_var, &context_info_static.base, NULL);
    assert(context_test != NULL);
    checked(ctf_var_get_map(var, context_test, NULL));
    uint32_t value_read = ctf_var_get_int32(var, context_test);
    
    printf("Value read is %u(initial value is %u).\n",
        (unsigned)value_read, (unsigned)values[1]);
    assert(value_read == values[1]);
    
    ctf_context_destroy(context_test);
    
    ctf_meta_destroy(meta);
    
    return 0;
}