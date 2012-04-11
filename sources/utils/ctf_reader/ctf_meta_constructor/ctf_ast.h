/* Abstract Syntax Tree for CTF metadata */

#ifndef CTF_AST_H
#define CTF_AST_H

#include "linked_list.h"

/****** Hierarhy of objects, which represent parsed grammar(AST) ******/

struct ctf_ast;
struct ctf_ast_visitor;

struct ctf_parse_statement;
struct ctf_parse_scope_operations;
struct ctf_parse_statement_operations;
struct ctf_parse_type_spec_operations;


/* Abstract scope, which contains statements */
struct ctf_parse_scope
{
    struct linked_list_head statements;

    const struct ctf_parse_scope_operations* scope_ops;
};

/* Different types of the scope */
enum ctf_parse_scope_type
{
    ctf_parse_scope_type_root,
    ctf_parse_scope_type_top,
    ctf_parse_scope_type_struct,
    ctf_parse_scope_type_variant,
    ctf_parse_scope_type_enum,
    ctf_parse_scope_type_integer,
};

/* Operations for the scope - simple destroy, visit and get_type.*/
struct ctf_parse_scope_operations
{
    void (*destroy)(struct ctf_parse_scope* scope);
    int (*visit)(struct ctf_parse_scope* scope,
        struct ctf_ast_visitor* visitor);
    enum ctf_parse_scope_type (*get_type)(struct ctf_parse_scope* scope);
};

/* Destroy scope */
void ctf_parse_scope_destroy(struct ctf_parse_scope* scope);

int ctf_ast_visitor_visit_scope(struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope* scope);

enum ctf_parse_scope_type ctf_parse_scope_get_type(
    struct ctf_parse_scope* scope);

/* One statement in the scope (terminated with ';' in syntax) */
struct ctf_parse_statement
{
    /* Scope containing given statement */
    struct ctf_parse_scope* scope_parent;
    /* List organization of statements inside scope */
    struct linked_list_elem list_elem;
    
    const struct ctf_parse_statement_operations* statement_ops;
};


/* Different types of the statement */
enum ctf_parse_statement_type
{
    ctf_parse_statement_type_struct_decl,
    ctf_parse_statement_type_variant_decl,
    ctf_parse_statement_type_integer_decl,
    ctf_parse_statement_type_enum_decl,
    ctf_parse_statement_type_typedef_decl,
    ctf_parse_statement_type_field_decl,
    ctf_parse_statement_type_parameter_def,
    ctf_parse_statement_type_top_scope_decl,
    ctf_parse_statement_type_type_assignment,
};

/* Operations for the statement - simple destroy, visit and get_type.*/
struct ctf_parse_statement_operations
{
    void (*destroy)(struct ctf_parse_statement* statement);
    int (*visit)(struct ctf_parse_statement* statement,
        struct ctf_ast_visitor* visitor);
    enum ctf_parse_statement_type (*get_type)(
        struct ctf_parse_statement* statement);
};

/* Destroy statement */
void ctf_parse_statement_destroy(struct ctf_parse_statement* statement);

int ctf_ast_visitor_visit_statement(struct ctf_ast_visitor* visitor,
    struct ctf_parse_statement* statement);

enum ctf_parse_statement_type ctf_parse_statement_get_type(
    struct ctf_parse_statement* statement);

/* Add statement to the end of the scope */
void ctf_parse_scope_add_statement(struct ctf_parse_scope* scope,
    struct ctf_parse_statement* statement);

#define parse_scope_for_each_statement(scope, statement) \
linked_list_for_each_entry(&(scope)->statements, statement, list_elem)

/* Root scope */
struct ctf_parse_scope_root
{
    struct ctf_parse_scope base;/* type = root */
};

struct ctf_parse_scope_root* ctf_parse_scope_root_create(void);

/* Declaration of top-level scope */
struct ctf_parse_scope_top;
struct ctf_parse_scope_top_decl
{
    struct ctf_parse_statement base; /* type = top_scope_decl */
    
    char* scope_name;
    
    struct ctf_parse_scope_top* scope_top;
};

struct ctf_parse_scope_top
{
    struct ctf_parse_scope base;/* type = top */
    
    /* Statement declared scope */
    struct ctf_parse_scope_top_decl* scope_top_decl;
};

struct ctf_parse_scope_top_decl* ctf_parse_scope_top_decl_create(void);

struct ctf_parse_scope_top* ctf_parse_scope_top_create(void);

/* Connect top scope with its declaration */
void ctf_parse_scope_top_connect(struct ctf_parse_scope_top* scope_top,
    struct ctf_parse_scope_top_decl* scope_top_decl);

/* Abstract type specification */
struct ctf_parse_type_spec
{
    const struct ctf_parse_type_spec_operations* type_spec_ops;
};

/* Types of type specification */
enum ctf_parse_type_spec_type
{
    ctf_parse_type_spec_type_id,
    ctf_parse_type_spec_type_struct,
    ctf_parse_type_spec_type_variant,
    ctf_parse_type_spec_type_enum,
    ctf_parse_type_spec_type_integer
};

/* Operations for the type specification - simple destroy, visit and get_type.*/
struct ctf_parse_type_spec_operations
{
    void (*destroy)(struct ctf_parse_type_spec* type_spec);
    int (*visit)(struct ctf_parse_type_spec* type_spec,
        struct ctf_ast_visitor* visitor);
    enum ctf_parse_type_spec_type (*get_type)(
        struct ctf_parse_type_spec* type_spec);
};

void ctf_parse_type_spec_destroy(struct ctf_parse_type_spec* type_spec);

int ctf_ast_visitor_visit_type_spec(struct ctf_ast_visitor* visitor,
    struct ctf_parse_type_spec* type_spec);

enum ctf_parse_type_spec_type ctf_parse_type_spec_get_type(
    struct ctf_parse_type_spec* type_spec);


/* Struct specification and scope for it*/
struct ctf_parse_scope_struct;
struct ctf_parse_struct_spec
{
    struct ctf_parse_type_spec base; /* type = struct */
    
    char* struct_name;/* NULL if unnamed */
    
    struct ctf_parse_scope_struct* scope_struct; /* NULL if not exist */
    
    int align; /* -1 if not set */
};

struct ctf_parse_scope_struct
{
    struct ctf_parse_scope base; /* type = struct */
    
    struct ctf_parse_struct_spec* struct_spec;
};

struct ctf_parse_struct_spec* ctf_parse_struct_spec_create(void);
struct ctf_parse_scope_struct* ctf_parse_scope_struct_create(void);
/* Connect structure scope with its specification */
void ctf_parse_scope_struct_connect(
    struct ctf_parse_scope_struct* scope_struct,
    struct ctf_parse_struct_spec* struct_spec);

/* Integer specification and scope for it*/
struct ctf_parse_scope_int;
struct ctf_parse_int_spec
{
    struct ctf_parse_type_spec base; /* type = integer */
    
    struct ctf_parse_scope_int* scope_int; 
};

struct ctf_parse_scope_int
{
    struct ctf_parse_scope base; /* type = integer */
    
    struct ctf_parse_int_spec* int_spec;
};

struct ctf_parse_int_spec* ctf_parse_int_spec_create(void);
struct ctf_parse_scope_int* ctf_parse_scope_int_create(void);
/* Connect integer scope with its specification */
void ctf_parse_scope_int_connect(
    struct ctf_parse_scope_int* scope_int,
    struct ctf_parse_int_spec* int_spec);


/* Struct declaration */
struct ctf_parse_struct_decl
{
    struct ctf_parse_statement base; /* type = struct_decl */
    
    struct ctf_parse_struct_spec* struct_spec;
};

struct ctf_parse_struct_decl* ctf_parse_struct_decl_create(void);

/* 
 * Post modificator for type.
 * 
 * This modificators follows field or typedef identificator and
 * means array or sequence ("[...]").
 */
struct ctf_parse_type_post_mod_operations;
struct ctf_parse_type_post_mod
{
	/* list organization of type post specificators */
	struct linked_list_elem list_elem;
	
	const struct ctf_parse_type_post_mod_operations* type_post_mod_ops;
};

enum ctf_parse_type_post_mod_type
{
	ctf_parse_type_post_mod_type_array = 0,
	ctf_parse_type_post_mod_type_sequence = 1,
};

/* Operations for the type post specificators - simple destroy, visit and get_type.*/
struct ctf_parse_type_post_mod_operations
{
    void (*destroy)(struct ctf_parse_type_post_mod* type_post_mod);
    int (*visit)(struct ctf_parse_type_post_mod* type_post_mod,
        struct ctf_ast_visitor* visitor);
    enum ctf_parse_type_post_mod_type (*get_type)(
        struct ctf_parse_type_post_mod* type_post_mod);
};

void ctf_parse_type_post_mod_destroy(
	struct ctf_parse_type_post_mod* type_post_mod);
int ctf_ast_visitor_visit_type_post_mod(
	struct ctf_ast_visitor* visitor,
	struct ctf_parse_type_post_mod* type_post_mod);
enum ctf_parse_type_post_mod_type ctf_parse_type_post_mod_get_type(
	struct ctf_parse_type_post_mod* type_post_mod);


/* 
 * List of type post modifiers.
 * 
 * Contains zero or more arranged type post modifiers.
 */
struct ctf_parse_type_post_mod_list
{
	struct linked_list_head mods;
};

struct ctf_parse_type_post_mod_list*
ctf_parse_type_post_mod_list_create(void);

void ctf_parse_type_post_mod_list_destroy(
	struct ctf_parse_type_post_mod_list* type_post_mod_list);

/* Add type post modifier into list. */
void ctf_parse_type_post_mod_list_add_mod(
	struct ctf_parse_type_post_mod_list* type_post_mod_list,
	struct ctf_parse_type_post_mod* type_post_mod);

#define ctf_parse_type_post_mod_list_for_each_mod(list, mod) \
	linked_list_for_each_entry(&(list)->mods, mod, list_elem)

/* Array type post modifier */
struct ctf_parse_type_post_mod_array
{
	struct ctf_parse_type_post_mod base;
	/* String contained length of array */
	char* array_len;
};

struct ctf_parse_type_post_mod_array*
ctf_parse_type_post_mod_array_create(void);

/* Sequence type post modifier */
struct ctf_parse_type_post_mod_sequence
{
	struct ctf_parse_type_post_mod base;
	/* String contained tagged integer, which contain length of sequence */
	char* sequence_len;
};

struct ctf_parse_type_post_mod_sequence*
ctf_parse_type_post_mod_sequence_create(void);

/* Field declaration */
struct ctf_parse_field_decl
{
    struct ctf_parse_statement base; /* type = field_decl */
    
    struct ctf_parse_type_spec* type_spec;
    
    char* field_name;
    /* Not NULL, but may be empty */
    struct ctf_parse_type_post_mod_list* type_post_mod_list;
};

struct ctf_parse_field_decl* ctf_parse_field_decl_create(void);

/* Parameter definition(in top-level scope or integer scope) */
struct ctf_parse_param_def
{
    struct ctf_parse_statement base; /* type = parameter_def */
    
    char* param_name;
    char* param_value;
};

struct ctf_parse_param_def* ctf_parse_param_def_create(void);

/* Type specification using type identificator */
struct ctf_parse_type_spec_id
{
    struct ctf_parse_type_spec base; /* type = id */
    
    char* type_name;
};

struct ctf_parse_type_spec_id* ctf_parse_type_spec_id_create(void);

/* Assignment of the dynamic types. */
struct ctf_parse_type_assignment
{
    struct ctf_parse_statement base; /* type = parameter_def */
    
    char* tag;
    struct ctf_parse_type_spec* type_spec;
};

struct ctf_parse_type_assignment* ctf_parse_type_assignment_create(void);

/* Typedef declaration */
struct ctf_parse_typedef_decl
{
    struct ctf_parse_statement base; /* type = typedef_decl */
    
    struct ctf_parse_type_spec* type_spec_base;
    char* type_name;
    /* Not NULL, but may be empty */
    struct ctf_parse_type_post_mod_list* type_post_mod_list;
};

struct ctf_parse_typedef_decl* ctf_parse_typedef_decl_create(void);

/* Basic definition of enumeration value */
struct ctf_parse_enum_value_operations;
struct ctf_parse_enum_value
{
	/* List organization in enum scope */
	struct linked_list_elem list_elem;
	
	const struct ctf_parse_enum_value_operations* enum_value_ops;
};
/* Different types of enumeration value definitions */
enum ctf_parse_enum_value_type
{
	ctf_parse_enum_value_type_simple = 0, /* without boundaries */
	ctf_parse_enum_value_type_presize, 	/* presize integer value */
	ctf_parse_enum_value_type_range,	/* range of integer values */
};


/* Operations for enumeration value definition */
struct ctf_parse_enum_value_operations
{
    void (*destroy)(struct ctf_parse_enum_value* value);
    int (*visit)(struct ctf_parse_enum_value* value,
        struct ctf_ast_visitor* visitor);
    enum ctf_parse_enum_value_type (*get_type)(
        struct ctf_parse_enum_value* value);
	
};

void ctf_parse_enum_value_destroy(struct ctf_parse_enum_value* enum_value);

int ctf_ast_visitor_visit_enum_value(struct ctf_ast_visitor* visitor,
    struct ctf_parse_enum_value* enum_value);

enum ctf_parse_enum_value_type ctf_parse_enum_value_get_type(
    struct ctf_parse_enum_value* enum_value);

/* Definition of simple enumeration value */
struct ctf_parse_enum_value_simple
{
	struct ctf_parse_enum_value base; /* type = simple */
	
	char* val_name;
};

struct ctf_parse_enum_value_simple* ctf_parse_enum_value_simple_create(void);

/* Definition of enumeration value with presize integer value */
struct ctf_parse_enum_value_presize
{
	struct ctf_parse_enum_value base; /* type = presize */

	char* val_name;
	char* int_value;
};

struct ctf_parse_enum_value_presize* ctf_parse_enum_value_presize_create(void);

/* Definition of enumeration value with range of integer values */
struct ctf_parse_enum_value_range
{
	struct ctf_parse_enum_value base; /* type = range */
	
	char* val_name;
	char* int_value_start;
	char* int_value_end;
};

struct ctf_parse_enum_value_range* ctf_parse_enum_value_range_create(void);

/* Enumeration specification and its scope */
struct ctf_parse_enum_spec
{
    struct ctf_parse_type_spec base; /* type = enum */
    
    char* enum_name;/* NULL if unnamed */
    
    struct ctf_parse_type_spec* type_spec_int;/* NULL if not set */
    
    struct ctf_parse_scope_enum* scope_enum; /* NULL if not exist */
};

struct ctf_parse_enum_spec* ctf_parse_enum_spec_create(void);

struct ctf_parse_scope_enum
{
	struct ctf_parse_scope base; /* type = enum */
	
	struct ctf_parse_enum_spec* enum_spec;
	/* List of values definition */
	struct linked_list_head values;
};

struct ctf_parse_scope_enum* ctf_parse_scope_enum_create(void);

/* Connect enumeration scope to enum specification */
void ctf_parse_scope_enum_connect(
	struct ctf_parse_scope_enum* scope_enum,
	struct ctf_parse_enum_spec* enum_spec);

/* Add definition of value to the enumeration scope */
void ctf_parse_scope_enum_add_value(
	struct ctf_parse_scope_enum* scope_enum,
	struct ctf_parse_enum_value* value);

/* Enumeration declaration */
struct ctf_parse_enum_decl
{
    struct ctf_parse_statement base; /* type = enum_decl */
    
    struct ctf_parse_enum_spec* enum_spec;
};

struct ctf_parse_enum_decl* ctf_parse_enum_decl_create(void);

/* Variant specification and scope for it*/
struct ctf_parse_scope_variant;
struct ctf_parse_variant_spec
{
    struct ctf_parse_type_spec base; /* type = variant */
    
    char* variant_name;/* NULL if unnamed */
    
    char* variant_tag;/* NULL if no tag */
    
    struct ctf_parse_scope_variant* scope_variant; /* NULL if not exist */
};

struct ctf_parse_scope_variant
{
    struct ctf_parse_scope base; /* type = variant */
    
    struct ctf_parse_variant_spec* variant_spec;
};

struct ctf_parse_variant_spec* ctf_parse_variant_spec_create(void);
struct ctf_parse_scope_variant* ctf_parse_scope_variant_create(void);
/* Connect variant scope with its specification */
void ctf_parse_scope_variant_connect(
    struct ctf_parse_scope_variant* scope_variant,
    struct ctf_parse_variant_spec* variant_spec);


/* Variant declaration */
struct ctf_parse_variant_decl
{
    struct ctf_parse_statement base; /* type = variant_decl */
    
    struct ctf_parse_variant_spec* variant_spec;
};

struct ctf_parse_variant_decl* ctf_parse_variant_decl_create(void);

/****************** AST for CTF metadata description *****************/
struct ctf_ast
{
    struct ctf_parse_scope_root* scope_root;
};

struct ctf_ast* ctf_ast_create(void);

void ctf_ast_destroy(struct ctf_ast* ast);

/* Parse file and return AST of parsing */
struct ctf_ast* ctf_meta_parse(const char* filename);

/* Visitor for AST tree for CTF metadata description */
struct ctf_ast_visitor_operations;

struct ctf_ast_visitor
{
    const struct ctf_ast_visitor_operations* visitor_ops;
};

struct ctf_ast_visitor_operations
{
    /* Visit scope subclasses */
    int (*visit_scope_root)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_scope_root* scope_root);
    int (*visit_scope_top)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_scope_top* scope_top);
    int (*visit_scope_struct)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_scope_struct* scope_struct);
    int (*visit_scope_variant)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_scope_variant* scope_variant);
    int (*visit_scope_int)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_scope_int* scope_int);
    int (*visit_scope_enum)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_scope_enum* scope_enum);
    
    /* Visit statement subclasses */
    int (*visit_scope_top_decl)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_scope_top_decl* scope_top_decl);
    int (*visit_struct_decl)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_struct_decl* struct_decl);
    int (*visit_variant_decl)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_variant_decl* variant_decl);
    int (*visit_enum_decl)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_enum_decl* enum_decl);
    int (*visit_typedef_decl)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_typedef_decl* typedef_decl);
    int (*visit_field_decl)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_field_decl* field_decl);
    int (*visit_param_def)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_param_def* param_def);
    int (*visit_type_assignment)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_type_assignment* type_assignment);
    
    /* Visit type specification subclasses */
    int (*visit_struct_spec)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_struct_spec* struct_spec);
    int (*visit_variant_spec)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_variant_spec* variant_spec);
    int (*visit_enum_spec)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_enum_spec* enum_spec);
    int (*visit_type_spec_id)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_type_spec_id* type_spec_id);
    int (*visit_int_spec)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_int_spec* int_spec);
    
    /* Visit different enumeration value definitions */
    int (*visit_enum_value_simple)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_enum_value_simple* enum_value_simple);
    int (*visit_enum_value_presize)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_enum_value_presize* enum_value_presize);
    int (*visit_enum_value_range)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_enum_value_range* enum_value_range);
    
    /* Visit different type post modifiers */
    int (*visit_type_post_mod_array)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_type_post_mod_array* type_post_mod_array);
    int (*visit_type_post_mod_sequence)(struct ctf_ast_visitor* visitor,
        struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence);
};

int ctf_ast_visitor_visit_ast(struct ctf_ast_visitor* visitor,
    struct ctf_ast* ast);

#endif /* CTF_AST_H */