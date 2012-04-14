/* Implements creation of CTF metadata from file */

#include "ctf_meta.h"
#include "ctf_meta_parse.h"
#include "ctf_ast.h"

#include "ctf_meta_internal.h" /* container_of */

#include <stdio.h> /* printf */
#include <stdarg.h> /* va_arg */

#include <string.h> /* strdup, strcat...*/
#include <malloc.h> /* realloc for append string */
#include <assert.h> /* assertions */

#include <errno.h> /* error codes */

#include <stdlib.h> /* strtoul */

/* CTF AST visitor which construct metadata. */
struct ctf_meta_constructor
{
    struct ctf_ast_visitor base;
    /* Metadata under construction */
    struct ctf_meta* meta;
    /* Currently used type(type is already added to meta) */
    struct ctf_type* current_type;
    /* 
     * Enumeration value which should be used by default.
     * (only inside enumeration scope).
     */
    //TODO: currently only 32-bit signed integer is supported.
    int32_t next_enum_value;
};

static void ctf_meta_constructor_init(
    struct ctf_meta_constructor* constructor, struct ctf_meta* meta);

#define semantic_error(format, ...) do { fprintf(stderr,   \
    "Semantic error: " format "\n", ##__VA_ARGS__);        \
    return -1; } while(0)

#define semantic_warning(format, ...) fprintf(stderr,    \
    "Semantic warning: " format "\n", ##__VA_ARGS__)


struct ctf_meta* ctf_meta_create_from_file(const char* filename)
{
    struct ctf_ast* ast = ctf_meta_parse(filename);
    
    if(ast == NULL) return NULL;
    
    struct ctf_meta* meta = ctf_meta_create();
    
    if(meta == NULL)
    {
        ctf_ast_destroy(ast);
        return NULL;
    }

    struct ctf_meta_constructor constructor;
    ctf_meta_constructor_init(&constructor, meta);
    
    int result = ctf_ast_visitor_visit_ast(&constructor.base, ast);
    ctf_ast_destroy(ast);
    
    if(result < 0)
    {
        ctf_meta_destroy(meta);
        return NULL;
    }
    
    return meta;
}


/* Maximum integer value of type suffix for make type unique. */
#define TYPE_SUFFIX_MAX 9999

/* Print given integer suffix into string in snprintf-style. */
static int snprintf_suffix(char* dest, int dest_len, int suffix)
{
    return snprintf(dest, dest_len, "$%.4d", suffix);
}

/* Assign parameter in integer scope */
static int assign_int_parameter(struct ctf_meta* meta,
    const char* param_name, const char* param_value);

/*
 * Append formatted string to allocated one.
 */
static char* strappend_format(char* str, const char* append_format,...);

/*********************** Visitor implementation ***********************/

/* Same visitor for all types of scopes except enum */
static int constructor_visit_scope_common(
    struct ctf_meta_constructor* constructor,
    struct ctf_parse_scope* scope)
{
    int result = 0;
    struct ctf_parse_statement* statement;
    parse_scope_for_each_statement(scope, statement)
    {
        result = ctf_ast_visitor_visit_statement(&constructor->base,
            statement);
        if(result) break;
    }
    
    return result;
}


static int constructor_ops_visit_scope_root(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope_root* scope_root)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    
    return constructor_visit_scope_common(constructor, &scope_root->base);
}

static int constructor_ops_visit_scope_top(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope_top* scope_top)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    
    return constructor_visit_scope_common(constructor, &scope_top->base);
}

static int constructor_ops_visit_scope_struct(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope_struct* scope_struct)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    
    return constructor_visit_scope_common(constructor, &scope_struct->base);
}

static int constructor_ops_visit_scope_variant(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope_variant* scope_variant)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    
    return constructor_visit_scope_common(constructor, &scope_variant->base);
}


static int constructor_ops_visit_scope_int(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope_int* scope_int)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    
    return constructor_visit_scope_common(constructor, &scope_int->base);
}

static int constructor_ops_visit_scope_enum(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope_enum* scope_enum)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);

    constructor->next_enum_value = 0;
    
    int result = 0;
    struct ctf_parse_enum_value* enum_value;
    linked_list_for_each_entry(&scope_enum->values, enum_value, list_elem)
    {
        result = ctf_ast_visitor_visit_enum_value(visitor, enum_value);
        if(result) break;
    }
    
    return result;
}


/* 
 * After successfull visiting any type specification,
 * 'current_type' will be set.
 */
static int constructor_ops_visit_struct_spec(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_struct_spec* struct_spec)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    int result;

    if((struct_spec->struct_name == NULL)
        && (struct_spec->scope_struct == NULL))
    {
        semantic_error("Structure type without name and scope.");
    }
    //TODO: process align
    char* struct_name;
    if(struct_spec->struct_name)
        struct_name = strappend_format(NULL, "struct %s", struct_spec->struct_name);
    else
        struct_name = strappend_format(NULL, "struct @unnamed");
    if(struct_name == NULL) return -ENOMEM;
    
    if(struct_spec->scope_struct)
    {
        result = ctf_meta_struct_begin(meta, struct_name,
            (struct_spec->struct_name == 0));
        free(struct_name);
        if(result < 0)
        {
            semantic_error("Failed to create structure type.");
        }
        result = ctf_meta_struct_begin_scope(meta);
        if(result < 0)
        {
            semantic_error("Failed to begin structure scope.");
        }
        result = ctf_ast_visitor_visit_scope(visitor,
            &struct_spec->scope_struct->base);
        if(result < 0) return result;
        
        ctf_meta_struct_end_scope(meta);
        constructor->current_type = ctf_meta_struct_end(meta);
        if(constructor->current_type == NULL)
        {
            semantic_error("Failed to finish structure definition.");
        }
    }
    else
    {
        constructor->current_type = ctf_meta_find_type(meta, struct_name);
        free(struct_name);
        if(constructor->current_type == NULL)
        {
            semantic_error("Unknown structure type '%s'.", struct_spec->struct_name);
        }
    }
    return 0;
}

static int constructor_ops_visit_variant_spec(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_variant_spec* variant_spec)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    int result;

    if((variant_spec->variant_name == NULL)
        && (variant_spec->scope_variant == NULL))
    {
        semantic_error("Variant type without name and scope.");
    }
    //TODO: process align
    char* variant_name;
    if(variant_spec->variant_name)
        variant_name = strappend_format(NULL, "variant %s", variant_spec->variant_name);
    else
        variant_name = strappend_format(NULL, "variant @unnamed");
    if(variant_name == NULL) return -ENOMEM;
    
    if(variant_spec->scope_variant)
    {
        result = ctf_meta_variant_begin(meta, variant_name,
            (variant_spec->variant_name == 0));
        free(variant_name);
        if(result < 0)
        {
            semantic_error("Failed to create variant type.");
        }
        result = ctf_meta_variant_begin_scope(meta);
        if(result < 0)
        {
            semantic_error("Failed to begin variant scope.");
        }
        result = ctf_ast_visitor_visit_scope(visitor,
            &variant_spec->scope_variant->base);
        if(result < 0) return result;
        
        ctf_meta_variant_end_scope(meta);
        constructor->current_type = ctf_meta_variant_end(meta);
        if(constructor->current_type == NULL)
        {
            semantic_error("Failed to finish variant definition.");
        }
        
        if(variant_spec->variant_tag)
        {
            result = ctf_meta_variant_set_tag(meta,
                constructor->current_type,
                variant_spec->variant_tag);
            if(result)
            {
                semantic_error("Failed add tag to the variant.");
            }
        }
    }
    else
    {
        constructor->current_type = ctf_meta_find_type(meta, variant_name);
        free(variant_name);
        if(constructor->current_type == NULL)
        {
            semantic_error("Unknown variant type '%s'.", variant_spec->variant_name);
        }
        
        if(variant_spec->variant_tag)
        {
            /* Create internal variant via typedef and set tag for it */
            constructor->current_type = ctf_meta_typedef_create(meta,
                "variant @tagged", constructor->current_type, 1);
            if(constructor->current_type == NULL)
            {
                ctf_err("Failed to copy variant type.");
                return -ENOMEM;
            }
            
            result = ctf_meta_variant_set_tag(
                meta,
                constructor->current_type,
                variant_spec->variant_tag);
            if(result)
            {
                semantic_error("Failed add tag to the variant.");
            }
        }

    }
    return 0;
}

static int constructor_ops_visit_enum_spec(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_enum_spec* enum_spec)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    int result;

    if((enum_spec->enum_name == NULL)
        && (enum_spec->scope_enum == NULL))
    {
        semantic_error("Enumeration type without name and scope.");
    }

    char* enum_name;
    if(enum_spec->enum_name)
        enum_name = strappend_format(NULL, "enum %s", enum_spec->enum_name);
    else
        enum_name = strappend_format(NULL, "enum @unnamed");
    if(enum_name == NULL) return -ENOMEM;
    
    if(enum_spec->scope_enum)
    {
        //TODO: default base integer type may be introduce later
        assert(enum_spec->type_spec_int != NULL);
        result = ctf_ast_visitor_visit_type_spec(visitor,
            enum_spec->type_spec_int);
        
        if(result)
        {
            free(enum_name);
            return result;
        }
        
        if(!ctf_type_is_int(constructor->current_type))
        {
            free(enum_name);
            semantic_error("Only integer type may be based for enumeration.");
        }
        
        result = ctf_meta_enum_begin(meta, enum_name, constructor->current_type,
            (enum_spec->enum_name == 0));
        free(enum_name);
        if(result < 0)
        {
            semantic_error("Failed to create enumeration type.");
        }
        result = ctf_meta_enum_begin_scope(meta);
        if(result < 0)
        {
            semantic_error("Failed to begin enumeration scope.");
        }
        result = ctf_ast_visitor_visit_scope(visitor,
            &enum_spec->scope_enum->base);
        if(result < 0) return result;
        
        ctf_meta_enum_end_scope(meta);
        constructor->current_type = ctf_meta_enum_end(meta);
        if(constructor->current_type == NULL)
        {
            semantic_error("Failed to finish enumeration definition.");
        }
    }
    else
    {
        constructor->current_type = ctf_meta_find_type(meta, enum_name);
        free(enum_name);
        if(constructor->current_type == NULL)
        {
            semantic_error("Unknown enumeration type '%s'.", enum_spec->enum_name);
        }
    }
    return 0;
}


static int constructor_ops_visit_type_spec_id(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_type_spec_id* type_spec_id)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    constructor->current_type = ctf_meta_find_type(meta,
        type_spec_id->type_name);
    if(constructor->current_type == NULL)
    {
        semantic_error("Unknown type identificator '%s'.",
            type_spec_id->type_name);
    }
    
    return 0;
}

static int constructor_ops_visit_int_spec(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_int_spec* int_spec)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    int result;

    assert(int_spec->scope_int);
    
    //TODO: more informative integer internal name
    result = ctf_meta_int_begin(meta, "@integer");
    if(result < 0)
    {
        semantic_error("Failed to create integer type.");
    }
    result = ctf_meta_int_begin_scope(meta);
    if(result < 0)
    {
        semantic_error("Failed to begin integer scope.");
    }
    result = ctf_ast_visitor_visit_scope(visitor,
        &int_spec->scope_int->base);
    if(result < 0) return result;
    
    ctf_meta_int_end_scope(meta);
    constructor->current_type = ctf_meta_int_end(meta);
    if(constructor->current_type == NULL)
    {
        semantic_error("Failed to finish integer definition.");
    }
    return 0;
}

/* 
 * Before visit enumeration value, 'next_enum_value' should be set.
 * 
 * After successfull visit this value should be updated.
 */
static int constructor_ops_visit_enum_value_simple(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_enum_value_simple* enum_value_simple)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    int result = ctf_meta_enum_add_value32(meta,
        enum_value_simple->val_name, constructor->next_enum_value,
        constructor->next_enum_value);
    
    if(result)
    {
        semantic_error("Failed to add enumeration value.");
        return result;
    }
    
    constructor->next_enum_value++;
    
    return 0;
}

static int constructor_ops_visit_enum_value_presize(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_enum_value_presize* enum_value_presize)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    const char* endptr;
    int32_t val = (int32_t)strtol(enum_value_presize->int_value,
        (char**)&endptr, 0);
    
    if(*endptr != '\0')
    {
        semantic_error("Failed to parse presize enumeration value as integer.");
    }
    
    int result = ctf_meta_enum_add_value32(meta,
        enum_value_presize->val_name, val, val);
    
    if(result)
    {
        semantic_error("Failed to add enumeration value.");
        return result;
    }
    
    constructor->next_enum_value = val + 1;
    
    return 0;
}

static int constructor_ops_visit_enum_value_range(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_enum_value_range* enum_value_range)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;

    const char* endptr;
    int32_t val_start = (int32_t)strtol(enum_value_range->int_value_start,
        (char**)&endptr, 0);
    
    if(*endptr != '\0')
    {
        semantic_error("Failed to parse start enumeration value as integer.");
    }

    int32_t val_end = (int32_t)strtol(enum_value_range->int_value_end,
        (char**)&endptr, 0);
    
    if(*endptr != '\0')
    {
        semantic_error("Failed to parse end enumeration value as integer.");
    }

    int result = ctf_meta_enum_add_value32(meta,
        enum_value_range->val_name, val_start, val_end);
    
    if(result)
    {
        semantic_error("Failed to add enumeration value.");
        return result;
    }
    
    constructor->next_enum_value = val_end + 1;
    
    return 0;
}


static int constructor_ops_visit_struct_decl(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_struct_decl* struct_decl)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);

    struct ctf_parse_struct_spec* struct_spec = struct_decl->struct_spec;

    if(struct_spec->struct_name == NULL)
    {
        semantic_warning("Declaring structure without name has no effect.");
    }
    else if(struct_spec->scope_struct == NULL)
    {
        semantic_warning("Usage of existing structure type is not a declaration.");
    }
    return ctf_ast_visitor_visit_type_spec(visitor, &struct_spec->base);
}

static int constructor_ops_visit_variant_decl(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_variant_decl* variant_decl)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);

    struct ctf_parse_variant_spec* variant_spec = variant_decl->variant_spec;

    if(variant_spec->variant_name == NULL)
    {
        semantic_warning("Declaring variant without name has no effect.");
    }
    else if(variant_spec->scope_variant == NULL)
    {
        semantic_warning("Usage of existing variant type is not a declaration.");
    }
    return ctf_ast_visitor_visit_type_spec(visitor, &variant_spec->base);
}


static int constructor_ops_visit_enum_decl(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_enum_decl* enum_decl)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);

    struct ctf_parse_enum_spec* enum_spec = enum_decl->enum_spec;

    if(enum_spec->enum_name == NULL)
    {
        semantic_warning("Declaring enumeration without name has no effect.");
    }
    else if(enum_spec->scope_enum == NULL)
    {
        semantic_warning("Usage of existing enumeration type is not a declaration.");
    }
    return ctf_ast_visitor_visit_type_spec(visitor, &enum_spec->base);
}


static int constructor_ops_visit_scope_top_decl(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_scope_top_decl* scope_top_decl)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;
    
    int result = ctf_meta_top_scope_begin(meta, scope_top_decl->scope_name);
    if(result < 0)
    {
        semantic_error("Failed to create top-level scope.");
    }
    
    result = ctf_ast_visitor_visit_scope(visitor,
        &scope_top_decl->scope_top->base);
    
    if(result < 0) return result;
    
    result = ctf_meta_top_scope_end(meta);
    if(result < 0)
    {
        semantic_error("Failed to end top-level scope.");
    }
    return 0;
}

static int constructor_ops_visit_field_decl(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_field_decl* field_decl)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;
    
    //TODO: process arrays
    int result = ctf_ast_visitor_visit_type_spec(visitor,
        field_decl->type_spec);
    if(result < 0) return result;
    
    struct ctf_parse_type_post_mod* type_post_mod;
    ctf_parse_type_post_mod_list_for_each_mod(
        field_decl->type_post_mod_list, type_post_mod)
    {
        result = ctf_ast_visitor_visit_type_post_mod(visitor, type_post_mod);
        if(result < 0) return result;
    }
        
    
    switch(ctf_parse_scope_get_type(field_decl->base.scope_parent))
    {
    case ctf_parse_scope_type_struct:
        result = ctf_meta_struct_add_field(meta, field_decl->field_name,
            constructor->current_type);
        if(result < 0)
        {
            semantic_error("Failed to add field to the structure.");
        }
    break;
    case ctf_parse_scope_type_variant:
        result = ctf_meta_variant_add_field(meta, field_decl->field_name,
            constructor->current_type);
        if(result < 0)
        {
            semantic_error("Failed to add field to the variant.");
        }
    break;
    default:
        semantic_error("Incorrect scope for declare field.");
    }
    return 0;
}

static int constructor_ops_visit_param_def(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_param_def* param_def)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;
    int result;
    
    switch(ctf_parse_scope_get_type(param_def->base.scope_parent))
    {
    case ctf_parse_scope_type_integer:
        result = assign_int_parameter(meta, param_def->param_name,
            param_def->param_value);
    break;
    default:
        semantic_error("Parameters cannot be defined in the given scope.");
    }
    return result;
}

static int constructor_ops_visit_type_assignment(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_type_assignment* type_assignment)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);
    struct ctf_meta* meta = constructor->meta;
    int result;
    
    result = ctf_ast_visitor_visit_type_spec(visitor, type_assignment->type_spec);
    if(result < 0) return result;
    /* 'current_type' contains type */
    switch(ctf_parse_scope_get_type(type_assignment->base.scope_parent))
    {
    case ctf_parse_scope_type_top:
        result = ctf_meta_assign_type(meta, type_assignment->tag,
            constructor->current_type);
        if(result < 0)
        {
            semantic_error("Failed to assign type.");
        }
    break;
    default:
        semantic_error("Type cannot be assigned in given scope.");
    }
    return result;
}

static int constructor_ops_visit_typedef_decl(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_typedef_decl* typedef_decl)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);

    struct ctf_meta* meta = constructor->meta;
    
    struct ctf_parse_type_spec* type_spec_base =
        typedef_decl->type_spec_base;

    int result = ctf_ast_visitor_visit_type_spec(visitor,
        type_spec_base);
    
    if(result < 0) return result;
    
    struct ctf_parse_type_post_mod* type_post_mod;
    ctf_parse_type_post_mod_list_for_each_mod(
        typedef_decl->type_post_mod_list, type_post_mod)
    {
        result = ctf_ast_visitor_visit_type_post_mod(visitor, type_post_mod);
        if(result) return result;
    }
    
    struct ctf_type* type = ctf_meta_typedef_create(meta,
        typedef_decl->type_name, constructor->current_type, 0);
    
    if(type == NULL)
    {
        semantic_error("Failed to create typedefed type '%s'.",
            typedef_decl->type_name);
    }
    
    /* Declaration shouldn't set current_type */
    return 0;
}

static int constructor_ops_visit_type_post_mod_array(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_type_post_mod_array* type_post_mod_array)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);

    struct ctf_meta* meta = constructor->meta;

    const char* endptr;
    errno = 0;
    long array_len = strtol(type_post_mod_array->array_len, (char**)&endptr, 0);
    
    if((errno != 0) || (*endptr != '\0'))
    {
        semantic_error("Failed parse array length as integer.");
    }
    //TODO: more descriptive array name
    constructor->current_type = ctf_meta_array_create(meta, "@array[]",
        (int)array_len, constructor->current_type, 1);
    if(constructor->current_type == NULL)
    {
        semantic_error("Failed to create array type.");
    }
    
    return 0;
}

static int constructor_ops_visit_type_post_mod_sequence(
    struct ctf_ast_visitor* visitor,
    struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence)
{
    struct ctf_meta_constructor* constructor = container_of(
        visitor, typeof(*constructor), base);

    struct ctf_meta* meta = constructor->meta;

    //TODO: more descriptive sequence name
    constructor->current_type = ctf_meta_sequence_create(meta,
        "@sequence[]", type_post_mod_sequence->sequence_len,
        constructor->current_type, 1);
    if(constructor->current_type == NULL)
    {
        semantic_error("Failed to create sequence type.");
    }
    
    return 0;
}


static struct ctf_ast_visitor_operations constructor_ops =
{
    .visit_scope_root       = constructor_ops_visit_scope_root,
    .visit_scope_top        = constructor_ops_visit_scope_top,
    .visit_scope_struct     = constructor_ops_visit_scope_struct,
    .visit_scope_variant     = constructor_ops_visit_scope_variant,
    .visit_scope_enum       = constructor_ops_visit_scope_enum,
    .visit_scope_int        = constructor_ops_visit_scope_int,
    
    .visit_struct_decl      = constructor_ops_visit_struct_decl,
    .visit_variant_decl      = constructor_ops_visit_variant_decl,
    .visit_enum_decl        = constructor_ops_visit_enum_decl,
    .visit_typedef_decl     = constructor_ops_visit_typedef_decl,
    .visit_scope_top_decl   = constructor_ops_visit_scope_top_decl,
    .visit_field_decl       = constructor_ops_visit_field_decl,
    .visit_param_def        = constructor_ops_visit_param_def,
    .visit_type_assignment  = constructor_ops_visit_type_assignment,
    
    .visit_struct_spec      = constructor_ops_visit_struct_spec,
    .visit_variant_spec      = constructor_ops_visit_variant_spec,
    .visit_enum_spec        = constructor_ops_visit_enum_spec,
    .visit_type_spec_id     = constructor_ops_visit_type_spec_id,
    .visit_int_spec         = constructor_ops_visit_int_spec,
    
    .visit_enum_value_simple = constructor_ops_visit_enum_value_simple,
    .visit_enum_value_presize = constructor_ops_visit_enum_value_presize,
    .visit_enum_value_range = constructor_ops_visit_enum_value_range,
    
    .visit_type_post_mod_array = constructor_ops_visit_type_post_mod_array,
    .visit_type_post_mod_sequence = constructor_ops_visit_type_post_mod_sequence,
};

void ctf_meta_constructor_init(
    struct ctf_meta_constructor* constructor,
    struct ctf_meta* meta)
{
    constructor->meta = meta;
    constructor->base.visitor_ops = &constructor_ops;
}

/********************* Helpers implementation *************************/
char* strappend_format(char* str, const char* append_format,...)
{
    char* result_str;
    va_list ap;

    va_start(ap, append_format);
    int append_len = vsnprintf(NULL, 0, append_format, ap);
    va_end(ap);
    
    int len = str ? strlen(str) : 0;
    result_str = realloc(str, len + append_len + 1);
    if(result_str == NULL)
    {
        printf("Failed to reallocate string for append.\n");
        return NULL;
    }
    
    va_start(ap, append_format);
    vsnprintf(result_str + len, append_len + 1, append_format, ap);
    va_end(ap);
    
    return result_str;
}


int assign_int_parameter(struct ctf_meta* meta,
    const char* param_name, const char* param_value)
{
#define param_is(str) (strcmp(param_name, str) == 0)
#define value_is(str) (strcmp(param_value, str) == 0)

    if(param_is("signed"))
    {
        int is_signed;

        if(value_is("true"))
            is_signed = 1;
        else if(value_is("false"))
            is_signed = 0;
        else
            semantic_error("Unknown value of 'signed' integer parameter: "
                "%s.", param_value);

        if(ctf_meta_int_set_signed(meta, is_signed) < 0)
            semantic_error("Failed to set signedness for integer.");
    }
    else if(param_is("byte_order"))
    {
        enum ctf_int_byte_order byte_order;
        
        if(value_is("native"))
            byte_order = ctf_int_byte_order_native;
        else if(value_is("network") || value_is("be"))
            byte_order = ctf_int_byte_order_be;
        else if(value_is("le"))
            byte_order = ctf_int_byte_order_le;
        else
            semantic_error("Unknown value of 'byte_order' integer parameter: "
                "%s.", param_value);

        if(ctf_meta_int_set_byte_order(meta, byte_order) < 0)
            semantic_error("Failed to set byte order for integer.");
    }
    else if(param_is("size"))
    {
        const char* number_ends;
        errno = 0;
        int size = (int)strtoul(param_value, (char**)&number_ends, 0);
        if((errno != 0) || (*number_ends != '\0'))
            semantic_error("Failed to parse 'size' parameter as unsigned integer: "
                "%s.", param_value);

        if(ctf_meta_int_set_size(meta, size) < 0)
            semantic_error("Failed to set size for integer.");
    }
    else if(param_is("align"))
    {
        const char* number_ends;
        errno = 0;
        int align = (int)strtoul(param_value, (char**)&number_ends, 0);
        if((errno != 0) || (*number_ends != '\0'))
            semantic_error("Failed to parse 'align' parameter as unsigned integer: "
                "%s.", param_value);

        if(ctf_meta_int_set_align(meta, align) < 0)
            semantic_error("Failed to set size for integer.");
    }
    else if(param_is("base"))
    {
        enum ctf_int_base base;
        
        if(value_is("decimal") || value_is("dec") || value_is("")
            || value_is("d") || value_is("i") || value_is("10"))
            base = ctf_int_base_decimal;
        else if(value_is("u"))
            base = ctf_int_base_unsigned;
        else if(value_is("hexadecimal") || value_is("hex") || value_is("x")
            || value_is("16"))
            base = ctf_int_base_hexadecimal;
        else if(value_is("X"))
            base = ctf_int_base_hexadecimal_upper;
        else if(value_is("p"))
            base = ctf_int_base_pointer;
        else if(value_is("octal") || value_is("oct") || value_is("o")
            || value_is("8"))
            base = ctf_int_base_octal;
        else if(value_is("binary") || value_is("b") || value_is("2"))
            base = ctf_int_base_binary;
        else
            semantic_error("Unknown value of 'base' integer parameter: "
                "%s.", param_value);

        if(ctf_meta_int_set_base(meta, base) < 0)
            semantic_error("Failed to set base for integer.");
    }
    else if(param_is("encoding"))
    {
        enum ctf_int_encoding encoding;
        
        if(value_is("none"))
            encoding = ctf_int_encoding_none;
        if(value_is("UTF8"))
            encoding = ctf_int_encoding_utf8;
        if(value_is("ASCII"))
            encoding = ctf_int_encoding_ascii;
        else
            semantic_error("Unknown value of 'encoding' integer parameter: "
                "%s.", param_value);

        if(ctf_meta_int_set_encoding(meta, encoding) < 0)
            semantic_error("Failed to set encoding for integer.");
    }
    else
    {
        semantic_warning("Unknown integer parameter: %s.",
            param_name);
    }
#undef param_is
#undef value_is
    return 0;
}
