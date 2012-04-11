#include "ctf_ast.h"

#include <malloc.h> /* malloc */
#include "ctf_meta_internal.h" /* ctf_err, container_of */
#include <assert.h> /* assert */

/* Initialize basic scope structure(for internal use) */
static void ctf_parse_scope_init(struct ctf_parse_scope* scope)
{
    linked_list_head_init(&scope->statements);
}

/* Initialize basic statement structure(for internal use) */
static void ctf_parse_statement_init(struct ctf_parse_statement* statement)
{
    linked_list_elem_init(&statement->list_elem);
    statement->scope_parent = NULL;
}

void ctf_parse_scope_destroy(struct ctf_parse_scope* scope)
{
    if(scope == NULL) return;

    while(!linked_list_is_empty(&scope->statements))
    {
        struct ctf_parse_statement* statement;
        linked_list_remove_first_entry(&scope->statements, statement,
            list_elem);

        ctf_parse_statement_destroy(statement);
    }
    
    if(scope->scope_ops && scope->scope_ops->destroy)
        scope->scope_ops->destroy(scope);
}

int ctf_ast_visitor_visit_scope(struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope* scope)
{
    return scope->scope_ops->visit(scope, visitor);
}

enum ctf_parse_scope_type ctf_parse_scope_get_type(
    struct ctf_parse_scope* scope)
{
    return scope->scope_ops->get_type(scope);
}

void ctf_parse_statement_destroy(struct ctf_parse_statement* statement)
{
    if(statement && statement->statement_ops
        && statement->statement_ops->destroy)
        statement->statement_ops->destroy(statement);
}

int ctf_ast_visitor_visit_statement(struct ctf_ast_visitor* visitor,
    struct ctf_parse_statement* statement)
{
    return statement->statement_ops->visit(statement, visitor);
}


enum ctf_parse_statement_type ctf_parse_statement_get_type(
    struct ctf_parse_statement* statement)
{
    return statement->statement_ops->get_type(statement);
}

void ctf_parse_scope_add_statement(struct ctf_parse_scope* scope,
    struct ctf_parse_statement* statement)
{
    assert(statement->scope_parent == NULL);
    
    linked_list_add_elem(&scope->statements, &statement->list_elem);
    
    statement->scope_parent = scope;
}

/* Root scope */
static enum ctf_parse_scope_type scope_root_ops_get_type(
    struct ctf_parse_scope* scope)
{
    (void)scope;
    
    return ctf_parse_scope_type_root;
}
static void scope_root_ops_destroy(struct ctf_parse_scope* scope)
{
    struct ctf_parse_scope_root* scope_root = container_of(scope,
        typeof(*scope_root), base);
    
    free(scope_root);
}

static int scope_root_ops_visit(struct ctf_parse_scope* scope,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_scope_root* scope_root = container_of(scope,
        typeof(*scope_root), base);
    
    return visitor->visitor_ops->visit_scope_root(visitor, scope_root);
}

static struct ctf_parse_scope_operations scope_root_ops =
{
    .get_type = scope_root_ops_get_type,
    .destroy = scope_root_ops_destroy,
    .visit = scope_root_ops_visit,
};

struct ctf_parse_scope_root* ctf_parse_scope_root_create(void)
{
    struct ctf_parse_scope_root* scope_root = malloc(sizeof(*scope_root));
    if(scope_root == NULL)
    {
        ctf_err("Failed to allocate root scope node in AST.");
        return NULL;
    }
    
    ctf_parse_scope_init(&scope_root->base);
    
    scope_root->base.scope_ops = &scope_root_ops;
    
    return scope_root;
}

/* Top scope declaration */
static enum ctf_parse_scope_type scope_top_ops_get_type(
    struct ctf_parse_scope* scope)
{
    (void)scope;
    
    return ctf_parse_scope_type_top;
}
static void scope_top_ops_destroy(struct ctf_parse_scope* scope)
{
    struct ctf_parse_scope_top* scope_top = container_of(scope,
        typeof(*scope_top), base);
    
    free(scope_top);
}

static int scope_top_ops_visit(struct ctf_parse_scope* scope,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_scope_top* scope_top = container_of(scope,
        typeof(*scope_top), base);
    
    return visitor->visitor_ops->visit_scope_top(visitor, scope_top);
}

static struct ctf_parse_scope_operations scope_top_ops =
{
    .get_type = scope_top_ops_get_type,
    .destroy = scope_top_ops_destroy,
    .visit = scope_top_ops_visit,
};

struct ctf_parse_scope_top* ctf_parse_scope_top_create(void)
{
    struct ctf_parse_scope_top* scope_top = malloc(sizeof(*scope_top));
    if(scope_top == NULL)
    {
        ctf_err("Failed to allocate top-level scope node in AST.");
        return NULL;
    }
    
    ctf_parse_scope_init(&scope_top->base);
    scope_top->scope_top_decl = NULL;
    
    scope_top->base.scope_ops = &scope_top_ops;
    
    return scope_top;
}


static enum ctf_parse_statement_type scope_top_decl_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_top_scope_decl;
}
static void scope_top_decl_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_scope_top_decl* scope_top_decl = container_of(
        statement, typeof(*scope_top_decl), base);
    
    if(scope_top_decl->scope_top)
        ctf_parse_scope_destroy(&scope_top_decl->scope_top->base);
    
    if(scope_top_decl->scope_name)
        free(scope_top_decl->scope_name);
    
    free(scope_top_decl);
}

static int scope_top_decl_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_scope_top_decl* scope_top_decl = container_of(statement,
        typeof(*scope_top_decl), base);
    
    return visitor->visitor_ops->visit_scope_top_decl(visitor, scope_top_decl);
}


static struct ctf_parse_statement_operations scope_top_decl_ops =
{
    .get_type = scope_top_decl_ops_get_type,
    .destroy = scope_top_decl_ops_destroy,
    .visit = scope_top_decl_ops_visit,
};


struct ctf_parse_scope_top_decl* ctf_parse_scope_top_decl_create(void)
{
    struct ctf_parse_scope_top_decl* scope_top_decl =
        malloc(sizeof(*scope_top_decl));
    if(scope_top_decl == NULL)
    {
        ctf_err("Failed to allocate top-level scope declaration node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&scope_top_decl->base);
    scope_top_decl->scope_top = NULL;
    scope_top_decl->scope_name = NULL;
    
    scope_top_decl->base.statement_ops = &scope_top_decl_ops;
    
    return scope_top_decl;
}



void ctf_parse_scope_top_connect(struct ctf_parse_scope_top* scope_top,
    struct ctf_parse_scope_top_decl* scope_top_decl)
{
    scope_top->scope_top_decl = scope_top_decl;
    scope_top_decl->scope_top = scope_top;
}

/* Initialize basic type specification structure */
static void ctf_parse_type_spec_init(struct ctf_parse_type_spec* type_spec)
{
    (void) type_spec;
    return;
}

void ctf_parse_type_spec_destroy(struct ctf_parse_type_spec* type_spec)
{
    if(type_spec && type_spec->type_spec_ops
        && type_spec->type_spec_ops->destroy)
        type_spec->type_spec_ops->destroy(type_spec);
}

int ctf_ast_visitor_visit_type_spec(struct ctf_ast_visitor* visitor,
    struct ctf_parse_type_spec* type_spec)
{
    return type_spec->type_spec_ops->visit(type_spec, visitor);
}

enum ctf_parse_type_spec_type ctf_parse_type_spec_get_type(
	struct ctf_parse_type_spec* type_spec)
{
    return type_spec->type_spec_ops->get_type(type_spec);
}

/* Structure specification and its scope */
static enum ctf_parse_scope_type scope_struct_ops_get_type(
    struct ctf_parse_scope* scope)
{
    (void)scope;
    
    return ctf_parse_scope_type_struct;
}
static void scope_struct_ops_destroy(struct ctf_parse_scope* scope)
{
    struct ctf_parse_scope_struct* scope_struct = container_of(scope,
        typeof(*scope_struct), base);
    
    free(scope_struct);
}

static int scope_struct_ops_visit(struct ctf_parse_scope* scope,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_scope_struct* scope_struct = container_of(scope,
        typeof(*scope_struct), base);
    
    return visitor->visitor_ops->visit_scope_struct(visitor, scope_struct);
}


static struct ctf_parse_scope_operations scope_struct_ops =
{
    .get_type = scope_struct_ops_get_type,
    .destroy = scope_struct_ops_destroy,
    .visit = scope_struct_ops_visit,
};

struct ctf_parse_scope_struct* ctf_parse_scope_struct_create(void)
{
    struct ctf_parse_scope_struct* scope_struct = malloc(sizeof(*scope_struct));
    if(scope_struct == NULL)
    {
        ctf_err("Failed to allocate structure scope node in AST.");
        return NULL;
    }
    
    ctf_parse_scope_init(&scope_struct->base);
    scope_struct->struct_spec = NULL;
    
    scope_struct->base.scope_ops = &scope_struct_ops;
    
    return scope_struct;
}

static enum ctf_parse_type_spec_type struct_spec_ops_get_type(
    struct ctf_parse_type_spec* type_spec)
{
    (void)type_spec;
    
    return ctf_parse_type_spec_type_struct;
}
static void struct_spec_ops_destroy(
    struct ctf_parse_type_spec* type_spec)
{
    struct ctf_parse_struct_spec* struct_spec = container_of(
        type_spec, typeof(*struct_spec), base);
    
    if(struct_spec->scope_struct)
        ctf_parse_scope_destroy(&struct_spec->scope_struct->base);
    
    if(struct_spec->struct_name)
        free(struct_spec->struct_name);
    
    free(struct_spec);
}

static int struct_spec_ops_visit(struct ctf_parse_type_spec* type_spec,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_struct_spec* struct_spec = container_of(type_spec,
        typeof(*struct_spec), base);
    
    return visitor->visitor_ops->visit_struct_spec(visitor, struct_spec);
}


static struct ctf_parse_type_spec_operations struct_spec_ops =
{
    .get_type = struct_spec_ops_get_type,
    .destroy = struct_spec_ops_destroy,
    .visit = struct_spec_ops_visit,
};

struct ctf_parse_struct_spec* ctf_parse_struct_spec_create(void)
{
    struct ctf_parse_struct_spec* struct_spec = malloc(sizeof(*struct_spec));
    if(struct_spec == NULL)
    {
        ctf_err("Failed to allocate structure spec node in AST.");
        return NULL;
    }
    
    ctf_parse_type_spec_init(&struct_spec->base);

    struct_spec->scope_struct = NULL;
    struct_spec->struct_name = NULL;
    struct_spec->align = -1;
    
    struct_spec->base.type_spec_ops = &struct_spec_ops;
    
    return struct_spec;
}

void ctf_parse_scope_struct_connect(
	struct ctf_parse_scope_struct* scope_struct,
	struct ctf_parse_struct_spec* struct_spec)
{
    assert(scope_struct->struct_spec == NULL);
    
    scope_struct->struct_spec = struct_spec;
    struct_spec->scope_struct = scope_struct;
}

/* Integer specification and its scope */
static enum ctf_parse_scope_type scope_int_ops_get_type(
    struct ctf_parse_scope* scope)
{
    (void)scope;
    
    return ctf_parse_scope_type_integer;
}
static void scope_int_ops_destroy(struct ctf_parse_scope* scope)
{
    struct ctf_parse_scope_int* scope_int = container_of(scope,
        typeof(*scope_int), base);
    
    free(scope_int);
}

static int scope_int_ops_visit(struct ctf_parse_scope* scope,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_scope_int* scope_int = container_of(scope,
        typeof(*scope_int), base);
    
    return visitor->visitor_ops->visit_scope_int(visitor, scope_int);
}


static struct ctf_parse_scope_operations scope_int_ops =
{
    .get_type = scope_int_ops_get_type,
    .destroy = scope_int_ops_destroy,
    .visit = scope_int_ops_visit,
};

struct ctf_parse_scope_int* ctf_parse_scope_int_create(void)
{
    struct ctf_parse_scope_int* scope_int = malloc(sizeof(*scope_int));
    if(scope_int == NULL)
    {
        ctf_err("Failed to allocate structure scope node in AST.");
        return NULL;
    }
    
    ctf_parse_scope_init(&scope_int->base);
    scope_int->int_spec = NULL;
    
    scope_int->base.scope_ops = &scope_int_ops;
    
    return scope_int;
}

static enum ctf_parse_type_spec_type int_spec_ops_get_type(
    struct ctf_parse_type_spec* type_spec)
{
    (void)type_spec;
    
    return ctf_parse_type_spec_type_integer;
}
static void int_spec_ops_destroy(
    struct ctf_parse_type_spec* type_spec)
{
    struct ctf_parse_int_spec* int_spec = container_of(
        type_spec, typeof(*int_spec), base);
    
    ctf_parse_scope_destroy(&int_spec->scope_int->base);
    
    free(int_spec);
}

static int int_spec_ops_visit(struct ctf_parse_type_spec* type_spec,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_int_spec* int_spec = container_of(type_spec,
        typeof(*int_spec), base);
    
    return visitor->visitor_ops->visit_int_spec(visitor, int_spec);
}


static struct ctf_parse_type_spec_operations int_spec_ops =
{
    .get_type = int_spec_ops_get_type,
    .destroy = int_spec_ops_destroy,
    .visit = int_spec_ops_visit,
};

struct ctf_parse_int_spec* ctf_parse_int_spec_create(void)
{
    struct ctf_parse_int_spec* int_spec = malloc(sizeof(*int_spec));
    if(int_spec == NULL)
    {
        ctf_err("Failed to allocate structure spec node in AST.");
        return NULL;
    }
    
    ctf_parse_type_spec_init(&int_spec->base);

    int_spec->scope_int = NULL;
    
    int_spec->base.type_spec_ops = &int_spec_ops;
    
    return int_spec;
}

void ctf_parse_scope_int_connect(
	struct ctf_parse_scope_int* scope_int,
	struct ctf_parse_int_spec* int_spec)
{
    assert(scope_int->int_spec == NULL);
    
    scope_int->int_spec = int_spec;
    int_spec->scope_int = scope_int;
}


/* Struct declaration */
static enum ctf_parse_statement_type struct_decl_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_struct_decl;
}
static void struct_decl_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_struct_decl* struct_decl = container_of(
        statement, typeof(*struct_decl), base);
    
    if(struct_decl->struct_spec)
        ctf_parse_type_spec_destroy(&struct_decl->struct_spec->base);
    
    free(struct_decl);
}

static int struct_decl_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_struct_decl* struct_decl = container_of(statement,
        typeof(*struct_decl), base);
    
    return visitor->visitor_ops->visit_struct_decl(visitor, struct_decl);
}


static struct ctf_parse_statement_operations struct_decl_ops =
{
    .get_type = struct_decl_ops_get_type,
    .destroy = struct_decl_ops_destroy,
    .visit = struct_decl_ops_visit,
};


struct ctf_parse_struct_decl* ctf_parse_struct_decl_create(void)
{
    struct ctf_parse_struct_decl* struct_decl =
        malloc(sizeof(*struct_decl));
    if(struct_decl == NULL)
    {
        ctf_err("Failed to allocate structure declaration node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&struct_decl->base);
    struct_decl->struct_spec = NULL;
    
    struct_decl->base.statement_ops = &struct_decl_ops;
    
    return struct_decl;
}

/* Post modifiers for type. */
static void ctf_parse_type_post_mod_init(struct ctf_parse_type_post_mod* type_post_mod)
{
    linked_list_elem_init(&type_post_mod->list_elem);
    return;
}

void ctf_parse_type_post_mod_destroy(struct ctf_parse_type_post_mod* type_post_mod)
{
    if(type_post_mod && type_post_mod->type_post_mod_ops
        && type_post_mod->type_post_mod_ops->destroy)
        type_post_mod->type_post_mod_ops->destroy(type_post_mod);
}

int ctf_ast_visitor_visit_type_post_mod(struct ctf_ast_visitor* visitor,
    struct ctf_parse_type_post_mod* type_post_mod)
{
    return type_post_mod->type_post_mod_ops->visit(type_post_mod, visitor);
}

enum ctf_parse_type_post_mod_type ctf_parse_type_post_mod_get_type(
	struct ctf_parse_type_post_mod* type_post_mod)
{
    return type_post_mod->type_post_mod_ops->get_type(type_post_mod);
}

/* List of type post modifiers. */
struct ctf_parse_type_post_mod_list*
ctf_parse_type_post_mod_list_create(void)
{
	struct ctf_parse_type_post_mod_list* type_post_mod_list =
		malloc(sizeof(*type_post_mod_list));
	if(type_post_mod_list == NULL)
	{
		ctf_err("Failed to allocate list of type post specifiers.");
	}
	
	linked_list_head_init(&type_post_mod_list->mods);
	
	return type_post_mod_list;
}

void ctf_parse_type_post_mod_list_destroy(
	struct ctf_parse_type_post_mod_list* type_post_mod_list)
{
	while(!linked_list_is_empty(&type_post_mod_list->mods))
	{
		struct ctf_parse_type_post_mod* type_post_mod;
		linked_list_remove_first_entry(&type_post_mod_list->mods,
			type_post_mod, list_elem);
		ctf_parse_type_post_mod_destroy(type_post_mod);
	}
	free(type_post_mod_list);
}

/* Add type post modifier into list. */
void ctf_parse_type_post_mod_list_add_mod(
	struct ctf_parse_type_post_mod_list* type_post_mod_list,
	struct ctf_parse_type_post_mod* type_post_mod)
{
	linked_list_add_elem(&type_post_mod_list->mods,
		&type_post_mod->list_elem);
}

/* Array type post specifier */
static enum ctf_parse_type_post_mod_type type_post_mod_array_ops_get_type(
    struct ctf_parse_type_post_mod* type_post_mod)
{
    (void)type_post_mod;
    
    return ctf_parse_type_post_mod_type_array;
}
static void type_post_mod_array_ops_destroy(
    struct ctf_parse_type_post_mod* type_post_mod)
{
    struct ctf_parse_type_post_mod_array* type_post_mod_array =
		container_of(type_post_mod, typeof(*type_post_mod_array), base);
    
    if(type_post_mod_array->array_len)
        free(type_post_mod_array->array_len);
    
    free(type_post_mod_array);
}

static int type_post_mod_array_ops_visit(
	struct ctf_parse_type_post_mod* type_post_mod,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_type_post_mod_array* type_post_mod_array =
		container_of(type_post_mod, typeof(*type_post_mod_array), base);
    
    return visitor->visitor_ops->visit_type_post_mod_array(visitor,
		type_post_mod_array);
}


static struct ctf_parse_type_post_mod_operations type_post_mod_array_ops =
{
    .get_type = type_post_mod_array_ops_get_type,
    .destroy = type_post_mod_array_ops_destroy,
    .visit = type_post_mod_array_ops_visit,
};

struct ctf_parse_type_post_mod_array* ctf_parse_type_post_mod_array_create(void)
{
    struct ctf_parse_type_post_mod_array* type_post_mod_array =
		malloc(sizeof(*type_post_mod_array));
    if(type_post_mod_array == NULL)
    {
        ctf_err("Failed to allocate array type post specifier in AST.");
        return NULL;
    }
    
    ctf_parse_type_post_mod_init(&type_post_mod_array->base);

    type_post_mod_array->array_len = NULL;
    
    type_post_mod_array->base.type_post_mod_ops = &type_post_mod_array_ops;
    
    return type_post_mod_array;
}

/* Sequence type post modifier */
static enum ctf_parse_type_post_mod_type type_post_mod_sequence_ops_get_type(
    struct ctf_parse_type_post_mod* type_post_mod)
{
    (void)type_post_mod;
    
    return ctf_parse_type_post_mod_type_sequence;
}
static void type_post_mod_sequence_ops_destroy(
    struct ctf_parse_type_post_mod* type_post_mod)
{
    struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence =
		container_of(type_post_mod, typeof(*type_post_mod_sequence), base);
    
    if(type_post_mod_sequence->sequence_len)
        free(type_post_mod_sequence->sequence_len);
    
    free(type_post_mod_sequence);
}

static int type_post_mod_sequence_ops_visit(
	struct ctf_parse_type_post_mod* type_post_mod,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence =
		container_of(type_post_mod, typeof(*type_post_mod_sequence), base);
    
    return visitor->visitor_ops->visit_type_post_mod_sequence(visitor,
		type_post_mod_sequence);
}


static struct ctf_parse_type_post_mod_operations type_post_mod_sequence_ops =
{
    .get_type = type_post_mod_sequence_ops_get_type,
    .destroy = type_post_mod_sequence_ops_destroy,
    .visit = type_post_mod_sequence_ops_visit,
};

struct ctf_parse_type_post_mod_sequence* ctf_parse_type_post_mod_sequence_create(void)
{
    struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence =
		malloc(sizeof(*type_post_mod_sequence));
    if(type_post_mod_sequence == NULL)
    {
        ctf_err("Failed to allocate sequence type post specifier in AST.");
        return NULL;
    }
    
    ctf_parse_type_post_mod_init(&type_post_mod_sequence->base);

    type_post_mod_sequence->sequence_len = NULL;
    
    type_post_mod_sequence->base.type_post_mod_ops = &type_post_mod_sequence_ops;
    
    return type_post_mod_sequence;
}
/* Field declaration */
static enum ctf_parse_statement_type field_decl_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_field_decl;
}
static void field_decl_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_field_decl* field_decl = container_of(
        statement, typeof(*field_decl), base);
    
    if(field_decl->type_spec)
        ctf_parse_type_spec_destroy(field_decl->type_spec);
    if(field_decl->field_name)
        free(field_decl->field_name);
    if(field_decl->type_post_mod_list)
		ctf_parse_type_post_mod_list_destroy(field_decl->type_post_mod_list);
    
    free(field_decl);
}

static int field_decl_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_field_decl* field_decl = container_of(statement,
        typeof(*field_decl), base);
    
    return visitor->visitor_ops->visit_field_decl(visitor, field_decl);
}


static struct ctf_parse_statement_operations field_decl_ops =
{
    .get_type = field_decl_ops_get_type,
    .destroy = field_decl_ops_destroy,
    .visit = field_decl_ops_visit,
};
struct ctf_parse_field_decl* ctf_parse_field_decl_create(void)
{
    struct ctf_parse_field_decl* field_decl =
        malloc(sizeof(*field_decl));
    if(field_decl == NULL)
    {
        ctf_err("Failed to allocate structure declaration node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&field_decl->base);
    field_decl->type_spec = NULL;
    field_decl->field_name = NULL;
    field_decl->type_post_mod_list = NULL;
    
    field_decl->base.statement_ops = &field_decl_ops;
    
    return field_decl;
}

/* Type specification using type identificator */
static enum ctf_parse_type_spec_type type_spec_id_ops_get_type(
    struct ctf_parse_type_spec* type_spec)
{
    (void)type_spec;
    
    return ctf_parse_type_spec_type_id;
}
static void type_spec_id_ops_destroy(
    struct ctf_parse_type_spec* type_spec)
{
    struct ctf_parse_type_spec_id* type_spec_id = container_of(
        type_spec, typeof(*type_spec_id), base);
    
    if(type_spec_id->type_name)
        free(type_spec_id->type_name);
    
    free(type_spec_id);
}

static int type_spec_id_ops_visit(struct ctf_parse_type_spec* type_spec,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_type_spec_id* type_spec_id = container_of(type_spec,
        typeof(*type_spec_id), base);
    
    return visitor->visitor_ops->visit_type_spec_id(visitor, type_spec_id);
}


static struct ctf_parse_type_spec_operations type_spec_id_ops =
{
    .get_type = type_spec_id_ops_get_type,
    .destroy = type_spec_id_ops_destroy,
    .visit = type_spec_id_ops_visit,
};

struct ctf_parse_type_spec_id* ctf_parse_type_spec_id_create(void)
{
    struct ctf_parse_type_spec_id* type_spec_id = malloc(sizeof(*type_spec_id));
    if(type_spec_id == NULL)
    {
        ctf_err("Failed to allocate structure spec node in AST.");
        return NULL;
    }
    
    ctf_parse_type_spec_init(&type_spec_id->base);

    type_spec_id->type_name = NULL;
    
    type_spec_id->base.type_spec_ops = &type_spec_id_ops;
    
    return type_spec_id;
}

/* Parameter definition */
static enum ctf_parse_statement_type param_def_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_parameter_def;
}
static void param_def_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_param_def* param_def = container_of(
        statement, typeof(*param_def), base);
    
    if(param_def->param_name)
        free(param_def->param_name);
    
    if(param_def->param_value)
        free(param_def->param_value);
    
    free(param_def);
}

static int param_def_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_param_def* param_def = container_of(statement,
        typeof(*param_def), base);
    
    return visitor->visitor_ops->visit_param_def(visitor, param_def);
}


static struct ctf_parse_statement_operations param_def_ops =
{
    .get_type = param_def_ops_get_type,
    .destroy = param_def_ops_destroy,
    .visit = param_def_ops_visit,
};


struct ctf_parse_param_def* ctf_parse_param_def_create(void)
{
    struct ctf_parse_param_def* param_def =
        malloc(sizeof(*param_def));
    if(param_def == NULL)
    {
        ctf_err("Failed to allocate parameter definition node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&param_def->base);
    param_def->param_name = NULL;
    param_def->param_value = NULL;
    
    param_def->base.statement_ops = &param_def_ops;
    
    return param_def;
}

/* Type assignment definition */
static enum ctf_parse_statement_type type_assignment_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_type_assignment;
}
static void type_assignment_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_type_assignment* type_assignment = container_of(
        statement, typeof(*type_assignment), base);
    
    if(type_assignment->tag)
        free(type_assignment->tag);
    
    if(type_assignment->type_spec)
        ctf_parse_type_spec_destroy(type_assignment->type_spec);
    
    free(type_assignment);
}

static int type_assignment_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_type_assignment* type_assignment = container_of(statement,
        typeof(*type_assignment), base);
    
    return visitor->visitor_ops->visit_type_assignment(visitor, type_assignment);
}


static struct ctf_parse_statement_operations type_assignment_ops =
{
    .get_type = type_assignment_ops_get_type,
    .destroy = type_assignment_ops_destroy,
    .visit = type_assignment_ops_visit,
};


struct ctf_parse_type_assignment* ctf_parse_type_assignment_create(void)
{
    struct ctf_parse_type_assignment* type_assignment =
        malloc(sizeof(*type_assignment));
    if(type_assignment == NULL)
    {
        ctf_err("Failed to allocate type assignment node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&type_assignment->base);
    type_assignment->tag = NULL;
    type_assignment->type_spec = NULL;
    
    type_assignment->base.statement_ops = &type_assignment_ops;
    
    return type_assignment;
}

/* Typedef declaration */

static enum ctf_parse_statement_type typedef_decl_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_typedef_decl;
}
static void typedef_decl_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_typedef_decl* typedef_decl = container_of(
        statement, typeof(*typedef_decl), base);
    
    if(typedef_decl->type_name)
        free(typedef_decl->type_name);
    
    if(typedef_decl->type_spec_base)
        ctf_parse_type_spec_destroy(typedef_decl->type_spec_base);
    
    if(typedef_decl->type_post_mod_list)
        ctf_parse_type_post_mod_list_destroy(typedef_decl->type_post_mod_list);
    
    free(typedef_decl);
}

static int typedef_decl_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_typedef_decl* typedef_decl = container_of(statement,
        typeof(*typedef_decl), base);
    
    return visitor->visitor_ops->visit_typedef_decl(visitor, typedef_decl);
}


static struct ctf_parse_statement_operations typedef_decl_ops =
{
    .get_type = typedef_decl_ops_get_type,
    .destroy = typedef_decl_ops_destroy,
    .visit = typedef_decl_ops_visit,
};


struct ctf_parse_typedef_decl* ctf_parse_typedef_decl_create(void)
{
    struct ctf_parse_typedef_decl* typedef_decl =
        malloc(sizeof(*typedef_decl));
    if(typedef_decl == NULL)
    {
        ctf_err("Failed to allocate type assignment node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&typedef_decl->base);
    typedef_decl->type_name = NULL;
    typedef_decl->type_spec_base = NULL;
    typedef_decl->type_post_mod_list = NULL;
    
    typedef_decl->base.statement_ops = &typedef_decl_ops;
    
    return typedef_decl;
}

/* Initialize basic type of enumeration value definition */
static void ctf_parse_enum_value_init(struct ctf_parse_enum_value* enum_value)
{
    linked_list_elem_init(&enum_value->list_elem);
}

void ctf_parse_enum_value_destroy(struct ctf_parse_enum_value* enum_value)
{
    if(enum_value && enum_value->enum_value_ops
        && enum_value->enum_value_ops->destroy)
        enum_value->enum_value_ops->destroy(enum_value);
}

int ctf_ast_visitor_visit_enum_value(struct ctf_ast_visitor* visitor,
    struct ctf_parse_enum_value* enum_value)
{
    return enum_value->enum_value_ops->visit(enum_value, visitor);
}

enum ctf_parse_enum_value_type ctf_parse_enum_value_get_type(
	struct ctf_parse_enum_value* enum_value)
{
    return enum_value->enum_value_ops->get_type(enum_value);
}

/* Simple definitions of enumerations value */
static enum ctf_parse_enum_value_type enum_value_simple_ops_get_type(
    struct ctf_parse_enum_value* enum_value)
{
    (void)enum_value;
    
    return ctf_parse_enum_value_type_simple;
}
static void enum_value_simple_ops_destroy(
    struct ctf_parse_enum_value* enum_value)
{
    struct ctf_parse_enum_value_simple* enum_value_simple = container_of(
        enum_value, typeof(*enum_value_simple), base);
    
    if(enum_value_simple->val_name)
       free(enum_value_simple->val_name);
    
    free(enum_value_simple);
}

static int enum_value_simple_ops_visit(struct ctf_parse_enum_value* enum_value,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_enum_value_simple* enum_value_simple = container_of(enum_value,
        typeof(*enum_value_simple), base);
    
    return visitor->visitor_ops->visit_enum_value_simple(visitor, enum_value_simple);
}


static struct ctf_parse_enum_value_operations enum_value_simple_ops =
{
    .get_type = enum_value_simple_ops_get_type,
    .destroy = enum_value_simple_ops_destroy,
    .visit = enum_value_simple_ops_visit,
};


struct ctf_parse_enum_value_simple* ctf_parse_enum_value_simple_create(void)
{
    struct ctf_parse_enum_value_simple* enum_value_simple =
        malloc(sizeof(*enum_value_simple));
    if(enum_value_simple == NULL)
    {
        ctf_err("Failed to allocate type assignment node in AST.");
        return NULL;
    }
    
    ctf_parse_enum_value_init(&enum_value_simple->base);
    
    enum_value_simple->val_name = NULL;
    
    enum_value_simple->base.enum_value_ops = &enum_value_simple_ops;
    
    return enum_value_simple;
}

/* Definition of enumerations value used presize integer value */
static enum ctf_parse_enum_value_type enum_value_presize_ops_get_type(
    struct ctf_parse_enum_value* enum_value)
{
    (void)enum_value;
    
    return ctf_parse_enum_value_type_presize;
}
static void enum_value_presize_ops_destroy(
    struct ctf_parse_enum_value* enum_value)
{
    struct ctf_parse_enum_value_presize* enum_value_presize = container_of(
        enum_value, typeof(*enum_value_presize), base);
    
    if(enum_value_presize->val_name)
       free(enum_value_presize->val_name);
    
    if(enum_value_presize->int_value)
       free(enum_value_presize->int_value);
    
    free(enum_value_presize);
}

static int enum_value_presize_ops_visit(struct ctf_parse_enum_value* enum_value,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_enum_value_presize* enum_value_presize = container_of(enum_value,
        typeof(*enum_value_presize), base);
    
    return visitor->visitor_ops->visit_enum_value_presize(visitor, enum_value_presize);
}


static struct ctf_parse_enum_value_operations enum_value_presize_ops =
{
    .get_type = enum_value_presize_ops_get_type,
    .destroy = enum_value_presize_ops_destroy,
    .visit = enum_value_presize_ops_visit,
};


struct ctf_parse_enum_value_presize* ctf_parse_enum_value_presize_create(void)
{
    struct ctf_parse_enum_value_presize* enum_value_presize =
        malloc(sizeof(*enum_value_presize));
    if(enum_value_presize == NULL)
    {
        ctf_err("Failed to allocate type assignment node in AST.");
        return NULL;
    }
    
    ctf_parse_enum_value_init(&enum_value_presize->base);
    
    enum_value_presize->val_name = NULL;
    enum_value_presize->int_value = NULL;
    
    enum_value_presize->base.enum_value_ops = &enum_value_presize_ops;
    
    return enum_value_presize;
}

/* Definition of enumerations value used range integer value */
static enum ctf_parse_enum_value_type enum_value_range_ops_get_type(
    struct ctf_parse_enum_value* enum_value)
{
    (void)enum_value;
    
    return ctf_parse_enum_value_type_range;
}
static void enum_value_range_ops_destroy(
    struct ctf_parse_enum_value* enum_value)
{
    struct ctf_parse_enum_value_range* enum_value_range = container_of(
        enum_value, typeof(*enum_value_range), base);
    
    if(enum_value_range->val_name)
       free(enum_value_range->val_name);
    
    if(enum_value_range->int_value_start)
       free(enum_value_range->int_value_start);
    
    if(enum_value_range->int_value_end)
       free(enum_value_range->int_value_end);
    
    free(enum_value_range);
}

static int enum_value_range_ops_visit(struct ctf_parse_enum_value* enum_value,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_enum_value_range* enum_value_range = container_of(enum_value,
        typeof(*enum_value_range), base);
    
    return visitor->visitor_ops->visit_enum_value_range(visitor, enum_value_range);
}


static struct ctf_parse_enum_value_operations enum_value_range_ops =
{
    .get_type = enum_value_range_ops_get_type,
    .destroy = enum_value_range_ops_destroy,
    .visit = enum_value_range_ops_visit,
};


struct ctf_parse_enum_value_range* ctf_parse_enum_value_range_create(void)
{
    struct ctf_parse_enum_value_range* enum_value_range =
        malloc(sizeof(*enum_value_range));
    if(enum_value_range == NULL)
    {
        ctf_err("Failed to allocate type assignment node in AST.");
        return NULL;
    }
    
    ctf_parse_enum_value_init(&enum_value_range->base);
    
    enum_value_range->val_name = NULL;
    enum_value_range->int_value_start = NULL;
    enum_value_range->int_value_end = NULL;
    
    enum_value_range->base.enum_value_ops = &enum_value_range_ops;
    
    return enum_value_range;
}

/* Enumeration specification and its scope */
static enum ctf_parse_type_spec_type enum_spec_ops_get_type(
    struct ctf_parse_type_spec* type_spec)
{
    (void)type_spec;
    
    return ctf_parse_type_spec_type_struct;
}
static void enum_spec_ops_destroy(
    struct ctf_parse_type_spec* type_spec)
{
    struct ctf_parse_enum_spec* enum_spec = container_of(
        type_spec, typeof(*enum_spec), base);
    
    if(enum_spec->scope_enum)
        ctf_parse_scope_destroy(&enum_spec->scope_enum->base);
    
    if(enum_spec->enum_name)
        free(enum_spec->enum_name);
    
    if(enum_spec->type_spec_int)
		ctf_parse_type_spec_destroy(enum_spec->type_spec_int);
    
    free(enum_spec);
}

static int enum_spec_ops_visit(struct ctf_parse_type_spec* type_spec,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_enum_spec* enum_spec = container_of(type_spec,
        typeof(*enum_spec), base);
    
    return visitor->visitor_ops->visit_enum_spec(visitor, enum_spec);
}


static struct ctf_parse_type_spec_operations enum_spec_ops =
{
    .get_type = enum_spec_ops_get_type,
    .destroy = enum_spec_ops_destroy,
    .visit = enum_spec_ops_visit,
};

struct ctf_parse_enum_spec* ctf_parse_enum_spec_create(void)
{
    struct ctf_parse_enum_spec* enum_spec = malloc(sizeof(*enum_spec));
    if(enum_spec == NULL)
    {
        ctf_err("Failed to allocate enumeration spec node in AST.");
        return NULL;
    }
    
    ctf_parse_type_spec_init(&enum_spec->base);

    enum_spec->scope_enum = NULL;
    enum_spec->enum_name = NULL;

	enum_spec->type_spec_int = NULL;
    
    enum_spec->base.type_spec_ops = &enum_spec_ops;
    
    return enum_spec;
}

static enum ctf_parse_scope_type scope_enum_ops_get_type(
    struct ctf_parse_scope* scope)
{
    (void)scope;
    
    return ctf_parse_scope_type_enum;
}
static void scope_enum_ops_destroy(struct ctf_parse_scope* scope)
{
    struct ctf_parse_scope_enum* scope_enum = container_of(scope,
        typeof(*scope_enum), base);
    
    while(!linked_list_is_empty(&scope_enum->values))
    {
		struct ctf_parse_enum_value* enum_value;
		linked_list_remove_first_entry(&scope_enum->values, enum_value, list_elem);
		ctf_parse_enum_value_destroy(enum_value);
    }
    
    free(scope_enum);
}

static int scope_enum_ops_visit(struct ctf_parse_scope* scope,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_scope_enum* scope_enum = container_of(scope,
        typeof(*scope_enum), base);
    
    return visitor->visitor_ops->visit_scope_enum(visitor, scope_enum);
}


static struct ctf_parse_scope_operations scope_enum_ops =
{
    .get_type = scope_enum_ops_get_type,
    .destroy = scope_enum_ops_destroy,
    .visit = scope_enum_ops_visit,
};

struct ctf_parse_scope_enum* ctf_parse_scope_enum_create(void)
{
    struct ctf_parse_scope_enum* scope_enum = malloc(sizeof(*scope_enum));
    if(scope_enum == NULL)
    {
        ctf_err("Failed to allocate structure scope node in AST.");
        return NULL;
    }
    
    ctf_parse_scope_init(&scope_enum->base);
    
    linked_list_head_init(&scope_enum->values);
    
    scope_enum->base.scope_ops = &scope_enum_ops;
    
    return scope_enum;
}

void ctf_parse_scope_enum_connect(
	struct ctf_parse_scope_enum* scope_enum,
	struct ctf_parse_enum_spec* enum_spec)
{
	assert(scope_enum->enum_spec == NULL);
	
	scope_enum->enum_spec = enum_spec;
	enum_spec->scope_enum = scope_enum;
}

/* Add definition of value to the enumeration scope */
void ctf_parse_scope_enum_add_value(
	struct ctf_parse_scope_enum* scope_enum,
	struct ctf_parse_enum_value* value)
{
	linked_list_add_elem(&scope_enum->values, &value->list_elem);
}

/* Enumeration declaration */
static enum ctf_parse_statement_type enum_decl_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_enum_decl;
}
static void enum_decl_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_enum_decl* enum_decl = container_of(
        statement, typeof(*enum_decl), base);
    
    if(enum_decl->enum_spec)
        ctf_parse_type_spec_destroy(&enum_decl->enum_spec->base);
    
    free(enum_decl);
}

static int enum_decl_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_enum_decl* enum_decl = container_of(statement,
        typeof(*enum_decl), base);
    
    return visitor->visitor_ops->visit_enum_decl(visitor, enum_decl);
}


static struct ctf_parse_statement_operations enum_decl_ops =
{
    .get_type = enum_decl_ops_get_type,
    .destroy = enum_decl_ops_destroy,
    .visit = enum_decl_ops_visit,
};


struct ctf_parse_enum_decl* ctf_parse_enum_decl_create(void)
{
    struct ctf_parse_enum_decl* enum_decl =
        malloc(sizeof(*enum_decl));
    if(enum_decl == NULL)
    {
        ctf_err("Failed to allocate structure declaration node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&enum_decl->base);
    enum_decl->enum_spec = NULL;
    
    enum_decl->base.statement_ops = &enum_decl_ops;
    
    return enum_decl;
}

/* Variant specification and its scope */
static enum ctf_parse_scope_type scope_variant_ops_get_type(
    struct ctf_parse_scope* scope)
{
    (void)scope;
    
    return ctf_parse_scope_type_variant;
}
static void scope_variant_ops_destroy(struct ctf_parse_scope* scope)
{
    struct ctf_parse_scope_variant* scope_variant = container_of(scope,
        typeof(*scope_variant), base);
    
    free(scope_variant);
}

static int scope_variant_ops_visit(struct ctf_parse_scope* scope,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_scope_variant* scope_variant = container_of(scope,
        typeof(*scope_variant), base);
    
    return visitor->visitor_ops->visit_scope_variant(visitor, scope_variant);
}


static struct ctf_parse_scope_operations scope_variant_ops =
{
    .get_type = scope_variant_ops_get_type,
    .destroy = scope_variant_ops_destroy,
    .visit = scope_variant_ops_visit,
};

struct ctf_parse_scope_variant* ctf_parse_scope_variant_create(void)
{
    struct ctf_parse_scope_variant* scope_variant = malloc(sizeof(*scope_variant));
    if(scope_variant == NULL)
    {
        ctf_err("Failed to allocate structure scope node in AST.");
        return NULL;
    }
    
    ctf_parse_scope_init(&scope_variant->base);
    scope_variant->variant_spec = NULL;
    
    scope_variant->base.scope_ops = &scope_variant_ops;
    
    return scope_variant;
}

static enum ctf_parse_type_spec_type variant_spec_ops_get_type(
    struct ctf_parse_type_spec* type_spec)
{
    (void)type_spec;
    
    return ctf_parse_type_spec_type_struct;
}
static void variant_spec_ops_destroy(
    struct ctf_parse_type_spec* type_spec)
{
    struct ctf_parse_variant_spec* variant_spec = container_of(
        type_spec, typeof(*variant_spec), base);
    
    if(variant_spec->scope_variant)
        ctf_parse_scope_destroy(&variant_spec->scope_variant->base);
    
    if(variant_spec->variant_name)
        free(variant_spec->variant_name);
    
    if(variant_spec->variant_tag)
        free(variant_spec->variant_tag);
    
    free(variant_spec);
}

static int variant_spec_ops_visit(struct ctf_parse_type_spec* type_spec,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_variant_spec* variant_spec = container_of(type_spec,
        typeof(*variant_spec), base);
    
    return visitor->visitor_ops->visit_variant_spec(visitor, variant_spec);
}


static struct ctf_parse_type_spec_operations variant_spec_ops =
{
    .get_type = variant_spec_ops_get_type,
    .destroy = variant_spec_ops_destroy,
    .visit = variant_spec_ops_visit,
};

struct ctf_parse_variant_spec* ctf_parse_variant_spec_create(void)
{
    struct ctf_parse_variant_spec* variant_spec = malloc(sizeof(*variant_spec));
    if(variant_spec == NULL)
    {
        ctf_err("Failed to allocate structure spec node in AST.");
        return NULL;
    }
    
    ctf_parse_type_spec_init(&variant_spec->base);

    variant_spec->scope_variant = NULL;
    variant_spec->variant_name = NULL;
    variant_spec->variant_tag = NULL;
    
    variant_spec->base.type_spec_ops = &variant_spec_ops;
    
    return variant_spec;
}

void ctf_parse_scope_variant_connect(
	struct ctf_parse_scope_variant* scope_variant,
	struct ctf_parse_variant_spec* variant_spec)
{
    assert(scope_variant->variant_spec == NULL);
    
    scope_variant->variant_spec = variant_spec;
    variant_spec->scope_variant = scope_variant;
}

/* Variant declaration */
static enum ctf_parse_statement_type variant_decl_ops_get_type(
    struct ctf_parse_statement* statement)
{
    (void)statement;
    
    return ctf_parse_statement_type_variant_decl;
}
static void variant_decl_ops_destroy(
    struct ctf_parse_statement* statement)
{
    struct ctf_parse_variant_decl* variant_decl = container_of(
        statement, typeof(*variant_decl), base);
    
    if(variant_decl->variant_spec)
        ctf_parse_type_spec_destroy(&variant_decl->variant_spec->base);
    
    free(variant_decl);
}

static int variant_decl_ops_visit(struct ctf_parse_statement* statement,
    struct ctf_ast_visitor* visitor)
{
    struct ctf_parse_variant_decl* variant_decl = container_of(statement,
        typeof(*variant_decl), base);
    
    return visitor->visitor_ops->visit_variant_decl(visitor, variant_decl);
}


static struct ctf_parse_statement_operations variant_decl_ops =
{
    .get_type = variant_decl_ops_get_type,
    .destroy = variant_decl_ops_destroy,
    .visit = variant_decl_ops_visit,
};


struct ctf_parse_variant_decl* ctf_parse_variant_decl_create(void)
{
    struct ctf_parse_variant_decl* variant_decl =
        malloc(sizeof(*variant_decl));
    if(variant_decl == NULL)
    {
        ctf_err("Failed to allocate structure declaration node in AST.");
        return NULL;
    }
    
    ctf_parse_statement_init(&variant_decl->base);
    variant_decl->variant_spec = NULL;
    
    variant_decl->base.statement_ops = &variant_decl_ops;
    
    return variant_decl;
}

/********************************* AST ********************************/
struct ctf_ast* ctf_ast_create(void)
{
	struct ctf_ast* ast = malloc(sizeof(*ast));
    if(ast == NULL)
    {
        printf("Failed to allocate AST tree.\n");
        return NULL;
    }
    ast->scope_root = ctf_parse_scope_root_create();
    if(ast->scope_root == NULL)
    {
        free(ast);
        return NULL;
    }
    
    return ast;
}

void ctf_ast_destroy(struct ctf_ast* ast)
{
    ctf_parse_scope_destroy(&ast->scope_root->base);
    
    free(ast);
}

int ctf_ast_visitor_visit_ast(struct ctf_ast_visitor* visitor,
    struct ctf_ast* ast)
{
    return ctf_ast_visitor_visit_scope(visitor, &ast->scope_root->base);
}