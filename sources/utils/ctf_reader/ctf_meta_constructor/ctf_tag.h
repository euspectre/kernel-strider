/*
 * CTF tag - pointer to some place in type-field hierarchy.
 * 
 * Variants and sequences use this pointer to refer to their base
 * enumeration and integer correspondingly.
 */

#ifndef CTF_TAG_H
#define CTF_TAG_H

#include "ctf_meta_internal.h"

/* One component in the tag */
struct ctf_tag_component
{
    struct ctf_tag_component* next_component;
    /* 
     * Name of variable(not a field!), which will corresponds to that
     * tag component.
     */
    char* name;
    /* Type of the component */
    struct ctf_type* type;
    /* Index of element in array or sequence, -1 if not applicable */
    int index;
};

struct ctf_tag_component* ctf_tag_component_create(const char* name,
    struct ctf_type* type, int index);
void ctf_tag_component_destroy(struct ctf_tag_component* component);

/* Tag for variants and sequences */
struct ctf_tag
{
    struct ctf_tag_component* first_component;
    /* Base type for the tag */
    struct ctf_type* base_type;
};

/* 
 * Try to create tag according to given string and using given type
 * as base.
 * 
 * On success, return created tag, 'unresolved_component' points to 
 * null-symbol.
 * Otherwise return tag created for the most longest part of the string,
 * 'unresolved_component' points to the first symbol after resolved
 * substring.
 * If cannot resolve first tag component, or creation of 
 * first 'struct ctf_tag_component' fails, return NULL.
 */
struct ctf_tag* ctf_tag_create(struct ctf_type* base_type,
    const char* str, const char** unresolved_component);

void ctf_tag_destroy(struct ctf_tag* tag);

/* Create clone of the tag(for typedefs)*/
struct ctf_tag* ctf_tag_clone(struct ctf_tag* tag);

/* 
 * Additional 'virtual' context for resolving tag.
 * 
 * This context corresponds to the array element context, which should
 * be created for read tag target variable.
 */
struct ctf_var_tag_array_context
{
    /* List organization of contexts */
    struct ctf_var_tag_array_context* next_tag_array_context;
    /* 
     * Index(relative to the variable, which use resolved tag)
     * of the element of the array, for which context should be created.
     */
    var_rel_index_t var_array_elem_index;
    /* Index of element in the array (>=0). */
    int index;
};

/* Resolved tag */
struct ctf_var_tag
{
    /* 
     * Target variable index
     * (relative to the variable, which use resolved tag).
     */
    var_rel_index_t target_index;
    /* Pointer to the first additional tag context */
    struct ctf_var_tag_array_context* additional_contexts;
};

/* 
 * Resolve tag when it applied to the given variable.
 * 
 * Function may be used while create variables.
 */
struct ctf_var_tag* ctf_var_tag_create(struct ctf_tag* tag,
    struct ctf_var* var);

/* 
 * Get context for the tag variable using base one. Also, map variable
 * in this context.
 * 
 * If needed, intermediate contexts will be created for
 * array elements.
 * 
 * Context returned may be used until ctf_var_tag_put_context()
 * will be performed.
 * 
 * 
 * If base context is insufficient for tag variable,
 * return -1(converted to pointer)
 * If tag is not exist in given context or some error occures,
 * NULL is returned.
 * 
 * NOTE: 'var' should be variable for which tag is created.
 */
struct ctf_context* ctf_var_tag_get_context(struct ctf_var_tag* var_tag,
    struct ctf_var* var,
    struct ctf_context* base_context);

/*
 * Release all resources allocated by call ctf_var_tag_get_context().
 * 'tag_context' is context which has been returned by that function.
 * */
void ctf_var_tag_put_context(struct ctf_var_tag* var_tag,
    struct ctf_var* var,
    struct ctf_context* tag_context);

void ctf_var_tag_destroy(struct ctf_var_tag* var_tag);


/* 
 * Return type of element, to which tag points.
 */
struct ctf_type* ctf_tag_get_type(struct ctf_tag* tag);



/* 
 * Helper for 'resolve_tag_component' callback of type.
 * 
 * Check, whether given name may be first component of the tag,
 * given as string.
 * 
 * If so, returns pointer to where name is ends.
 * 
 * Otherwise return NULL.
 */
const char* test_tag_component(const char* name,
    const char* str);

#endif /* CTF_TAG_H */
