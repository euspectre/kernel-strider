/* Implementation of CTF scope. */

#include "ctf_meta_internal.h"

#include "ctf_scope.h"
#include "ctf_type.h" /* type creation, checks,...*/

#include <assert.h> /* assert macro */
#include <malloc.h> /* malloc() */

#include <errno.h> /* error codes */

#include <string.h> /* strcmp, strlen,...*/

/********************* Interface implementation ***********************/
void ctf_scope_destroy(struct ctf_scope* scope)
{
    if(scope->scope_ops->destroy_scope)
        scope->scope_ops->destroy_scope(scope);
}

struct ctf_type* ctf_scope_find_type(struct ctf_scope* scope,
    const char* name)
{
    struct ctf_scope* scope_tmp;
    for(scope_tmp = scope; scope_tmp != NULL;
        scope_tmp = ctf_scope_get_parent(scope_tmp))
    {
        if(scope->scope_ops->find_type == NULL)
        {
            //printf("Scope %p doesn't support types.\n", scope_tmp);
            continue;
        }
        
        struct ctf_type* type = scope_tmp->scope_ops->find_type(scope_tmp, name);
        if(type != NULL) return type;
        //printf("Scope %p doesn't contain type '%s'.\n", scope_tmp, name);
    }
    
    return NULL;
}

struct ctf_type* ctf_scope_find_type_strict(struct ctf_scope* scope,
    const char* name)
{
    return scope->scope_ops->find_type
        ? scope->scope_ops->find_type(scope, name)
        : NULL;
}


struct ctf_type* ctf_scope_get_type_connected(struct ctf_scope* scope)
{
    return scope->scope_ops->get_type_connected
        ? scope->scope_ops->get_type_connected(scope) : NULL;
}

/* Return non-zero if scope support inner types */
int ctf_scope_is_support_types(struct ctf_scope* scope)
{
    return scope->scope_ops->add_type != NULL;
}

struct ctf_type* ctf_scope_create_type(struct ctf_scope* scope,
    const char* type_name)
{
    assert(scope->scope_ops->add_type);
    
    struct ctf_type* type = ctf_type_create(type_name);
    if(type == NULL) return NULL;
    
    int result = scope->scope_ops->add_type(scope, type);
    if(result < 0)
    {
        ctf_type_destroy(type);
        return NULL;
    }
    type->scope = scope;
    //printf("Type '%s' has been added to the scope %p.\n", type_name, scope);
    return type;
}


struct ctf_type* ctf_scope_create_type_internal(struct ctf_scope* scope,
    const char* type_name)
{
    assert(scope->scope_ops->store_type);
    
    struct ctf_type* type = ctf_type_create(type_name);
    if(type == NULL) return NULL;
    
    int result = scope->scope_ops->store_type(scope, type);
    if(result < 0)
    {
        ctf_type_destroy(type);
        return NULL;
    }
    type->scope = scope;
    return type;
}

void ctf_scope_destroy_type(struct ctf_type* type)
{
    type->scope->scope_ops->destroy_type(type->scope, type);
}

void ctf_scope_destroy_type_internal(struct ctf_type* type)
{
    type->scope->scope_ops->destroy_stored_type(type->scope, type);
}


struct ctf_scope* ctf_scope_create_for_type(struct ctf_type* type)
{
    struct ctf_scope* parent_scope = type->scope;
    assert(parent_scope);
    assert(parent_scope->scope_ops->add_scope_connected);
    
    return parent_scope->scope_ops->add_scope_connected(parent_scope, type);
}

int ctf_scope_is_root(struct ctf_scope* scope)
{
    return scope->scope_ops->get_type(scope) == ctf_scope_type_root;
}

struct ctf_scope* ctf_scope_root_add_top_scope(struct ctf_scope* scope,
    const char* scope_name)
{
    struct ctf_scope_root_operations* ops_root =
        container_of(scope->scope_ops, typeof(*ops_root), base);
    
    return ops_root->add_top_scope(scope, scope_name);
}

struct ctf_scope* ctf_scope_root_find_top_scope(struct ctf_scope* scope,
    const char* scope_name)
{
    struct ctf_scope_root_operations* ops_root =
        container_of(scope->scope_ops, typeof(*ops_root), base);
    
    return ops_root->find_top_scope(scope, scope_name);
}

int ctf_scope_is_top(struct ctf_scope* scope)
{
    return scope->scope_ops->get_type(scope) == ctf_scope_type_top;
}


int ctf_scope_top_assign_type(struct ctf_scope* scope,
    const char* assign_position, struct ctf_type* assigned_type)
{
    struct ctf_scope_top_operations* ops_top =
        container_of(scope->scope_ops, typeof(*ops_top), base);
    
    const char* scope_name = ops_top->get_name(scope);
    assert(scope_name);
    
    struct ctf_scope* scope_root = ctf_scope_get_parent(scope);
    struct ctf_type* type_root = ctf_scope_get_type_connected(scope_root);
    
    assert(ctf_type_get_type(type_root) == ctf_type_type_root);
    
    /* Absolute assign position. */
    
    char* aap;
    int aap_len = snprintf(NULL, 0, "%s.%s", scope_name, assign_position);
    
    aap = malloc(aap_len + 1);
    if(aap == NULL)
    {
        ctf_err("Failed to allocate absolute assign position.");
        return -ENOMEM;
    }
    
    snprintf(aap, aap_len + 1, "%s.%s", scope_name, assign_position);
    
    int result = ctf_type_root_assign_type(type_root, aap, assigned_type);
    free(aap);
    
    return result;
}

int ctf_scope_top_add_parameter(struct ctf_scope* scope,
    const char* param_name, const char* param_value)
{
    struct ctf_scope_top_operations* ops_top =
        container_of(scope->scope_ops, typeof(*ops_top), base);
    
    return ops_top->add_parameter(scope, param_name, param_value);
}
    
/* Return value of named parameter defined in the scope. */
const char* ctf_scope_top_get_parameter(struct ctf_scope* scope,
    const char* param_name)
{
    struct ctf_scope_top_operations* ops_top =
        container_of(scope->scope_ops, typeof(*ops_top), base);
    
    return ops_top->get_parameter(scope, param_name);
}


/* Common initialization of the scope*/
static void ctf_scope_init(struct ctf_scope* scope,
    struct ctf_scope* parent_scope)
{
    scope->parent_scope = parent_scope;
}

/*********************** Scopes implementation ************************/

static const char* top_scope_names[] =
{
    "trace",
    "stream",
    "event",
    "env"
};

#define TOP_SCOPES_NUMBER \
(sizeof(top_scope_names) / sizeof(top_scope_names[0]))


/* Object used as base for all scopes which connected to some type. */
struct ctf_scope_connected
{
    struct ctf_scope base;
    
    struct ctf_type* type_connected;
    /* List in the container */
    struct linked_list_elem list_elem;
};


static void ctf_scope_connected_init(
    struct ctf_scope_connected* scope_connected, struct ctf_type* type)
{
    ctf_scope_init(&scope_connected->base, type->scope);
    
    scope_connected->type_connected = type;
    
    linked_list_elem_init(&scope_connected->list_elem);
}

static struct ctf_type* scope_connected_ops_get_type_connected(
    struct ctf_scope* scope)
{
    struct ctf_scope_connected* scope_connected =
        container_of(scope, typeof(*scope_connected), base);
    
    return scope_connected->type_connected;
}



/* Object used for all scopes which may contain types. */
struct scope_type_container
{
    /* Types which may be searched by name */
    struct ctf_type_container types;
    /* Types which should be deleted with scope */
    struct ctf_type_container types_stored;
    
    struct linked_list_head scopes_connected;
};

void scope_type_container_init(struct scope_type_container* container)
{
    ctf_type_container_init(&container->types);
    ctf_type_container_init(&container->types_stored);
    
    linked_list_head_init(&container->scopes_connected);
}

static void scope_type_container_add_type(
    struct scope_type_container* container, struct ctf_type* type)
{
    ctf_type_container_add_type(&container->types, type);
}

static void scope_type_container_destroy_type(
    struct scope_type_container* container, struct ctf_type* type)
{
    ctf_type_container_remove_type(&container->types, type);
    ctf_type_destroy(type);
}

static void scope_type_container_store_type(
    struct scope_type_container* container, struct ctf_type* type)
{
    ctf_type_container_add_type(&container->types_stored, type);
}

static void scope_type_container_destroy_stored_type(
    struct scope_type_container* container, struct ctf_type* type)
{
    ctf_type_container_remove_type(&container->types_stored, type);
    ctf_type_destroy(type);
}


static struct ctf_type* scope_type_container_find_type(
    struct scope_type_container* container, const char* type_name)
{
    return ctf_type_container_find_type(&container->types, type_name);
}

static void scope_type_container_add_scope_connected(
    struct scope_type_container* container,
    struct ctf_scope_connected* scope_connected)
{
    linked_list_add_elem(&container->scopes_connected,
        &scope_connected->list_elem);
}

static void scope_type_container_destroy(
    struct scope_type_container* container)
{
    while(!linked_list_is_empty(&container->scopes_connected))
    {
        struct ctf_scope_connected* scope_connected;
        linked_list_remove_first_entry(&container->scopes_connected,
            scope_connected, list_elem);

        ctf_scope_destroy(&scope_connected->base);
    }
    
    ctf_type_container_destroy(&container->types);
    ctf_type_container_destroy(&container->types_stored);
}

/* 
 * Same object type for struct and variant scopes.
 * Differs only in operations set.
 */
struct ctf_scope_struct
{
    struct ctf_scope_connected base;
    
    struct scope_type_container type_container;
};

/* Create scope for struct or variant, depended on flag 'is_struct' */
static struct ctf_scope_connected* ctf_scope_struct_create(
    struct ctf_type* type, int is_struct);

/* 
 * Object type for integer and enumeration scopes is 
 * 'struct ctf_scope_connected'.
 * Differs only in operations set.
 */

/* Create scope for int or enum, depended on flag 'is_int' */
static struct ctf_scope_connected* ctf_scope_int_create(
    struct ctf_type* type, int is_int);

/* Create scope connected to the given type */
static struct ctf_scope_connected* ctf_scope_connected_create(
    struct ctf_type* type)
{
    struct ctf_scope_connected* scope_connected;
    switch(ctf_type_get_type(type))
    {
    case ctf_type_type_struct:
        scope_connected = ctf_scope_struct_create(type, 1);
        return scope_connected ? scope_connected : NULL;
    break;
    case ctf_type_type_variant:
        scope_connected = ctf_scope_struct_create(type, 0);
        return scope_connected ? scope_connected : NULL;
    break;
    case ctf_type_type_int:
        scope_connected = ctf_scope_int_create(type, 1);
        return scope_connected ? scope_connected : NULL;
    break;
    case ctf_type_type_enum:
        scope_connected = ctf_scope_int_create(type, 0);
        return scope_connected ? scope_connected : NULL;
    break;
    default:
        ctf_err("Type doesn't support connected scopes.");
        return NULL;
    }
}

/* Root scope */
struct ctf_scope_root
{
    struct ctf_scope base;
    
    struct ctf_type* root_type;
    
    struct scope_type_container type_container;
    
    struct ctf_scope* top_scopes[TOP_SCOPES_NUMBER];
};

struct ctf_scope_top
{
    struct ctf_scope base;
    
    /* Pointer to the name in static array */
    const char* name;
    
    struct scope_type_container type_container;
};

static struct ctf_scope_top* ctf_scope_top_create(
    struct ctf_scope* parent_scope, int name_index);

/* Operations for root scope */
static struct ctf_scope* scope_root_ops_add_top_scope(
    struct ctf_scope* scope, const char* scope_name)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);
    
    int name_index;
    for(name_index = 0; name_index < (int)TOP_SCOPES_NUMBER; name_index++)
    {
        if(strcmp(scope_name, top_scope_names[name_index]) == 0) break;
    }
    
    if(name_index == (int)TOP_SCOPES_NUMBER)
    {
        ctf_err("Name '%s' cannot be used for top-level scope.", scope_name);
        return NULL;
    }
    else if(scope_root->top_scopes[name_index] != NULL)
    {
        ctf_err("Top-level scope with name '%s' already exists.",
            scope_name);
    }
    
    struct ctf_scope_top* scope_top = ctf_scope_top_create(
        &scope_root->base, name_index);
    if(scope_top == NULL) return NULL;

    scope_root->top_scopes[name_index] = &scope_top->base;
    
    return &scope_top->base;
}

static struct ctf_scope* scope_root_ops_find_top_scope(
    struct ctf_scope* scope, const char* scope_name)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);
    
    int name_index;
    for(name_index = 0; name_index < (int)TOP_SCOPES_NUMBER; name_index++)
    {
        if(strcmp(scope_name, top_scope_names[name_index]) == 0) break;
    }
    
    if(name_index == (int)TOP_SCOPES_NUMBER)
    {
        //TODO: should be warning
        ctf_err("Name '%s' cannot be used for top-level scope.", scope_name);
        return NULL;
    }

    return scope_root->top_scopes[name_index];
}

static struct ctf_type* scope_root_ops_get_root_type(
    struct ctf_scope* scope)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);

    return scope_root->root_type;
}

static enum ctf_scope_type scope_root_ops_get_type(
    struct ctf_scope* scope)
{
    (void) scope;
    return ctf_scope_type_root;
}

static int scope_root_ops_add_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);

    scope_type_container_add_type(&scope_root->type_container, type);
    
    return 0;
}

static void scope_root_ops_destroy_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);

    scope_type_container_destroy_type(&scope_root->type_container, type);
}


static int scope_root_ops_store_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);

    scope_type_container_store_type(&scope_root->type_container, type);
    
    return 0;
}

static void scope_root_ops_destroy_stored_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);

    scope_type_container_destroy_stored_type(&scope_root->type_container, type);
}


static struct ctf_type* scope_root_ops_find_type(struct ctf_scope* scope,
    const char* type_name)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);

    return scope_type_container_find_type(&scope_root->type_container,
        type_name);
}

static struct ctf_scope* scope_root_ops_add_scope_connected(
    struct ctf_scope* scope, struct ctf_type* type)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);
    
    struct ctf_scope_connected* scope_connected =
        ctf_scope_connected_create(type);
    
    if(scope_connected == NULL) return NULL;
    
    scope_type_container_add_scope_connected(&scope_root->type_container,
        scope_connected);
    
    return &scope_connected->base;
}

static void scope_root_ops_destroy_scope(struct ctf_scope* scope)
{
    struct ctf_scope_root* scope_root =
        container_of(scope, typeof(*scope_root), base);

    scope_type_container_destroy(&scope_root->type_container);
    
    int i;
    for(i = 0; i < (int)TOP_SCOPES_NUMBER; i++)
    {
        if(scope_root->top_scopes[i])
            ctf_scope_destroy(scope_root->top_scopes[i]);
    }
    
    free(scope_root);
}

static struct ctf_scope_root_operations scope_root_ops =
{
    .base =
    {
        .destroy_scope          = scope_root_ops_destroy_scope,
        .get_type               = scope_root_ops_get_type,
        .get_type_connected     = scope_root_ops_get_root_type,
        .find_type              = scope_root_ops_find_type,
        .add_type               = scope_root_ops_add_type,
        .destroy_type           = scope_root_ops_destroy_type,
        .store_type             = scope_root_ops_store_type,
        .destroy_stored_type    = scope_root_ops_destroy_stored_type,
        .add_scope_connected    = scope_root_ops_add_scope_connected,
        
    },
    .add_top_scope  = scope_root_ops_add_top_scope,
    .find_top_scope = scope_root_ops_find_top_scope,
};

struct ctf_scope* ctf_scope_create_root(struct ctf_type* root_type)
{
    struct ctf_scope_root* scope_root = malloc(sizeof(*scope_root));
    if(scope_root == NULL)
    {
        ctf_err("Failed to allocate root scope.");
        return NULL;
    }
    
    ctf_scope_init(&scope_root->base, NULL);
    
    scope_root->root_type = root_type;
    scope_type_container_init(&scope_root->type_container);
    memset(&scope_root->top_scopes, 0, sizeof(scope_root->top_scopes));
    
    scope_root->base.scope_ops = &scope_root_ops.base;
    
    return &scope_root->base;
}


/* Operations for struct and variant scopes */
static enum ctf_scope_type scope_struct_ops_get_type(
    struct ctf_scope* scope)
{
    (void)scope;
    return ctf_scope_type_struct;
}

static enum ctf_scope_type scope_variant_ops_get_type(
    struct ctf_scope* scope)
{
    (void)scope;
    return ctf_scope_type_variant;
}

static int scope_struct_ops_add_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_struct* scope_struct =
        container_of(scope, typeof(*scope_struct), base.base);

    scope_type_container_add_type(&scope_struct->type_container, type);
    
    return 0;
}

static void scope_struct_ops_destroy_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_struct* scope_struct =
        container_of(scope, typeof(*scope_struct), base.base);

    scope_type_container_destroy_type(&scope_struct->type_container, type);
}


static int scope_struct_ops_store_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_struct* scope_struct =
        container_of(scope, typeof(*scope_struct), base.base);

    scope_type_container_store_type(&scope_struct->type_container, type);
    
    return 0;
}

static void scope_struct_ops_destroy_stored_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_struct* scope_struct =
        container_of(scope, typeof(*scope_struct), base.base);

    scope_type_container_destroy_stored_type(&scope_struct->type_container, type);
}

static struct ctf_type* scope_struct_ops_find_type(
    struct ctf_scope* scope, const char* type_name)
{
    struct ctf_scope_struct* scope_struct = 
        container_of(scope, typeof(*scope_struct), base.base);
    
    return scope_type_container_find_type(&scope_struct->type_container,
        type_name);
}

static struct ctf_scope* scope_struct_ops_add_scope_connected(
    struct ctf_scope* scope, struct ctf_type* type)
{
    struct ctf_scope_struct* scope_struct = 
        container_of(scope, typeof(*scope_struct), base.base);

    struct ctf_scope_connected* scope_connected =
        ctf_scope_connected_create(type);
    
    if(scope_connected == NULL) return NULL;
    
    scope_type_container_add_scope_connected(&scope_struct->type_container,
        scope_connected);
    
    return &scope_connected->base;
}

static void scope_struct_ops_destroy_scope(struct ctf_scope* scope)
{
    struct ctf_scope_struct* scope_struct = 
        container_of(scope, typeof(*scope_struct), base.base);

    scope_type_container_destroy(&scope_struct->type_container);
    
    free(scope_struct);
}

static struct ctf_scope_operations scope_struct_ops =
{
    .destroy_scope          = scope_struct_ops_destroy_scope,
    .get_type               = scope_struct_ops_get_type,
    .add_type               = scope_struct_ops_add_type,
    .destroy_type           = scope_struct_ops_destroy_type,
    .store_type             = scope_struct_ops_store_type,
    .destroy_stored_type    = scope_struct_ops_destroy_stored_type,
    .find_type              = scope_struct_ops_find_type,
    .add_scope_connected    = scope_struct_ops_add_scope_connected,
    .get_type_connected     = scope_connected_ops_get_type_connected,
};

static struct ctf_scope_operations scope_variant_ops =
{
    .destroy_scope          = scope_struct_ops_destroy_scope,
    .get_type               = scope_variant_ops_get_type,
    .add_type               = scope_struct_ops_add_type,
    .destroy_type           = scope_struct_ops_destroy_type,
    .store_type             = scope_struct_ops_store_type,
    .destroy_stored_type    = scope_struct_ops_destroy_stored_type,
    .find_type              = scope_struct_ops_find_type,
    .add_scope_connected    = scope_struct_ops_add_scope_connected,
    .get_type_connected     = scope_connected_ops_get_type_connected
};

struct ctf_scope_connected* ctf_scope_struct_create(
    struct ctf_type* type, int is_struct)
{
    struct ctf_scope_struct* scope_struct = malloc(sizeof(*scope_struct));
    if(scope_struct == NULL)
    {
        ctf_err("Failed to allocate structure for %s scope.",
            is_struct ? "structure" : "variant");
        return NULL;
    }
    
    ctf_scope_connected_init(&scope_struct->base, type);
    
    scope_type_container_init(&scope_struct->type_container);
    
    scope_struct->base.base.scope_ops = is_struct
        ? &scope_struct_ops : &scope_variant_ops;
    
    return &scope_struct->base;
}

/* Operations for int and enum scopes */
static enum ctf_scope_type scope_int_ops_get_type(
    struct ctf_scope* scope)
{
    (void)scope;
    return ctf_scope_type_int;
}

static enum ctf_scope_type scope_enum_ops_get_type(
    struct ctf_scope* scope)
{
    (void)scope;
    return ctf_scope_type_enum;
}

static void scope_int_ops_destroy_scope(struct ctf_scope* scope)
{
    struct ctf_scope_connected* scope_connected = 
        container_of(scope, typeof(*scope_connected), base);

    free(scope_connected);
}

static struct ctf_scope_operations scope_int_ops =
{
    .destroy_scope      = scope_int_ops_destroy_scope,
    .get_type           = scope_int_ops_get_type,
    .get_type_connected = scope_connected_ops_get_type_connected
};

static struct ctf_scope_operations scope_enum_ops =
{
    .destroy_scope      = scope_int_ops_destroy_scope,
    .get_type           = scope_enum_ops_get_type,
    .get_type_connected = scope_connected_ops_get_type_connected
};


struct ctf_scope_connected* ctf_scope_int_create(
    struct ctf_type* type, int is_int)
{
    struct ctf_scope_connected* scope_int =
        malloc(sizeof(*scope_int));
    if(scope_int == NULL)
    {
        ctf_err("Failed to allocate structure for %s scope.",
            is_int ? "integer" : "enumeration");
        return NULL;
    }
    
    ctf_scope_connected_init(scope_int, type);
    
    scope_int->base.scope_ops = is_int
        ? &scope_int_ops : &scope_enum_ops;
    
    return scope_int;
}

/* Operations for top scope */

static const char* scope_top_ops_get_name(
    struct ctf_scope* scope)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);

    return scope_top->name;
}

static enum ctf_scope_type scope_top_ops_get_type(
    struct ctf_scope* scope)
{
    (void)scope;
    return ctf_scope_type_top;
}

static int scope_top_ops_add_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);

    scope_type_container_add_type(&scope_top->type_container, type);
    
    return 0;
}

static void scope_top_ops_destroy_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);

    scope_type_container_destroy_type(&scope_top->type_container, type);
}


static int scope_top_ops_store_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);

    scope_type_container_store_type(&scope_top->type_container, type);
    
    return 0;
}

static void scope_top_ops_destroy_stored_type(struct ctf_scope* scope,
    struct ctf_type* type)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);

    scope_type_container_destroy_stored_type(&scope_top->type_container, type);
}

static struct ctf_type* scope_top_ops_find_type(struct ctf_scope* scope,
    const char* type_name)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);

    return scope_type_container_find_type(&scope_top->type_container,
        type_name);
}

static struct ctf_scope* scope_top_ops_add_scope_connected(
    struct ctf_scope* scope, struct ctf_type* type)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);
    
    struct ctf_scope_connected* scope_connected =
        ctf_scope_connected_create(type);
    
    if(scope_connected == NULL) return NULL;
    
    scope_type_container_add_scope_connected(&scope_top->type_container,
        scope_connected);
    
    return &scope_connected->base;
}

static void scope_top_ops_destroy_scope(struct ctf_scope* scope)
{
    struct ctf_scope_top* scope_top =
        container_of(scope, typeof(*scope_top), base);

    scope_type_container_destroy(&scope_top->type_container);
    
    free(scope_top);
}

static struct ctf_scope_top_operations scope_top_ops =
{
    .base =
    {
        .destroy_scope          = scope_top_ops_destroy_scope,
        .get_type               = scope_top_ops_get_type,
        .add_type               = scope_top_ops_add_type,
        .destroy_type           = scope_top_ops_destroy_type,
        .store_type             = scope_top_ops_store_type,
        .destroy_stored_type    = scope_top_ops_destroy_stored_type,
        .find_type              = scope_top_ops_find_type,
        .add_scope_connected    = scope_top_ops_add_scope_connected,
    },
    .get_name  = scope_top_ops_get_name,
};

struct ctf_scope_top* ctf_scope_top_create(
    struct ctf_scope* parent_scope, int name_index)
{
    struct ctf_scope_top* scope_top = malloc(sizeof(*scope_top));
    if(scope_top == NULL)
    {
        ctf_err("Failed to allocate top-levelroot scope.");
        return NULL;
    }
    
    ctf_scope_init(&scope_top->base, parent_scope);
    scope_type_container_init(&scope_top->type_container);
    
    scope_top->name = top_scope_names[name_index];
    
    scope_top->base.scope_ops = &scope_top_ops.base;
    
    return scope_top;
}
