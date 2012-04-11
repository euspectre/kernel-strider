/*
 * CTF type. //TODO: more desctiption.
 */

#ifndef CTF_TYPES_H
#define CTF_TYPES_H

#include "ctf_meta.h"
#include "ctf_meta_internal.h"

#include "linked_list.h"

/* 
 * Meta types of CTF types.
 * 
 * These meta types determine operation set, which is available for
 * variables or types.
 */
enum ctf_type_type
{
    /* 
     * Absence of meta type. Usually these are types which doesn't have
     * implementation - just created or just before deleting on error.
     */
    ctf_type_type_none = 0,
    
    /* 
     * This type is used for root variable.
     * 
     * Note, that root variable cannot be searching by name.
     */
    
    ctf_type_type_root,
    
    ctf_type_type_int,
    ctf_type_type_struct,
    ctf_type_type_enum,
    ctf_type_type_variant,
    ctf_type_type_array,
    ctf_type_type_sequence,
    ctf_type_type_string,
};


struct ctf_type_impl_operations;
struct ctf_type_impl_interpret_operations;
struct ctf_type_impl
{
    const struct ctf_type_impl_operations* type_ops;
    const struct ctf_type_impl_interpret_operations* interpret_ops;
};

/* 
 * CTF type.
 * 
 * All specializations are implemented via its 'type_impl' field.
 */
struct ctf_type
{
    /*
     * Scope of type definition.
     */
    struct ctf_scope* scope;

    /* List of types inside scope */
    struct linked_list_elem list_elem;

    /* Name of the type. */
    char* name;
    
    struct ctf_type_impl* type_impl;
};

/* 
 * Container for types.
 * 
 * Used for scopes supported types addition.
 */
struct ctf_type_container
{
    /* List organization of types, defined in this scope */
    struct linked_list_head types;
};

/* Initialize instance of type container */
void ctf_type_container_init(struct ctf_type_container* type_container);
/* Add type to the container. Container will respond for type lifetime. */
void ctf_type_container_add_type(
    struct ctf_type_container* type_container, struct ctf_type* type);
/* Remove type from the container. Usually removed type is last added. */
void ctf_type_container_remove_type(
    struct ctf_type_container* type_container, struct ctf_type* type);
/* Find type in the container using type name */
struct ctf_type* ctf_type_container_find_type(
    struct ctf_type_container* type_container,
    const char* type_name);
/* Destroy container and all types it contains. */
void ctf_type_container_destroy(
    struct ctf_type_container* type_container);

/* 
 * 'Virtual' operations for the type implementation.
 * 
 * Implementation is taken from type->type_impl.
 * Also, other fields of type may be accessed.
 */
struct ctf_type_impl_operations
{
    /* destructor */
    void (*destroy_impl)(struct ctf_type_impl* type_impl);
    
    /* 
     * Return maximum alignment of the type.
     * 
     * Needed for fields inside compound types.
     */
    int (*get_max_alignment)(struct ctf_type* type);

    /*
     * Set implementation for variable of this type.
     * 
     * Return 0 on success, negative error code otherwise.
     * 
     * Compound types may add subvariables on this stage.
     */
    int (*set_var_impl)(struct ctf_type* type, struct ctf_var* var,
        struct ctf_meta* meta);
    
    /*
     * Create tag component according to the string.
     *
     * Set 'component_end' to the first symbol after component name
     * in the tag.
     * 
     * If tag cannot be resolved for this type (e.g, no field with
     * given name in the structure) return NULL.
     * 
     * NULL callback is treated as always returned NULL.
     */
    struct ctf_tag_component* (*resolve_tag_component)(
        struct ctf_type* type, const char* str,
        const char** component_end);

    /*
     * Create clone of the type.
     * 
     * Used when type is typedef'ed.
     * 
     * Clone may be 'hard' (copy all fields of type implementation, copy
     * operations), but also may be 'soft' (store reference to the type,
     * operations wraps).
     */
    
    struct ctf_type_impl* (*clone)(struct ctf_type_impl* type_impl);
};

/* Wrappers for common type operations */
static inline void ctf_type_impl_destroy(struct ctf_type_impl* type_impl)
{
    type_impl->type_ops->destroy_impl(type_impl);
}

static inline int ctf_type_get_max_alignment(struct ctf_type* type)
{
    return type->type_impl->type_ops->get_max_alignment(type);
}

int ctf_type_set_var_impl(struct ctf_type* type,
    struct ctf_var* var, struct ctf_meta* meta);

static inline struct ctf_tag_component* ctf_type_resolve_tag_component(
    struct ctf_type* type, const char* str, const char** component_end)
{
    return type->type_impl->type_ops->resolve_tag_component
        ? type->type_impl->type_ops->resolve_tag_component(
            type, str, component_end)
        : NULL;
}


/* 
 * 'Virtual' operations for the type implementation, different for
 * differrend kind of types.
 * 
 * Implementation is taken from type->type_impl.
 * Also, other fields of type may be accessed.
 */
struct ctf_type_impl_interpret_operations
{
    /* RTTI */
    enum ctf_type_type (*get_type)(struct ctf_type* type);
    /* 
     * Finalize type definition, performing checking if needed.
     * 
     * Return 0 on success and negative error on fail.
     * NULL callback is interpreted as always returned 0.
     */
    int (*end_type)(struct ctf_type* type);
};

enum ctf_type_type ctf_type_get_type(struct ctf_type* type);

int ctf_type_end_type(struct ctf_type* type);

/* Operations for root type */
struct ctf_type_impl_root_operations
{
    struct ctf_type_impl_interpret_operations base;
    
    /* Assign type to the given (absolute) assign position */
    int (*assign_type)(struct ctf_type* type,
        const char* assign_position_abs, struct ctf_type* assigned_type);
};

/* Assign type to the given (absolute) assign position */
int ctf_type_root_assign_type(struct ctf_type* type_root,
    const char* assign_position_abs, struct ctf_type* assigned_type);

/* Operations for integer types */
struct ctf_type_impl_int_operations
{
    struct ctf_type_impl_interpret_operations base;
    
    /* Set different parameters for the type */
    int (*set_size)(struct ctf_type* type, int size);
    int (*set_align)(struct ctf_type* type, int align);
    int (*set_signed)(struct ctf_type* type, int is_signed);
    int (*set_byte_order)(struct ctf_type* type,
        enum ctf_int_byte_order byte_order);
    int (*set_encoding)(struct ctf_type* type,
        enum ctf_int_encoding encoding);
    int (*set_base)(struct ctf_type* type,
        enum ctf_int_base base);
    
    /* Get parameters for the type(may be called only after construction) */
    int (*get_size)(struct ctf_type* type);
    int (*get_align)(struct ctf_type* type);
    int (*is_signed)(struct ctf_type* type);
    enum ctf_int_byte_order (*get_byte_order)(struct ctf_type* type);
    enum ctf_int_encoding (*get_encoding)(struct ctf_type* type);
    enum ctf_int_base (*get_base)(struct ctf_type* type);
};

/* Set corresponded parameter for the integer */
static inline int ctf_type_int_set_signed(struct ctf_type* type,
    int is_signed)
{
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_signed(type, is_signed);
}
static inline int ctf_type_int_set_size(struct ctf_type* type, int size)
{
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_size(type, size);
}
static inline int ctf_type_int_set_align(struct ctf_type* type, int align)
{
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_align(type, align);
}
static inline int ctf_type_int_set_byte_order(struct ctf_type* type,
    enum ctf_int_byte_order byte_order)
{
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_byte_order(type, byte_order);
}
static inline int ctf_type_int_set_base(struct ctf_type* type,
    enum ctf_int_base base)
{
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_base(type, base);
}
static inline int ctf_type_int_set_encoding(struct ctf_type* type,
    enum ctf_int_encoding encoding)
{   
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_encoding(type, encoding);
}

/* Operations for structure types */
struct ctf_type_impl_struct_operations
{
    struct ctf_type_impl_interpret_operations base;
    
    /* Add field to the structure */
    int (*add_field)(struct ctf_type* type, const char* field_name,
        struct ctf_type* field_type);
    //TODO: add_bitfield
};


/* Operations for enumeration types */
struct ctf_type_impl_enum_operations
{
    struct ctf_type_impl_interpret_operations base;
    
    /* Add value for map */
    int (*add_value32)(struct ctf_type* type, const char* value,
        int32_t start, int32_t end);
    //TODO: add_value64
};

struct ctf_tag;

/* Operations for variant types */
struct ctf_type_impl_variant_operations
{
    struct ctf_type_impl_interpret_operations base;
    
    /* Add field to the variant */
    int (*add_field)(struct ctf_type* type, const char* field_name,
        struct ctf_type* field_type);
    
    /* Set tag for variant, if it has no one */
    int (*set_tag)(struct ctf_type* type, struct ctf_tag* tag);
    
    /* Test, whether variant has tag */
    int (*has_tag)(struct ctf_type* type);
};

int ctf_type_variant_has_tag(struct ctf_type* type);
int ctf_type_variant_set_tag(struct ctf_type* type, struct ctf_tag* tag);

/* Operations for array types */
struct ctf_type_impl_array_operations
{
    struct ctf_type_impl_interpret_operations base;
    
    /* Return number of elements in the array */
    int (*get_n_elems)(struct ctf_type* type);
};

//TODO: string operations, sequence operations

/* Create type without implementation */
struct ctf_type* ctf_type_create(const char* name);

/* 
 * Set implementation for the type. 'NULL' may be passed for clear
 * implementation.
 * 
 * Return previouse implementation.
 * 
 * Type own its implementation, and is response for its lifetime.
 * 
 * When ctf_type_set_impl() is called, type become owner of new
 * implementation, and caller become owner of old implementation.
 */
struct ctf_type_impl* ctf_type_set_impl(struct ctf_type* type,
    struct ctf_type_impl* type_impl);

/* Destroy type and its implementation, if it was set before */
void ctf_type_destroy(struct ctf_type* type);

/************************** Integer type ***************************/
/* 
 * Create instance of integer type implementation.
 * 
 * Parameters may be set later.
 */
struct ctf_type_impl* ctf_type_impl_int_create(void);
/************************ CTF structure *******************************/
/* 
 * Create structure type implementation without fields.
 * 
 * Fields may be added later.
 */
struct ctf_type_impl* ctf_type_impl_struct_create(void);
    
/************************** CTF enum *********************************/
/* Create enum type according to integer type */
struct ctf_type_impl* ctf_type_impl_enum_create(
    struct ctf_type* type_int);

/*************************** CTF variant ******************************/
/* 
 * Create untagged variant with no fields.
 * 
 * Tag may be set later, field may be added later.
 */
struct ctf_type_impl* ctf_type_impl_variant_create(void);

/************************** CTF array *********************************/
/* 
 * Create type implementation for array of given size and
 * given types of elements.
 * 
 * Note, that type is already final - no additional modifications
 * are allowed.
 */
struct ctf_type_impl* ctf_type_impl_array_create(int size,
    struct ctf_type* elem_type);

/********************** CTF sequence **********************************/
/* 
 * Create type implementation for sequence with size contained in given
 * tag and given types of elements.
 * 
 * Note, that type is already final - no additional modifications
 * are allowed.
 */
struct ctf_type_impl* ctf_type_impl_sequence_create(
    struct ctf_tag* tag_size, struct ctf_type* elem_type);

/**************************** Typedef *********************************/
/*
 * Create type implementation which wraps existing type.
 */
struct ctf_type_impl* ctf_type_impl_typedef_create(
    struct ctf_type* type);

/*********************** Layout support *******************************/
/* Helpers for implement layout callbacks for variables. */

/* 
 * Return (minimum) number which is greater or equal to val
 * and satisfy to alignment.
 */
static inline int align_val(int val, int align)
{
    int mask = align - 1;
    return (val + mask) & ~mask;
}

/* 
 * Return start offset of the variable using start offset of some
 * base variable('var_base') and offset of the given variable
 * relative to the start offset of base('relative_offset').
 */

static inline int generic_var_get_start_offset_use_base(
    struct ctf_context* context, struct ctf_var* var_base,
    int relative_offset)
{
    int base_start_offset = ctf_var_get_start_offset(var_base, context);
    
    if(base_start_offset == -1) return -1;
    return base_start_offset + relative_offset;
}

/* 
 * Return start offset of the variable using aligned end offset
 * of previous variable.
 * 
 * This variant is used instead of 'use_base', when:
 *  - previous variable has a non-constant size, or
 *  - previous variable has a non-constant aligment, or
 *  - cannot find base variable which has suitable alignment
 *      (not less than one of any of intermediate variables,
 *      including current).
 */

static inline int generic_var_get_start_offset_use_prev(
    struct ctf_context* context, struct ctf_var* var_prev, int align)
{
    int prev_end_offset = ctf_var_get_end_offset(var_prev, context);
    
    if(prev_end_offset == -1) return -1;
    return align_val(prev_end_offset, align);
}

/* 
 * Return start offset of the variable using aligned end offset
 * of previous variable.
 * 
 * This variant is used instead of 'use_base', when:
 *  - variable is first in the container and
 *  - cannot find base variable which has suitable alignment
 *      (not less than one of any of intermediate variables,
 *      including current).
 * 
 * It seems, that only fields of variant may require such
 * mechanizm of calculating start offset.
 */
static inline int generic_var_get_start_offset_use_container(
    struct ctf_context* context, struct ctf_var* var_container, int align)
{
    int container_start_offset = ctf_var_get_start_offset(var_container,
        context);
    
    if(container_start_offset == -1) return -1;
    return align_val(container_start_offset, align);
}

/* Used as a result of the next function */
enum layout_content_type
{
    layout_content_error = 0, /* for error reporting */
    layout_content_absolute,
    layout_content_use_base,
    layout_content_use_prev,
    layout_content_use_container,
};

/* 
 * Determine way for calculate variable layout and return parameters
 * for that calculation.
 * 
 * Before function call, implementation of the variable should be set
 * with correct get_alignment() callback(other layout callbacks
 * and other type of callbacks may be not set at this stage).
 * 
 * Content of the variable 'result_var_p' and offset 'result_offset_p'
 * depends on returning value:
 *  (absolute)      - not defined and absolute offset,
 *  (use_base)      - base variable and relative offset,
 *  (use_prev)      - previous variable and not defined,
 *  (use_container) - container variable and not defined.
 * 
 * On error return layout_content_error.
 * 
 * NOTE: Function is implemented as used get_alignment() callback
 * instead of using additional parameter with align because
 * child variable may request alignment of the container while
 * determine its layout type.
 * 'variant' variable has callback, which return alignment of the child
 * variable.
 */
enum layout_content_type ctf_meta_get_layout_content(
    struct ctf_meta* meta, struct ctf_var* var,
    struct ctf_var** result_var_p, int* result_offset_p);

/**************************** Root type********************************/
/* 
 * Create root type.
 * 
 * TODO: different layouts of top variables may be available
 * (e.g., all require different context and
 * packet and events variables require different context).
 */
struct ctf_type_impl* ctf_type_impl_create_root(void);

#endif /* CTF_TYPES_H */
