/*
 * Internal objects for CTF reader.
 */

#ifndef CTF_READER_INTERNAL_H
#define CTF_READER_INTERNAL_H

#include <stdio.h>
#include <kedr/ctf_reader/ctf_reader.h>

#include <alloc.h> /* malloc */

#define ctf_err(format, ...) fprintf(stderr, "<CTF_READER> "format"\n", ##__VA_ARGS__)

struct ctf_reader
{

};

struct ctf_type;
struct ctf_var;

/* 
 * Define mapping of CTF variables in memory.
 * 
 * Normally created by user.
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
struct ctf_var_impl_operations;
struct ctf_var_impl
{
    const struct ctf_var_impl_operations* var_ops;
}

/* 
 * CTF Variable.
 * 
 * Unit on the constructed CTF metadata.
 * 
 * Have a type and corresponds to:
 *  -instantiated top-level type (simple or compound)
 *  -instantiated field of the instansiated type
 * 
 * Every variable has a unique id, which corresponds to its index
 * in the array of all variables.
 */

typedef int32_t var_id_t;

struct ctf_var
{
    /* 
     * List-organized layout hierarchies.
     * 
     * May be used while create implementation for variables.
     * 
     * TODO: After all variables and their implementations are created,
     * these fields may be dropped - different array of ctf_var_layout? 
     */
    
    /*
     * Nearest container of the variable.
     * 
     * Element may use only 'get_start' and 'get_alignment' from its
     * container.
     * 
     * If variable is top-level for some CTF context, container should
     * be NULL.
     */
    struct ctf_var* container;
    /*
     * Previous element with same container.
     * 
     * Element may use any layout functions from it
     * ('get_alignment', 'get_start', 'get_size'...).
     * 
     * If element is first in container or it is top-level variable,
     * field is NULL.
     */
    struct ctf_var prev_sibling;
    /* 
     * Top variable of this hierarchy.
     * 
     * Context, corresponded to that variable, contains memory region
     * to which this variable is mapped.
     * 
     * For top-level variable contains reference to itself.
     */
    struct ctf_var* top_variable;
    
    /* Name of the variable (inside container)*/
    char* name;
    /* Hash of the variable */
    uint32_t hash;
    
    struct ctf_var_impl* var_impl;
};

/* 
 * 'virtual' operations for variable.
 */
struct ctf_var_impl_operations
{
    void (*destroy_impl)(struct ctf_var_impl* var_impl);
    /* 
     * Return alignment(in bits) of the variable.
     * 
     * Return -1 if not defined for given context.
     * 
     * Used for optimizations of callbacks for inner variables.
     */
    int (*get_alignment)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    /* 
     * Return offset(in bits) where variable is start inside its context.
     * 
     * Return -1 if not defined for given context.
     */
    int (*get_start_offset)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    /* 
     * Return size(in bits) of the variable.
     * 
     * Return -1 if not defined for given context.
     */
    int (*get_size)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    
    /* 
     * Return offset(in bits) where variable is end inside its context.
     * 
     * Return -1 if not defined for given context.
     */
    int (*get_end_offset)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* Interpretators of value */

    /* 
     * Whether variable may be read(context is sufficient for this).
     * 
     * Other interpretation callbacks may be called only when this
     * one return non-zero.
     */
    int (*can_read)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* 
     * Return integer interpretation.
     * 
     * If has no integer interpretation, or integer type doesn't fit
     * into 'int', should be NULL.
     */
    unsigned int (*get_int)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* 
     * Specialization for 64-bit integer.
     * 
     * If NULL and 'get_int' is not NULL, 'get_int' is used.
     */
    uint64_t (*get_int64)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    
    /*
     * Extract abstract integer.
     * 
     * 'dest' should be point to the buffer, which is sutable for store
     * given variable ('get_size()' bits).
     * 
     * Copied value has native byteorder.
     * 
     * Should be NULL if no integer interpretation.
     */
    void copy_int(void* dest, struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);

    /* 
     * Return enumeration string corresponded to the integer value.
     * 
     * If has no integer interpretation, should be NULL.
     * 
     * If no string representation for the integer, should return NULL.
     */
    const char* (*get_enum)(struct ctf_var_impl* var_impl,
        struct ctf_var* var, struct ctf_context* context);
    //TODO: Other interpretators should be here(strings, arrays)

};

//TODO: default is need in this form?
static int default_var_can_read(struct ctf_var* var,
    struct ctf_context* context)
{
    for(;context != NULL; context = context->prev_context)
    {
        if(context->variable == var->top_variable) return 1;
    }
    return 0;
}


/* 
 * Add variable to the reader.
 *
 * For use in callbacks for compound types which create subvariables.
 * 
 * NOTE: 'container' may not be NULL.
 */
struct ctf_var* ctf_reader_add_var(struct ctf_reader* reader,
    const char* name,
    struct ctf_var* container, struct ctf_var* prev_sibling);

/* Helpers for get variable value */
int ctf_var_can_read(struct ctf_var* var, struct ctf_context* context)
{
    if(var->var_impl->var_ops->can_read == NULL)
    {
        return default_var_can_read(var, context);
    }
    else
    {
        return var->var_impl->var_ops->can_read(var->var_impl, var, context);
    }
}

unsigned int ctf_var_get_int(struct ctf_var* var, struct ctf_context* context)
{
    return var->var_impl->var_ops->get_int(var->var_impl, var, context);
}


uint64_t ctf_var_get_int64(struct ctf_var* var, struct ctf_context* context)
{
    if(var->var_impl->var_ops->get_int64)
        return var->var_impl->var_ops->get_int64(var->var_impl, var, context);
    else
        return var->var_impl->var_ops->get_int(var->var_impl, var, context);
}


/* 
 * Helper for set variable implementation.
 * 
 * Correctly process 'set_impl' callback.
 */
void ctf_var_set_impl(struct ctf_var* var,
    struct ctf_var_impl* var_impl)
{
    if(var->var_impl->var_ops->set_impl)
        var->var_impl->var_ops->set_impl(var->var_impl, var_impl);
    else
        var->var_impl = var_impl;
}

/* Return variable with given index */
struct ctf_var* ctf_reader_get_var(ctf_reader* reader, int32_t index)
{
    //TODO
}

/* Helpers */
int ctf_var_get_start(struct ctf_var* var, struct ctf_context* context)
{
    return var->var_impl->var_ops->get_start(var->var_impl,
        var, context);
}

int ctf_var_get_alignment(struct ctf_var* var, struct ctf_context* context)
{
    return var->var_impl->var_ops->get_alignment(var->var_impl,
        var, context);
}

int ctf_var_get_size(struct ctf_var* var, struct ctf_context* context)
{
    return var->var_impl->var_ops->get_size(var->var_impl,
        var, context);
}


/* 
 * Generic function for determine start of the variable.
 * 
 * May be used in the 'get_start' callback.
 */
int ctf_var_get_start_generic(struct ctf_var* var,
    struct ctf_context* context)
{
    int align;
    if(var->prev != NULL)
    {
        int prev_start;
        int prev_size;

        prev_start = ctf_var_get_start(var->prev, context);
        if(prev_start == -1) return -1;/* undefined for given data */
        
        prev_size = ctf_var_get_size(var->prev, context);
        if(prev_size == -1) return -1;/* undefined for given data */
        
        align = ctf_var_get_alignment(var, context);
        if(align == -1) return -1;/* undefined for given data */

        return ALIGN_VAL(prev_start + prev_size, align);
    }

    if(var->container != NULL)
    {
        int container_start = ctf_var_get_start(var->container, context);
        if(container_start == -1) return -1;/* undefined for given data */
        
        align = ctf_var_get_alignment(var, context);
        if(align == -1) return -1;/* undefined for given data */
        
        return ALIGN_VAL(container_start, align);
    }
    
    return 0;/* First variable in the scope */
}


/************************** CTF type **********************************/

enum ctf_type_type
{
    ctf_type_type_none = 0,
    
    ctf_type_type_int,
    ctf_type_type_struct,
    ctf_type_type_variant,
    //TODO: others types
};

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

struct ctf_type* ctf_type_create(const char* name,
    struct ctf_type* scope, struct ctf_type_impl* type_impl)
{
    struct ctf_type* type = malloc(sizeof(*type));
    if(type == NULL)
    {
        ctf_err("Failed to allocate type instance.");
        return NULL;
    }
    
    type->name = strdup(name);
    if(type->name == NULL)
    {
        ctf_err("Failed to allocate name of the type.");
        free(type);
        return NULL;
    }
    type->scope = scope;

    type->type_impl = type_impl;
    
    return type;
}

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
     * Return alignment of the type, if it is constant and known.
     * 
     * Otherwise should return -1 or should't be set.
     */
    int (*get_alignment)(struct ctf_type* type);

    /* 
     * Return maximum alignment of the type.
     * 
     * Needed for fields inside structure.
     */
    int (*get_max_alignment)(struct ctf_type* type);


    /* 
     * Return size of the type, if it is constant and known.
     * 
     * Otherwise should return -1 or should't be set.
     */
    int (*get_size)(struct ctf_type* type);
    
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

void ctf_type_destroy(struct ctf_type* type)
{
    free(type->name);
    type->type_impl->type_ops->destroy_impl(type->type_impl);
    
    free(type);
}

//struct ctf_field_operations;


/* Instance of variable in particular scope instance */
struct ctf_var_instance
{
    //TODO: hash organization
    int32_t var_id;
};


#endif /* CTF_READER_INTERNAL_H */