#include <stdio.h>
#include <kedr/ctf_reader/ctf_reader.h>

#include "malloc.h" /* malloc */
#include "string.h" /* string operations */

#include <errno.h> /* error codes */

#include <assert.h> /* assert macro */

#include "ctf_meta.h"
#include "ctf_type.h"
#include "ctf_tag.h"
#include "ctf_scope.h"

#include "ctf_meta_internal.h"


/* 
 * Helper for the next function.
 * 
 * Check if given symbol may terminate variable name component.
 */
static int is_var_component_delimiter(char c)
{
    switch(c)
    {
    case '\0': return 1;
    case '.': return 1;
    case '[' : return 1;
    default: return 0;
    }
}


/* 
 * Similar as 'test_tag_component', but process variable names.
 */

const char* test_var_component(const char* name,
    const char* str)
{
    assert(*name != '\0');
    /* Something like strcmp but with additional processing */
    const char* str_current;
    const char* name_current;
    for(str_current = str, name_current = name;
        *str_current != '\0';
        str_current++, name_current++)
    {
        if(*str_current != *name_current) break;
    }

    if(*name_current != '\0') return NULL;
    return is_var_component_delimiter(*str_current) ? str_current : NULL;
}
/*************************CTF context**********************************/
struct ctf_context* ctf_context_get_context_for_var(
    struct ctf_context* context, struct ctf_var* var)
{
    struct ctf_var* context_var = var + var->context_index;
    for(;context != NULL; context = context->parent_context)
    {
        if(context->variable == context_var) return context;
    }
    return NULL;
}

/* Create empty context for given variable */
static struct ctf_context* ctf_context_create(struct ctf_var* var,
    struct ctf_meta* meta)
{
    struct ctf_context* context = malloc(sizeof(*context));
    if(context == NULL)
    {
        ctf_err("Failed to allocate context structure.");
        return NULL;
    }
    
    context->variable = var;
    context->meta = meta;
    context->parent_context = NULL;
    context->context_impl = NULL;
    
    context->map_size = 0;
    
    return context;
}

int ctf_context_extend_map(struct ctf_context* context,
    int new_size, const char** map_start_p, int* start_shift_p)
{
    if(new_size > context->map_size)
    {
        struct ctf_context_impl* context_impl = context->context_impl;
        assert(context_impl);

        const char* map_start;
        int map_start_shift;
        int map_size = context_impl->map_ops->extend_map(context_impl,
            new_size, &map_start, &map_start_shift);
        if(map_size < 0) return map_size;
        
        context->map_start = map_start;
        context->map_start_shift = map_start_shift;
        context->map_size = map_size;
    }
    if(map_start_p) *map_start_p = context->map_start;
    if(start_shift_p) *start_shift_p = context->map_start_shift;
    
    return context->map_size;
}


int ctf_context_set_impl(struct ctf_context* context,
    struct ctf_context_impl* context_impl)
{
    /* Request initial mapping */
    const char* map_start;
    int map_start_shift;
    int map_size = context_impl->map_ops->extend_map(
        context_impl, 0,
        &map_start, &map_start_shift);
    
    if(map_size < 0)
    {
        ctf_err("Initial mapping request for context failed.");
        return map_size;
    }
    
    context->context_impl = context_impl;
    
    context->map_size = map_size;
    context->map_start = map_start;
    context->map_start_shift = map_start_shift;
    
    return 0;
}

void ctf_context_set_parent(struct ctf_context* context,
    struct ctf_context* parent_context)
{
    context->parent_context = parent_context;
}


struct ctf_context* ctf_meta_create_context(struct ctf_meta* meta,
    struct ctf_var* var, struct ctf_context_info* context_info,
    struct ctf_context* base_context)
{
    if(var->context_index != 0)
    {
        ctf_err("Given variable doesn't require its own context.");
        return NULL;
    }
    
    struct ctf_var* root_var = ctf_var_get_parent(var);
    
    if(ctf_type_get_type(ctf_var_get_type(root_var)) != ctf_type_type_root)
    {
        ctf_err("ctf_meta_create_context() create context only for top-level variables.");
        return NULL;
    }
    //printf("123\n");
    struct ctf_context* context = ctf_context_create(var, meta);
    if(context == NULL) return NULL;

    const struct ctf_var_impl_root_operations* root_ops =
        container_of(root_var->var_impl->interpret_ops, typeof(*root_ops), base);
    //printf("345\n");
    int result = root_ops->set_context_impl(context,
        root_var->var_impl, root_var, var, base_context, context_info);
    //printf("567\n");
    if(result < 0)
    {
        ctf_context_destroy(context);
        return NULL;
    }

    return context;
}

int ctf_context_is_top(struct ctf_context* context)
{
    enum ctf_context_type context_type =
        context->context_impl->interpret_ops->get_type(
            context->context_impl);
    
    return context_type == ctf_context_type_top;
}


struct ctf_context* ctf_var_elem_create_context(struct ctf_var* var,
    struct ctf_context* base_context, int element_index)
{
    if(var->context_index != 0)
    {
        ctf_err("Given variable doesn't require its own context.");
        return NULL;
    }
    
    struct ctf_var* array_var = ctf_var_get_parent(var);
    enum ctf_type_type array_var_type = 
        ctf_type_get_type(ctf_var_get_type(array_var));
    
    if((array_var_type != ctf_type_type_array)
        && (array_var_type != ctf_type_type_sequence))
    {
        ctf_err("ctf_var_elem_create_context() create context only for "
            "elements of array or sequences.");
        return NULL;
    }

    base_context = ctf_context_get_context_for_var(base_context, array_var);
    if(base_context == NULL)
    {
        ctf_err("Base context is insufficient for create context for array element.");
        return NULL;
    }
    
    struct ctf_context* context = ctf_context_create(var, base_context->meta);
    if(context == NULL) return NULL;

    const struct ctf_var_impl_array_operations* array_ops =
        container_of(array_var->var_impl->interpret_ops, typeof(*array_ops), base);
    
    int result = array_ops->set_context_impl_elem(context,
            array_var->var_impl, array_var, var, base_context);
    if(result < 0)
    {
        ctf_context_destroy(context);
        return NULL;
    }
    
    assert(ctf_context_is_elem(context));
    
    if(element_index > 0)
    {
        const struct ctf_context_impl_elem_operations* elem_ops =
            container_of(context->context_impl->interpret_ops,
                typeof(*elem_ops), base);
        int result = elem_ops->set_elem_index(context, element_index);
        if(result < 0)
        {
            ctf_context_destroy(context);
            return NULL;
        }
    }
    
    return context;
}

int ctf_context_is_elem(struct ctf_context* context)
{
    enum ctf_context_type context_type =
        context->context_impl->interpret_ops->get_type(
            context->context_impl);
    
    return context_type == ctf_context_type_array_elem;
}

int ctf_context_is_end(struct ctf_context* context)
{
    assert(ctf_context_is_elem(context));
    
    const struct ctf_context_impl_elem_operations* elem_ops =
        container_of(context->context_impl->interpret_ops, typeof(*elem_ops), base);
    
    return elem_ops->is_end(context);
}

int ctf_context_get_element_index(struct ctf_context* context)
{
    assert(ctf_context_is_elem(context));
    
    const struct ctf_context_impl_elem_operations* elem_ops =
        container_of(context->context_impl->interpret_ops, typeof(*elem_ops), base);
    
    return elem_ops->get_elem_index(context);
}


struct ctf_context* ctf_context_set_element_index(struct ctf_context* context,
    int element_index)
{
    const struct ctf_context_impl_elem_operations* elem_ops =
        container_of(context->context_impl->interpret_ops, typeof(*elem_ops), base);

    int result = elem_ops->set_elem_index(context, element_index);

    if(result < 0)
    {
        ctf_context_destroy(context);
        return NULL;
    }
    
    return context;
}

struct ctf_context* ctf_context_set_element_next(
    struct ctf_context* context)
{
    const struct ctf_context_impl_elem_operations* elem_ops =
        container_of(context->context_impl->interpret_ops, typeof(*elem_ops), base);

    int result = elem_ops->set_elem_next(context);

    if(result < 0)
    {
        ctf_context_destroy(context);
        return NULL;
    }
    
    return context;
}


void ctf_context_destroy(struct ctf_context* context)
{
    if(context->context_impl)
        ctf_context_impl_destroy(context->context_impl);
    
    free(context);
}

/************************ CTF variable ********************************/
static int ctf_var_init(struct ctf_var* var, const char* var_name,
    struct ctf_var* parent, struct ctf_var* container)
{
    //printf("Initialize variable at %p (parent: %p, container: %p).\n",
    //    var, parent, container);
    
    if(var_name)
    {
        var->name = strdup(var_name);
        if(var->name == NULL)
        {
            ctf_err("Failed to allocate variable name.");
            return -ENOMEM;
        }
    }
    else
    {
        var->name = NULL;
    }
    
    var->first_child_index = 0;
    var->last_child_index = 0;
    var->next_sibling_index = 0;
    
    if(parent)
    {
        var->parent_index = parent - var;
        
        if(parent->last_child_index)
        {
            struct ctf_var* last_child_var =
                parent + parent->last_child_index;
            
            last_child_var->next_sibling_index = var - last_child_var;
        }
        else
        {
            parent->first_child_index = var - parent;
        }
        parent->last_child_index = var - parent;
        /* Set existence index */

        if(parent->var_impl->layout_ops->is_child_exist
            && (parent->var_impl->layout_ops->is_child_exist(
                parent->var_impl, parent, var, NULL) != 1))
        {
            /* Parent may say that variable is not exist */
            var->existence_index = 0;
        }
        else
        {
            /* Parent always says that variable is exist */
            struct ctf_var* parent_existence =
                ctf_var_get_existence(parent);
            var->existence_index = parent_existence ? parent_existence - var : 1;
        }
    }
    else
    {
        /* Top-level variable is always exist */
        var->existence_index = 1;
    }
    /* Set context index */
    if(container)
    {
        struct ctf_var* container_context = ctf_var_get_context(container);
        var->context_index = container_context - var;
    }
    else
    {
        /* Variable require own context */
        var->context_index = 0;
    }
    
    
    //TODO: should default implementation should be set?
    
    return 0;
}

static void ctf_var_destroy(struct ctf_var* var)
{
    free(var->name);
    if(var->var_impl && var->var_impl->destroy_impl)
        var->var_impl->destroy_impl(var->var_impl);
}

int ctf_var_get_alignment(struct ctf_var* var,
    struct ctf_context* context)
{
    //printf("Alignment of variable %p was requested.\n", var);
    return var->var_impl->layout_ops->get_alignment(var->var_impl,
        var, context);
}

int ctf_var_get_size(struct ctf_var* var,
    struct ctf_context* context)
{
    return var->var_impl->layout_ops->get_size(var->var_impl,
        var, context);
}


struct ctf_type* ctf_var_get_type(struct ctf_var* var)
{
    return var->var_impl->interpret_ops->get_type(var->var_impl);
}

struct ctf_var* ctf_var_find_var(struct ctf_var* var,
    const char* name)
{
    const char* name_rest = name;
    struct ctf_var* var_current = var;
    while(*name_rest != '\0')
    {
        struct ctf_var* child;
        for(child = ctf_var_get_first_child(var_current);
            child != NULL;
            child = ctf_var_get_next_sibling(child))
        {
            const char* name_end = test_var_component(child->name,
                name_rest);
            if(name_end != NULL)
            {
                name_rest = name_end;
                if(*name_rest == '.') name_rest++;
                var_current = child;
                break;
            }
        }
        if(child == NULL) return NULL;/* Not found */
    }
    return var_current;
}

struct ctf_var* ctf_meta_find_var(struct ctf_meta* meta,
    const char* name)
{
    return ctf_var_find_var(&meta->vars[0], name);
}

/* 
 * Helper for the next function.
 * 
 * Prepend string with another one.
 * 
 * Return pointer to the resulted string.
 */
static char* prepend_str(char* str, const char* prefix)
{
    size_t prefix_len = strlen(prefix);
    memcpy(str - prefix_len, prefix, prefix_len);
    
    return str - prefix_len;
}

char* ctf_var_get_full_name(struct ctf_var* var)
{
    if(var->name == NULL)
    {
        /* Strange situation - someone get internal variable */
        ctf_err("Internal variables has no name.");
        return NULL;
    }

    /* Calculate name size */
    int size = strlen(var->name);
    int last_variable_is_array_elem = ctf_var_is_elem(var);

    struct ctf_var* var_tmp;
    for(var_tmp = ctf_var_get_parent(var);
        var_tmp != NULL;
        var_tmp = ctf_var_get_parent(var))
    {
        if(var->name == NULL) continue;
        if(!last_variable_is_array_elem) size++;
        size += strlen(var->name);
    }
    
    /* Allocate name */
    char* full_name = malloc(size + 1);
    if(full_name == NULL)
    {
        ctf_err("Failed to allocate full name of the variable");
        return NULL;
    }
    char* current_str = full_name + size;
    *current_str = '\0';
    
    /* Fill name */
    current_str = prepend_str(current_str, var->name);
    last_variable_is_array_elem = ctf_var_is_elem(var);
    
    for(var_tmp = ctf_var_get_parent(var);
        var_tmp != NULL;
        var_tmp = ctf_var_get_parent(var))
    {
        if(var->name == NULL) continue;
        if(!last_variable_is_array_elem)
            current_str = prepend_str(current_str, ".");
        current_str = prepend_str(current_str, var->name);
    }
    ctf_bug_on(current_str != full_name);
    
    return full_name;
}

int ctf_var_is_exist(struct ctf_var* var, struct ctf_context* context)
{
    struct ctf_var* current_var = var;
    
    while(current_var->existence_index <= 0)
    {
        struct ctf_var* existence_var =
            current_var->existence_index + current_var;
        struct ctf_var* existence_parent_var =
            ctf_var_get_parent(existence_var);
        
        const struct ctf_var_impl_layout_operations* layout_ops =
            existence_parent_var->var_impl->layout_ops;
        assert(layout_ops->is_child_exist);
        switch(layout_ops->is_child_exist(existence_parent_var->var_impl,
            existence_parent_var, existence_var, context))
        {
        case 1:
            current_var = existence_parent_var;
            break;
        case 0:
            return 0;
        case -1:
            return -1;
        default:
            ctf_err("is_child_exist() callback returns incorrect value.");
            assert(0);
        }
    }
    return 1;
}

const char* ctf_var_get_map(struct ctf_var* var,
    struct ctf_context* context, int* start_shift)
{
    assert(ctf_var_is_exist(var, context) == 1);
    
    context = ctf_context_get_context_for_var(context, var);
    if(context == NULL) return NULL;
    
    int end_offset = ctf_var_get_end_offset(var, context);
    if(end_offset == -1) return NULL;
    
    if(context->map_size < end_offset)
    {
        //printf("Extend map to the end of variable(%d).\n", end_offset);
        const char* map_start;
        int map_start_shift;
        int map_size = context->context_impl->map_ops->extend_map(
            context->context_impl, end_offset,
            &map_start, &map_start_shift);
        if(map_size >= 0)
        {
            context->map_size = map_size;
            context->map_start = map_start;
            context->map_start_shift = map_start_shift;
        }
        if(map_size < end_offset)
        {
            ctf_err("Error occures while extending context's mapping.");
            return NULL;
        }
        
    }
    int start_offset = ctf_var_get_start_offset(var, context);
    assert(start_offset != -1);
    
    if(start_shift)
    {
        *start_shift = (context->map_start_shift + start_offset) % 8;
    }
    
    return context->map_start +
        (context->map_start_shift + start_offset) / 8;
}

int ctf_var_is_elem(struct ctf_var* var)
{
    return var->name && (strcmp(var->name, "[]") == 0);
}

/************************* Build information **************************/
static struct ctf_meta_build_info* ctf_meta_build_info_create(void)
{
    struct ctf_meta_build_info* build_info = 
        malloc(sizeof(*build_info));
    if(build_info == NULL)
    {
        ctf_err("Failed to allocate CTF meta build information structure.");
        return NULL;
    }
    
    build_info->layout_info = NULL;
    build_info->current_scope = NULL;/* should be set after */
    build_info->current_type = NULL;/* should be set after */
    
    return build_info;
}

static void ctf_meta_build_info_destroy(
    struct ctf_meta_build_info* build_info)
{
    free(build_info->layout_info);
    free(build_info);
}

struct ctf_meta* ctf_meta_create(void)
{
    struct ctf_meta* meta = malloc(sizeof(*meta));
    if(meta == NULL)
    {
        ctf_err("Failed to allocate structure with CTF meta information.");
        return NULL;
    }
    
    meta->vars = NULL;
    meta->vars_n = 0;
    
    struct ctf_type_impl* type_impl = ctf_type_impl_create_root();
    if(type_impl == NULL) goto err_type_impl;

    struct ctf_type* type = ctf_type_create(NULL);
    if(type == NULL)
    {
        ctf_type_impl_destroy(type_impl);
        goto err_type_impl;
    }
    
    ctf_type_set_impl(type, type_impl);

    struct ctf_scope* scope = ctf_scope_create_root(type);
    if(scope == NULL) goto err_scope;
    
    struct ctf_meta_build_info* build_info = ctf_meta_build_info_create();
    if(build_info == NULL) goto err_build_info;

    meta->root_type = type;
    meta->root_scope = scope;
    
    build_info->current_scope = scope;
    build_info->current_type = type;
    
    meta->build_info = build_info;
    
    return meta;

err_build_info:
    ctf_scope_destroy(scope);
err_scope:
    ctf_type_destroy(type);
err_type_impl:
    free(meta);
    return NULL;
}

void ctf_meta_destroy(struct ctf_meta* meta)
{
    if(meta->build_info) ctf_meta_build_info_destroy(meta->build_info);
    
    struct ctf_var* var;
    struct ctf_var* var_end = meta->vars + meta->vars_n;
    for(var = meta->vars ; var != var_end; var++)
    {
        ctf_var_destroy(var);
    }
    free(meta->vars);
    
    ctf_scope_destroy(meta->root_scope);
    ctf_type_destroy(meta->root_type);
    
    free(meta);
}

/************************* CTF tag ************************************/
struct ctf_tag* ctf_meta_make_tag(struct ctf_meta* meta,
    const char* str)
{
    struct ctf_tag* tag;
    
    const char* unresolved_component;;
    
    struct ctf_type* base_type;
    
    /* Firstly test, whether tag is relative */
    base_type = meta->build_info->current_type;
    if(base_type == meta->root_type) goto absolute_scope;
    
    tag = ctf_tag_create(base_type, str, &unresolved_component);
    if(tag != NULL)
    {
        if(*unresolved_component == '\0') return tag;
        
        ctf_err("Failed to resolve tag subcomponents '%s' in type %s.",
            unresolved_component, ctf_tag_get_type(tag)->name);
        
        ctf_tag_destroy(tag);
        return NULL;
    }
    /* 
     * Assume that tag failed to create because of first
     * unresolved component.
     */

    /* Test, whether tag is absolute */
    base_type = meta->root_type;

absolute_scope:   
    tag = ctf_tag_create(base_type, str, &unresolved_component);

    if(tag != NULL)
    {
        if(*unresolved_component == '\0') return tag;
        
        ctf_err("Failed to resolve tag subcomponents '%s' in global scope.",
            unresolved_component);
        
        ctf_tag_destroy(tag);
        return NULL;
    }
    
    ctf_err("Failed to resolve tag '%s'.", str);
    
    return NULL;
}

/*************************************************************/

var_rel_index_t ctf_meta_add_var(struct ctf_meta* meta,
    const char* var_name, struct ctf_type* var_type,
    struct ctf_var* parent,
    struct ctf_var* container, struct ctf_var* prev)
{
    //printf("Add variable with parent %p, container %p, prev %p.\n",
    //    parent, container, prev);
    
    int result;
    
    int new_var_index = meta->vars_n;
    /* Store indices before reallocation of variables array */
    var_rel_index_t parent_index = parent
        ? parent - meta->vars - new_var_index: 0;
    var_rel_index_t container_index = container ?
        container - meta->vars - new_var_index: 0;
    var_rel_index_t prev_index = prev ?
        prev - meta->vars - new_var_index: 0;
    
    struct ctf_var* vars_new = realloc(meta->vars,
        sizeof(*vars_new) * (new_var_index + 1));
    if(vars_new == NULL)
    {
        ctf_err("Failed to allocate array of variables.");
        return -ENOMEM;
    }

    meta->vars = vars_new;
    //printf("Array of variables now starts from %p(size of element is %zu).\n",
    //    meta->vars, sizeof(*meta->vars));
    struct ctf_var_layout_info* layout_info_new = realloc(
        meta->build_info->layout_info,
        sizeof(*layout_info_new) * (new_var_index + 1));
    if(layout_info_new == NULL)
    {
        ctf_err("Failed to allocate array of layout info for variables.");
        return -ENOMEM;
    }
    
    meta->build_info->layout_info = layout_info_new;
    
    struct ctf_var* var = meta->vars + new_var_index;
    
    result = ctf_var_init(var, var_name,
        parent ? var + parent_index : NULL,
        container ? var + container_index : NULL);
    if(result < 0) return result;
    
    struct ctf_var_layout_info* layout_info = 
        meta->build_info->layout_info + new_var_index;

    layout_info->container_index = container_index;
    layout_info->prev_index = prev_index;
    
    meta->vars_n = new_var_index + 1;
    
    result = ctf_type_set_var_impl(var_type, var, meta);
    
    if(result < 0)
    {
        /* Additional error message */
        char* full_name = ctf_var_get_full_name(var);
        ctf_err("Instantiation of variable %s failed.", full_name);
        free(full_name);
        /* Variable array will be freed at top-level. */
        return result;
    }
    
    //printf("Variable with index %d is created.\n", new_var_index);
    return new_var_index;
}


int ctf_meta_instantiate(struct ctf_meta* meta)
{
    int result;
    if(meta->build_info == NULL)
    {
        ctf_err("Meta information already freezed.");
        return -EINVAL;
    }
    
    if(meta->build_info->current_type != meta->root_type)
    {
        ctf_err("Meta information cannot be freezed while there are "
            "types under constuction.");
        return -EINVAL;
    }
    
    if(meta->build_info->current_scope != meta->root_scope)
    {
        ctf_err("Meta information cannot be freezed while there are "
            "scopes under constuction.");
        return -EINVAL;
    }
    
    result = (int)ctf_meta_add_var(meta, NULL, meta->root_type,
        NULL, NULL, NULL);
    
    if(result < 0)
    {
        ctf_err("Failed to create variables. Rollback.");
        
        int i;
        for(i = meta->vars_n - 1; i >= 0; i--)
        {
            ctf_var_destroy(&meta->vars[i]);
        }
        
        free(meta->vars);
        meta->vars = NULL;
        
        free(meta->build_info->layout_info);
        meta->build_info->layout_info = NULL;
        
        return result;
    }
    
    ctf_meta_build_info_destroy(meta->build_info);
    meta->build_info = NULL;
    /* Success! */
    //printf("Meta information has been instantiated.\n");
    
    return 0;
}

/********************* Variable interpretation ************************/
int ctf_var_contains_int(struct ctf_var* var)
{
    enum ctf_type_type type_type =
        ctf_type_get_type(ctf_var_get_type(var));
    
    return (type_type == ctf_type_type_int)
        || (type_type == ctf_type_type_enum);
}

void ctf_var_copy_int(void* dest, struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    
    int_ops->copy_int(dest, var->var_impl, var, context);
}

int ctf_var_is_fit_int32(struct ctf_var* var)
{
    return ctf_var_get_size(var, NULL) <= 32;
}

uint32_t ctf_var_get_int32(struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    
    assert(int_ops->get_int32);
    return int_ops->get_int32(var->var_impl, var, context);
}


int ctf_var_is_fit_int64(struct ctf_var* var)
{
    return ctf_var_get_size(var, NULL) <= 64;
}

uint64_t ctf_var_get_int64(struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_int_operations* int_ops =
        container_of(var->var_impl->interpret_ops, typeof(*int_ops), base);
    
    assert(int_ops->get_int64);
    return int_ops->get_int64(var->var_impl, var, context);
}

int ctf_var_is_enum(struct ctf_var* var)
{
    enum ctf_type_type type_type =
        ctf_type_get_type(ctf_var_get_type(var));
    
    return type_type == ctf_type_type_enum;
}

const char* ctf_var_get_enum(struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_enum_operations* enum_ops =
        container_of(var->var_impl->interpret_ops, typeof(*enum_ops), base.base);
    
    assert(enum_ops->get_enum);
    return enum_ops->get_enum(var->var_impl, var, context);
}


int ctf_var_is_variant(struct ctf_var* var)
{
    enum ctf_type_type type_type =
        ctf_type_get_type(ctf_var_get_type(var));
    
    return type_type == ctf_type_type_variant;
}

int ctf_var_get_variant(struct ctf_var* var,
    struct ctf_context* context, struct ctf_var** active_field_p)
{
    const struct ctf_var_impl_variant_operations* variant_ops =
        container_of(var->var_impl->interpret_ops, typeof(*variant_ops), base);

    return variant_ops->get_active_field(var->var_impl, var, context,
        active_field_p);
}

int ctf_var_contains_array(struct ctf_var* var)
{
    enum ctf_type_type type_type =
        ctf_type_get_type(ctf_var_get_type(var));
    return (type_type == ctf_type_type_array)
        || (type_type == ctf_type_type_sequence);
}

int ctf_var_get_n_elems(struct ctf_var* var,
    struct ctf_context* context)
{
    const struct ctf_var_impl_array_operations* array_ops =
        container_of(var->var_impl->interpret_ops, typeof(*array_ops), base);
        
    return array_ops->get_n_elems(var->var_impl, var, context);
}

/************************* CTF meta construction **********************/
/* Returns last type which has been added but not freezed */
struct ctf_type* ctf_meta_get_current_type(struct ctf_meta* meta)
{
    struct ctf_type* current_type = meta->build_info->current_type;
    /* do not return root type */
    if(current_type == meta->root_type) return NULL;
    
    return current_type;
}

/* 
 * Search type with given name.
 * 
 * Possible scopes for search are detected automatically.
 */
struct ctf_type* ctf_meta_find_type(struct ctf_meta* meta,
    const char* name)
{
    struct ctf_scope* scope_current = meta->build_info->current_scope;
    assert(scope_current);
    
    struct ctf_type* type = ctf_scope_find_type(scope_current, name);

    if(type == NULL) return NULL;
    
    /* Now check that type is fully constructed */
    if(type == meta->build_info->current_type)
    {
        //printf("Type with name '%s' is currently build.\n", name);
        return NULL;
    }
    
    /* 
     * 1) Type is under costruction -> its scope is under construction.
     * 2) Scope is under construction-> type connected is under construction.
     * 3) Scope is under construction-> its parent scope is under construction.
     * 
     * If scope connected to some type, scope of that type is always
     * precessor of connected scope. So we may safetly ignore relation 1
     * when generate next scope for test, whether given type is 
     * under costruction.
     */
    
    struct ctf_scope* scope_constructed;
    
    for(scope_constructed = scope_current;
        scope_constructed != NULL;
        scope_constructed = ctf_scope_get_parent(scope_constructed))
    {
        if(type == ctf_scope_get_type_connected(scope_constructed))
        {
            //printf("Type with name '%s' is under construction.\n", name);
            return NULL;
        }
    }
    
    return type;
}

struct ctf_type* ctf_meta_find_type_strict(struct ctf_meta* meta,
    const char* name)
{
    struct ctf_scope* scope_current = meta->build_info->current_scope;
    assert(scope_current);
    
    struct ctf_type* type = ctf_scope_find_type_strict(scope_current, name);

    if(type == NULL) return NULL;
    
    /* Now check that type is fully constructed */
    if(type == meta->build_info->current_type) return NULL;
    
    return type;
}

/* 
 * Check that current scope support type addition.
 * If it so, return it.
 * 
 * Otherwise print error and return NULL.
 */
static struct ctf_scope* ctf_meta_get_scope_for_new_type(
    struct ctf_meta* meta)
{
    struct ctf_meta_build_info* build_info = meta->build_info;
    assert(build_info);
    
    struct ctf_scope* current_scope = build_info->current_scope;
    assert(current_scope);
    
    if(!ctf_scope_is_support_types(current_scope))
    {
        ctf_err("Cannot add type because current scope doesn't support "
            "inner types");
        return NULL;
    }
    
    struct ctf_type* current_type = build_info->current_type;
    if(current_type->scope == current_scope)
    {
        ctf_err("Currently constructed type should be commited before "
            "new type may be added.");
        return NULL;
    }
    
    return current_scope;
}

/* 
 * Common type 'starter'.
 */
static int ctf_meta_type_begin(struct ctf_meta* meta,
    const char* type_name, struct ctf_type_impl* type_impl,
    int is_internal)
{
    struct ctf_scope* current_scope =
        ctf_meta_get_scope_for_new_type(meta);
    if(current_scope == NULL) return -EINVAL;
    
    struct ctf_type* type;
    //if(is_internal) printf("Begin internal type '%s'.\n", type_name);
    if(!is_internal)
    {
        if(ctf_scope_find_type_strict(current_scope, type_name))
        {
            ctf_err("Type '%s' is already defined in this scope.", type_name);
            return -EEXIST;
        }
        type = ctf_scope_create_type(current_scope, type_name);
    }
    else
    {
        type = ctf_scope_create_type_internal(current_scope, type_name);
    }
    if(type == NULL) return -ENOMEM;
    
    //printf("Type '%s'(%s) has been created in scope %p.\n", type_name,
    //    is_internal ? "internal" : "non-internal", current_scope);
    
    ctf_type_set_impl(type, type_impl);
    
    meta->build_info->current_type = type;
    
    return 0;
}

/* 
 * Create scope connected to the current type.
 * 
 * Newly created scope become current.
 * 
 * NOTE: not all types support connected scopes.
 */
static int ctf_meta_scope_connected_begin(struct ctf_meta* meta)
{
    struct ctf_type* type = meta->build_info->current_type;
    assert(type);
    assert(type != meta->root_type);
    
    struct ctf_scope* scope_connected = ctf_scope_create_for_type(type);
    if(scope_connected == NULL) return -ENOMEM;
    
    meta->build_info->current_scope = scope_connected;
    
    return 0;
}

/* 
 * End scope connected to some type.
 */
static void ctf_meta_scope_connected_end(struct ctf_meta* meta)
{
    struct ctf_scope* current_scope = meta->build_info->current_scope;
    assert(current_scope);
    assert(current_scope != meta->root_scope);
    
    struct ctf_type* type_connected =
        ctf_scope_get_type_connected(current_scope);
    assert(type_connected);
    
    meta->build_info->current_scope = type_connected->scope;
}

/* 
 * Common type 'commiter'.
 */
static void ctf_meta_type_end(struct ctf_meta* meta)
{
    struct ctf_type** current_type_p = &meta->build_info->current_type;
    struct ctf_scope* current_scope = meta->build_info->current_scope;

    assert((*current_type_p)->scope == current_scope);

    struct ctf_scope* scope_constructed;
    for(scope_constructed = current_scope;
        scope_constructed != NULL;
        scope_constructed = ctf_scope_get_parent(scope_constructed))
    {
        struct ctf_type* type = ctf_scope_get_type_connected(
            scope_constructed);
        if(type)
        {
            *current_type_p = type;
            return;
        }
    }
    ctf_bug();
}

static struct ctf_type* ctf_meta_get_current_type_checked(
    struct ctf_meta* meta, enum ctf_type_type type_type,
    const char* type_metaname)
{
    struct ctf_type* type = ctf_meta_get_current_type(meta);
    if(type == NULL)
    {
        ctf_err("No type is currently constructed.");
        return NULL;
    }
    
    if(ctf_type_get_type(type) != type_type)
    {
        ctf_err("Type under construction is not an %s.",
            type_metaname);
        return NULL;
    }
    
    return type;
}

/* Begin definition of integer type */
int ctf_meta_int_begin(struct ctf_meta* meta,
    const char* name)
{
    struct ctf_type_impl* type_impl_int = ctf_type_impl_int_create();
    if(type_impl_int == NULL) return -ENOMEM;
    
    int result = ctf_meta_type_begin(meta, name, type_impl_int, 1);
    if(result < 0)
    {
        ctf_type_impl_destroy(type_impl_int);
        return result;
    }
    
    return 0;
}

int ctf_meta_int_begin_scope(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return -EINVAL;

    return ctf_meta_scope_connected_begin(meta);
}

/* Set corresponded parameter for the integer */
int ctf_meta_int_set_signed(struct ctf_meta* meta, int is_signed)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return -EINVAL;
    
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_signed(type, is_signed);
}
int ctf_meta_int_set_size(struct ctf_meta* meta, int size)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return -EINVAL;
    
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_size(type, size);
}
int ctf_meta_int_set_align(struct ctf_meta* meta, int align)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return -EINVAL;
    
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_align(type, align);
}
int ctf_meta_int_set_byte_order(struct ctf_meta* meta,
    enum ctf_int_byte_order byte_order)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return -EINVAL;
    
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_byte_order(type, byte_order);
}
int ctf_meta_int_set_base(struct ctf_meta* meta,
    enum ctf_int_base base)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return -EINVAL;
    
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_base(type, base);
}
int ctf_meta_int_set_encoding(struct ctf_meta* meta,
    enum ctf_int_encoding encoding)
{   
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return -EINVAL;
    
    const struct ctf_type_impl_int_operations* int_ops =
        container_of(type->type_impl->interpret_ops, typeof(*int_ops), base);
    
    return int_ops->set_encoding(type, encoding);
}

void ctf_meta_int_end_scope(struct ctf_meta* meta)
{
    ctf_meta_scope_connected_end(meta);
}


/* Finish definition of integer type. Return type constructed. */
struct ctf_type* ctf_meta_int_end(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_int, "integer");
    if(type == NULL) return NULL;
    
    if(ctf_type_end_type(type) < 0) return NULL;
    
    ctf_meta_type_end(meta);
    
    return type;
}

int ctf_meta_struct_begin(struct ctf_meta* meta,
    const char* name, int is_internal)
{
    struct ctf_type_impl* type_impl_struct =
        ctf_type_impl_struct_create();
    if(type_impl_struct == NULL) return -ENOMEM;
    
    int result = ctf_meta_type_begin(meta, name, type_impl_struct,
        is_internal);
    if(result < 0)
    {
        ctf_type_impl_destroy(type_impl_struct);
        return result;
    }
    
    return 0;
}

int ctf_meta_struct_begin_scope(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_struct, "struct");
    if(type == NULL) return -EINVAL;

    return ctf_meta_scope_connected_begin(meta);
}

int ctf_meta_struct_add_field(struct ctf_meta* meta,
    const char* field_name,
    struct ctf_type* field_type)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_struct, "struct");
    if(type == NULL) return -EINVAL;
    
    struct ctf_type_impl_struct_operations* type_ops_struct =
        container_of(type->type_impl->interpret_ops,
            typeof(*type_ops_struct), base);
    
    return type_ops_struct->add_field(type, field_name, field_type);
}

int ctf_meta_struct_has_field(struct ctf_meta* meta,
    const char* field_name)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_struct, "struct");
    if(type == NULL) return -EINVAL;
    
    const char* component_end;
    struct ctf_tag_component* tag_component =
        ctf_type_resolve_tag_component(type, field_name, &component_end);
    if(tag_component)
    {
        ctf_tag_component_destroy(tag_component);
        return 1;
    }
    else
    {
        return 0;
    }
}


void ctf_meta_struct_end_scope(struct ctf_meta* meta)
{
    ctf_meta_scope_connected_end(meta);
}


/* Finish definition of structure. Return type constructed. */
struct ctf_type* ctf_meta_struct_end(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_struct, "struct");
    if(type == NULL) return NULL;
    
    if(ctf_type_end_type(type) < 0) return NULL;
    
    ctf_meta_type_end(meta);
    
    return type;
}

/* Begin definition of enumeration type */
int ctf_meta_enum_begin(struct ctf_meta* meta,
    const char* name,
    struct ctf_type* type_int,
    int is_internal)
{
    struct ctf_type_impl* type_impl_enum =
        ctf_type_impl_enum_create(type_int);
    if(type_impl_enum == NULL) return -ENOMEM;
    
    int result = ctf_meta_type_begin(meta, name, type_impl_enum,
        is_internal);
    if(result < 0)
    {
        ctf_type_impl_destroy(type_impl_enum);
        return result;
    }
    
    return 0;
}

int ctf_meta_enum_begin_scope(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_enum, "enum");
    if(type == NULL) return -EINVAL;

    return ctf_meta_scope_connected_begin(meta);
}


/* 
 * Add value for enumeration type, which base type may be represented
 * as 32-bit integer.
 */
int ctf_meta_enum_add_value32(struct ctf_meta* meta, const char* val,
    int32_t start, int32_t end)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_enum, "enum");
    if(type == NULL) return -EINVAL;
    
    struct ctf_type_impl_enum_operations* type_ops_enum =
        container_of(type->type_impl->interpret_ops,
            typeof(*type_ops_enum), base);

    return type_ops_enum->add_value32(type, val, start, end);

}

/* 
 * Add value for enumeration type, which base type may be represented
 * as 64-bit int type.
 */
int ctf_meta_enum_add_value64(struct ctf_meta* meta, const char* val,
    int64_t start, int64_t end);


void ctf_meta_enum_end_scope(struct ctf_meta* meta)
{
    ctf_meta_scope_connected_end(meta);
}


struct ctf_type* ctf_meta_enum_end(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_enum, "enum");
    if(type == NULL) return NULL;
    
    if(ctf_type_end_type(type) < 0) return NULL;
    
    ctf_meta_type_end(meta);
    
    return type;
}


/* 
 * Begins variant.
 * 
 * For untagged variant 'tag_str' should be NULL.
 * 
 * 'is_internal' should be non-zero for create internal enumeration type,
 * which cannot be searched by name.
 */
int ctf_meta_variant_begin(struct ctf_meta* meta,
    const char* name,
    int is_internal)
{
    struct ctf_type_impl* type_impl_variant =
        ctf_type_impl_variant_create();
    if(type_impl_variant == NULL) return -ENOMEM;
    
    int result = ctf_meta_type_begin(meta, name, type_impl_variant,
        is_internal);
    if(result < 0)
    {
        ctf_type_impl_destroy(type_impl_variant);
        return result;
    }
    
    return 0;
}

int ctf_meta_variant_begin_scope(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_variant, "variant");
    if(type == NULL) return -EINVAL;

    return ctf_meta_scope_connected_begin(meta);
}

int ctf_meta_variant_add_field(struct ctf_meta* meta,
    const char* field_name,
    struct ctf_type* field_type)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_variant, "variant");
    if(type == NULL) return -EINVAL;
    
    struct ctf_type_impl_variant_operations* type_ops_variant =
        container_of(type->type_impl->interpret_ops,
            typeof(*type_ops_variant), base);
    
    return type_ops_variant->add_field(type, field_name, field_type);
}

int ctf_meta_variant_has_field(struct ctf_meta* meta,
    const char* field_name)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_variant, "variant");
    if(type == NULL) return -EINVAL;
    
    const char* component_end;
    struct ctf_tag_component* tag_component =
        ctf_type_resolve_tag_component(type, field_name, &component_end);
    if(tag_component)
    {
        ctf_tag_component_destroy(tag_component);
        return 1;
    }
    else
    {
        return 0;
    }
}


void ctf_meta_variant_end_scope(struct ctf_meta* meta)
{
    ctf_meta_scope_connected_end(meta);
}


/* Finish definition of variant. Return type constructed. */
struct ctf_type* ctf_meta_variant_end(struct ctf_meta* meta)
{
    struct ctf_type* type = ctf_meta_get_current_type_checked(meta,
        ctf_type_type_variant, "variant");
    if(type == NULL) return NULL;
    
    if(ctf_type_end_type(type) < 0) return NULL;
    
    ctf_meta_type_end(meta);
    
    return type;
}

/* Set tag for constructed variant. */
int ctf_meta_variant_set_tag(struct ctf_meta* meta, struct ctf_type* type,
    const char* str)
{
    assert(ctf_type_is_variant(type));
    
    struct ctf_tag* tag = ctf_meta_make_tag(meta, str);
    if(tag == NULL) return -EINVAL;
    
    int result = ctf_type_variant_set_tag(type, tag);
    
    if(result) ctf_tag_destroy(tag);
    
    return result;
}

struct ctf_type* ctf_meta_array_create(struct ctf_meta* meta,
    const char* name, int array_size, struct ctf_type* elem_type,
    int is_internal)
{
    struct ctf_type_impl* type_impl_array =
        ctf_type_impl_array_create(array_size, elem_type);
    if(type_impl_array == NULL) return NULL;
    
    int result = ctf_meta_type_begin(meta, name, type_impl_array,
        is_internal);
    if(result < 0)
    {
        ctf_type_impl_destroy(type_impl_array);
        return NULL;
    }
    
    struct ctf_type* type_array = ctf_meta_get_current_type(meta);
    ctf_bug_on(type_array == NULL);
    
    ctf_meta_type_end(meta);
    
    return type_array;
}

struct ctf_type* ctf_meta_sequence_create(struct ctf_meta* meta,
    const char* name, const char* size_str, struct ctf_type* elem_type,
    int is_internal)
{
    struct ctf_tag* size_tag = ctf_meta_make_tag(meta, size_str);
    if(size_tag == NULL) return NULL;
    
    if(ctf_type_get_type(ctf_tag_get_type(size_tag)) != ctf_type_type_int)
    {
        ctf_err("Size tag for sequence should be of integer type.");
        ctf_tag_destroy(size_tag);
        return NULL;
    }
    
    struct ctf_type_impl* type_impl_sequence =
        ctf_type_impl_sequence_create(size_tag, elem_type);
    if(type_impl_sequence == NULL)
    {
        ctf_tag_destroy(size_tag);
        return NULL;
    }
    
    int result = ctf_meta_type_begin(meta, name, type_impl_sequence,
        is_internal);
    if(result < 0)
    {
        ctf_type_impl_destroy(type_impl_sequence);
        return NULL;
    }
    
    struct ctf_type* type_sequence = ctf_meta_get_current_type(meta);
    ctf_bug_on(type_sequence == NULL);
    
    ctf_meta_type_end(meta);
    
    return type_sequence;
}

struct ctf_type* ctf_meta_typedef_create(struct ctf_meta* meta,
    const char* name, struct ctf_type* type,
    int is_internal)
{
    struct ctf_type_impl* type_impl_typedef =
        ctf_type_impl_typedef_create(type);
    if(type_impl_typedef == NULL) return NULL;
    
    int result = ctf_meta_type_begin(meta, name, type_impl_typedef,
        is_internal);
    if(result < 0)
    {
        ctf_type_impl_destroy(type_impl_typedef);
        return NULL;
    }
    
    struct ctf_type* type_typedef = ctf_meta_get_current_type(meta);
    ctf_bug_on(type_typedef == NULL);
    
    ctf_meta_type_end(meta);
    
    return type_typedef;
}


int ctf_meta_top_scope_begin(struct ctf_meta* meta,
    const char* scope_name)
{
    struct ctf_scope* current_scope = meta->build_info->current_scope;
    if(current_scope != meta->root_scope)
    {
        ctf_err("Top level scopes may be defined only in root scope.");
        return -EINVAL;
    }
    
    struct ctf_scope* scope_top = ctf_scope_root_add_top_scope(
        current_scope, scope_name);
    if(scope_top == NULL) return -EINVAL;
    
    meta->build_info->current_scope = scope_top;
    
    return 0;
}

int ctf_meta_assign_type(struct ctf_meta* meta,
    const char* position, struct ctf_type* type)
{
    struct ctf_scope* current_scope = meta->build_info->current_scope;
    struct ctf_type* current_type = meta->build_info->current_type;
    
    if(!ctf_scope_is_top(current_scope))
    {
        ctf_err("Type may be assigned only in top scope.");
        return -EINVAL;
    }
    
    if(current_type != meta->root_type)
    {
        ctf_err("Type assignment is disallowed while a type "
            "is under construction.");
        return -EINVAL;
    }
    return ctf_scope_top_assign_type(current_scope, position, type);
}

int ctf_meta_top_scope_end(struct ctf_meta* meta)
{
    struct ctf_scope* current_scope = meta->build_info->current_scope;
    struct ctf_type* current_type = meta->build_info->current_type;
    
    if(current_type != meta->root_type)
    {
        ctf_err("Scope cannot be ended while a type is under construction.");
        return -EBUSY;
    }
    
    if(current_scope == meta->root_scope)
    {
        ctf_err("No scope is currently started.");
        return -EINVAL;
    }
    
    meta->build_info->current_scope = current_scope->parent_scope;
    
    return 0;
}

int ctf_meta_add_param(struct ctf_meta* meta,
    const char* param_name, const char* param_value)
{
    struct ctf_scope* current_scope = meta->build_info->current_scope;
    struct ctf_type* current_type = meta->build_info->current_type;
    
    if(current_type != meta->root_type)
    {
        ctf_err("Parameter cannot be added while a type is under construction.");
        return -EBUSY;
    }
    
    if(!ctf_scope_is_root(current_scope))
    {
        ctf_err("Parameters may be added only to top-level scopes.");
        return -EINVAL;
    }
    
    if(ctf_scope_top_get_parameter(current_scope,param_name))
    {
        ctf_err("Parameter with name '%s' is already defined in this scope.",
            param_name);
        return -EEXIST;
    }
    
    return ctf_scope_top_add_parameter(current_scope, param_name,
        param_value);
}

const char* ctf_meta_get_param(struct ctf_meta* meta,
    const char* param_name);
