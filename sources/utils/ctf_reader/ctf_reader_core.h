/*
 * Internal objects for CTF reader.
 */

#ifndef CTF_READER_CORE_H
#define CTF_READER_CORE_H

#include <stdio.h>
#include <kedr/ctf_reader/ctf_reader.h>

#include <alloc.h> /* malloc */

#define ctf_err(format, ...) fprintf(stderr, "<CTF_READER> "format"\n", ##__VA_ARGS__)

struct ctf_type;
struct ctf_var;

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
     * If variable is top-level for some CTF context, field is NULL.
     */
    struct ctf_var* container;

    /*
     * Previous element with same container.
     * 
     * If element is first in container or it is top-level variable,
     * field is NULL.
     */
    struct ctf_var* prev;
};



struct ctf_reader
{
    /* Array of allocated variables */
    struct ctf_var* vars;
    int vars_n;
    
    /* 
     * Corresponded array of layout information.
     * 
     * Exist only while create variables and their implementation.
     */
    struct ctf_var_layout_info* vars_layout_info;
};

/* 
 * Freeze reader.
 * 
 * Variables and types may not be added after this function.
 * 
 * Additional objects used while construct new types and variables
 * is dropped at this stage.
 * 
 * As opposite, context creation become available after this stage.
 */
void ctf_reader_freeze(ctf_reader* reader);

/* 
 * Return variable with given name.
 * 
 * If 'var_scope' is not NULL, 'name' is assumed to be relative to
 * the given variable.
 * 
 * Example of full name:
 * "event.fields.lock.type".
 * 
 * Same name but relative to "event.fields":
 * "lock.type".
 */

struct ctf_var* ctf_reader_find_var(struct ctf_reader* reader,
    const char* name, struct ctf_var* var_scope);


/*
 * Return type with given name.
 * 
 * If 'type_scope' is not NULL, look for type in the given scope or upper,
 * otherwise look for type in global scope.
 * 
 * NOTE: In contrast to variables, types cannot be searched using fully-
 * quialified name, "outer.inner" is not a type 'inner' in scope 'outer'.
 */
struct ctf_type* ctf_reader_find_type(struct ctf_reader* reader,
    const char* name, struct ctf_type* type_scope);

/* 
 * Define mapping of CTF variables in memory.
 * 
 * Normally created in responce to the user request.
 */
struct ctf_context
{
    struct ctf_reader* reader;
    /* 
     * CTF variable which is mapped to the memory region, defined
     * by this context.
     * 
     * NOTE: This variable may contain sub-variables, so them will
     * also be mapped.
     */
    struct ctf_var* variable;
    /* Linear hierarchy of contexts */
    struct ctf_context* prev_context;/* NULL if first context */
    
    const char* map_start;
    //...
};

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
     */
    struct ctf_var* parent;/* for extract name of variable */
    struct ctf_var* first_child;
    struct ctf_var* next_subling;
    
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
     * Top variable of this hierarchy.
     * 
     * Context, corresponded to that variable, contains memory region
     * to which this variable is mapped.
     * 
     * For top-level variable contains reference to itself.
     */
    struct ctf_var* top_variable;
    
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
     * variant), contains reference to itself.
     * If variable is always exists, contains NULL.
     */
    struct ctf_var* existence_context;
    
    /* Type-depended inmplementation of the variable */
    struct ctf_var_impl* var_impl;
};

/* 
 * Auxiliary functions for use while construct variables.
 * 
 * (shouldn't be used after freezing of the reader)
 */
static inline struct ctf_var* ctf_var_get_prev(
    struct ctf_reader* reader, struct ctf_var* var)
{
    int index = var - reader->vars;
    
    return reader->vars_layout_info[index].prev;
}

static inline struct ctf_var* ctf_var_get_container(
    struct ctf_reader* reader, struct ctf_var* var)
{
    int index = var - reader->vars;
    
    return reader->vars_layout_info[index].container;
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

static inline int ctf_var_get_alignment(struct ctf_var* var,
    struct ctf_context* context)
{
    return var->var_impl->layout_ops->get_alignment(var->var_impl,
        var, context);
}

static inline int ctf_var_get_size(struct ctf_var* var,
    struct ctf_context* context)
{
    return var->var_impl->layout_ops->get_size(var->var_impl,
        var, context);
}

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
    void copy_int(void* dest, struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* 
     * Return integer interpretation.
     * 
     * If integer type doesn't fit into 'int', should be NULL.
     * 
     * While return value has 'unsigned' specificator, it really
     * has same signess as value require.
     */
    unsigned int (*get_int)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* 
     * Specialization for 64-bit integer.
     * 
     * If NULL and 'get_int' is not NULL, 'get_int' is used.
     * 
     * While return value has 'unsigned' specificator, it really
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
    
    /*
     * Create context for array element (variable with subname "[]").
     * 
     * Should be called with context corresponded to array
     * (context->variable == var).
     * 
     * Return NULL on error.
     */
    struct ctf_array_context* (*create_element_context)(
        struct ctf_var_impl* var_impl, struct ctf_var* var,
        struct ctf_context* context);
};

struct ctf_var_impl_top_operations
{
    struct ctf_var_impl_interpret_operations base;
    
    /*
     * Create context for top-level variable
     * (such as "stream.packet.context").
     * 
     * Return NULL on error or if context is insufficient.
     */
    struct ctf_context* (*create_top_context)(
        struct ctf_var_impl* var_impl, struct ctf_var* var,
        struct ctf_context* context,
        struct ctf_context_info*);
};


//TODO: Other interpretators should be here(strings, arrays)


/* Check that variable has integer interpretation. */
static inline int ctf_var_contains_int(struct ctf_var* var)
{
    enum ctf_type_type type_type =
        var->type->type_impl->type_ops->get_type(var->type);
    
    return (type_type == ctf_type_type_int)
        || (type_type == ctf_type_type_enum);
}


static inline void ctf_var_copy_int(void* dest, struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    return int_ops->copy_int(dest, var->var_impl, var, context);
}

/* 
 * Check that variable fit into "C" native type 'int'.
 * 
 * NOTE: May be called only when ctf_var_contains_int() returns true;
 */
static inline int ctf_var_is_fit_int(struct ctf_var* var)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    return int_ops->get_int != NULL;
}

/* 
 * Check that variable fit into 64-bit type 'int'.
 * 
 * NOTE: May be called only when ctf_var_contains_int() returns true;
 */
static inline int ctf_var_is_fit_int64(struct ctf_var* var)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    return (int_ops->get_int != NULL) || (int_ops->get_int64 != NULL) ;
}

static inline unsigned int ctf_var_get_int(struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    return int_ops->get_int(var->var_impl, var, context);
}


static inline unsigned int ctf_var_get_int64(struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    return int_ops->get_int64
        ? int_ops->get_int64(var->var_impl, var, context)
        : int_ops->get_int(var->var_impl, var, context);
}

/* Check that variable has enum interpretation. */
static inline int ctf_var_contains_enum(struct ctf_var* var)
{
    enum ctf_type_type type_type =
        var->type->type_impl->type_ops->get_type(var->type);
    
    return type_type == ctf_type_type_enum;
}


static inline const char* get_enum(struct ctf_var* var,
    struct ctf_context)
{
    const struct ctf_var_impl_enum_operations* enum_ops =
        container_of(var->var_impl->interpret_ops, typeof(*enum_ops), base.base);
    return enum_ops->get_enum(var->var_impl, var, context);
}


/* 
 * Add variable which require its own context to the reader.
 *
 * NOTE: 'container' may not be NULL.
 */
struct ctf_var* ctf_reader_add_context_var(struct ctf_reader* reader,
    struct ctf_var* parent,
    const char* var_name, struct ctf_type* var_type,
    //TODO
    );


/* 
 * Add variable to the reader.
 *
 * For use in callbacks for compound types which create subvariables.
 * 
 * NOTE: 'container' may not be NULL.
 */
struct ctf_var* ctf_reader_add_var(struct ctf_reader* reader,
    const char* var_name, struct ctf_type* var_type,
    struct ctf_var* parent,
    struct ctf_var* container, struct ctf_var* prev_sibling);

/* Helpers for variable interpretation */
static inline int ctf_var_can_read(struct ctf_var* var,
    struct ctf_context* context)
{
    if(var->var_impl->var_ops->can_read == NULL)
    {
        return default_var_can_read(var, context);
    }
    else
    {
        return var->var_impl->interpret_ops->can_read(var->var_impl,
            var, context);
    }
}



/* 
 * Helper for set variable implementation.
 */
static inline void ctf_var_set_impl(struct ctf_var* var,
    struct ctf_var_impl* var_impl)
{
    var->var_impl = var_impl;
}

/************************** CTF type **********************************/

struct ctf_type_impl_operations;
struct ctf_type_impl
{
    static struct ctf_type_impl_operations* type_ops;
}
/* 
 * CTF type.
 * 
 * All specializations are implemented via its 'type_impl' field.
 */
struct ctf_type
{
    /*
     * Name of the type.
     * 
     * If it is inner type, full name of the type is
     * 
     * .scope->name + "." + .name
     * 
     * Unnamed types has name equal to
     * "@" + fieldname
     */
    char* name;
    /*
     * For inner type - type contained definition of given type.
     * 
     * For global types NULL;
     */
    struct ctf_type* scope;
    
    struct ctf_type_impl* type_impl;
};

/* 
 * 'Virtual' operations for the type implementation.
 * 
 * Implementation is taken from type->type_impl.
 * Also, other fields of type may be accessed.
 */
struct ctf_type_impl_operations
{
    /* RTTI */
    enum ctf_type_type (*get_type)(struct ctf_type* type);
    /* destructor */
    void destroy_impl(struct ctf_type_impl* type_impl);
    
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
    int (*set_var_impl)(struct ctf_type* type, struct ctf_var* var);
    
    /*
     * Return type of the field with given name.
     * 
     * If type doesn't support inner fields, or has no field with
     * given name, return NULL or not set.
     * 
     * Need for tag search.
     */
    struct ctf_type* (*find_field)(struct ctf_type* type,
        const char field_name);
};

static inline void ctf_type_impl_destroy(struct ctf_type_impl* type_impl)
{
    type_impl->type_ops->destroy_impl(type_impl);
}

static inline int ctf_type_get_max_alignment(struct ctf_type* type)
{
    return type->type_impl->type_ops->get_max_alignment(type);
}

/* Create type with given implementation. */
struct ctf_type* ctf_type_create(const char* name,
    struct ctf_type* scope, struct ctf_type_impl* type_impl);

/* Destroy type */
void ctf_type_destroy(struct ctf_type* type);

#endif /* CTF_READER_CORE_H */