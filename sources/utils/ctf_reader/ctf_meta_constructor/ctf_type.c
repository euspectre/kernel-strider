/* Implementation of CTF types */

#include "ctf_type.h"
#include "ctf_tag.h"

#include <malloc.h> /* malloc */
#include <string.h> /* string operations*/

#include <errno.h> /* error codes */

#include <endian.h> /* determine endianess */

#include <assert.h> /* assert macro */

#include <ctype.h> /* isdigit */

#include <stdlib.h> /* strtoul */

#include "linked_list.h" /* lists in type implementations */

void ctf_type_container_init(struct ctf_type_container* type_container)
{
    linked_list_head_init(&type_container->types);
}

void ctf_type_container_add_type(
    struct ctf_type_container* type_container, struct ctf_type* type)
{
    linked_list_add_elem(&type_container->types, &type->list_elem);
    //printf("Type %p has been added to type container %p.\n",
    //    type, type_container);
}

void ctf_type_container_remove_type(
    struct ctf_type_container* type_container, struct ctf_type* type)
{
    int result = linked_list_remove(&type_container->types, &type->list_elem);
    ctf_bug_on(result == 0);
}
/* Find type in the container using type name */
struct ctf_type* ctf_type_container_find_type(
    struct ctf_type_container* type_container,
    const char* type_name)
{
    //printf("Search type with name '%s' in container %p.\n",
    //        type_name, type_container);
    struct ctf_type* type;
    linked_list_for_each_entry(&type_container->types, type, list_elem)
    {
        //printf("Check type %p in container %p.\n",
        //    type, type_container);

        if(strcmp(type->name, type_name) == 0) return type;
    }
    
    return NULL;
}
/* Destroy container and all types it contains. */
void ctf_type_container_destroy(
    struct ctf_type_container* type_container)
{
    while(!linked_list_is_empty(&type_container->types))
    {
        struct ctf_type* type;
        linked_list_remove_first_entry(&type_container->types, type, list_elem);
        ctf_type_destroy(type);
    }
}


/*********************** Wrappers implementation **********************/
enum ctf_type_type ctf_type_get_type(struct ctf_type* type)
{
    return type->type_impl->interpret_ops->get_type(type);
}

int ctf_type_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta)
{
    if(type->type_impl->type_ops->set_var_impl == NULL)
    {
        char* var_name = ctf_var_get_full_name(var);
        ctf_err("Implementation for variable '%s' cannot be set because "
            " type cannot be instantiated.", var_name);
        free(var_name);
        return -EINVAL;
    }
    
    return type->type_impl->type_ops->set_var_impl(type, var, meta);
}

int ctf_type_end_type(struct ctf_type* type)
{
    return type->type_impl->interpret_ops->end_type
        ? type->type_impl->interpret_ops->end_type(type)
        : 0;
}


struct ctf_type* ctf_type_create(const char* name)
{
    struct ctf_type *type = malloc(sizeof(*type));
    if(type == NULL)
    {
        ctf_err("Failed to allocate type structure.");
        return NULL;
    }
    
    type->scope = NULL; /* Should be set after if needed */
    linked_list_elem_init(&type->list_elem);
    if(name)
    {
        type->name = strdup(name);
        if(type->name == NULL)
        {
            ctf_err("Failed to allocate name of the type.");
            free(type);
            return NULL;
        }
    }
    else
    {
        type->name = NULL;
    }

    type->type_impl = NULL;
    
    return type;
}

struct ctf_type_impl* ctf_type_set_impl(struct ctf_type* type,
    struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl* old_impl = type->type_impl;
    type->type_impl = type_impl;
    return old_impl;
}

void ctf_type_destroy(struct ctf_type* type)
{
    if(type->type_impl)
        ctf_type_impl_destroy(type->type_impl);
    
    free(type->name);
    
    free(type);
}


/* Exported wrappers for types */

#define ctf_type_is(suffix, meta_suffix)                    \
int ctf_type_is_##suffix(struct ctf_type* type)             \
{                                                           \
    return type->type_impl->interpret_ops->get_type(type)   \
        == ctf_type_type_##meta_suffix;                     \
}

ctf_type_is(int, int)
ctf_type_is(struct, struct)
ctf_type_is(enum, enum)
ctf_type_is(variant, variant)
ctf_type_is(array, array)


#define ctf_type_int_get(callback_name, value_type)                             \
value_type ctf_type_int_##callback_name(struct ctf_type* type)                  \
{                                                                               \
    const struct ctf_type_impl_int_operations* int_ops =                        \
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);   \
    return int_ops->callback_name(type);                                        \
}

ctf_type_int_get(get_byte_order, enum ctf_int_byte_order)
ctf_type_int_get(get_base, enum ctf_int_base)
ctf_type_int_get(get_encoding, enum ctf_int_encoding)
ctf_type_int_get(get_align, int)
ctf_type_int_get(get_size, int)
ctf_type_int_get(is_signed, int)

int ctf_type_array_get_n_elems(struct ctf_type* type)
{
    const struct ctf_type_impl_array_operations* array_ops =
        container_of(type->type_impl->interpret_ops, typeof(*array_ops), base);
    return array_ops->get_n_elems(type);

}

int ctf_type_variant_has_tag(struct ctf_type* type)
{
    const struct ctf_type_impl_variant_operations* variant_ops =
        container_of(type->type_impl->interpret_ops, typeof(*variant_ops), base);
    return variant_ops->has_tag(type);
}

int ctf_type_variant_set_tag(struct ctf_type* type, struct ctf_tag* tag)
{
    const struct ctf_type_impl_variant_operations* variant_ops =
        container_of(type->type_impl->interpret_ops, typeof(*variant_ops), base);
    return variant_ops->set_tag(type, tag);
}


int ctf_type_root_assign_type(struct ctf_type* type_root,
    const char* assign_position_abs, struct ctf_type* assigned_type)
{
    const struct ctf_type_impl_root_operations* root_ops =
        container_of(type_root->type_impl->interpret_ops, typeof(*root_ops), base);
    assert(ctf_type_get_type(type_root) == ctf_type_type_root);
    return root_ops->assign_type(type_root, assign_position_abs, assigned_type);
}

/********** Layout support for variables with fixed alignment *********/
struct ctf_var_impl_fixed_align
{
    struct ctf_var_impl base;
    int align;
    /* Different ways to calculate start offset*/
    union
    {
        /* (absolute) */
        int absolute_offset;
        /* (use_base) */
        struct
        {
            var_rel_index_t base_index;
            int relative_offset;
        }use_base;
        /* (use_prev) */
        struct
        {
            var_rel_index_t prev_index;
        }use_prev;
        /* (use_container) */
        struct
        {
            var_rel_index_t container_index;
        }use_container;
    }start_offset_data;

};

static int var_fixed_align_ops_get_alignment(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    (void)var;
    (void)context;
    
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    return var_impl_fixed_align->align;
}

/* Return absolute value of start offset */
static int var_fixed_align_ops_get_start_offset_absolute(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    (void)var;
    (void)context;

    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    return var_impl_fixed_align->start_offset_data.absolute_offset;
}

/* Return start offset calculated using some base variable */
static int var_fixed_align_ops_get_start_offset_use_base(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    return generic_var_get_start_offset_use_base(context,
        var + var_impl_fixed_align->start_offset_data.use_base.base_index,
        var_impl_fixed_align->start_offset_data.use_base.relative_offset);
}

/* Return start offset calculated using previous variable */
static int var_fixed_align_ops_get_start_offset_use_prev(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    return generic_var_get_start_offset_use_prev(context,
        var + var_impl_fixed_align->start_offset_data.use_prev.prev_index,
        var_impl_fixed_align->align);
}

/* Return start offset calculated using container variable */
static int var_fixed_align_ops_get_start_offset_use_container(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    return generic_var_get_start_offset_use_container(context,
        var + var_impl_fixed_align->start_offset_data.use_container.container_index,
        var_impl_fixed_align->align);
}

/* Same for end offset getters */

static int var_fixed_align_ops_get_end_offset_absolute(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    int size = ctf_var_get_size(var, context);
    if(size == -1) return -1;
    
    return var_impl_fixed_align->start_offset_data.absolute_offset
        + size;
}


static int var_fixed_align_ops_get_end_offset_use_base(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    int start_offset = generic_var_get_start_offset_use_base(context,
        var + var_impl_fixed_align->start_offset_data.use_base.base_index,
        var_impl_fixed_align->start_offset_data.use_base.relative_offset);
    
    if(start_offset == -1) return -1;
    
    int size = ctf_var_get_size(var, context);
    if(size == -1) return -1;

    return start_offset + size;
}

static int var_fixed_align_ops_get_end_offset_use_prev(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    int start_offset = generic_var_get_start_offset_use_prev(context,
        var + var_impl_fixed_align->start_offset_data.use_prev.prev_index,
        var_impl_fixed_align->align);
    
    if(start_offset == -1) return -1;
    
    int size = ctf_var_get_size(var, context);
    if(size == -1) return -1;

    return start_offset + size;
}

static int var_fixed_align_ops_get_end_offset_use_container(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_fixed_align* var_impl_fixed_align =
        container_of(var_impl, typeof(*var_impl_fixed_align), base);
    
    int start_offset = generic_var_get_start_offset_use_container(context,
        var + var_impl_fixed_align->start_offset_data.use_container.container_index,
        var_impl_fixed_align->align);
    
        if(start_offset == -1) return -1;
    
    int size = ctf_var_get_size(var, context);
    if(size == -1) return -1;

    return start_offset + size;
}


/* 
 * Initial operations while determine layout.
 * 
 * Need for creation fields of unions, which 'mirror' field alignment. 
 */
static struct ctf_var_impl_layout_operations
var_fixed_align_ops_layout_initial =
{
    .get_alignment = var_fixed_align_ops_get_alignment
};

/* 
 * Fill layout support for variable.
 * 
 * 'align' field should be set before call.
 * 
 * Returned type of layout may be used for choose layout functions set.
 */
static enum layout_content_type ctf_var_impl_fixed_fill_layout(
    struct ctf_var_impl_fixed_align* var_impl_fixed_align,
    struct ctf_var* var,
    struct ctf_meta* meta)
{
    struct ctf_var* result_var;
    int result_offset;
    enum layout_content_type layout;
    
    var_impl_fixed_align->base.layout_ops =
        &var_fixed_align_ops_layout_initial;
    ctf_var_set_impl(var, &var_impl_fixed_align->base);
    
    layout = ctf_meta_get_layout_content(meta,
        var, &result_var, &result_offset);
    
    switch(layout)
    {
    case layout_content_absolute:
        var_impl_fixed_align->start_offset_data.absolute_offset =
            result_offset;
    break;
    case layout_content_use_base:
        var_impl_fixed_align->start_offset_data.use_base.base_index =
            result_var - var;
        var_impl_fixed_align->start_offset_data.use_base.relative_offset =
            result_offset;
    break;
    case layout_content_use_prev:
        var_impl_fixed_align->start_offset_data.use_prev.prev_index =
            result_var - var;
    break;
    case layout_content_use_container:
        var_impl_fixed_align->start_offset_data.use_container.container_index =
            result_var - var;
    break;
    default:
        ctf_err("Failed to determine layout of integer variable.");
        return layout_content_error;
    }

    return layout;
}

/* 
 * Assign layout callbacks specific for variable with fixed alignment.
 * 
 * Should be used inside 'struct ctf_var_impl_layout_operations'
 * definition.
 * 
 * Note, that .get_size callback should be additionally assigned.
 */
#define ctf_var_impl_fixed_assign_callbacks(layout_type)                \
.get_alignment = var_fixed_align_ops_get_alignment,                     \
.get_start_offset = var_fixed_align_ops_get_start_offset_##layout_type, \
.get_end_offset = var_fixed_align_ops_get_end_offset_##layout_type
     
/************************** Integer type ***************************/
/* Type */

struct ctf_type_impl_int
{
    struct ctf_type_impl base;
    
    /* Whether type is signed or unsigned */
    int is_signed;
    /* Byte ordering */
    enum ctf_int_byte_order order;
    /* Size of type in bits(-1 if unset) */
    int size;
    /* Alignment of type in bits(-1 if unset) */
    int align;
    /* Base of the type; used for pretty-printing */
    enum ctf_int_base _base;
    /* encoding of the type */
    enum ctf_int_encoding encoding;
};

/* Variable */
struct ctf_var_impl_int
{
    /* Integer variable has fixed alignment */
    struct ctf_var_impl_fixed_align base;
    
    struct ctf_type* type;
};

/* 
 * One layout operation for the var, others are provided with
 * 'ctf_var_impl_fixed_align type'.
 */
static int var_int_ops_get_size(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    (void)var;
    (void)context;

    struct ctf_var_impl_int* var_impl_int = container_of(
        var_impl, typeof(*var_impl_int), base.base);

    struct ctf_type_impl_int* type_impl_int = container_of(
        var_impl_int->type->type_impl, typeof(*type_impl_int), base);

    return type_impl_int->size;
}

#define var_int_ops_layout(layout_type)                 \
static struct ctf_var_impl_layout_operations            \
var_int_ops_layout_##layout_type =                      \
{                                                       \
    ctf_var_impl_fixed_assign_callbacks(layout_type),   \
    .get_size = var_int_ops_get_size,                   \
}

var_int_ops_layout(absolute);
var_int_ops_layout(use_base);
var_int_ops_layout(use_prev);
var_int_ops_layout(use_container);

#undef var_int_ops_layout

/* Others callback operations */
static void var_int_destroy_impl(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_int* var_impl_int =
        container_of(var_impl, typeof(*var_impl_int), base.base);

    free(var_impl_int);
}

static struct ctf_type* var_int_ops_get_type(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_int* var_impl_int =
        container_of(var_impl, typeof(*var_impl_int), base.base);
    return var_impl_int->type;
}

/*
 * Interpretators for integers of different sizes and alignments
 */


/*
 * For variable 'var', contained 'size' meaningfull bits(including sign),
 * performs sign extension.
 * NOTE: Meaningless bits assumed to be zeroed.
 */
#define sign_extension(var, size) do{ \
    typeof(var) sign_mask = ((typeof(var))1) << (size - 1); \
    var = (var ^ sign_mask) - sign_mask; \
}while(0)

/*
 * byte-aligned, byte-sized, fit into int32
 */

static uint32_t var_int_ops_get_int32_normal(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    context = ctf_context_get_context_for_var(context, var);
    assert(context != NULL);
    
    int start_offset = ctf_var_get_start_offset(var, context);
    assert(start_offset != -1);
    
    const char* src = context->map_start + start_offset / 8;
    
    struct ctf_var_impl_int* var_impl_int =
        container_of(var_impl, typeof(*var_impl_int), base.base);
    
    struct ctf_type_impl_int* type_impl_int = container_of(
        var_impl_int->type->type_impl, typeof(*type_impl_int), base);

    int size_bytes = type_impl_int->size / 8;
    
    uint32_t value = 0;
    
    int i;
    
    if(type_impl_int->order == ctf_int_byte_order_be)
    {
        for(i = 0; i < size_bytes; i++)
        {
            value = (value << 8) | src[i];
        }
    }
    else
    {
        const char* src_end = &src[size_bytes - 1];
        for(i = 0; i < size_bytes; i++)
        {
            value = (value << 8) | *(src_end - i);
        }
    }
    if((size_bytes < (int)sizeof(value))
        && type_impl_int->is_signed)
    {
        sign_extension(value, size_bytes * 8);
    }
    
    return value;
}


/*
 * byte-aligned, byte-sized.
 */

static void var_int_ops_copy_int_normal(void* dest,
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    context = ctf_context_get_context_for_var(context, var);
    assert(context != NULL);
    
    int start_offset = ctf_var_get_start_offset(var, context);
    assert(start_offset != -1);
    
    struct ctf_var_impl_int* var_impl_int =
        container_of(var_impl, typeof(*var_impl_int), base.base);
    
    struct ctf_type_impl_int* type_impl_int = container_of(
        var_impl_int->type->type_impl, typeof(*type_impl_int), base);

    int size_bytes = type_impl_int->size / 8;

#if __BYTE_ORDER == __BIG_ENDIAN
    if(type_impl_int->order == ctf_int_byte_order_be)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    if(type_impl_int->order == ctf_int_byte_order_le)
#else
#error "Unknown byte order"
#endif
    {
        memcpy(dest, context->map_start + start_offset,
            size_bytes);
    }
#if __BYTE_ORDER == __BIG_ENDIAN
    else if(type_impl_int->order == ctf_int_byte_order_le)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    else if(type_impl_int->order == ctf_int_byte_order_be)
#else
#error "Unknown byte order"
#endif
    {
        const char* src = context->map_start + start_offset
            + size_bytes - 1;

        int i;
        for(i = 0; i < size_bytes; i++, src--, dest++)
            *((char*)dest) = *src;
    }
    else
    {
        ctf_err("Cannot resolve endianess of integer variable.");
        assert(0);
    }
}

/*
 * bit-sized, size <= align(so integer does not cross byte-boundary)
 */
static uint32_t var_int_ops_get_int32_bits(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_int* var_impl_int =
        container_of(var_impl, typeof(*var_impl_int), base.base);
    
    struct ctf_type_impl_int* type_impl_int = container_of(
        var_impl_int->type->type_impl, typeof(*type_impl_int), base);

    context = ctf_context_get_context_for_var(context,
        ctf_var_get_context(var));
    assert(context != NULL);

    int start_offset = ctf_var_get_start_offset(var, context);
    assert(start_offset != -1);
    
    start_offset += context->map_start_shift;
    
    const char* start = context->map_start + start_offset / 8;
    int start_shift = start_offset % 8;
    int size = type_impl_int->size;
    
    uint32_t value = *((const unsigned char*)start);
    value >>= start_shift;
    
    uint32_t value_mask = (1 << size) - 1;
    
    value &= value_mask;
    
    if(type_impl_int->is_signed)
    {
        sign_extension(value, size);
    }
    
    return value;
}

/*
 * bit-sized, size <= align(so integer does not cross byte-boundary)
 */

static void var_int_ops_copy_int_bits(void* dest,
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_int* var_impl_int =
        container_of(var_impl, typeof(*var_impl_int), base.base);

    struct ctf_type_impl_int* type_impl_int = container_of(
        var_impl_int->type->type_impl, typeof(*type_impl_int), base);
    
    context = ctf_context_get_context_for_var(context,
        ctf_var_get_context(var));
    assert(context != NULL);

    int start_offset = ctf_var_get_start_offset(var, context);
    assert(start_offset != -1);
    
    start_offset += context->map_start_shift;
    
    const char* start = context->map_start + start_offset / 8;
    int start_shift = start_offset % 8;
    
    unsigned char value = *((const unsigned char*)start);
    value >>= start_shift;

    *(unsigned char*)dest = value;
}


static struct ctf_var_impl_int_operations var_int_ops_interpret_bytes =
{
    .base = {.get_type = var_int_ops_get_type},
    .copy_int = var_int_ops_copy_int_normal
};

static struct ctf_var_impl_int_operations var_int_ops_interpret_normal32 =
{
    .base = {.get_type = var_int_ops_get_type},
    .copy_int = var_int_ops_copy_int_normal,
    .get_int32 = var_int_ops_get_int32_normal,
};

static struct ctf_var_impl_int_operations var_int_ops_interpret_bits =
{
    .base = {.get_type = var_int_ops_get_type},
    .copy_int = var_int_ops_copy_int_bits,
    .get_int32 = var_int_ops_get_int32_bits,
};



/* Callback operations for the type */
static void type_int_ops_destroy_impl(
    struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type_impl, typeof(*type_impl_int), base);
    
    free(type_impl_int);
}


static int type_int_ops_get_max_alignment(struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    return type_impl_int->align;
}

/* 
 * Create variable implementation for integer type.
 * 
 * This function is reused in enumeration type.
 */

static int ctf_var_impl_int_init(struct ctf_var_impl_int* var_impl_int,
    struct ctf_type* type,
    struct ctf_meta* meta,
    struct ctf_var* var)
{
    var_impl_int->type = type;
    
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    
    var_impl_int->base.align = type_impl_int->align;
    
    enum layout_content_type layout = ctf_var_impl_fixed_fill_layout(
        &var_impl_int->base, var, meta);
    
    switch(layout)
    {
#define case_layout_ops(layout_type) case layout_content_##layout_type: \
var_impl_int->base.base.layout_ops = &var_int_ops_layout_##layout_type;  \
break
    case_layout_ops(absolute);
    case_layout_ops(use_base);
    case_layout_ops(use_prev);
    case_layout_ops(use_container);
#undef case_layout_ops
    default:
        return -EINVAL;
    }
    
    if(type_impl_int->size < 8)
    {
        assert(type_impl_int->size <= type_impl_int->align);
        
        var_impl_int->base.base.interpret_ops =
            &var_int_ops_interpret_bits.base;
    }
    else
    {
        assert((type_impl_int->size % 8) == 0);
        assert((type_impl_int->align % 8) == 0);
        
        if(type_impl_int->size <= 32)
        {
            var_impl_int->base.base.interpret_ops =
                &var_int_ops_interpret_normal32.base;
        }
        else
        {
            var_impl_int->base.base.interpret_ops =
                &var_int_ops_interpret_bytes.base;
        }
    }
    return 0;
}

static int type_int_ops_set_var_impl(struct ctf_type* type,
    struct ctf_var* var,
    struct ctf_meta* meta)
{
    int result;
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    struct ctf_var_impl_int* var_impl_int =
        malloc(sizeof(*var_impl_int));
    if(var_impl_int == NULL)
    {
        ctf_err("Failed to allocate memory for int variable.");
        return -ENOMEM;
    }
    
    result = ctf_var_impl_int_init(var_impl_int, type, meta, var);
    if(result < 0)
    {
        free(var_impl_int);
        return result;
    }
    
    var_impl_int->base.base.destroy_impl = var_int_destroy_impl;
    ctf_var_set_impl(var, &var_impl_int->base.base);
    
    return 0;
}

static struct ctf_type_impl* type_int_ops_clone(
    struct ctf_type_impl* type_impl)
{
    /* Make 'hard' clone simply copiing fields. */
    struct ctf_type_impl_int* type_impl_int = container_of(
        type_impl, typeof(*type_impl_int), base);
        
    struct ctf_type_impl_int* type_impl_int_clone = malloc(
        sizeof(*type_impl_int_clone));
    if(type_impl_int_clone == NULL)
    {
        ctf_err("Failed to allocate type implementation for cloned int.");
        return NULL;
    }
    memcpy(type_impl_int_clone, type_impl_int,
        sizeof(*type_impl_int_clone));
    
    return &type_impl_int_clone->base;
}


struct ctf_type_impl_operations type_int_ops =
{
    .destroy_impl =         type_int_ops_destroy_impl,
    .get_max_alignment =    type_int_ops_get_max_alignment,
    .set_var_impl =         type_int_ops_set_var_impl,
    .clone =                type_int_ops_clone,
};


static enum ctf_type_type type_int_ops_get_type(
    struct ctf_type* type)
{
    (void)type;
    return ctf_type_type_int;
}

static int type_int_ops_end_type(struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    enum ctf_int_encoding encoding = type_impl_int->encoding;
    if(encoding == ctf_int_encoding_unknown)
        encoding = ctf_int_encoding_none;
    if(encoding != ctf_int_encoding_none)
    {
        ctf_err("Encodings other than 'none' are currently not supported.");
        return -EINVAL;
    }
    
    int size = type_impl_int->size;
    if(size == -1)
    {
        ctf_err("Size of the integer should be set.");
        return -EINVAL;
    }
    
    int align = type_impl_int->align;
    if(align == -1)
    {
        if(size < 8) align = 1;
        else align = 8;
    }
    
    if((size > 8) && (size % 8))
    {
        ctf_err("Sizes which are not multiple to bytes and more than "
            "byte are not supported.");
        return -EINVAL;
    }
    else if((size < 8) && (align < size))
    {
        ctf_err("Sizes which are not multiple to bytes are not supported "
            "if them cross byte bounadry");
        return -EINVAL;
    }
    int is_signed = type_impl_int->is_signed;
    if(is_signed == -1) is_signed = 0;
    
    enum ctf_int_base base = type_impl_int->_base;
    if(base == ctf_int_base_unknown) base = ctf_int_base_decimal;
    
    enum ctf_int_byte_order order = type_impl_int->order;
    if(order == ctf_int_byte_order_unknown)
        order = ctf_int_byte_order_native;
    
    if(order == ctf_int_byte_order_native)
    {
        ctf_err("Native byte order is currently not supported.");
        return -EINVAL;
    }
    
    type_impl_int->align = align;
    type_impl_int->size = size;
    type_impl_int->is_signed = is_signed;
    type_impl_int->encoding = encoding;
    type_impl_int->order = order;
    type_impl_int->_base = base;
    
    return 0;
}


static int is_power_2(int value)
{
    while(value != 0)
    {
        if(value & 1) return (value == 1);
        value >>= 1;
    };
    
    return 0;
}


static int type_int_ops_set_size(struct ctf_type* type, int size)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    if(size <= 0)
    {
        ctf_err("Only positive size is allowed for integers.");
        return -EINVAL;
    }
    
    type_impl_int->size = size;
    return 0;
}

static int type_int_ops_set_align(struct ctf_type* type, int align)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    if(align <= 0)
    {
        ctf_err("Only positive alignment is allowed for integers.");
        return -EINVAL;
    }
    if(!is_power_2(align))
    {
        ctf_err("Alignment should be power of two.");
        return -EINVAL;
    }
    
    type_impl_int->align = align;
    return 0;
}

static int type_int_ops_set_signed(struct ctf_type* type, int is_signed)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    type_impl_int->is_signed = is_signed ? 1 : 0;
    return 0;
}

static int type_int_ops_set_byte_order(struct ctf_type* type,
    enum ctf_int_byte_order byte_order)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    switch(byte_order)
    {
    case ctf_int_byte_order_be:
    case ctf_int_byte_order_le:
        type_impl_int->order = byte_order;
    break;
    case ctf_int_byte_order_native:
        ctf_err("Native byte order currently not supported");
        return -EINVAL;
    default:
        ctf_err("Incorrect byte order for set");
        return -EINVAL;
    }
    return 0;
}

static int type_int_ops_set_encoding(struct ctf_type* type,
    enum ctf_int_encoding encoding)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    switch(encoding)
    {
    case ctf_int_encoding_none:
        type_impl_int->encoding = encoding;
    break;
    case ctf_int_encoding_ascii:
    case ctf_int_encoding_utf8:
        ctf_err("Integer encodings other than 'none' currently are not supported.");
        return -EINVAL;
    default:
        ctf_err("Incorrect encoding for set");
        return -EINVAL;
    }
    return 0;
}

static int type_int_ops_set_base(struct ctf_type* type,
    enum ctf_int_base base)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);

    switch(base)
    {
    case ctf_int_base_binary:
    case ctf_int_base_decimal:
    case ctf_int_base_hexadecimal:
    case ctf_int_base_hexadecimal_upper:
    case ctf_int_base_octal:
    case ctf_int_base_pointer:
    case ctf_int_base_unsigned:
        type_impl_int->_base = base;
    break;
    default:
        ctf_err("Incorrect integer's base for set");
        return -EINVAL;
    }
    return 0;
}


static enum ctf_int_byte_order type_int_ops_get_byte_order(
    struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    
    return type_impl_int->order;
}
static enum ctf_int_base type_int_ops_get_base(struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    
    return type_impl_int->_base;
}

static enum ctf_int_encoding type_int_ops_get_encoding(struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    
    return type_impl_int->encoding;
}
static int type_int_ops_get_align(struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    
    return type_impl_int->align;
}

static int type_int_ops_get_size(struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    
    return type_impl_int->size;
}

static int type_int_ops_is_signed(struct ctf_type* type)
{
    struct ctf_type_impl_int* type_impl_int =
        container_of(type->type_impl, typeof(*type_impl_int), base);
    
    return type_impl_int->is_signed;
}

static struct ctf_type_impl_int_operations type_int_ops_interpret =
{
    .base =
    {
        .get_type   = type_int_ops_get_type,
        .end_type        = type_int_ops_end_type,
    },
    .set_align      = type_int_ops_set_align,
    .set_size       = type_int_ops_set_size,
    .set_signed     = type_int_ops_set_signed,
    .set_byte_order = type_int_ops_set_byte_order,
    .set_base       = type_int_ops_set_base,
    .set_encoding   = type_int_ops_set_encoding,
    
    .get_align      = type_int_ops_get_align,
    .get_size       = type_int_ops_get_size,
    .is_signed      = type_int_ops_is_signed,
    .get_byte_order = type_int_ops_get_byte_order,
    .get_base       = type_int_ops_get_base,
    .get_encoding   = type_int_ops_get_encoding
};

struct ctf_type_impl* ctf_type_impl_int_create(void)
{
    struct ctf_type_impl_int* type_impl_int =
        malloc(sizeof(*type_impl_int));
    if(type_impl_int == NULL)
    {
        ctf_err("Failed to allocate memory for integer type implementation.");
        return NULL;
    }
    
    type_impl_int->size = -1;
    type_impl_int->align = -1;
    type_impl_int->is_signed = -1;
    
    type_impl_int->_base = ctf_int_base_unknown;
    type_impl_int->encoding = ctf_int_encoding_unknown;
    type_impl_int->order = ctf_int_byte_order_unknown;
    
    type_impl_int->base.type_ops = &type_int_ops;
    type_impl_int->base.interpret_ops = &type_int_ops_interpret.base;
    
    return &type_impl_int->base;
}

/************************** CTF structure ****************************/
/* Field of the structure */
struct ctf_struct_field
{
    struct linked_list_elem list_elem;

    char* name;
    struct ctf_type* type;
};

static struct ctf_struct_field* ctf_struct_field_create(
    const char* name, struct ctf_type* type)
{
    struct ctf_struct_field* field = malloc(sizeof(*field));
    if(field == NULL)
    {
        ctf_err("Failed to allocate field for the structure.");
        return NULL;
    }
    field->name = strdup(name);
    if(field->name == NULL)
    {
        ctf_err("Failed to allocate name of the structure field.");
        free(field);
        return NULL;
    }
    
    linked_list_elem_init(&field->list_elem);
    
    field->type = type;
    
    return field;
}

void ctf_struct_field_destroy(struct ctf_struct_field* field)
{
    free(field->name);
    free(field);
}

/* Type */
struct ctf_type_impl_struct
{
    struct ctf_type_impl base;
    
    struct linked_list_head fields;
    /* Total alignment of the struct */
    int align;
};

/* Variable */
struct ctf_var_impl_struct
{
    struct ctf_var_impl_fixed_align base;

    struct ctf_type* type;
    
    union
    {
        int size_constant;
        /* Need for searching end boundary */
        var_rel_index_t last_field_index;
    }size_data;
};

/* Callback operations for struct variable */
static void var_struct_destroy_impl(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_struct* var_impl_struct =
        container_of(var_impl, typeof(*var_impl_struct), base.base);
    
    free(var_impl_struct);
}

static struct ctf_type* var_struct_ops_get_type(
    struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_struct* var_impl_struct =
        container_of(var_impl, typeof(*var_impl_struct), base.base);
    
    return var_impl_struct->type;
}

static struct ctf_var_impl_interpret_operations var_struct_ops_interpret =
{
    .get_type = var_struct_ops_get_type,
};

/* 
 * Only one layout operations is required - others are defined
 * by 'ctf_var_impl_fixed_align' type.
 */

/* Return constant size of the structure */
static int var_struct_ops_get_size_constant(
    struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    (void)var;
    (void)context;
    
    struct ctf_var_impl_struct* var_impl_struct =
        container_of(var_impl, typeof(*var_impl_struct), base.base);
    
    return var_impl_struct->size_data.size_constant;
}

/* Return size when structure has element with non-constant size*/
static int var_struct_ops_get_size_float(
    struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    struct ctf_var_impl_struct* var_impl_struct =
        container_of(var_impl, typeof(*var_impl_struct), base.base);
    
    int last_child_end_offset = ctf_var_get_end_offset(
        var + var_impl_struct->size_data.last_field_index, context);
    if(last_child_end_offset == -1) return -1;
    
    int start_offset = ctf_var_get_start_offset(var, context);
    if(start_offset == -1)
    {
        /* In current implementation this is impossible, but anywhere */
        return -1;
    }
    
    return last_child_end_offset - start_offset;
}


#define var_struct_ops_layout(layout_type)                                  \
static struct ctf_var_impl_layout_operations                                \
var_struct_ops_constant_##layout_type =                                     \
{                                                                           \
    ctf_var_impl_fixed_assign_callbacks(layout_type),                       \
    .get_size = var_struct_ops_get_size_constant,                           \
};                                                                          \
static struct ctf_var_impl_layout_operations                                \
var_struct_ops_float_##layout_type =                                        \
{                                                                           \
    ctf_var_impl_fixed_assign_callbacks(layout_type),                       \
    .get_size = var_struct_ops_get_size_float,                              \
}


var_struct_ops_layout(absolute);
var_struct_ops_layout(use_base);
var_struct_ops_layout(use_prev);
var_struct_ops_layout(use_container);

#undef var_struct_ops_layout

/* Callbacks for structure type */
static void type_struct_ops_destroy_impl(
    struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl_struct* type_impl_struct =
        container_of(type_impl, typeof(*type_impl_struct), base);

    while(!linked_list_is_empty(&type_impl_struct->fields))
    {
        struct ctf_struct_field* field;
        linked_list_remove_first_entry(&type_impl_struct->fields,
            field, list_elem);
        ctf_struct_field_destroy(field);
    }
    
    free(type_impl_struct);
}

static int type_struct_ops_get_max_alignment(struct ctf_type* type)
{
    struct ctf_type_impl_struct* type_impl_struct =
        container_of(type->type_impl, typeof(*type_impl_struct), base);

    return type_impl_struct->align;
}

static struct ctf_tag_component* type_struct_ops_resolve_tag_component(
    struct ctf_type* type, const char* str, const char** component_end)
{
    struct ctf_type_impl_struct* type_impl_struct =
        container_of(type->type_impl, typeof(*type_impl_struct), base);

    struct ctf_struct_field* field;
    linked_list_for_each_entry(&type_impl_struct->fields, field, list_elem)
    {
        const char* _component_end = test_tag_component(field->name, str);
        if(_component_end)
        {
            struct ctf_tag_component* tag_component =
                ctf_tag_component_create(field->name, field->type, -1);
            if(tag_component == NULL) return NULL;
            
            *component_end = _component_end;
            return tag_component;
        }
    }
    return NULL;
}

static int type_struct_ops_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta)
{
    enum layout_content_type layout;
    
    struct ctf_struct_field* field;
    
    struct ctf_type_impl_struct* type_impl_struct =
        container_of(type->type_impl, typeof(*type_impl_struct), base);

    struct ctf_var_impl_struct* var_impl_struct =
        malloc(sizeof(*var_impl_struct));
    
    if(var_impl_struct == NULL)
    {
        ctf_err("Failed to allocate structure for structure type.");
        return -ENOMEM;
    }
    
    var_impl_struct->type = type;
    var_impl_struct->base.base.destroy_impl = var_struct_destroy_impl;
    var_impl_struct->base.base.interpret_ops = &var_struct_ops_interpret;
    var_impl_struct->base.align = type_impl_struct->align;
    layout = ctf_var_impl_fixed_fill_layout(&var_impl_struct->base,
        var, meta);
    
    /* Set layout operations while field variables are costructed */
    switch(layout)
    {
#define case_struct_layout_ops_constructed(layout_type)                         \
case layout_content_##layout_type:                                              \
var_impl_struct->base.base.layout_ops = &var_struct_ops_constant_##layout_type; \
break

    case_struct_layout_ops_constructed(absolute);
    case_struct_layout_ops_constructed(use_base);
    case_struct_layout_ops_constructed(use_prev);
    case_struct_layout_ops_constructed(use_container);

#undef case_struct_layout_ops
    default:
        free(var_impl_struct);
        return -EINVAL;
    }
    
    ctf_var_set_impl(var, &var_impl_struct->base.base);
    /* Instantiate fields */
    
    /* Lastly instantiated field */
    struct ctf_var* last_field_var = NULL;
    /* 
     * If current size of structure is constant, contains this size.
     * Otherwise -1.
     */
    int size_constant = 0;

    linked_list_for_each_entry(&type_impl_struct->fields, field, list_elem)
    {
        /* Store variable index before instantiate new field. */
        var_rel_index_t var_index = var - meta->vars;
        
        int result = ctf_meta_add_var(meta,
            field->name, field->type,
            var, var, last_field_var);
        /* Pointer to the structure variable may changed, update it*/
        var = meta->vars + var_index;
        if(result < 0)
        {
            ctf_err("Failed to add variable corresponded to the "
                "structure field.");
            /* Clear impl and destroy it */
            ctf_var_set_impl(var, NULL);
            free(var_impl_struct);
            
            return -ENOMEM;
        }
        last_field_var = meta->vars + result;
        
        if(size_constant != -1)
        {
            int field_align = ctf_var_get_alignment(last_field_var, NULL);
            if(field_align == -1)
            {
                /* 
                 * Field with non-constant alignment.
                 * Total size cannot be constant.
                 */
                size_constant = -1;
                continue;
            }
            int field_size = ctf_var_get_size(last_field_var, NULL);
            if(field_size == -1)
            {
                /* 
                 * Field with non-constant size.
                 * Total size cannot be constant.
                 */
                size_constant = -1;
                continue;
            }
            size_constant = align_val(size_constant, field_align)
                + field_size;
        }
    }
    
    if(size_constant != -1)
    {
        var_impl_struct->size_data.size_constant = size_constant;
    }
    else
    {
        var_impl_struct->size_data.last_field_index = last_field_var - var;
    }
    
    /* Final layout operations */
    switch(layout)
    {
#define case_struct_layout_ops(layout_type)                     \
case layout_content_##layout_type:                              \
var_impl_struct->base.base.layout_ops = (size_constant != -1)   \
    ? &var_struct_ops_constant_##layout_type                    \
    : &var_struct_ops_float_##layout_type;                      \
break

    case_struct_layout_ops(absolute);
    case_struct_layout_ops(use_base);
    case_struct_layout_ops(use_prev);
    case_struct_layout_ops(use_container);

#undef case_struct_layout_ops
    default:
        /* unreachable - layout already checked before */
        assert(0);
        break;
    }
    
    return 0;
}

static struct ctf_type_impl* type_struct_ops_clone(
    struct ctf_type_impl* type_impl)
{
    /* Hard clone, fields are copied also */
    struct ctf_type_impl_struct* type_impl_struct =
        container_of(type_impl, typeof(*type_impl_struct), base);
    
    struct ctf_type_impl_struct* type_impl_struct_clone = malloc(
        sizeof(*type_impl_struct_clone));
    if(type_impl_struct_clone == NULL)
    {
        ctf_err("Failed to allocate cloned type implementation for structure.");
        return NULL;
    }
    
    type_impl_struct_clone->base = type_impl_struct->base;
    type_impl_struct_clone->align = type_impl_struct->align;
    
    linked_list_head_init(&type_impl_struct_clone->fields);
    
    struct ctf_struct_field* field;
    
    linked_list_for_each_entry(&type_impl_struct->fields, field, list_elem)
    {
        struct ctf_struct_field* field_clone = ctf_struct_field_create(
            field->name, field->type);
        if(field_clone == NULL)
        {
            ctf_err("Failed to clone structure field.");
            ctf_type_impl_destroy(&type_impl_struct_clone->base);
            return NULL;
        }
        linked_list_add_elem(&type_impl_struct_clone->fields,
            &field_clone->list_elem);
    }
    
    return &type_impl_struct_clone->base;
}

static struct ctf_type_impl_operations type_struct_ops =
{
    .destroy_impl =             type_struct_ops_destroy_impl,
    .get_max_alignment =        type_struct_ops_get_max_alignment,
    .set_var_impl =             type_struct_ops_set_var_impl,
    .resolve_tag_component =    type_struct_ops_resolve_tag_component,
    .clone =                    type_struct_ops_clone,
};

/* Interpretation callbacks for structure type */
static enum ctf_type_type type_struct_ops_get_type(
    struct ctf_type* type)
{
    (void)type;
    return ctf_type_type_struct;
}

static int type_struct_ops_add_field(struct ctf_type* type,
    const char* field_name, struct ctf_type* field_type)
{
    assert(ctf_type_get_type(type) == ctf_type_type_struct);
    struct ctf_type_impl_struct* type_impl_struct =
        container_of(type->type_impl, typeof(*type_impl_struct), base);

    struct ctf_struct_field* field = ctf_struct_field_create(field_name, field_type);
    if(field == NULL) return -ENOMEM;
    
    linked_list_add_elem(&type_impl_struct->fields, &field->list_elem);
    
    int field_max_align = ctf_type_get_max_alignment(field_type);
    assert(field_max_align != -1);
    
    if(type_impl_struct->align < field_max_align)
        type_impl_struct->align = field_max_align;
    
    return 0;
}

static struct ctf_type_impl_struct_operations type_struct_ops_interpret =
{
    .base =
    {
        .get_type = type_struct_ops_get_type,
        /* 'end' callback do nothing */
    },
    .add_field = type_struct_ops_add_field
};

struct ctf_type_impl* ctf_type_impl_struct_create(void)
{
    struct ctf_type_impl_struct* type_impl_struct =
        malloc(sizeof(*type_impl_struct));
    if(type_impl_struct == NULL)
    {
        ctf_err("Failed allocate memory for structure type desctiptor.\n");
        return NULL;
    }
    
    linked_list_head_init(&type_impl_struct->fields);
    
    type_impl_struct->align = 1;
    
    type_impl_struct->base.type_ops = &type_struct_ops;
    type_impl_struct->base.interpret_ops = &type_struct_ops_interpret.base;
    
    return &type_impl_struct->base;
}


/************************** CTF enum *********************************/
/* One string-value of enumeration */
struct ctf_enum_value
{
    //TODO: Currently list organization, no search optimization.
    struct linked_list_elem list_elem;
    
    char* val_name;
    /* 
     * Range of the value: [start, end]
     * 
     * Assume that enum is wrap integer, which values may be represented
     * using 32-bit.
     */
    int32_t start;
    int32_t end;
};

static struct ctf_enum_value* ctf_enum_value_create(
    const char* val_name, int32_t start, int32_t end)
{
    struct ctf_enum_value* val = malloc(sizeof(*val));
    if(val == NULL)
    {
        ctf_err("Failed to allocate enumeration value structure.");
        return NULL;
    }
    
    val->val_name = strdup(val_name);
    if(val_name == NULL)
    {
        ctf_err("Failed to allocate name of enumeration value");
        free(val);
        return NULL;
    }
    
    linked_list_elem_init(&val->list_elem);
    
    val->start = start;
    val->end = end;
    
    return val;
}

static void ctf_enum_value_destroy(struct ctf_enum_value* val)
{
    free(val->val_name);
    free(val);
}

/* Type */
struct ctf_type_impl_enum
{
    struct ctf_type_impl base;
    
    struct ctf_type* type_int;
    
    struct linked_list_head values;
};

/* Variable */
struct ctf_var_impl_enum
{
    struct ctf_var_impl base;
    /* Pointer to the implementation of variable as integer */
    struct ctf_var_impl* var_int_impl;
    
    struct ctf_type* type;
};


static void ctf_var_impl_enum_destroy(struct ctf_var_impl_enum* var_impl_enum)
{
    if(var_impl_enum->var_int_impl->destroy_impl)
        var_impl_enum->var_int_impl->destroy_impl(var_impl_enum->var_int_impl);

    free(var_impl_enum);
}

static void var_enum_destroy_impl(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_enum* var_impl_enum = 
        container_of(var_impl, typeof(*var_impl_enum), base);
    
    ctf_var_impl_enum_destroy(var_impl_enum);
}

/* 
 * Layout callbacks for enum variable.
 * 
 * Really, them are wrappers around corresponded integer callbacks.
 */

#define var_enum_ops_layout_wrapper(callback)                           \
static int var_enum_ops_##callback(struct ctf_var_impl* var_impl,       \
    struct ctf_var* var, struct ctf_context* context)                   \
{                                                                       \
    struct ctf_var_impl_enum* var_impl_enum =                           \
        container_of(var_impl, typeof(*var_impl_enum), base);           \
    struct ctf_var_impl* var_int_impl = var_impl_enum->var_int_impl;    \
    return var_int_impl->layout_ops->callback(var_int_impl, var,        \
        context);                                                       \
}

var_enum_ops_layout_wrapper(get_alignment)
var_enum_ops_layout_wrapper(get_start_offset)
var_enum_ops_layout_wrapper(get_size)
var_enum_ops_layout_wrapper(get_end_offset)

static struct ctf_var_impl_layout_operations var_enum_ops_layout =
{
    .get_alignment      = var_enum_ops_get_alignment,
    .get_start_offset   = var_enum_ops_get_start_offset,
    .get_size           = var_enum_ops_get_size,
    .get_end_offset     = var_enum_ops_get_end_offset
};

/*
 * Interpret callbacks for enum variable.
 * 
 * Integer interpretators are also wrappers.
 */
static void var_enum_ops_copy_int(void* dest,
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_enum* var_impl_enum =
        container_of(var_impl, typeof(*var_impl_enum), base);
    struct ctf_var_impl* var_int_impl = var_impl_enum->var_int_impl;
    struct ctf_var_impl_int_operations* int_ops =
        container_of(var_int_impl->interpret_ops, typeof(*int_ops), base);
    int_ops->copy_int(dest, var_int_impl, var,
        context);
}

static uint32_t var_enum_ops_get_int32(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    struct ctf_var_impl_enum* var_impl_enum =
        container_of(var_impl, typeof(*var_impl_enum), base);
    struct ctf_var_impl* var_int_impl = var_impl_enum->var_int_impl;
    struct ctf_var_impl_int_operations* int_ops =
        container_of(var_int_impl->interpret_ops, typeof(*int_ops), base);
    return int_ops->get_int32(var_int_impl, var,
        context);
}

/* Enum specialization over integer one. */

static struct ctf_type* var_enum_ops_get_type(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_enum* var_impl_enum =
        container_of(var_impl, typeof(*var_impl_enum), base);
    return var_impl_enum->type;
}

static const char* var_enum_ops_get_enum(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    struct ctf_var_impl_enum* var_impl_enum = 
        container_of(var_impl, typeof(*var_impl_enum), base);
    struct ctf_type_impl_enum* type_impl_enum = 
        container_of(var_impl_enum->type->type_impl, typeof(*type_impl_enum), base);
    
    int int_val = ctf_var_get_int32(var, context);
    
    struct ctf_enum_value* enum_val;
    linked_list_for_each_entry(&type_impl_enum->values, enum_val, list_elem)
    {
        if((int_val <= enum_val->end) && (int_val >= enum_val->start))
            return enum_val->val_name;
    }
    return NULL;
}

static struct ctf_var_impl_enum_operations var_enum_ops_interpret =
{
    .base =
    {
        .base = {.get_type = var_enum_ops_get_type},
        .copy_int = var_enum_ops_copy_int,
        .get_int32 = var_enum_ops_get_int32,
    //... (other interpret operations for int)
    },
    .get_enum = var_enum_ops_get_enum
};

/* Callbacks for type */

static void type_enum_ops_destroy_impl(
    struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl_enum* type_impl_enum =
        container_of(type_impl, typeof(*type_impl_enum), base);
    
    while(!linked_list_is_empty(&type_impl_enum->values))
    {
        struct ctf_enum_value* value;
        linked_list_remove_first_entry(&type_impl_enum->values, value, list_elem);

        ctf_enum_value_destroy(value);
    }
    
    free(type_impl_enum);
}

static int type_enum_ops_get_max_alignment(struct ctf_type* type)
{
    struct ctf_type_impl_enum* type_impl_enum =
        container_of(type->type_impl, typeof(*type_impl_enum), base);
    
    struct ctf_type* type_int = type_impl_enum->type_int;
    
    return type_int->type_impl->type_ops->get_max_alignment(type_int);
}

static int type_enum_ops_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta)
{
    int result;
    struct ctf_type_impl_enum* type_impl_enum = 
        container_of(type->type_impl, typeof(*type_impl_enum), base);
    
    struct ctf_var_impl_enum* var_impl_enum =
        malloc(sizeof(*var_impl_enum));
    if(var_impl_enum == NULL)
    {
        ctf_err("Failed to allocate implementation for enum variable.");
        return -ENOMEM;
    }
    
    struct ctf_type* type_int = type_impl_enum->type_int;
    /* Set integer variable implementation.. */
    result = type_int->type_impl->type_ops->set_var_impl(type_int, var,
        meta);
    
    if(result)
    {
        free(var_impl_enum);
        return result;
    }
    
    /* .. and insert enum variable implementation instead of int.*/
    var_impl_enum->var_int_impl = var->var_impl;
    var_impl_enum->type = type;
    
    var_impl_enum->base.destroy_impl = var_enum_destroy_impl;
    var_impl_enum->base.layout_ops = &var_enum_ops_layout;
    var_impl_enum->base.interpret_ops = &var_enum_ops_interpret.base.base;
    
    ctf_var_set_impl(var, &var_impl_enum->base);
    
    return 0;
}

static struct ctf_type_impl* type_enum_ops_clone(
    struct ctf_type_impl* type_impl)
{
    /* Hard clone, fields are copied also */
    struct ctf_type_impl_enum* type_impl_enum =
        container_of(type_impl, typeof(*type_impl_enum), base);
    
    struct ctf_type_impl_enum* type_impl_enum_clone = malloc(
        sizeof(*type_impl_enum_clone));
    if(type_impl_enum_clone == NULL)
    {
        ctf_err("Failed to allocate cloned type implementation for enumeration.");
        return NULL;
    }
    
    type_impl_enum_clone->base = type_impl_enum->base;
    type_impl_enum_clone->type_int = type_impl_enum->type_int;
    
    linked_list_head_init(&type_impl_enum_clone->values);
    
    struct ctf_enum_value* value;
    linked_list_for_each_entry(&type_impl_enum->values, value, list_elem)
    {
        struct ctf_enum_value* value_clone = ctf_enum_value_create(
            value->val_name, value->start, value->end);
        if(value_clone == NULL)
        {
            ctf_err("Failed to clone enumeration value.");
            ctf_type_impl_destroy(&type_impl_enum_clone->base);
            return NULL;
        }
        
        linked_list_add_elem(&type_impl_enum_clone->values,
            &value_clone->list_elem);
    }
    
    return &type_impl_enum_clone->base;
}


static struct ctf_type_impl_operations type_enum_ops =
{
    .destroy_impl = type_enum_ops_destroy_impl,
    .get_max_alignment = type_enum_ops_get_max_alignment,
    .set_var_impl = type_enum_ops_set_var_impl,
    .clone = type_enum_ops_clone,
};

static enum ctf_type_type type_enum_ops_get_type(
    struct ctf_type* type)
{
    (void)type;
    return ctf_type_type_enum;
}

static int type_enum_ops_add_value32(struct ctf_type* type,
    const char* val_name, int32_t start, int32_t end)
{
    struct ctf_type_impl_enum* type_impl_enum = 
        container_of(type->type_impl, typeof(*type_impl_enum), base);

    struct ctf_enum_value* value = ctf_enum_value_create(val_name,
        start, end);
    if(value == NULL) return -ENOMEM;
    
    linked_list_add_elem(&type_impl_enum->values, &value->list_elem);
   
    return 0;
}

static struct ctf_type_impl_enum_operations type_enum_ops_interpret =
{
    .base =
    {
        .get_type = type_enum_ops_get_type,
        /* 'end' callback do nothing */
    },
    .add_value32 = type_enum_ops_add_value32,
};

struct ctf_type_impl* ctf_type_impl_enum_create(
    struct ctf_type* type_int)
{
    if(ctf_type_get_type(type_int) != ctf_type_type_int)
    {
        ctf_err("Enumeration should be based on integer type.");
        return NULL;
    }
    
    if(ctf_type_int_get_size(type_int) > 32)
    {
        ctf_err("Enumerations based on integers which not fit into 32-bit "
        "are currently not supported.");
        return NULL;
    }
    
    struct ctf_type_impl_enum* type_impl_enum =
        malloc(sizeof(*type_impl_enum));
    if(type_impl_enum == NULL)
    {
        ctf_err("Failed to allocate implementation for enum type.");
        return NULL;
    }
    
    type_impl_enum->type_int = type_int;
    type_impl_enum->base.type_ops = &type_enum_ops;
    type_impl_enum->base.interpret_ops = &type_enum_ops_interpret.base;
    
    linked_list_head_init(&type_impl_enum->values);
    
    return &type_impl_enum->base;
}



/************************* CTF variant ********************************/
/* Variant field */
struct ctf_variant_field
{
    struct linked_list_elem list_elem;
    
    char* name;
    struct ctf_type* type;
};

static struct ctf_variant_field*
ctf_variant_field_create(const char* name,
    struct ctf_type* type)
{
    struct ctf_variant_field* field = malloc(sizeof(*field));
    if(field == NULL)
    {
        ctf_err("Failed to allocate field of variant.");
        return NULL;
    }
    
    field->name = strdup(name);
    if(field->name == NULL)
    {
        ctf_err("Failed to allocate name of the field of variant.");
        free(field);
        return NULL;
    }
    
    field->type = type;
    
    linked_list_elem_init(&field->list_elem);
    
    return field;
}

static void ctf_variant_field_destroy(
    struct ctf_variant_field* field)
{
    free(field->name);
    free(field);
}

/* Type */
struct ctf_type_impl_variant
{
    struct ctf_type_impl base;
    
    int max_alignment;
    
    struct linked_list_head fields;
    /* NULL for untagged variant */
    struct ctf_tag* tag;
};

/* Variable */
struct ctf_var_impl_variant
{
    struct ctf_var_impl base;
    
    struct ctf_type* type_variant;
    /* 
     * Because alignment of variant is variable, its layout functions
     * may use either previous element or container.
     */
    union
    {
        var_rel_index_t prev_index;
        var_rel_index_t container_index;
    } start_offset_data;
    
    union
    {
        struct ctf_var_tag* var_tag;
        /* This field is used while created fields of the variant */
        var_rel_index_t field_index;
    }u;
};

/* Variable interpret callbacks */

static void var_variant_destroy_impl(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);

    ctf_var_tag_destroy(var_impl_variant->u.var_tag);
    free(var_impl_variant);
}

static struct ctf_type* var_variant_ops_get_type(
    struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);
    
    return var_impl_variant->type_variant;
}

static int var_variant_ops_get_active_field(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context,
    struct ctf_var** active_field_p)
{
    struct ctf_var* active_field = NULL;
    
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);

    struct ctf_var_tag* var_tag = var_impl_variant->u.var_tag;
    struct ctf_var* tag_variable = var + var_tag->target_index;

    context = ctf_context_get_context_for_var(context, tag_variable);
    if(context == NULL) return -1;
    
    /* Get context for the tag variable */
    struct ctf_context* tag_context = ctf_var_tag_get_context(
        var_tag, var, context);
    
    if(tag_context == NULL)
    {
        /* Tag unexist, no active field. */
        goto out;
    }
    else if(tag_context == (void*)(-1))
    {  
        /* Insufficient information for map tag. Unknown active field*/
        return -1;
    }
    
    const char* active_field_name =
        ctf_var_get_enum(tag_variable, tag_context);
    if(active_field_name != NULL)
    {
        active_field = ctf_var_find_var(var, active_field_name);
    }

    /* Do not forget to put context for tag back */
    ctf_var_tag_put_context(var_tag, var, tag_context);
out:
    if(active_field_p) *active_field_p = active_field;
    
    return 0;
}

static struct ctf_var_impl_variant_operations var_variant_ops_interpret =
{
    .base = {.get_type = var_variant_ops_get_type},
    .get_active_field = var_variant_ops_get_active_field
};

/* Variable layout operations */
/* Child existence */
static int var_variant_ops_is_child_exists(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_var* child_var,
    struct ctf_context* context)
{
    struct ctf_var* active_field;
    int result = var_variant_ops_get_active_field(var_impl, var, context,
        &active_field);
    
    if(result == -1) return -1;/* unknown */
    
    return active_field == child_var ? 1 : 0;
}

static int var_variant_ops_get_alignment(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    struct ctf_var* active_field;
    int result = var_variant_ops_get_active_field(var_impl, var, context,
        &active_field);
    
    if(result == -1) return -1;/* unknown */
    
    return ctf_var_get_alignment(active_field, context);
}

static int var_variant_ops_get_size(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    struct ctf_var* active_field;
    int result = var_variant_ops_get_active_field(var_impl, var, context,
        &active_field);
    
    if(result == -1) return -1;/* unknown */
    
    return ctf_var_get_size(active_field, context);
}

static int var_variant_ops_get_end_offset(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    struct ctf_var* active_field;
    int result = var_variant_ops_get_active_field(var_impl, var, context,
        &active_field);
    
    if(result == -1) return -1;/* unknown */
    
    return ctf_var_get_end_offset(active_field, context);
}


/* 
 * Because variant has non-constant alignment, it may have use-base
 * type of layout only when it starts on zeroth bit of the context,
 * that is absolute layout.
 * 
 */
static int var_variant_ops_get_start_offset_absolute(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    (void)var_impl;
    (void)var;
    (void)context;
    return 0;
}


static int var_variant_ops_get_start_offset_use_prev(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);
    
    int align = var_variant_ops_get_alignment(var_impl, var, context);
    if(align == -1) return -1;
    
    return generic_var_get_start_offset_use_prev(context,
        var + var_impl_variant->start_offset_data.prev_index,
        align);
}

static int var_variant_ops_get_start_offset_use_container(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);
    
    int align = var_variant_ops_get_alignment(var_impl, var, context);
    if(align == -1) return -1;
    
    return generic_var_get_start_offset_use_container(context,
        var + var_impl_variant->start_offset_data.container_index,
        align);
}

static struct ctf_var_impl_layout_operations
var_variant_ops_layout_absolute =
{
    .get_alignment      = var_variant_ops_get_alignment,
    .get_start_offset   = var_variant_ops_get_start_offset_absolute,
    .get_size           = var_variant_ops_get_size,
    .get_end_offset     = var_variant_ops_get_end_offset,
    .is_child_exist     = var_variant_ops_is_child_exists,
};


static struct ctf_var_impl_layout_operations
var_variant_ops_layout_use_prev =
{
    .get_alignment      = var_variant_ops_get_alignment,
    .get_start_offset   = var_variant_ops_get_start_offset_use_prev,
    .get_size           = var_variant_ops_get_size,
    .get_end_offset     = var_variant_ops_get_end_offset,
    .is_child_exist     = var_variant_ops_is_child_exists,
};

static struct ctf_var_impl_layout_operations
var_variant_ops_layout_use_container =
{
    .get_alignment      = var_variant_ops_get_alignment,
    .get_start_offset   = var_variant_ops_get_start_offset_use_container,
    .get_size           = var_variant_ops_get_size,
    .get_end_offset     = var_variant_ops_get_end_offset,
    .is_child_exist     = var_variant_ops_is_child_exists,
};

/* Layout operations while determine type of layout */

static int var_variant_ops_get_alignment_initial(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    (void)var_impl;
    (void)var;
    (void)context;
    return -1;
}

static struct ctf_var_impl_layout_operations
var_variant_ops_layout_initial =
{
    .get_alignment = var_variant_ops_get_alignment_initial,
};

/* Layout operations while determine layout of fields */
static int var_variant_ops_get_alignment_for_field(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);

    struct ctf_var* field = var + var_impl_variant->u.field_index;
    
    /* 
     * Return alignment of currently constructed field.
     * 
     * The thing is that layout functions should return correct value
     * only in case when variable is really exist in given context.
     * In other cases layout functions may return any value
     * (but do not crash!).
     * 
     * Result of layout functions of variant's field is meaningfull only
     * when one is interested in its value(or in value of its subfield).
     * But value may be requested only when variable is really exist
     * in context.
     * From the other side, field of variant is exists only when
     * variant exists and field is its only active one. In that case,
     * layout of variant is same as layout of the field. E.g., 
     * alignments are the same.
     * 
     */
    return ctf_var_get_alignment(field, context);
}


/* 
 * 'Normal' getters of start offset use 'normal' get_alignment().
 * Rewrite them for use fake get_alignment().
 * 
 * Also rewrite is_child_exist() callback, which normal variant use
 * tag, which is inaccessible while create fields.
 */
static int var_variant_ops_get_start_offset_absolute_for_field(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    (void)var_impl;
    (void)var;
    (void)context;
    return 0;
}


static int var_variant_ops_get_start_offset_use_prev_for_field(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);
    
    int align = var_variant_ops_get_alignment_for_field(var_impl,
        var, context);
    if(align == -1) return -1;
    
    return generic_var_get_start_offset_use_prev(context,
        var + var_impl_variant->start_offset_data.prev_index,
        align);
}

static int var_variant_ops_get_start_offset_use_container_for_field(
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_context* context)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);
    
    int align = var_variant_ops_get_alignment_for_field(var_impl,
        var, context);
    if(align == -1) return -1;
    
    return generic_var_get_start_offset_use_container(context,
        var + var_impl_variant->start_offset_data.container_index,
        align);
}

static int var_variant_ops_is_child_exists_for_field(
    struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_var* child_var,
    struct ctf_context* context)
{
    (void)var_impl;
    (void)var;
    (void)child_var;
    (void)context;
    return -1;
}

static struct ctf_var_impl_layout_operations
var_variant_ops_layout_absolute_for_field =
{
    .get_alignment      = var_variant_ops_get_alignment_for_field,
    .get_start_offset   = var_variant_ops_get_start_offset_absolute_for_field,
    .is_child_exist     = var_variant_ops_is_child_exists_for_field
};


static struct ctf_var_impl_layout_operations
var_variant_ops_layout_use_prev_for_field =
{
    .get_alignment      = var_variant_ops_get_alignment_for_field,
    .get_start_offset   = var_variant_ops_get_start_offset_use_prev_for_field,
    .is_child_exist     = var_variant_ops_is_child_exists_for_field
};

static struct ctf_var_impl_layout_operations
var_variant_ops_layout_use_container_for_field =
{
    .get_alignment      = var_variant_ops_get_alignment_for_field,
    .get_start_offset   = var_variant_ops_get_start_offset_use_container_for_field,
    .is_child_exist     = var_variant_ops_is_child_exists_for_field
};

/* Because of field resusing, need to rewrite destructor */
static void var_variant_destroy_impl_for_field(
    struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_variant* var_impl_variant =
        container_of(var_impl, typeof(*var_impl_variant), base);
    free(var_impl_variant);
}

/* Callbacks for type */
static void type_variant_destroy_impl(struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type_impl, typeof(*type_impl_variant), base);
    
    while(!linked_list_is_empty(&type_impl_variant->fields))
    {
        struct ctf_variant_field* field;
        linked_list_remove_first_entry(&type_impl_variant->fields,
            field, list_elem);

        ctf_variant_field_destroy(field);
    }
    
    if(type_impl_variant->tag)
        ctf_tag_destroy(type_impl_variant->tag);
    
    free(type_impl_variant);
    
}

static int type_variant_ops_get_max_alignment(struct ctf_type* type)
{
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type->type_impl, typeof(*type_impl_variant), base);
    
    return type_impl_variant->max_alignment;
}

static int type_variant_ops_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta)
{
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type->type_impl, typeof(*type_impl_variant), base);
    
    struct ctf_var_impl_variant* var_impl_variant =
        malloc(sizeof(*var_impl_variant));
    if(var_impl_variant == NULL)
    {
        ctf_err("Failed to allocate implementation of variant variable.");
        return -ENOMEM;
    }
    
    /* Initial operations */
    var_impl_variant->type_variant = type;
    var_impl_variant->base.interpret_ops = &var_variant_ops_interpret.base;
    var_impl_variant->base.layout_ops = &var_variant_ops_layout_initial;
    var_impl_variant->base.destroy_impl = var_variant_destroy_impl_for_field,
    
    ctf_var_set_impl(var, &var_impl_variant->base);
    
    /* 
     * Determine layout type.
     * 
     * Because size of the variant is undefined at this stage, layout
     * type may be only 'use_prev' or 'use_container' or absolute
     * (started at 0). In these cases offset parameter is not used.
     */
    struct ctf_var* result_var;
    int result_offset;
    enum layout_content_type layout = ctf_meta_get_layout_content(meta,
        var, &result_var, &result_offset);
    
    /* 
     * Fill start_offset_data according to the layout type
     * and prepare for adding fields.
     */
    switch(layout)
    {
    case layout_content_absolute:
        assert(result_offset == 0);
        var_impl_variant->base.layout_ops =
            &var_variant_ops_layout_absolute_for_field;
    break;
    case layout_content_use_prev:
        var_impl_variant->start_offset_data.prev_index =
            result_var - var;
        var_impl_variant->base.layout_ops =
            &var_variant_ops_layout_use_prev_for_field;
    break;
    case layout_content_use_container:
        var_impl_variant->start_offset_data.container_index =
            result_var - var;
        var_impl_variant->base.layout_ops =
            &var_variant_ops_layout_use_container_for_field;
    break;
    default:
        ctf_err("Unexpected layout for variant variable.");
        return -EINVAL;
    }
    
    /* Add variables corresponded to fields */
    struct ctf_variant_field* field;
    var_rel_index_t var_index = var - meta->vars;
    
    linked_list_for_each_entry(&type_impl_variant->fields, field, list_elem)
    {
        /* 
         * We need to set field variable index BEFORE adding field
         * variable.
         * 
         * One way is fake wrapper type for the variable created.
         * This type would set variable index and then call set_var_impl
         * for the real type.
         * Another way - using currently number of variables. This number
         * will be the index of the field created.
         */
        var_impl_variant->u.field_index = meta->vars_n - var_index;
        var_rel_index_t field_index = ctf_meta_add_var(meta, field->name,
            field->type, var, var, NULL);
        
        var = meta->vars + var_index; /* update var*/
        if(field_index < 0)
        {
            return field_index;
        }
        /* selfcheck */
        assert(field_index == var_impl_variant->u.field_index + var_index);
    }
    
    // TODO: create fake empty field for distinguish cases, when
    // active_field is unknown and when active_field is absent
    // (because target variable is non-exist in current context,
    // its int value has not mapped to enum, or enum value do not mapped
    // to the variant field).
    
    /* Final fields and operations for variable */

    struct ctf_var_tag* var_tag = ctf_var_tag_create(
        type_impl_variant->tag, var);
    if(var_tag == NULL)
    {
        ctf_err("Failed to create tag variable for variant");
        return -ENOMEM;
    }
    
    var_impl_variant->u.var_tag = var_tag;
    var_impl_variant->base.destroy_impl = var_variant_destroy_impl;
    switch(layout)
    {
    case layout_content_absolute:
        var_impl_variant->base.layout_ops = &var_variant_ops_layout_absolute;
    break;
    case layout_content_use_prev:
        var_impl_variant->base.layout_ops = &var_variant_ops_layout_use_prev;
    break;
    case layout_content_use_container:
        var_impl_variant->base.layout_ops = &var_variant_ops_layout_use_container;
    break;
    default:
        ctf_bug();
    }
    return 0;
}

static struct ctf_tag_component* type_variant_ops_resolve_tag_component(
    struct ctf_type* type, const char* str, const char** component_end)
{
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type->type_impl, typeof(*type_impl_variant), base);

    struct ctf_variant_field* field;
    linked_list_for_each_entry(&type_impl_variant->fields, field, list_elem)
    {
        const char* _component_end = test_tag_component(field->name, str);
        if(_component_end)
        {
            struct ctf_tag_component* tag_component =
                ctf_tag_component_create(field->name, field->type, -1);
            if(tag_component == NULL) return NULL;
            
            *component_end = _component_end;
            return tag_component;
        }
    }
    return NULL;
}

static struct ctf_type_impl* type_variant_ops_clone(
    struct ctf_type_impl* type_impl)
{
    /* Hard clone, fields are copied also */
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type_impl, typeof(*type_impl_variant), base);
    
    struct ctf_type_impl_variant* type_impl_variant_clone = malloc(
        sizeof(*type_impl_variant_clone));
    if(type_impl_variant_clone == NULL)
    {
        ctf_err("Failed to allocate cloned type implementation for variant.");
        return NULL;
    }
    
    type_impl_variant_clone->base = type_impl_variant->base;
    type_impl_variant_clone->max_alignment = type_impl_variant->max_alignment;
    
    linked_list_head_init(&type_impl_variant_clone->fields);
    
    struct ctf_variant_field* field;
    linked_list_for_each_entry(&type_impl_variant->fields, field, list_elem)
    {
        struct ctf_variant_field* field_clone = ctf_variant_field_create(
            field->name, field->type);
        if(field_clone == NULL)
        {
            ctf_err("Failed to clone structure field.");
            ctf_type_impl_destroy(&type_impl_variant_clone->base);
            return NULL;
        }
        linked_list_add_elem(&type_impl_variant_clone->fields,
            &field_clone->list_elem);
    }
    
    if(type_impl_variant->tag)
    {
        type_impl_variant_clone->tag = ctf_tag_clone(type_impl_variant->tag);
        if(type_impl_variant_clone->tag == NULL)
        {
            ctf_type_impl_destroy(&type_impl_variant_clone->base);
            return NULL;
        }
    }
    else
    {
        type_impl_variant_clone->tag = NULL;
    }
    return &type_impl_variant_clone->base;
}


struct ctf_type_impl_operations type_variant_ops =
{
    .destroy_impl           = type_variant_destroy_impl,
    .get_max_alignment      = type_variant_ops_get_max_alignment,
    .set_var_impl           = type_variant_ops_set_var_impl,
    .resolve_tag_component  = type_variant_ops_resolve_tag_component,
    .clone                  = type_variant_ops_clone,
};

/* Operations for untagged variant*/
struct ctf_type_impl_operations type_variant_ops_untagged =
{
    .destroy_impl           = type_variant_destroy_impl,
    .get_max_alignment      = type_variant_ops_get_max_alignment,

    .resolve_tag_component  = type_variant_ops_resolve_tag_component,
    .clone                  = type_variant_ops_clone,
};

/* Interpretation callbacks for variant */
static enum ctf_type_type type_variant_ops_get_type(struct ctf_type* type)
{
    (void)type;
    return ctf_type_type_variant;
}

static int type_variant_ops_set_tag(struct ctf_type* type,
    struct ctf_tag* tag)
{
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type->type_impl, typeof(*type_impl_variant), base);

    if(ctf_type_get_type(ctf_tag_get_type(tag)) != ctf_type_type_enum)
    {
        ctf_err("Only enumerations are allowed to be tags of the variant.");
        return -EINVAL;
    }
    
    if(type_impl_variant->tag != NULL)
    {
        ctf_err("Attempt to set tag for the variant, which already has tag.");
        return -EINVAL;
    }
    
    type_impl_variant->tag = tag;
    type_impl_variant->base.type_ops = &type_variant_ops;
    
    return 0;
}

static int type_variant_ops_add_field(struct ctf_type* type,
    const char* field_name, struct ctf_type* field_type)
{
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type->type_impl, typeof(*type_impl_variant), base);

    struct ctf_variant_field* field =
        ctf_variant_field_create(field_name, field_type);
    if(field == NULL) return -ENOMEM;
    
    linked_list_add_elem(&type_impl_variant->fields, &field->list_elem);

    int field_max_align = ctf_type_get_max_alignment(type);
    assert(field_max_align != -1);
    
    if(type_impl_variant->max_alignment < field_max_align)
        type_impl_variant->max_alignment = field_max_align;
    
    return 0;
}

static int type_variant_ops_has_tag(struct ctf_type* type)
{
    struct ctf_type_impl_variant* type_impl_variant =
        container_of(type->type_impl, typeof(*type_impl_variant), base);

    return type_impl_variant->tag != NULL;
}

static struct ctf_type_impl_variant_operations type_variant_ops_interpret =
{
    .base =
    {
        .get_type = type_variant_ops_get_type,
        /* 'end' callback do nothing */
    },
    .add_field  = type_variant_ops_add_field,
    .set_tag    = type_variant_ops_set_tag,
    .has_tag    = type_variant_ops_has_tag,
};

struct ctf_type_impl* ctf_type_impl_variant_create(void)
{
    struct ctf_type_impl_variant* type_impl_variant =
        malloc(sizeof(*type_impl_variant));
    if(type_impl_variant == NULL)
    {
        ctf_err("Failed to allocate variant type implementation.");
        return NULL;
    }

    linked_list_head_init(&type_impl_variant->fields);
    
    type_impl_variant->tag = NULL;
    
    type_impl_variant->max_alignment = 1;
    
    type_impl_variant->base.type_ops = &type_variant_ops_untagged;
    type_impl_variant->base.interpret_ops = &type_variant_ops_interpret.base;
    
    return &type_impl_variant->base;
}


/************************* Element contexts ***************************/

/*
 * Different contexts for array elements.
 */

/* Same callback for both variants of contexts */
static enum ctf_context_type context_array_ops_get_type(
    struct ctf_context_impl* context_impl)
{
    (void)context_impl;
    return ctf_context_type_array_elem;
}

/* 
 * Context for element with constant relative layout.
 * 
 * It used when alignment and size of all elements are same
 * in the current context of array(!).
 */
struct ctf_context_impl_array_const
{
    struct ctf_context_impl base;
    /* Common information about array and its elements */
    int n_elems;
    int elem_size;
    /*
     * Difference between two consequent elements.
     */
    int inter_size;
    
    /* Mapping of parent object, which is fully mapped. */
    const char* array_map_start;
    int array_map_start_shift;

    /* Information about current element */
    int current_index;
    
    const char* current_map_start;
    int current_map_start_shift;
};

/* Callbacks for context implementation */
static void context_array_const_ops_destroy_impl(
    struct ctf_context_impl* context_impl)
{
    struct ctf_context_impl_array_const* context_impl_array_const =
        container_of(context_impl, typeof(*context_impl_array_const), base);
    
    free(context_impl_array_const);
}

static int context_array_const_ops_extend_map(
    struct ctf_context_impl* context_impl, int new_size,
    const char** map_start_p, int* start_shift_p)
{
    struct ctf_context_impl_array_const* ca =
        container_of(context_impl, typeof(*ca), base);

    /* Only map requests are supported, others shouldn't occure */
    assert(new_size == 0);
    
    /* empty mapping for unexisting element */
    if(ca->current_index >= ca->n_elems) return 0;
    
    if(map_start_p)
    {
        *map_start_p = ca->current_map_start;
    }
    if(start_shift_p)
    {
        *start_shift_p = ca->current_map_start_shift;
    }
    
    return ca->elem_size;
}

static int context_array_const_ops_is_end(
    struct ctf_context* context)
{
    struct ctf_context_impl_array_const* ca =
        container_of(context->context_impl, typeof(*ca), base);

    return ca->current_index >= ca->n_elems;
}

static int context_array_const_ops_get_elem_index(
    struct ctf_context* context)
{
    struct ctf_context_impl_array_const* ca =
        container_of(context->context_impl, typeof(*ca), base);
    
    assert(ca->current_index < ca->n_elems);
    
    return ca->current_index;
}

static int context_array_const_ops_set_elem_index(
    struct ctf_context* context, int elem_index)
{
    struct ctf_context_impl_array_const* ca =
        container_of(context->context_impl, typeof(*ca), base);
    
    assert(ca->current_index < ca->n_elems);
    
    assert(elem_index >= 0);
    
    ca->current_index = elem_index;
    
    if(elem_index < ca->n_elems)
    {
        ca->current_map_start = ca->array_map_start +
            (ca->array_map_start_shift + elem_index * ca->inter_size) / 8;
        ca->current_map_start_shift =
            (ca->array_map_start_shift + elem_index * ca->inter_size) % 8;
    }
        
    return ctf_context_set_impl(context, &ca->base);
}

static int context_array_const_ops_set_elem_next(
    struct ctf_context* context)
{
    struct ctf_context_impl_array_const* ca =
        container_of(context->context_impl, typeof(*ca), base);
    
    assert(ca->current_index < ca->n_elems);
    
    ca->current_index++;
    
    if(ca->current_index < ca->n_elems)
    {
        ca->current_map_start +=
            (ca->current_map_start_shift + ca->inter_size) / 8;
        ca->current_map_start_shift =
            (ca->current_map_start_shift + ca->inter_size) % 8;
    }
        
    return ctf_context_set_impl(context, &ca->base);
}


static struct ctf_context_impl_map_operations
context_array_const_ops =
{
    .extend_map = context_array_const_ops_extend_map,
};
static struct ctf_context_impl_elem_operations
context_array_const_ops_elem =
{
    .base = {.get_type = context_array_ops_get_type},
    .is_end         = context_array_const_ops_is_end,
    .get_elem_index = context_array_const_ops_get_elem_index,
    .set_elem_index = context_array_const_ops_set_elem_index,
    .set_elem_next  = context_array_const_ops_set_elem_next
};

static struct ctf_context_impl_array_const*
ctf_context_impl_array_const_create(
    int n_elems, int elem_size, int elem_align,
    int array_start_offset,
    struct ctf_context* array_context)
{
    struct ctf_context_impl_array_const* ca = malloc(sizeof(*ca));
    if(ca == NULL)
    {
        ctf_err("Failed to allocate context implementation for array element.");
        return NULL;
    }
    
    ca->n_elems = n_elems;
    ca->elem_size = elem_size;
    ca->inter_size = align_val(elem_size, elem_align);
    
    const char* map_start;
    int map_start_shift;
    
    int map_full_size = array_start_offset
        + (n_elems - 1) * ca->inter_size + elem_size;
        
    int map_size = ctf_context_extend_map(array_context, map_full_size,
        &map_start, &map_start_shift);
    if(map_size < map_full_size)
    {
        ctf_err("Failed to map array.");
        free(ca);
        return NULL;
    }
    
    ca->array_map_start = map_start
        + (map_start_shift + array_start_offset) / 8;
    ca->array_map_start_shift =
        (map_start_shift + array_start_offset) % 8;
    
    ca->current_index = 0;
    ca->current_map_start = ca->array_map_start;
    ca->current_map_start_shift = ca->array_map_start_shift;
    
    ca->base.map_ops = &context_array_const_ops;
    ca->base.interpret_ops = &context_array_const_ops_elem.base;
    ca->base.destroy_impl = context_array_const_ops_destroy_impl;
    
    return ca;
}
    
/* 
 * Context for element with non-constant relative layout.
 */
struct ctf_context_impl_array
{
    struct ctf_context_impl base;
    /* Common information about array and its elements */
    int n_elems;
    /* Element alignment should be known at this stage */
    int elem_align;

    /* Pointer to the element variable for get its alignment and size */
    struct ctf_var* elem_var;
    
    /* Array is not fully mapped, so we need to store its context */
    struct ctf_context* array_context;
    /* Start offset of array is not changed*/
    int array_start_offset;

    /* Information about current element */
    int current_index;
    /* Offset relative to array context(!) */
    int current_start_offset;
    int current_context_size;
};

/* Callbacks for context implementation */
static void context_array_ops_destroy_impl(
    struct ctf_context_impl* context_impl)
{
    struct ctf_context_impl_array* context_impl_array =
        container_of(context_impl, typeof(*context_impl_array), base);
    
    free(context_impl_array);
}

static int context_array_ops_extend_map(
    struct ctf_context_impl* context_impl, int new_size,
    const char** map_start_p, int* start_shift_p)
{
    struct ctf_context_impl_array* ca =
        container_of(context_impl, typeof(*ca), base);

    /* empty mapping for unexisting element */
    if(ca->current_index >= ca->n_elems) return 0;
    
    const char* array_map_start;
    int array_map_start_shift;
    int array_map_size = ctf_context_extend_map(ca->array_context,
        new_size + ca->current_start_offset,
        &array_map_start, &array_map_start_shift);
    
    if(array_map_size < 0) return array_map_size;
    
    if(array_map_size < new_size + ca->current_start_offset)
    {
        ctf_err("Context for array element cannot be extended because "
            "context of array itself cannot be extended.");
    }
    
    if(map_start_p)
    {
        *map_start_p = array_map_start +
            (ca->current_start_offset + array_map_start_shift) / 8;
    }
    
    if(start_shift_p)
    {
        *start_shift_p = 
            (ca->current_start_offset + array_map_start_shift) % 8;
    }

    return array_map_size - ca->current_start_offset;
}

static int context_array_ops_is_end(
    struct ctf_context* context)
{
    struct ctf_context_impl_array* ca =
        container_of(context->context_impl, typeof(*ca), base);

    return ca->current_index >= ca->n_elems;
}

static int context_array_ops_get_elem_index(
    struct ctf_context* context)
{
    struct ctf_context_impl_array* ca =
        container_of(context->context_impl, typeof(*ca), base);
    
    assert(ca->current_index < ca->n_elems);
    
    return ca->current_index;
}

static int context_array_ops_set_elem_next(
    struct ctf_context* context)
{
    struct ctf_context_impl_array* ca =
        container_of(context->context_impl, typeof(*ca), base);
    
    assert(ca->current_index < ca->n_elems);
    
    if(ca->current_index == (ca->n_elems - 1))
    {
        /* Return 'end context' */
        ca->current_index++;
        /* Flush context - implementation changed */
        return ctf_context_set_impl(context, context->context_impl);
    }
    
    int elem_size = ctf_var_get_size(ca->elem_var, context);
    assert(elem_size != -1);
    /* Move to the next element */
    ca->current_index++;
    ca->current_start_offset =
        align_val(ca->current_start_offset + elem_size, ca->elem_align);
    
    /* Flush context - implementation changed */
    return ctf_context_set_impl(context, context->context_impl);
}


static int context_array_ops_set_elem_index(
    struct ctf_context* context, int elem_index)
{
    struct ctf_context_impl_array* ca =
        container_of(context->context_impl, typeof(*ca), base);
    
    assert(ca->current_index < ca->n_elems);
    
    assert(elem_index >= 0);
    
    if(elem_index >= ca->n_elems)
    {
        /* Return 'end context' */
        ca->current_index = elem_index;
        /* Flush context - implementation changed */
        return ctf_context_set_impl(context, context->context_impl);
    }
    
    /* 
     * Because array elements has different sizes(or/and alignment),
     * context doesn't support random access.
     * 
     * It should be emulated via successive accesses.
     * 
     * We have only two basic points - 0th element and current element -
     * and may only move toward.
     * 
     * So, depending on given index, we choose one basic point and 
     * move context toward by one.
     */
    
    if(elem_index == ca->current_index) return 0;
    if(elem_index < ca->current_index)
    {
        /* Use 0-th element as basic */
        ca->current_index = 0;
        ca->current_start_offset = ca->array_start_offset;
        ca->current_context_size = 0;
        /* Flush context - implementation changed */
        int result = ctf_context_set_impl(context, context->context_impl);
        if(result < 0) return result;
    }
    else
    {
        /* Use current element as basic - nothing to do */
    }
    
    int result = 0;
    while((result == 0) && (ca->current_index != elem_index))
    {
        result = context_array_ops_set_elem_next(context);
    }
    return result;
}


static struct ctf_context_impl_map_operations context_array_ops =
{
    .extend_map = context_array_ops_extend_map,
};
static struct ctf_context_impl_elem_operations
context_array_ops_elem =
{
    .base = {.get_type = context_array_ops_get_type },
    .is_end         = context_array_ops_is_end,
    .get_elem_index = context_array_ops_get_elem_index,
    .set_elem_index = context_array_ops_set_elem_index,
    .set_elem_next = context_array_ops_set_elem_next
};

static struct ctf_context_impl_array*
ctf_context_impl_array_create(
    int n_elems, int elem_align, int array_start_offset,
    struct ctf_context* array_context, struct ctf_var* elem_var)
{
    struct ctf_context_impl_array* ca = malloc(sizeof(*ca));
    if(ca == NULL)
    {
        ctf_err("Failed to allocate context implementation for array element.");
        return NULL;
    }
    
    ca->n_elems = n_elems;
    ca->elem_align = elem_align;
    
    ca->array_start_offset = array_start_offset;
    
    ca->array_context = array_context;
    ca->elem_var = elem_var;
    
    ca->current_index = 0;
    ca->current_start_offset = array_start_offset;
    
    ca->base.map_ops = &context_array_ops;
    ca->base.interpret_ops = &context_array_ops_elem.base;
    ca->base.destroy_impl = context_array_ops_destroy_impl;
    
    return ca;
}


/* 
 * Helper for set context implementation for elements.
 * 
 * Used by array and sequence variables.
 */

static int common_sequence_set_context_impl_elem(
    struct ctf_var* var_elem, struct ctf_context* context_elem,
    struct ctf_var* var_array, struct ctf_context* context_array)
{
    int n_elems = ctf_var_get_n_elems(var_array, context_array);
    if(n_elems == -1)
    {
        ctf_err("Failed to create context for element because size of "
            "sequence is undefined.");
        return -EINVAL;
    }
    
    int array_start_offset = ctf_var_get_start_offset(var_array,
        context_array);
    if(array_start_offset == -1)
    {
        ctf_err("Failed to create context for element because start of "
            "size of array mapping is undefined.");
        return -EINVAL;
    }
    
    int elem_align = ctf_var_get_alignment(var_elem, context_array);
    if(elem_align == -1)
    {
        ctf_err("Failed to create context for element because alignment "
            "of elements is undefined.");
        return -EINVAL;
    }
    
    int elem_size = ctf_var_get_size(var_elem, context_array);
    
    struct ctf_context_impl* context_impl;
    
    if(elem_size != -1)
    {
        /* Use array element context with constant alignment */
        struct ctf_context_impl_array_const* context_impl_array_const =
            ctf_context_impl_array_const_create(n_elems, elem_size,
                elem_align, array_start_offset, context_array);
        if(context_impl_array_const == NULL) return -ENOMEM;
        
        context_impl = &context_impl_array_const->base;
    }
    else
    {
        /* Use general array element context */
        struct ctf_context_impl_array* context_impl_array =
            ctf_context_impl_array_create(n_elems, elem_align,
                array_start_offset, context_array, var_elem);
        if(context_impl_array == NULL) return -ENOMEM;
        
        context_impl = &context_impl_array->base;
    }
    
    ctf_context_set_parent(context_elem, context_array);
    
    int result = ctf_context_set_impl(context_elem, context_impl);
    if(result < 0)
    {
        ctf_context_impl_destroy(context_impl);
        return result;
    }
    
    return 0;
}

/******************** Array and sequence common ***********************/
/* 
 * Calculate size of sequence or array variable.
 */
static int common_sequence_get_size(
    struct ctf_var* var_sequence,
    struct ctf_context* context,
    struct ctf_var* var_elem)
{
    int n_elems = ctf_var_get_n_elems(var_sequence, context);
    if(n_elems == -1) return -1;

    int elem_size = ctf_var_get_size(var_elem, context);
    
    if(elem_size != -1)
    {
        /* Simple case - layouts of all elements are the same and known */
        int elem_align = ctf_var_get_alignment(var_elem, context);
        if(elem_align == -1) return -1;/* strange but possible */

        return (n_elems - 1) * align_val(elem_size, elem_align)
            + elem_size;
    }
    
    /* 
     * Size of elements is not constant, so we need to iterate over
     * elements to sum their size(alignment is also taken into account).
     * 
     * Instead of explicit iteration, we create context positioned on
     * the last element (this imply implicit iteration) and request
     * end offset of this element.
     * This will be the size of the sequence.
     */
    
    /* 
     * Check that context is sufficient for map sequence.
     * 
     * Otherwise attempt for create context for variable may print
     * error, which we don't want: unknown size is a normal situation.
     */
    context = ctf_context_get_context_for_var(context, var_sequence);
    if(context == NULL) return -1;
    
    struct ctf_context* last_elem_context = 
        ctf_var_elem_create_context(var_elem, context, n_elems - 1);

    if(last_elem_context == NULL)
    {
        /* 
         * Strange error(perhaps, insufficient memory),
         * but we have nothing to do with it except return error indicator.
         */
        return -1;
    }

    int size = ctf_var_get_end_offset(var_elem, last_elem_context);
    ctf_context_destroy(last_elem_context);
    
    return size;
}

/* 
 * Helper for resolve tag of sequence/array.
 * 
 * Parse "[<number>]" construction.
 * 
 * On success return index in the sequence, requested by the tag.
 * Also set 'component_end' correspondingly.
 * 
 * On fail return -1.
 */
static int common_sequence_get_tag_index(const char* str,
    const char** component_end)
{
    if(*str != '[') return -1;
    str++;
    
    while(isspace(*str)) str++;
    
    int index;

    errno = 0;
    const char* number_ends;
    index = (int)strtoul(str, (char**)&number_ends, 10);
    if(errno != 0) return -1;
    str = number_ends;
    
    while(isspace(*str)) str++;
    
    str = test_tag_component("]", str);
    if(str == NULL) return -1;
    
    if(component_end) *component_end = str;
    
    return index;
}

/************************** CTF array *********************************/
/* Type */
struct ctf_type_impl_array
{
    struct ctf_type_impl base;
    
    int array_size;
    
    struct ctf_type* elem_type;
};

/* Variable */
struct ctf_var_impl_array
{
    /* Like a structure, array variable has fixed alignment */
    struct ctf_var_impl_fixed_align base;
    
    struct ctf_type* type_array;
    /* Relative index of element variable is always 1 */
};

static void var_array_destroy_impl(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_array* var_impl_array =
        container_of(var_impl, typeof(*var_impl_array), base.base);
    
    free(var_impl_array);
}

/* Interpret callbacks for variable implementation */
static struct ctf_type* var_array_ops_get_type(
    struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_array* var_impl_array =
        container_of(var_impl, typeof(*var_impl_array), base.base);
    
    return var_impl_array->type_array;
}

static int var_array_ops_set_context_impl_elem(
    struct ctf_context* context,
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_var* elem_var, struct ctf_context* array_context)
{
    (void)var_impl;
    return common_sequence_set_context_impl_elem(elem_var, context,
        var, array_context);
}

static int var_array_ops_get_n_elems(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    (void)var;
    (void)context;
    
    struct ctf_var_impl_array* var_impl_array =
        container_of(var_impl, typeof(*var_impl_array), base.base);

    struct ctf_type_impl_array* type_impl_array =
        container_of(var_impl_array->type_array->type_impl, typeof(*type_impl_array), base);

    return type_impl_array->array_size;
}

static struct ctf_var_impl_array_operations var_array_ops_interpret =
{
    .base = {.get_type = var_array_ops_get_type},
    .get_n_elems            = var_array_ops_get_n_elems,
    .set_context_impl_elem  = var_array_ops_set_context_impl_elem
};

/* 
 * Layout callbacks for variable implementation.
 * 
 * Only need to implement get_size() callback.
 */

static int var_array_ops_get_size(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    (void)var_impl;
    
    return common_sequence_get_size(var, context, var + 1);
}

#define var_array_ops_layout(layout_type)                   \
static struct ctf_var_impl_layout_operations                \
var_array_ops_layout_##layout_type =                        \
{                                                           \
    ctf_var_impl_fixed_assign_callbacks(layout_type),       \
    .get_size = var_array_ops_get_size,                     \
}

var_array_ops_layout(absolute);
var_array_ops_layout(use_base);
var_array_ops_layout(use_prev);
var_array_ops_layout(use_container);

#undef var_array_ops_layout

/* Callbacks for type */
static void type_array_ops_destroy_impl(struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl_array* type_impl_array =
        container_of(type_impl, typeof(*type_impl_array), base);
    
    free(type_impl_array);
}

static int type_array_ops_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta)
{
    struct ctf_type_impl_array* type_impl_array =
        container_of(type->type_impl, typeof(*type_impl_array), base);
    
    struct ctf_var_impl_array* var_impl_array =
        malloc(sizeof(*var_impl_array));
    if(var_impl_array == NULL)
    {
        ctf_err("Failed to allocate implementation for array variable.");
        return -ENOMEM;
    }
    
    var_impl_array->type_array = type;
    var_impl_array->base.align =
        ctf_type_get_max_alignment(type_impl_array->elem_type);
    var_impl_array->base.base.destroy_impl = var_array_destroy_impl;
    
    enum layout_content_type layout = ctf_var_impl_fixed_fill_layout(
        &var_impl_array->base, var, meta);

    switch(layout)
    {
#define case_array_layout_ops(layout_type)      \
case layout_content_##layout_type:              \
    var_impl_array->base.base.layout_ops =      \
        &var_array_ops_layout_##layout_type;    \
break
    
    case_array_layout_ops(absolute);
    case_array_layout_ops(use_base);
    case_array_layout_ops(use_prev);
    case_array_layout_ops(use_container);

#undef case_array_layout_ops

    default:
        ctf_err("Unexpected layout type of array.");
        assert(0);
    }
    
    var_impl_array->base.base.interpret_ops = &var_array_ops_interpret.base;
    
    ctf_var_set_impl(var, &var_impl_array->base.base);
    
    var_rel_index_t elem_index = ctf_meta_add_var(meta, "[]",
        type_impl_array->elem_type, var, NULL, NULL);
    if(elem_index < 0) return elem_index;
    
    return 0;
}

static int type_array_ops_get_max_alignment(struct ctf_type* type)
{
    struct ctf_type_impl_array* type_impl_array =
        container_of(type->type_impl, typeof(*type_impl_array), base);

    return ctf_type_get_max_alignment(type_impl_array->elem_type);
}


static struct ctf_tag_component* type_array_ops_resolve_tag_component(
    struct ctf_type* type, const char* str,
    const char** component_end)
{
    struct ctf_type_impl_array* type_impl_array =
        container_of(type->type_impl, typeof(*type_impl_array), base);

    int index = common_sequence_get_tag_index(str, component_end);
    if(index == -1) return NULL;
    
    if((index >= type_impl_array->array_size) || (index < 0))
    {
        ctf_err("Tag refers to array element with index out of range.");
        return NULL;
    }
    
    return ctf_tag_component_create("[]", type_impl_array->elem_type,
        index);
}

static struct ctf_type_impl* type_array_ops_clone(
    struct ctf_type_impl* type_impl)
{
    /* Hard copy */
    struct ctf_type_impl_array* type_impl_array =
        container_of(type_impl, typeof(*type_impl_array), base);
    
    struct ctf_type_impl_array* type_impl_array_clone = malloc(
        sizeof(*type_impl_array_clone));
    if(type_impl_array_clone == NULL)
    {
        ctf_err("Failed allocate type implementation of array clone.");
        return NULL;
    }
    
    memcpy(type_impl_array_clone, type_impl_array,
        sizeof(*type_impl_array_clone));
    
    return &type_impl_array_clone->base;
}

static struct ctf_type_impl_operations type_array_ops =
{
    .destroy_impl           = type_array_ops_destroy_impl,
    .get_max_alignment      = type_array_ops_get_max_alignment,
    .set_var_impl           = type_array_ops_set_var_impl,
    .resolve_tag_component  = type_array_ops_resolve_tag_component,
    .clone                  = type_array_ops_clone,
};

/* Interpret operations for array */
static enum ctf_type_type type_array_ops_get_type(struct ctf_type* type)
{
    (void)type;
    return ctf_type_type_array;
}

static int type_array_ops_get_n_elems(struct ctf_type* type)
{
    struct ctf_type_impl_array* type_impl_array =
        container_of(type->type_impl, typeof(*type_impl_array), base);
    
    return type_impl_array->array_size;
}

static struct ctf_type_impl_array_operations type_array_ops_interpret =
{
    .base =
    {
        .get_type = type_array_ops_get_type,
        /* 'end' callback do nothing */
    },
    .get_n_elems = type_array_ops_get_n_elems
};

struct ctf_type_impl* ctf_type_impl_array_create(int size,
    struct ctf_type* elem_type)
{
    struct ctf_type_impl_array* type_impl_array =
        malloc(sizeof(*type_impl_array));
    if(type_impl_array == NULL)
    {
        ctf_err("Failed to allocate implementation for array type.");
        return NULL;
    }
    
    type_impl_array->elem_type = elem_type;
    type_impl_array->array_size = size;
    type_impl_array->base.type_ops = &type_array_ops;
    type_impl_array->base.interpret_ops = &type_array_ops_interpret.base;
    
    return &type_impl_array->base;
}

/************************** CTF sequence ******************************/
/* Type */
struct ctf_type_impl_sequence
{
    struct ctf_type_impl base;
    
    struct ctf_tag* tag_size;
    
    struct ctf_type* elem_type;
};

/* Variable */
struct ctf_var_impl_sequence
{
    struct ctf_var_impl_fixed_align base;
    
    struct ctf_var_tag* var_tag_size;
    
    struct ctf_type* type_sequence;
    /* Relative index of element variable is always 1 */
};

static void var_sequence_destroy_impl(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_sequence* var_impl_sequence =
        container_of(var_impl, typeof(*var_impl_sequence), base.base);
    
    ctf_var_tag_destroy(var_impl_sequence->var_tag_size);
    
    free(var_impl_sequence);
}

/* Interpret callbacks for variable implementation */
static struct ctf_type* var_sequence_ops_get_type(
    struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_sequence* var_impl_sequence =
        container_of(var_impl, typeof(*var_impl_sequence), base.base);
    
    return var_impl_sequence->type_sequence;
}

static int var_sequence_ops_set_context_impl_elem(
    struct ctf_context* context,
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_var* elem_var, struct ctf_context* array_context)
{
    (void)var_impl;
    return common_sequence_set_context_impl_elem(elem_var, context,
        var, array_context);
}

static int var_sequence_ops_get_n_elems(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    int n_elems = 0;
    
    struct ctf_var_impl_sequence* var_impl_sequence =
        container_of(var_impl, typeof(*var_impl_sequence), base.base);

    struct ctf_var_tag* var_tag_size = var_impl_sequence->var_tag_size;
    struct ctf_var* var_size = var + var_tag_size->target_index;
    
    /* Get context for the size variable */
    struct ctf_context* context_size = ctf_var_tag_get_context(
        var_tag_size, var, context);
    
    if(context_size == NULL)
    {
        /* 
         * Absence of size variable or its incorrect value are interpret as
         * zero-sized sequence.
         */
        goto out;
    }
    if(context_size == (void*)(-1)) return -1;

    uint32_t var_size_val = ctf_var_get_int32(var_size, context_size);
    n_elems = (int)var_size_val;
    if(n_elems < 0) n_elems = 0;

    /* Do not forget to put context for size variable back */
    ctf_var_tag_put_context(var_tag_size, var, context_size);
out:
    return n_elems;
}

static struct ctf_var_impl_array_operations var_sequence_ops_interpret =
{
    .base = {.get_type = var_sequence_ops_get_type},
    .get_n_elems            = var_sequence_ops_get_n_elems,
    .set_context_impl_elem  = var_sequence_ops_set_context_impl_elem
};

/* 
 * Layout callbacks for variable implementation.
 * 
 * Only need to implement get_size() callback.
 */

static int var_sequence_ops_get_size(struct ctf_var_impl* var_impl,
    struct ctf_var* var, struct ctf_context* context)
{
    (void)var_impl;
    
    return common_sequence_get_size(var, context, var + 1);
}

#define var_sequence_ops_layout(layout_type)                   \
static struct ctf_var_impl_layout_operations                \
var_sequence_ops_layout_##layout_type =                        \
{                                                           \
    ctf_var_impl_fixed_assign_callbacks(layout_type),       \
    .get_size = var_sequence_ops_get_size,                     \
}

var_sequence_ops_layout(absolute);
var_sequence_ops_layout(use_base);
var_sequence_ops_layout(use_prev);
var_sequence_ops_layout(use_container);

#undef var_sequence_ops_layout

/* Callbacks for type */
static void type_sequence_ops_destroy_impl(struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl_sequence* type_impl_sequence =
        container_of(type_impl, typeof(*type_impl_sequence), base);
    
    ctf_tag_destroy(type_impl_sequence->tag_size);
    
    free(type_impl_sequence);
}

static int type_sequence_ops_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta)
{
    struct ctf_type_impl_sequence* type_impl_sequence =
        container_of(type->type_impl, typeof(*type_impl_sequence), base);
    
    struct ctf_var_tag* var_tag_size = ctf_var_tag_create(
        type_impl_sequence->tag_size, var);
    
    if(var_tag_size == NULL) return -ENOMEM;
    
    struct ctf_var_impl_sequence* var_impl_sequence =
        malloc(sizeof(*var_impl_sequence));
    if(var_impl_sequence == NULL)
    {
        ctf_err("Failed to allocate implementation for sequence variable.");
        ctf_var_tag_destroy(var_tag_size);
        return -ENOMEM;
    }
    
    var_impl_sequence->type_sequence = type;
    var_impl_sequence->var_tag_size = var_tag_size;
    var_impl_sequence->base.align =
        ctf_type_get_max_alignment(type_impl_sequence->elem_type);
    var_impl_sequence->base.base.destroy_impl = var_sequence_destroy_impl;
    
    enum layout_content_type layout = ctf_var_impl_fixed_fill_layout(
        &var_impl_sequence->base, var, meta);

    switch(layout)
    {
#define case_sequence_layout_ops(layout_type)   \
case layout_content_##layout_type:              \
    var_impl_sequence->base.base.layout_ops =   \
        &var_sequence_ops_layout_##layout_type; \
break
    
    case_sequence_layout_ops(absolute);
    case_sequence_layout_ops(use_base);
    case_sequence_layout_ops(use_prev);
    case_sequence_layout_ops(use_container);

#undef case_sequence_layout_ops

    default:
        ctf_err("Unexpected layout type of sequence.");
        assert(0);
    }
    
    var_impl_sequence->base.base.interpret_ops = &var_sequence_ops_interpret.base;
    
    ctf_var_set_impl(var, &var_impl_sequence->base.base);
    
    var_rel_index_t elem_index = ctf_meta_add_var(meta, "[]",
        type_impl_sequence->elem_type, var, NULL, NULL);
    if(elem_index < 0) return elem_index;
    
    return 0;
}

static int type_sequence_ops_get_max_alignment(struct ctf_type* type)
{
    struct ctf_type_impl_sequence* type_impl_sequence =
        container_of(type->type_impl, typeof(*type_impl_sequence), base);

    return ctf_type_get_max_alignment(type_impl_sequence->elem_type);
}


static struct ctf_tag_component* type_sequence_ops_resolve_tag_component(
    struct ctf_type* type, const char* str,
    const char** component_end)
{
    struct ctf_type_impl_sequence* type_impl_sequence =
        container_of(type->type_impl, typeof(*type_impl_sequence), base);

    int index = common_sequence_get_tag_index(str, component_end);
    if(index == -1) return NULL;
    
    return ctf_tag_component_create("[]", type_impl_sequence->elem_type,
        index);
}

static struct ctf_type_impl* type_sequence_ops_clone(
    struct ctf_type_impl* type_impl)
{
    /* Hard copy */
    struct ctf_type_impl_sequence* type_impl_sequence =
        container_of(type_impl, typeof(*type_impl_sequence), base);
    
    struct ctf_type_impl_sequence* type_impl_sequence_clone = malloc(
        sizeof(*type_impl_sequence_clone));
    if(type_impl_sequence_clone == NULL)
    {
        ctf_err("Failed allocate type implementation of sequence clone.");
        return NULL;
    }
    
    memcpy(type_impl_sequence_clone, type_impl_sequence,
        sizeof(*type_impl_sequence_clone));
    
    return &type_impl_sequence_clone->base;
}

static struct ctf_type_impl_operations type_sequence_ops =
{
    .destroy_impl           = type_sequence_ops_destroy_impl,
    .get_max_alignment      = type_sequence_ops_get_max_alignment,
    .set_var_impl           = type_sequence_ops_set_var_impl,
    .resolve_tag_component  = type_sequence_ops_resolve_tag_component,
    .clone                  = type_sequence_ops_clone,
};

/* Interpret operations for sequence */
static enum ctf_type_type type_sequence_ops_get_type(struct ctf_type* type)
{
    (void)type;
    return ctf_type_type_sequence;
}

static struct ctf_type_impl_interpret_operations type_sequence_ops_interpret =
{
    .get_type = type_sequence_ops_get_type,
    /* 'end' callback do nothing */
};

struct ctf_type_impl* ctf_type_impl_sequence_create(
    struct ctf_tag* tag_size,
    struct ctf_type* elem_type)
{
    struct ctf_type_impl_sequence* type_impl_sequence =
        malloc(sizeof(*type_impl_sequence));
    if(type_impl_sequence == NULL)
    {
        ctf_err("Failed to allocate implementation for sequence type.");
        return NULL;
    }
    
    type_impl_sequence->elem_type = elem_type;
    type_impl_sequence->tag_size = tag_size;
    type_impl_sequence->base.type_ops = &type_sequence_ops;
    type_impl_sequence->base.interpret_ops = &type_sequence_ops_interpret;
    
    return &type_impl_sequence->base;
}

/********************************* Typedef ****************************/
struct ctf_type_impl* ctf_type_impl_typedef_create(
    struct ctf_type* type)
{
    assert(type->type_impl->type_ops->clone);
    
    return type->type_impl->type_ops->clone(type->type_impl);
}


/************************Layout support *******************************/

static int get_relative_offset(struct ctf_meta* meta, struct ctf_var* var,
    struct ctf_var* base_var);

/* 
 * Return offset of 'var' relative to the start of 'base_var'.
 */
static int get_relative_offset0(struct ctf_meta* meta, 
    struct ctf_var* var, struct ctf_var* base_var)
{
    /*
     * Recursive function with linear optimization
     */
    
    /* Currently tested variable */
    struct ctf_var* current_var = var;
    /* Offset of requested variable relative to current one. */
    int intermediate_offset = 0;
    while(current_var != base_var)
    {
        int align = ctf_var_get_alignment(current_var, NULL);
        
        struct ctf_var* prev_var = ctf_var_get_prev(meta, current_var);
        
        if(prev_var == NULL)
        {
            struct ctf_var* container = ctf_var_get_container(meta, current_var);
            assert(container != NULL);
            
            int container_align = ctf_var_get_alignment(container, NULL);

            /* 
             * Currently alignment of the container always more or equal
             * to the alignment of any of its elements.
             */
            ctf_bug_on(container_align < align);
            
            current_var = container;
            continue;
        }

        int prev_align = ctf_var_get_alignment(prev_var, NULL);
        if(prev_align >= align)
        {
            int prev_size = ctf_var_get_size(prev_var, NULL);
            intermediate_offset += align_val(prev_size, align);
            current_var = prev_var;
            continue;
        }
        
        int offset = get_relative_offset0(meta, prev_var, base_var);
        int prev_size = ctf_var_get_size(prev_var, NULL);
        
        intermediate_offset += align_val(offset + prev_size, align);
        break;
    }
    
    return intermediate_offset;
}

/* Return offset of 'var' relative to the start of 'base_var' */
int get_relative_offset(struct ctf_meta* meta, struct ctf_var* var,
    struct ctf_var* base_var)
{
    return get_relative_offset0(meta, var, base_var);
}

/* Return absolute offset of 'var' */
static int get_absolute_offset(struct ctf_meta* meta, struct ctf_var* var)
{
    struct ctf_var* top_variable = ctf_var_get_context(var);
    return get_relative_offset(meta, var, top_variable);
}

/* 
 * Return layout based on container of previouse variable.
 * 
 * Or absolute, if variable has its own context.
 */
static enum layout_content_type ctf_meta_get_layout_content_nearest(
    struct ctf_meta* meta, struct ctf_var* var,
    struct ctf_var** result_var_p, int* result_offset_p)
{
    struct ctf_var* prev_var = ctf_var_get_prev(meta, var);
    if(prev_var == NULL)
    {
        struct ctf_var* container = ctf_var_get_container(meta, var);
        if(container == NULL)
        {
            /* top-level variable */
            *result_offset_p = 0;
            return layout_content_absolute;
        }
        /* container-based */
        *result_var_p = container;
        return layout_content_use_container;
    }
    /* prev-based */
    *result_var_p = prev_var;
    return layout_content_use_prev;
}

enum layout_content_type ctf_meta_get_layout_content(
    struct ctf_meta* meta, struct ctf_var* var,
    struct ctf_var** result_var_p, int* result_offset_p)
{
    int align = ctf_var_get_alignment(var, NULL);
    
    if(align == -1)
    {
        return ctf_meta_get_layout_content_nearest(meta, var,
            result_var_p, result_offset_p);
    }
    /* Currently found base var */
    struct ctf_var* current_base_var = NULL;/* not found */
    
    struct ctf_var* current_var = var;
    int max_align = align;
    while(1)
    {
        struct ctf_var* prev_var = ctf_var_get_prev(meta, current_var);
        if(prev_var == NULL)
        {
            struct ctf_var* container =
                ctf_var_get_container(meta, current_var);
            if(container == NULL)
            {
                /* Rich top-level variable - absolute layout */
                *result_offset_p = get_absolute_offset(meta, var);
                return layout_content_absolute;
            }
            int container_align = ctf_var_get_alignment(current_var, NULL);
            if(container_align == -1)
            {
                // cannot be base variable, stop search
                break;
            }
            if(container_align >= max_align)
            {
                // may be base variable
                current_base_var = container;
                max_align = container_align;
            }
            current_var = container;
            continue;
        }
        
        int prev_align = ctf_var_get_alignment(prev_var, NULL);
        if(prev_align == -1)
        {
            // cannot be base variable, stop search
            break;
        }
        int prev_size = ctf_var_get_size(prev_var, NULL);
        if(prev_size == -1)
        {
            // cannot be base variable, stop search
            break;
        }
        if(prev_align >= max_align)
        {
            //may be base variable
            current_base_var = prev_var;
            max_align = prev_align;
        }
        current_var = prev_var;
    }

    if(current_base_var)
    {
        /* Base is found */
        *result_offset_p = get_relative_offset(meta, var, current_base_var);
        return layout_content_use_base;
    }
    else
    {
        /* Base not found */
        return ctf_meta_get_layout_content_nearest(meta, var,
            result_var_p, result_offset_p);
    }
}

/**************************** Root type *******************************/
static const char* dynamic_scope_names[] =
{
    "trace.packet.header",
    "stream.packet.context",
    "stream.event.header",
    "stream.event.context",
    "event.context",
    "event.fields"
};

#define DYNAMIC_SCOPES_NUMBER \
(sizeof(dynamic_scope_names) / sizeof(dynamic_scope_names[0]))

struct ctf_var_impl_root
{
    struct ctf_var_impl base;
    
    /* 
     * Ordered(!) array of variables, corresponded to dynamic scope.
     * 
     * 0 corresponds to non-instantiated variables
     */
    var_rel_index_t dynamic_scopes[DYNAMIC_SCOPES_NUMBER];

    struct ctf_type* type_root;
};

/* Implementation for dynamic context */
struct ctf_context_impl_dynamic
{
    struct ctf_context_impl base;
    
    struct ctf_context_info* context_info;
};

static int context_dynamic_ops_extend_map(
    struct ctf_context_impl* context_impl, int new_size,
    const char** map_start_p, int* map_start_shift_p)
{
    struct ctf_context_impl_dynamic* context_impl_dynamic =
        container_of(context_impl, typeof(*context_impl_dynamic), base);
    
    struct ctf_context_info* context_info =
        context_impl_dynamic->context_info;

    return context_info->extend_map(context_info, new_size, map_start_p,
        map_start_shift_p);
}

static void context_dynamic_ops_destroy_impl(
    struct ctf_context_impl* context_impl)
{
    struct ctf_context_impl_dynamic* context_impl_dynamic =
        container_of(context_impl, typeof(*context_impl_dynamic), base);
    
    struct ctf_context_info* context_info =
        context_impl_dynamic->context_info;

    if(context_info->destroy_info)
        context_info->destroy_info(context_info);
    
    free(context_impl_dynamic);
}

static enum ctf_context_type context_dynamic_ops_get_type(
    struct ctf_context_impl* context_impl)
{
    (void) context_impl;
    return ctf_context_type_top;
}

static struct ctf_context_impl_map_operations context_dynamic_ops =
{
    .extend_map = context_dynamic_ops_extend_map,
};

static struct ctf_context_impl_top_operations context_dynamic_ops_top =
{
    .base = {.get_type = context_dynamic_ops_get_type}
};

/* Callbacks for root variable */
static int var_root_ops_set_context_impl(struct ctf_context* context,
    struct ctf_var_impl* var_impl, struct ctf_var* var,
    struct ctf_var* child_var, struct ctf_context* base_context,
    struct ctf_context_info* context_info)
{
    struct ctf_context_impl_dynamic* context_impl_dynamic;
    struct ctf_var_impl_root* var_impl_root =
        container_of(var_impl, typeof(*var_impl_root), base);
    
    struct ctf_var* prev_child_var;
    {
        /* Determine index of child variable */
        int i;
        for(i = DYNAMIC_SCOPES_NUMBER - 1; i >= 0; i--)
        {
            if(var + var_impl_root->dynamic_scopes[i] == child_var) break;
        }
        assert(i >= 0);
        /* Determine previouse (existed) child */
        for(--i; i>= 0; i--)
        {
            if(var_impl_root->dynamic_scopes[i]) break;
        }
        
        if(i >= 0) prev_child_var = var + var_impl_root->dynamic_scopes[i];
        else prev_child_var = NULL;
    }
    
    if(prev_child_var)
    {
        /* Adjust parent context if needed */ 
        base_context = ctf_context_get_context_for_var(base_context,
            prev_child_var);
        
        if(base_context == NULL)
        {
            ctf_err("Insufficient context for create new one.");
            return -EINVAL;
        }
    }
    else
    {
        /* First top-level variable, base context is needn't */
        base_context = NULL;
    }
    
    context_impl_dynamic = malloc(sizeof(*context_impl_dynamic));
    if(context_impl_dynamic == NULL)
    {
        ctf_err("Failed to allocate context implementation structure.");
        return -ENOMEM;
    }
    context_impl_dynamic->context_info = context_info;
    context_impl_dynamic->base.map_ops = &context_dynamic_ops;
    context_impl_dynamic->base.interpret_ops = &context_dynamic_ops_top.base;
    context_impl_dynamic->base.destroy_impl = context_dynamic_ops_destroy_impl;
    
    ctf_context_set_parent(context, base_context);
    
    int result = ctf_context_set_impl(context,
        &context_impl_dynamic->base);
    
    if(result < 0)
    {
        free(context_impl_dynamic);
        return result;
    }
    
    return 0;
}

static struct ctf_type* var_root_ops_get_type(
    struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_root* var_impl_root =
        container_of(var_impl, typeof(*var_impl_root), base);
    return var_impl_root->type_root;
}

static struct ctf_var_impl_root_operations var_root_ops =
{
    .base = {.get_type = var_root_ops_get_type},
    .set_context_impl = var_root_ops_set_context_impl
};

/* 
 * Root variable is not a continious, so it has not align and size.
 * All its childs are always exists.
 */
static struct ctf_var_impl_layout_operations var_root_ops_layout;

static void var_root_ops_destroy_impl(struct ctf_var_impl* var_impl)
{
    struct ctf_var_impl_root* var_impl_root =
        container_of(var_impl, typeof(*var_impl_root), base);
    
    free(var_impl_root);
}

/* Root type */
struct ctf_type_impl_root
{
    struct ctf_type_impl base;
    
    struct ctf_type* dynamic_scope_types[DYNAMIC_SCOPES_NUMBER];
};

static enum ctf_type_type type_root_ops_get_type(struct ctf_type* type)
{
    (void)type;
    return ctf_type_type_root;
}

static struct ctf_tag_component* type_root_ops_resolve_tag_component(
    struct ctf_type* type,
    const char* str,
    const char** component_end)
{
    struct ctf_type_impl_root* type_impl_root = 
        container_of(type->type_impl, typeof(*type_impl_root), base);
    
    int i;
    for(i = 0; i < (int)DYNAMIC_SCOPES_NUMBER; i++)
    {
        if(type_impl_root->dynamic_scope_types[i] == NULL) continue;

        const char* name_end = test_tag_component(dynamic_scope_names[i],
            str);
        
        if(name_end)
        {
            /* Found dynamic scope */
            *component_end = name_end;
            return ctf_tag_component_create(dynamic_scope_names[i],
                type_impl_root->dynamic_scope_types[i],
                -1);
        }
    }
    return NULL;
}

static int type_root_ops_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta)
{
    struct ctf_type_impl_root* type_impl_root = 
        container_of(type->type_impl, typeof(*type_impl_root), base);
    
    struct ctf_var_impl_root* var_impl_root =
        malloc(sizeof(*var_impl_root));
    if(var_impl_root == NULL)
    {
        ctf_err("Failed to allocate structure for root variable.");
        return -ENOMEM;
    }
    var_impl_root->base.destroy_impl = var_root_ops_destroy_impl;
    var_impl_root->base.interpret_ops = &var_root_ops.base;
    var_impl_root->base.layout_ops = &var_root_ops_layout;
    var_impl_root->type_root = type;
    ctf_var_set_impl(var, &var_impl_root->base);
    
    /* Really, root index is always 0, but nevetheless */
    var_rel_index_t root_index = var - meta->vars;
    int i;
    for(i = 0; i < (int)DYNAMIC_SCOPES_NUMBER; i++)
    {
        if(type_impl_root->dynamic_scope_types[i] == NULL)
        {
            var_impl_root->dynamic_scopes[i] = 0;
            continue;
        }
        
        var_rel_index_t dynamic_scope = ctf_meta_add_var(meta,
            dynamic_scope_names[i],
            type_impl_root->dynamic_scope_types[i],
            var, NULL, NULL);
        /* Update root var */
        var = ctf_meta_get_var(meta, root_index);
        if(dynamic_scope < 0)
        {
            return dynamic_scope;
        }
        var_impl_root->dynamic_scopes[i] = dynamic_scope - root_index;
    }
    
    return 0;
}

static void type_root_ops_destroy_impl(struct ctf_type_impl* type_impl)
{
    struct ctf_type_impl_root* type_impl_root = 
        container_of(type_impl, typeof(*type_impl_root), base);
    
    free(type_impl_root);
}

static int type_root_ops_assign_type(struct ctf_type* type,
    const char* assign_position_abs, struct ctf_type* assigned_type)
{
    struct ctf_type_impl_root* type_impl_root = 
        container_of(type->type_impl, typeof(*type_impl_root), base);
    
    int index;
    for(index = 0; index < (int)DYNAMIC_SCOPES_NUMBER; index++)
    {
        if(strcmp(assign_position_abs, dynamic_scope_names[index]) == 0)
            break;
    }
    if(index == (int)DYNAMIC_SCOPES_NUMBER)
    {
        ctf_err("Unknown dynamic scope for assign: '%s'.",
            assign_position_abs);
        return -EINVAL;
    }
    
    type_impl_root->dynamic_scope_types[index] = assigned_type;
    
    return 0;
}

static struct ctf_type_impl_operations type_root_ops =
{
    .destroy_impl = type_root_ops_destroy_impl,
    .set_var_impl = type_root_ops_set_var_impl,
    .resolve_tag_component = type_root_ops_resolve_tag_component
    /* Other callbacks are not required for root type */
};

static struct ctf_type_impl_root_operations type_root_ops_interpret =
{
    .base =
    {
        .get_type = type_root_ops_get_type,
    },
    .assign_type =  type_root_ops_assign_type
};

struct ctf_type_impl* ctf_type_impl_create_root(void)
{
    struct ctf_type_impl_root* type_impl_root =
        malloc(sizeof(*type_impl_root));
    if(type_impl_root == NULL)
    {
        ctf_err("Failed to allocate implementation for root type.");
        return NULL;
    }
    memset(type_impl_root->dynamic_scope_types, 0,
        sizeof(type_impl_root->dynamic_scope_types));
    
    type_impl_root->base.type_ops = &type_root_ops;
    type_impl_root->base.interpret_ops = &type_root_ops_interpret.base;
    
    return &type_impl_root->base;
}
