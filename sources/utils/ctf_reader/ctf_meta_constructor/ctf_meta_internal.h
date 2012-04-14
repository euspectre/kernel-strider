/*
 * Internal representation of main CTF objects.
 */

#ifndef CTF_META_INTERNAL_H
#define CTF_META_INTERNAL_H

#include <stdio.h>
#include "ctf_meta.h"

#include <stddef.h> /* offsetof macro */

#define ctf_err(format, ...) fprintf(stderr, "<CTF> "format"\n", ##__VA_ARGS__)

/*
 * Check that condition is true. Otherwise trigger bug in the
 * implementation.
 * 
 * assert(cond) is used for similar situations, but for check
 * pre conditions.
 */
#define ctf_bug_on(cond) do {if((cond)) {assert(0);} } while(0)

/* 
 * Mark current situation as bug in the implementation.
 * 
 * TODO: assert(0) is not good code. Rework it.
 */
#define ctf_bug() do {assert(0);} while(0)

/* Usefull macro for type convertion */
#define container_of(ptr, type, member) ({                      \
         const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
         (type *)( (char *)__mptr - offsetof(type,member) );})


struct ctf_type;
struct ctf_var;

struct ctf_global_var;

/* Type used for create relative reference to the variable. */
typedef int var_rel_index_t;


/* 
 * Information about variable layout.
 * 
 * Used while create layout callbacks for variables.
 */
struct ctf_var_layout_info
{
    /*
     * Nearest container of the variable.
     * 
     * If variable is first in the container, its start offset
     * is same as start offset of the container
     * (alignment of the variable should also be taken into account,
     * but it seems that alignment of any container is always greater or
     * equal to variable's one).
     * 
     * If variable is top-level for some CTF context, field is 0.
     */
    var_rel_index_t container_index;

    /*
     * Previous element with same container.
     * 
     * If element is first in container or it is top-level variable,
     * field is 0.
     */
    var_rel_index_t prev_index;
};


struct ctf_meta_build_info;

/* Meta information about CTF trace */
struct ctf_meta
{
    /* 
     * Array of allocated variables.
     * 
     * Array organization need for mantain correspondence between
     * variables and additional layout information for them.
     * 
     * Array is dynamic, and reallocation may change place where
     * variables are placed.
     * So, there is only one way to refer to some variable - its index.
     * When stored link between one variable and another, relative index
     * is used.
     * In the latter case, special NULL reference is modelled by 
     * indices, which may not occure for link in particular case:
     * 0, -1, 1 or others.
     */
    struct ctf_var* vars;
    int vars_n;
    /* Additional information for build stage */
    struct ctf_meta_build_info* build_info;
    
    /* 
     * Root variable - first variable in the array.
     * 
     * That variable is virtual - it cannot be searched by name,
     * it is not set as container for other variables.
     * 
     * But it set as a parent of top-level dynamic variables, such as 
     * "trace.packet.header", "stream.packet.context" etc.
     * 
     * Implementation of this variable define how to create context for
     * such top-level dynamic variables.
     */
    
    /*
     * Preallocated root type.
     * 
     * This type is virtual and set for 'root_var'.
     */
    struct ctf_type* root_type;
    
    /* Root scope */
    struct ctf_scope* root_scope;
};

struct ctf_meta_build_info
{
    /* Array of layout info, same size as array of vars in meta */
    struct ctf_var_layout_info* layout_info;
    /* Current scope */
    struct ctf_scope* current_scope;
    /* Currently constructed type */
    struct ctf_type* current_type;
};

#define ctf_meta_get_var(meta, var_index) ((meta)->vars + (var_index))

struct ctf_context_info;

struct ctf_context_impl;

enum ctf_context_type
{
    /* Context for top variable */
    ctf_context_type_top,
    /* Context for element of array or sequence */
    ctf_context_type_array_elem
};
/* 
 * Context which define mapping of CTF variables into memory.
 * 
 * Normally created in responce to the user request.
 */
struct ctf_context
{
    struct ctf_meta* meta;
    /* 
     * CTF variable which is mapped to the memory region, defined
     * by this context.
     * 
     * NOTE: This variable may contain sub-variables, so them will
     * also be mapped.
     */
    struct ctf_var* variable;
    
    /*
     * Parent context for this.
     * 
     * Each context has linear hierarchy of other contexts.
     * 
     * It is usefull for read variables when context is
     * 'more than needed'. It may be needed when read tag value of
     * variant or sequence.
     * 
     * Context is assumed sufficient for variable if it 
     * or one of its parent contexts corresponds to variable's strictly:
     * (context->variable == var->context_index + var).
     */
    
    struct ctf_context* parent_context;
    
    /* 
     * Cached context parameters, may be accessed directly by
     * variables 'getters' callbacks.
     * 
     * Shouldn't be accessed by context implementation callbacks.
     */
    int map_size;/* size in bits */
    const char* map_start;
    int map_start_shift;
    
    struct ctf_context_impl* context_impl;
};

struct ctf_context_impl_map_operations;
struct ctf_context_impl_interpret_operations;

/* Implementation of the context */
struct ctf_context_impl
{
    const struct ctf_context_impl_map_operations* map_ops;
    const struct ctf_context_impl_interpret_operations* interpret_ops;
    void (*destroy_impl)(struct ctf_context_impl* context_impl);
};

/* Virtual operations for context implementation, concerning mapping */
struct ctf_context_impl_map_operations
{
    /* 
     * Extend map to the given size.
     * 
     * Return size, to which map was really extended(>=new_size),
     * or negative error code.
     * 
     * When called with new_size = 0, return current mapping.
     */
    int (*extend_map)(struct ctf_context_impl* context_impl,
        int new_size, const char** map_start_p, int* start_shift_p);
};

/* Virtual operations for context implementation, concerning mapping */
struct ctf_context_impl_interpret_operations
{
    /* RTTI */
    enum ctf_context_type (*get_type)(struct ctf_context_impl* context_impl);
};

/* Specialization for contexts for top-level variables */
struct ctf_context_impl_top_operations
{
    struct ctf_context_impl_interpret_operations base;
    
    //TODO: move context
};

/* Specialization for contexts for array(sequence) elements */
struct ctf_context_impl_elem_operations
{
    struct ctf_context_impl_interpret_operations base;

    /* 
     * Return non-zero if context points to the element after
     * last one. For such context next callbacks shouldn't be called.
     */
    int (*is_end)(struct ctf_context* context);

    /* Return index of current element */
    int (*get_elem_index)(struct ctf_context* context);
    /* 
     * Adjust context to the element with given index.
     * 
     * Return 0 on success or negative error code.
     * 
     * NOTE: if 'index' is out of range(but positive),
     * context becomes 'end context'.
     */
    int (*set_elem_index)(struct ctf_context* context, int index);
    /*
     * Move context to the next element.
     * 
     * Return 0 on success or negative error code.
     * 
     * NOTE: If context was positioned to the last element,
     * it becomes 'end context'.
     */
    int (*set_elem_next)(struct ctf_context* context);
};

static inline void ctf_context_impl_destroy(
    struct ctf_context_impl* context_impl)
{
    if(context_impl->destroy_impl)
        context_impl->destroy_impl(context_impl);
}

/* 
 * Set or update implementation for context.
 * 
 * Context simply request current mapping from implementation and cache it.
 */
int ctf_context_set_impl(struct ctf_context* context,
    struct ctf_context_impl* context_impl);

/* 
 * Set parent context for given context.
 */
void ctf_context_set_parent(struct ctf_context* context,
    struct ctf_context* parent_context);

/* 
 * Extend mapping of the context.
 * 
 * Caching is processes correctly(if new_size is not exceed current one,
 * cached values simply returns, otherwise extend_map() callback
 * of context implementation is called and cached values are updated)
 */
int ctf_context_extend_map(struct ctf_context* context,
    int new_size, const char** map_start_p, int* start_shift_p);

struct ctf_context* ctf_context_get_context_for_var(
    struct ctf_context* context, struct ctf_var* var);

/************************** CTF variable ******************************/

/* 
 * Type-specific implementation of CTF variable.
 */
struct ctf_var_impl_layout_operations;
struct ctf_var_impl_interpret_operations;
struct ctf_var_impl
{
    /* 
     * Different pointers to operations with different operational
     * area.
     * This allows to change operations for one area and do not
     * reinitialize other operations.
     */
    const struct ctf_var_impl_layout_operations* layout_ops;
    const struct ctf_var_impl_interpret_operations* interpret_ops;
    /* Destructor (also may free object)*/
    void (*destroy_impl)(struct ctf_var_impl* var_impl);
};

/* 
 * CTF Variable.
 * 
 * Unit on the constructed CTF metadata.
 * 
 * Have a type and corresponds to:
 *  -instantiated top-level type (simple or compound)
 *  -instantiated field of the instansiated type
 */

struct ctf_var
{
    /* 
     * Tree hierarchy for search variables.
     * 
     * Note, that search hierarchy differ from layout one:
     * fields of variant are ordered here.
     * 
     * 0 value for the index means ubsent of the corresponded link.
     */
    var_rel_index_t parent_index;

    var_rel_index_t first_child_index;
    var_rel_index_t last_child_index;
    
    var_rel_index_t next_sibling_index;
    
    /* 
     * Name of the variable (relative to parent).
     * 
     * If parent is NULL, full name of the variable.
     * 
     * NULL name means that variable isn't accessible by name
     * (e.g., auxiliary variable for layout).
     * 
     * "[]" name is also special - floating element in the array.
     */
    char* name;

    /* 
     * Top variable for current context.
     * 
     * Context, corresponded to that variable, contains memory region
     * to which this variable is mapped.
     * 
     * For top-level variable contains 0.
     */
    var_rel_index_t context_index;
    
    /* Hash of the variable for use in the hash table of context */
    uint32_t hash;
    
    /* 
     * The most parent variable, which has same existence rule,
     * as this variable.
     * 
     * Parent of this variable should decide, whether variable is exist
     * in some context.
     * 
     * If variable starts existence context (e.g., it is a field of the
     * variant), contains reference to itself (that is, 0).
     * If variable is always exists, contains 1 (cannot refer towards).
     */
    var_rel_index_t existence_index;
    
    /* Type-depended inmplementation of the variable */
    struct ctf_var_impl* var_impl;
};

static inline struct ctf_var* ctf_var_get_parent(struct ctf_var* var)
{
    return var->parent_index ? var + var->parent_index : NULL;
}

static inline struct ctf_var* ctf_var_get_first_child(struct ctf_var* var)
{
    return var->first_child_index ? var + var->first_child_index : NULL;
}

static inline struct ctf_var* ctf_var_get_next_sibling(struct ctf_var* var)
{
    return var->next_sibling_index ? var + var->next_sibling_index : NULL;
}

static inline struct ctf_var* ctf_var_get_context(struct ctf_var* var)
{
    return var + var->context_index;
}

static inline struct ctf_var* ctf_var_get_existence(struct ctf_var* var)
{
    return var->existence_index <= 0 ? var + var->existence_index : NULL;
}


/* 
 * Auxiliary functions for use while construct variables.
 * 
 * (shouldn't be used after freezing of the meta)
 */
static inline struct ctf_var* ctf_var_get_prev(
    struct ctf_meta* meta, struct ctf_var* var)
{
    int index = var - meta->vars;
    int prev_index = meta->build_info->layout_info[index].prev_index;
    
    return prev_index ? prev_index + var : NULL;
}

static inline struct ctf_var* ctf_var_get_container(
    struct ctf_meta* meta, struct ctf_var* var)
{
    int index = var - meta->vars;
    int container_index = meta->build_info->layout_info[index].container_index;
    
    return container_index ? container_index + var : NULL;
}

/* 
 * 'virtual' operations for variable, concerned layout of the variable.
 * 
 * All functions accept:
 * 1) implementation of the variable 'var_impl',
 * 2) variable 'var', for this implementation is set (or 'as if set',
 *      this is reason why implementation is not extract from variable
 *      but is passed explicitly as parameter).
 * 3) 'context' which is known at this stage. NULL means 'no context'.
 * 
 * Functions should either return a value required or -1, which means
 * 'result is not constant within this context'.
 * 
 * These operations are expected to work with variant's field, which
 * may absent with this context (or it is unknown, exist it or not).
 * It is caller who shouldn't use these results for access to the 
 * unexistent variables.
 * 
 * Callbacks should try to return requested value with most minimal
 * context, even with empty one(NULL), if it is possible. This fact is
 * used when optimize same callbacks for next variables.
 * 
 * When variable is taken via
 * ctf_var_get_container() or
 * ctf_var_get_prev()
 * 
 * not all its callbacks may be used:
 * 
 * Var's callback       | Accessible callbacks
 * 
 * get_alignment        | (none)
 * 
 * get_start_offset,    | container's get_alignment, get_start_offset,
 * get_size,            | all prev's callbacks
 * get_end_offset
 * 
 * Also, container's get_alignment may use get_alignment of its fields.
 */
struct ctf_var_impl_layout_operations
{
    /* 
     * Return alignment(in bits) of the variable.
     */
    int (*get_alignment)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    /* 
     * Return offset(in bits) where variable is start inside its context.
     */
    int (*get_start_offset)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    /* 
     * Return size(in bits) of the variable.
     */
    int (*get_size)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    
    /* 
     * Return offset(in bits) where variable is end inside its context.
     */
    int (*get_end_offset)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    
    /*
     * Decide, whether child variable is exist in the given context,
     * IF(!) this variable is exist.
     * 
     * In other worlds, decide, whether this variable and given child
     * has SAME existence rule.
     * 
     * Return:
     *  -  1, if child definitly exists
     *        (when parent exists, child exists too),
     *  -  0, if child definitly absent,
     *        (when parent exists, child is absent)
     *  - -1, if context is insufficient for decide child existence.
     * 
     * NULL callback means "always return 1".
     * 
     * NOTE: While this function is also concerned layout, it isn't
     * interfer with previous layout functions.
     * Note also, that it operate with parent-child hierarchy, not with
     * container-content one.
     */
    int (*is_child_exist)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_var* child_var,
        struct ctf_context* context);
};

/* Wrappers for layout callbacks */
static inline int ctf_var_get_start_offset(struct ctf_var* var,
    struct ctf_context* context)
{
    return var->var_impl->layout_ops->get_start_offset(var->var_impl,
        var, context);
}

/* Used in non-internal header */
int ctf_var_get_alignment(struct ctf_var* var,
    struct ctf_context* context);

/* Used in non-internal header */
int ctf_var_get_size(struct ctf_var* var,
    struct ctf_context* context);

static inline int ctf_var_get_end_offset(struct ctf_var* var,
    struct ctf_context* context)
{
    return var->var_impl->layout_ops->get_end_offset(var->var_impl,
        var, context);
}

/* Check whether given variable exists. */
int ctf_var_is_exist(struct ctf_var*, struct ctf_context* context);


/* 
 * If context is sufficient for read variable, return
 * minimum context, from which variable may be read.
 * 
 * Also, verify that context length is sufficient(not less than
 * end_offset). If no, extents context.
 * 
 * This context may be passed to other callbacks for make them
 * faster.
 * 
 * Otherwise return NULL.
 * 
 * NOTE: Function may be called only when ctf_var_is_exist()
 * returns 1.
 * 
 */

struct ctf_context* ctf_var_make_read(struct ctf_var* var,
    struct ctf_context* context);

/* Interpretation operations(common part). */
struct ctf_var_impl_interpret_operations
{
    /* 
     * Return type of the variable.
     */
    struct ctf_type* (*get_type)(struct ctf_var_impl* var_impl);
};

struct ctf_var_impl_root_operations
{
    struct ctf_var_impl_interpret_operations base;
    /*
     * Set context implementation for top-level variable,
     * which is child for this.
     * 
     * 'parent_context' may be adjusted if needed.
     * E.g, when parent context points to the array in the upper variable,
     * it may be adjusted to that variable itself.
     */
    int (*set_context_impl)(struct ctf_context* context,
        struct ctf_var_impl* var_impl, struct ctf_var* var, 
        struct ctf_var* child_var, struct ctf_context* parent_context_p,
        struct ctf_context_info* context_info);
};

/* 
 * 'virtual' operations for variable, concerned interpretation as
 * integer.
 */
struct ctf_var_impl_int_operations
{
    struct ctf_var_impl_interpret_operations base;
    
    /*
     * Extract abstract integer.
     * 
     * 'dest' should be point to the buffer, which is sutable for store
     * given variable ('get_size()' bits).
     * 
     * Copied value has native byte order and byte-alignment.
     * 
     * Should be NULL if no integer interpretation.
     */
    void (*copy_int)(void* dest, struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* 
     * Return 32-bit integer interpretation.
     * 
     * If integer type doesn't fit into 32-bit, should be NULL.
     * 
     * While returned value has 'unsigned' specificator, it really
     * has same signess as value required.
     */
    uint32_t (*get_int32)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* 
     * Specialization for 64-bit integer.
     * 
     * If integer type doesn't fit into 64-bit, should be NULL.
     * 
     * While returned value has 'unsigned' specificator, it really
     * has same signess as value required.
     */
    uint64_t (*get_int64)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
};

struct ctf_var_impl_enum_operations
{
    /* Enum variable support all integer interpretations. */
    struct ctf_var_impl_int_operations base;

    /* 
     * Return enumeration string corresponded to the integer value.
     * 
     * If has no integer interpretation, should be NULL.
     * 
     * If no string representation for the integer, should return NULL.
     */
    const char* (*get_enum)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
};

/* Same operations for arrays and sequences */
struct ctf_var_impl_array_operations
{
    struct ctf_var_impl_interpret_operations base;
    
    int (*get_n_elems)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    
    /*
     * Set context implementation for array element
     * (variable with subname "[]").
     * 
     * Should be called with context corresponded to array
     * (context->variable == var->context_index + index).
     * (NOTE: this is precondition for callback, not for global function)
     */
    int (*set_context_impl_elem)(struct ctf_context* context,
        struct ctf_var_impl* var_impl, struct ctf_var* var,
        struct ctf_var* element_var,
        struct ctf_context* parent_context);
};

/* Operations for variants */
struct ctf_var_impl_variant_operations
{
    struct ctf_var_impl_interpret_operations base;
    
    /* 
     * Set 'active_field_p' to the current active field of variant.
     * 
     * Return 0 on success, return -1 if context is insufficient.
     * 
     * NOTE: active_field_p may be set to NULL. This is normal.
     */
    int (*get_active_field)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context,
        struct ctf_var** active_field_p);
};


//TODO: Other interpretators should be here(strings, arrays)



/* 
 * Add variable to the meta information.
 * 
 * If type contains fields, variables corresponded to its fields
 * are added too(recursive).
 * 
 * parent = NULL when add root variable,
 * container = NULL when add variable with its own context.
 * 
 * Return index(absolute) of variable created or negative error code.
 */
var_rel_index_t ctf_meta_add_var(struct ctf_meta* meta,
    const char* var_name, struct ctf_type* var_type,
    struct ctf_var* parent,
    struct ctf_var* container, struct ctf_var* prev);

/* 
 * Helper for set variable implementation.
 */
static inline void ctf_var_set_impl(struct ctf_var* var,
    struct ctf_var_impl* var_impl)
{
    var->var_impl = var_impl;
}

/* 
 * Search type with given name.
 * 
 * Possible scopes for search are detected automatically.
 */
struct ctf_type* ctf_meta_find_type(struct ctf_meta* meta,
    const char* type_name);

/* 
 * Make tag for given string.
 * 
 * Scope of the tag is determined automatically.
 */
struct ctf_tag* ctf_meta_make_tag(struct ctf_meta* meta,
    const char* str);

#endif /* CTF_META_INTERNAL_H */