/*
 * Implementation of CTF tag.
 */

#include "ctf_tag.h"
#include "ctf_type.h"

#include <malloc.h> /* malloc(), free() */
#include <string.h> /* strdup, strlen */
#include <assert.h>

/* Helpers */
static int is_tag_component_delimiter(char c)
{
    switch(c)
    {
    case '\0': return 1;
    case '.': return 1;
    case '[' : return 1;
    default: return 0;
    }
}

const char* test_tag_component(const char* name,
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
    //printf("Test tag component terminating...");
    return is_tag_component_delimiter(*str_current) ? str_current : NULL;
}


struct ctf_tag_component* ctf_tag_component_create(const char* name,
    struct ctf_type* type, int index)
{
    struct ctf_tag_component* component = malloc(sizeof(*component));
    if(component == NULL)
    {
        ctf_err("Failed to allocate tag component structure.");
        return NULL;
    }
    component->name = strdup(name);
    if(component->name == NULL)
    {
        ctf_err("Failed to allocate component name");
        free(component);
        return NULL;
    }
    component->type = type;
    component->index = index;
    component->next_component = NULL;
    
    return component;
}
void ctf_tag_component_destroy(struct ctf_tag_component* component)
{
    free(component->name);
    free(component);
}

/* 
 * Auxiliary function for the next one.
 * 
 * Try to continue tag according to the string.
 * 
 * On success return pointer to the null-character.
 * 
 * If the sting cannot be resolved as a tag, return pointer
 * to the character next to the last resolved component.
 * 
 * NOTE: Tag should already have at least one component.
 */
static const char* ctf_tag_continue(struct ctf_tag* tag,
    const char* str)
{
    const char* str_current = str;
    
    struct ctf_tag_component* component_current;
    /* Look for current (last) component of the tag */
    for(component_current = tag->first_component;
        component_current->next_component != NULL;
        component_current = component_current->next_component);
    
    while(*str_current != '\0')
    {
        const char* str_next;
        
        struct ctf_tag_component* component_next =
            ctf_type_resolve_tag_component(
                component_next->type, str_current, &str_next);
        if(component_next == NULL) return str_current;
        
        component_current->next_component = component_next;
        component_current = component_next;
        str_current = str_next;
    }

    return str_current;
}

struct ctf_tag* ctf_tag_create(struct ctf_type* base_type,
    const char* str, const char** unresolved_component)
{
    const char* str_next;
    
    struct ctf_tag_component* first_component =
        ctf_type_resolve_tag_component(base_type, str, &str_next);
    if(first_component == NULL) return NULL;
       
    struct ctf_tag* tag = malloc(sizeof(*tag));
    if(tag == NULL)
    {
        ctf_err("Failed to allocate tag structure.");
        ctf_tag_component_destroy(first_component);
        return NULL;
    }
    tag->first_component = first_component;
    tag->base_type = base_type;
    
    str_next = ctf_tag_continue(tag, str_next);
    *unresolved_component = str_next;
    
    return tag;

}

void ctf_tag_destroy(struct ctf_tag* tag)
{
    while(tag->first_component)
    {
        struct ctf_tag_component* component = tag->first_component;
        tag->first_component = component->next_component;
        ctf_tag_component_destroy(component);
    }
    free(tag);
}

struct ctf_tag* ctf_tag_clone(struct ctf_tag* tag)
{
    struct ctf_tag* tag_clone = malloc(sizeof(*tag_clone));
    if(tag_clone == NULL)
    {
        ctf_err("Failed to allocate cloned tag structure.");
        return NULL;
    }
    
    tag_clone->first_component = NULL;
    tag_clone->base_type = tag->base_type;
    
    struct ctf_tag_component* tag_component;
    struct ctf_tag_component** next_component_p = &tag_clone->first_component;
    
    for(tag_component = tag->first_component;
        tag_component != NULL;
        tag_component = tag_component->next_component)
    {
        struct ctf_tag_component* tag_component_clone =
            ctf_tag_component_create(tag_component->name,
                tag_component->type, tag_component->index);
        if(tag_component_clone == NULL)
        {
            ctf_tag_destroy(tag_clone);
            return NULL;
        }
        *next_component_p = tag_component_clone;
        next_component_p = &tag_component_clone->next_component;
    }
    
    return tag_clone;
}

static struct ctf_var_tag_array_context* ctf_var_tag_array_context_create(
    struct ctf_var* var,
    struct ctf_var* var_array_elem,
    int index)
{
    struct ctf_var_tag_array_context* tag_array_context = 
        malloc(sizeof(*tag_array_context));
    if(tag_array_context == NULL)
    {
        ctf_err("Failed to allocate additional array context for the tag");
        return NULL;
    }
    tag_array_context->var_array_elem_index = var_array_elem - var;
    tag_array_context->index = index;
    tag_array_context->next_tag_array_context = NULL;
    
    return tag_array_context;
}

static void ctf_var_tag_array_context_destroy(
    struct ctf_var_tag_array_context* tag_array_context)
{
    free(tag_array_context);
}

struct ctf_type* ctf_tag_get_type(struct ctf_tag* tag)
{
    struct ctf_tag_component* component;
    for(component = tag->first_component;
        component->next_component != NULL;
        component = component->next_component);
    
    return component->type;
}

struct ctf_var_tag* ctf_var_tag_create(struct ctf_tag* tag,
    struct ctf_var* var)
{
    struct ctf_var_tag* var_tag;
    
    struct ctf_type* base_type = tag->base_type;
    /* Base variable for the tag(corresponds to the tag's base type) */
    struct ctf_var* base_var;
    
    for(base_var = ctf_var_get_parent(var);
        (base_var != NULL)
            && (ctf_var_get_type(base_var) != base_type);
        base_var = ctf_var_get_parent(base_var));
    
    if(base_var == NULL)
    {
        ctf_err("Cannot detect tag base variable. It seems, "
            "tag is not correspond variable.");
        return NULL;
    }
    
    var_tag = malloc(sizeof(*var_tag));
    if(var_tag == NULL)
    {
        ctf_err("Failed to allocate resolved tag structure.");
        return NULL;
    }
    
    var_tag->additional_contexts = NULL;
    
    struct ctf_tag_component* component;
    struct ctf_var* var_component = base_var;
    struct ctf_var_tag_array_context** additional_context_top = 
        &var_tag->additional_contexts;
    
    for(component = tag->first_component;
        component != NULL;
        component = component->next_component)
    {
        var_component = ctf_var_find_var(var_component, component->name);
        if(var_component == NULL)
        {
            ctf_err("Failed to match '%s' tag's component to variable", 
                component->name);
            ctf_var_tag_destroy(var_tag);
                return NULL;
        }
        if(component->index != -1)
        {
            struct ctf_var_tag_array_context* additional_context =
                ctf_var_tag_array_context_create(var, var_component,
                    component->index);
            if(additional_context == NULL)
            {
                ctf_var_tag_destroy(var_tag);
                return NULL;
            }
            
            *additional_context_top = additional_context;
            additional_context_top = &additional_context->next_tag_array_context;
        }
    }
    var_tag->target_index = var_component - var;
    
    if(var_tag->target_index > 0)
    {
        ctf_err("Instantiated tag refers to the variable AFTER its user in "
            "dynamic scopes hierarchy. It is forbidden.");
        ctf_var_tag_destroy(var_tag);
        return NULL;
    }
    
    return var_tag;
}

/* 
 * Auxiliary function for put context, created for the tag.
 * 
 * Used both for normal destroying context and for destroying context in
 * case of error.
 * 
 * 'tag_array_context_last' points to the additional context descriptor,
 * which failed to create. NULL may be passed for destroy all additional
 * contexts.
 * 
 * NOTE: Destruction is performed in same order as creation,
 * but this is not an error in current implementation.
 */
static void ctf_var_tag_put_context_until(struct ctf_var_tag* var_tag,
    struct ctf_var* var,
    struct ctf_context* tag_context,
    struct ctf_var_tag_array_context* tag_array_context_last)
{
    struct ctf_var_tag_array_context* tag_array_context;
    for(tag_array_context = var_tag->additional_contexts;
        tag_array_context != tag_array_context_last;
        tag_array_context = tag_array_context->next_tag_array_context)
    {
        struct ctf_context* context = ctf_context_get_context_for_var(
            tag_context, var + tag_array_context->var_array_elem_index);
        assert(context);
        
        ctf_context_destroy(context);
    }

}

struct ctf_context* ctf_var_tag_get_context(struct ctf_var_tag* var_tag,
    struct ctf_var* var,
    struct ctf_context* base_context)
{
    struct ctf_context* tag_context;
    
    if(var_tag->additional_contexts)
    {
        /* Adjust context to the first array variable */
        struct ctf_var* var_array_first = ctf_var_get_parent(
            var + var_tag->additional_contexts->var_array_elem_index);
        
        tag_context = ctf_context_get_context_for_var(base_context,
            var_array_first);
        
        if(tag_context == NULL)
        {
            /* context is insufficient */
            return (void*)(-1);
        }
        
        int is_first_array_exist = ctf_var_is_exist(var_array_first,
            tag_context);
        if(is_first_array_exist != 1)
        {
            /* 
             * 'Undefined' existence (-1) is not possible, because
             * context is suitable for variable itself.
             */
            ctf_bug_on(is_first_array_exist != 0);
            
            /* 
             * First array variable is not exist in given context.
             */
            return NULL;
        }
        
        /* 
         * Map this array explicitly.
         * 
         * Tag target variable will be mapped automatically.
         * 
         * NOTE: This function may be called indirectly, when ask e.g.
         * alignment or size of variant variable. So, there is no
         * garantee that input context maps tag target variable.
         */
        
        if(ctf_var_get_map(var_array_first, tag_context, NULL) == NULL)
            return NULL;/* Some error occures while map */
        
        /* Create additional contexts */
        struct ctf_var_tag_array_context* tag_array_context =
            var_tag->additional_contexts;
        
        for(; tag_array_context != NULL;
            tag_array_context = tag_array_context->next_tag_array_context)
        {
            struct ctf_var* var_array_elem =
                var + tag_array_context->var_array_elem_index;
            
            struct ctf_context* elem_context = ctf_var_elem_create_context(
                var_array_elem,
                tag_context,
                tag_array_context->index);
            
            if(elem_context == NULL) break;/* error */
            
            if(ctf_context_is_end(elem_context))
            {
                /* Unexistent element of the array/sequence, tag is not exist */
                break;
            }
            
            tag_context = elem_context;
        }
        if(tag_array_context != NULL)
        {
            /* 
             * Error occures while create subcontexts.
             * 
             * Destroy all contexts which we created.
             */
            ctf_var_tag_put_context_until(var_tag, var, tag_context,
                tag_array_context);
            return NULL;
        }
    }
    else
    {
        /* Adjust context to the tag target variable */
        struct ctf_var* var_target = var + var_tag->target_index;
        
        tag_context = ctf_context_get_context_for_var(base_context,
            var_target);
        
        if(tag_context == NULL)
        {
            /* context is insufficient */
            return (void*)(-1);
        }
        
        /* 
         * Map target variable excplicitly.
         * 
         * NOTE: This function may be called indirectly, when ask e.g.
         * alignment or size of variant variable. So, there is no
         * garantee that input context maps tag target variable.
         */
        if(ctf_var_get_map(var_target, tag_context, NULL) == NULL) return NULL;
    }

    return tag_context;
}

void ctf_var_tag_put_context(struct ctf_var_tag* var_tag,
    struct ctf_var* var,
    struct ctf_context* tag_context)
{
    if(var_tag->additional_contexts)
        ctf_var_tag_put_context_until(var_tag, var, tag_context, NULL);
}


void ctf_var_tag_destroy(struct ctf_var_tag* var_tag)
{
    while(var_tag->additional_contexts != NULL)
    {
        struct ctf_var_tag_array_context* additional_context =
            var_tag->additional_contexts;
        var_tag->additional_contexts =
            additional_context->next_tag_array_context;
        ctf_var_tag_array_context_destroy(additional_context);
    }
    free(var_tag);
}