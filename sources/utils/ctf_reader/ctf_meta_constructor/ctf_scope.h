/*
 * CTF Scope - abstract range of sentences inside {}.
 * And its specializations.
 */

#ifndef CTF_SCOPE_H
#define CTF_SCOPE_H

#include "ctf_meta.h"
#include "ctf_meta_internal.h"

#include "linked_list.h"

struct ctf_scope_operations;
/* Scope - abstract range of sentences inside {} */
struct ctf_scope
{
    /* 
     * Parent scope, NULL for root scope.
     * 
     * The only common information about scopes hierarchy.
     */
    struct ctf_scope* parent_scope;
    /* List organization of child scopes */
    //struct linked_list_head child_scopes;
    
    //struct linked_list_elem list_elem;
    /* Name of the scope. For root scope is NULL. */
    char* name;
    
    /* 
     * If scope is connected with type(structure or variant),
     * this points to that type. Otherwise NULL.
     */
    struct ctf_type* connected_type;
    
    /* List organization of global variables, defined in this scope */
    //struct linked_list_head global_vars;
    
    const struct ctf_scope_operations* scope_ops;
};

/* Helper for search type */
static inline struct ctf_scope* ctf_scope_get_parent(
    struct ctf_scope* scope)
{
    return scope->parent_scope;
}

/* Types of scope specializations */
enum ctf_scope_type
{
    ctf_scope_type_invalid = 0,
    /* Root scope */
    ctf_scope_type_root,
    /* Top scope - may contain global parameters(variables) and type assignments */
    ctf_scope_type_top,
    /* Named scope - may contain global parameters(variables) */
    //ctf_scope_type_named,
    /* Scope contained structure fields definitions */
    ctf_scope_type_struct,
    /* Scope contained variant fields definitions */
    ctf_scope_type_variant,
    /* Scope contained enumeration values */
    ctf_scope_type_enum,
    /* Scope contained integer parameters */
    ctf_scope_type_int,
};

/* Commons scope virtual operations */
struct ctf_scope_operations
{
    void (*destroy_scope)(struct ctf_scope* scope);
    /* RTTI */
    enum ctf_scope_type (*get_type)(struct ctf_scope* scope);

    /* 
     * Search type in the scope.
     * 
     * Return type with given name or NULL.
     * 
     * NULL callback means "always return NULL."
     */
    struct ctf_type* (*find_type)(struct ctf_scope* scope,
        const char* type_name);
    
    /*
     * Return type connected to the scope, if it is.
     * 
     * When make tag inside scope, tag components are resolved using
     * 'ctf_type_resolve_tag_component' function on connected type.
     * 
     * Also, fields or other constructions, defined in the scope,
     * are interpret as some commands to the type.
     * 
     * When scope is not connected to the type, return NULL.
     * 
     * NULL callback means that scope doesn't connected to the type.
     */
    struct ctf_type* (*get_type_connected)(struct ctf_scope* scope);
    
    /*
     * Add type to the scope.
     * 
     * Return 0 on success, negative error on fail.
     * 
     * If scope doesn't support inner types, callback should be NULL.
     */
    int (*add_type)(struct ctf_scope* scope, struct ctf_type* type);
    
    /*
     * Store type in the scope, so scope will be responsive for its
     * lifetime.
     * 
     * Difference from 'add_type' is that type cannot be searched by name
     * and be otherwise accessed from the scope.
     */
    int (*store_type)(struct ctf_scope* scope, struct ctf_type* type);

    /* 
     * Create scope connected to the given type and add this scope
     * to given scope.
     * 
     * Return scope created or NULL on error.
     * 
     * 'scope' should be parent for 'type'.
     *
     * If scope doesn't support inner types, callback should be NULL.
     * (but it wouldn't access in any case).
     */
    struct ctf_scope* (*add_scope_connected)(struct ctf_scope* scope,
        struct ctf_type* type);
    
    /* 
     * Remove given type from the scope and destroy type itself.
     * 
     * NOTE: for types which has been added via 'add_type'
     * 
     * Usually this is last added type(recovering after some error).
     */
    void (*destroy_type)(struct ctf_scope* scope, struct ctf_type* type);
    
    /* 
     * Remove given type from the scope and destroy type itself.
     * 
     * NOTE: for types which has been added via 'store_type'
     * 
     * Usually this is last stored type(recovering after some error).
     */
    void (*destroy_stored_type)(struct ctf_scope* scope, struct ctf_type* type);
    
    

};

/* Destroy scope */
void ctf_scope_destroy(struct ctf_scope* scope);

/* 
 * Search type with given name, starting in the given scope.
 * 
 * If scope is not founded in the scope, its parent scope is tested.
 */
struct ctf_type* ctf_scope_find_type(struct ctf_scope* scope,
    const char* name);

/* 
 * Search type with given name in the given scope.
 * 
 * Other scopes are not searched.
 */
struct ctf_type* ctf_scope_find_type_strict(struct ctf_scope* scope,
    const char* name);

/* 
 * Get type, to which current scope is connected.
 * 
 * If scope doesn't connect to a type, return NULL.
 */
struct ctf_type* ctf_scope_get_type_connected(struct ctf_scope* scope);

/* Return non-zero if scope support inner types */
int ctf_scope_is_support_types(struct ctf_scope* scope);

/* 
 * Create type in the scope. 
 * 
 * May be called only for scopes supported inner types,
 * see ctf_scope_is_support_types().
 */
struct ctf_type* ctf_scope_create_type(struct ctf_scope* scope,
    const char* type_name);

/* 
 * Create type in the scope. 
 * 
 * May be called only for scopes supported inner types,
 * see ctf_scope_is_support_types().
 * 
 * Difference from 'create_type' is that type created cannot be searched
 * in the scope by name.
 */
struct ctf_type* ctf_scope_create_type_internal(struct ctf_scope* scope,
    const char* type_name);

/* 
 * Create scope, connected to given type.
 * 
 * Note that no all types support connected scopes.
 * 
 * Currently there are only structure, variant, integer, enumeration.
 */
struct ctf_scope* ctf_scope_create_for_type(struct ctf_type* type);

/* 
 * Destroy type declared in some scope(using ctf_scope_create_type()).
 */
void ctf_scope_destroy_type(struct ctf_type* type);

/* 
 * Destroy type declared in some scope(using ctf_scope_create_type_internal()).
 */
void ctf_scope_destroy_type_internal(struct ctf_type* type);


/* Operations for root scope */
struct ctf_scope_root_operations
{
    struct ctf_scope_operations base;
    
    /* Create top-level scope and add it to current one. */
    struct ctf_scope* (*add_top_scope)(struct ctf_scope* scope,
        const char* scope_name);
    /* Search top-level scope by name */
    struct ctf_scope* (*find_top_scope)(struct ctf_scope* scope,
        const char* scope_name);
};

/* Return non-zero if scope is root. */
int ctf_scope_is_root(struct ctf_scope* scope);

/* Add top-level scope with given name to the given root scope. */
struct ctf_scope* ctf_scope_root_add_top_scope(struct ctf_scope* scope,
    const char* scope_name);

/* Search top-level scope in the given root scope by name. */
struct ctf_scope* ctf_scope_root_find_top_scope(struct ctf_scope* scope,
    const char* scope_name);


/* Operations for top-level scope */
struct ctf_scope_top_operations
{
    struct ctf_scope_operations base;
    
    /* Return name of the scope */
    const char* (*get_name)(struct ctf_scope* scope);
    
    /* Add parameter to the scope */
    int (*add_parameter)(struct ctf_scope* scope, const char* param_name,
        const char* param_value);
    
    /* 
     * Get value of parameter defined in the scope.
     * 
     * Return NULL if parameter with this name is not defined in the scope.
     */
    const char* (*get_parameter)(struct ctf_scope* scope,
        const char* param_name);
};

/* Return non-zero if scope is top-level. */
int ctf_scope_is_top(struct ctf_scope* scope);

/* 
 * Assign type to some position relative to the given top-level scope.
 * 
 * Return 0 on success, negative error code on fail.
 */
int ctf_scope_top_assign_type(struct ctf_scope* scope,
    const char* assign_position, struct ctf_type* assigned_type);

/* Add named parameter to the scope. */
int ctf_scope_top_add_parameter(struct ctf_scope* scope,
    const char* param_name, const char* param_value);
    
/* Return value of named parameter defined in the scope. */
const char* ctf_scope_top_get_parameter(struct ctf_scope* scope,
    const char* param_name);

/* Create root scope */
struct ctf_scope* ctf_scope_create_root(struct ctf_type* root_type);

#endif /* CTF_SCOPE_H */