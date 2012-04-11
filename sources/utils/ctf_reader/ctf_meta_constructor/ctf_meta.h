/*
 * Object described CTF metadata - types, variables.
 * 
 * Also describe CTF context - mapping of variables into memory regions.
 */

#ifndef CTF_META_H
#define CTF_META_H

#include <kedr/ctf_reader/ctf_meta_types.h>

#include <stdint.h> /* int32_t, int64_t */

/* Descriptor of one CTF type */
struct ctf_type;
/* 
 * Destriptor of one CTF variable - instansiated type or fields of
 * instansiated types.
 * */
struct ctf_var;

/* Meta information about CTF trace */
struct ctf_meta;

/* Create empty meta information, prepared for adding data */
struct ctf_meta* ctf_meta_create(void);

/*
 * Create empty meta information and fill it from file.
 * 
 * NOTE: Current implementation is not reentrant.
 */
struct ctf_meta* ctf_meta_create_from_file(const char* filename);


void ctf_meta_destroy(struct ctf_meta* meta);


/* 
 * Instantiate variables according meta information.
 *  Also, freeze meta information against changings(types, scopes..).
 * 
 * Additional objects used while construct new types and variables
 * are dropped at this stage.
 * 
 * Context creation become available after this stage.
 */
int ctf_meta_instantiate(struct ctf_meta* meta);

/* 
 * Return variable with given full name.
 * 
 * Example of full name:
 * "event.fields.lock.type".
 */

struct ctf_var* ctf_meta_find_var(struct ctf_meta* meta,
    const char* name);

/* 
 * Return variable with given name relative to the scope of given
 * variable.
 * 
 * If given variable has name "event.fields", then relative name
 * "lock.type" is referred to variable "event.fields.lock.type".
 */

struct ctf_var* ctf_var_find_var(struct ctf_var* var,
    const char* name);

/* 
 * Return full name of the variable.
 * 
 * Returned string should be freed when no needed.
 * 
 * May be used in debugging messages.
 */
char* ctf_var_get_full_name(struct ctf_var* var);

/* 
 * Return value of parameter using its full name(e.g "trace.uuid").
 * 
 * Return NULL if no parameter with given name defined in metadata.
 */
const char* ctf_meta_get_param(struct ctf_meta* meta,
    const char* param_name);


/* Abstract information for create CTF context */
struct ctf_context_info
{
    /*
     * Extend mapping, so it should map at least 'new_size' bits.
     * 
     * Return number of bits, which describe mapping real size(>= new_size).
     * 
     * 'map_start_p' should be set to the first byte of mapping,
     * 'map_start_shift_p' - to the shift in this first byte.
     * 
     * On fail should return negative error code.
     * 
     * When called with new_size = 0, function should return current
     * mapping, which may be empty(size = 0).
     */
    int (*extend_map)(struct ctf_context_info* context_info,
        int new_size,
        const char** map_start_p, int* map_start_shift_p);
    /* 
     * Destroy context info.
     * 
     * Useful for automatically release resources when context
     * is destroyed.
     */
    void (*destroy_info)(struct ctf_context_info* context_info);
};


/* 
 * Context which define mapping of CTF variables into memory.
 * 
 * Normally created in responce to the user request.
 */
struct ctf_context;

/* 
 * Create context for the given variable.
 * 
 * 'base_context' should be context created for upper scope
 * (e.g., should be a context for variable "stream.event.header"
 * if given variable is "stream.event.context").
 * 
 * Base context is not required(may be NULL) only if given variable is
 * "trace.packet.header".
 * 
 * If 'base_context' is not NULL, created context will 'derive' it
 * (that is, it may be used whenever base context may be used).
 * 
 * Return NULL on error.
 * 
 * Also return NULL if variable is not required its own context
 * or base context is insufficient for create requested one.
 * 
 * NOTE: 'context_info' should provide mapping, suitable aligned for
 * given variable. Use ctf_var_get_alignment() for determine it.
 */
struct ctf_context* ctf_meta_create_context(struct ctf_meta* meta,
    struct ctf_var* var, struct ctf_context_info* context_info,
    struct ctf_context* base_context);

/* Return non-zero for context for top-level variable */
int ctf_context_is_top(struct ctf_context* context);

void ctf_context_destroy(struct ctf_context* context);

/************************** CTF type **********************************/

/* CTF type */
struct ctf_type;

/* 
 * Return name of the type.
 * 
 * This name may be any string, contained spaces and differen special
 * characters.
 * 
 * Function may be used for debug messages and for create unnamed types
 * based on other types.
 */
const char* ctf_type_get_name(struct ctf_type* type);

/* 
 * Determine meta type of CTF type object.
 * 
 * Return non-zero if type may be interpreted in given way.
 */
int ctf_type_is_int(struct ctf_type* type);
int ctf_type_is_struct(struct ctf_type* type);
int ctf_type_is_enum(struct ctf_type* type);
int ctf_type_is_variant(struct ctf_type* type);
int ctf_type_is_array(struct ctf_type* type);

/* Operations for integer types (ctf_type_is_int() return non-zero). */
enum ctf_int_byte_order ctf_type_int_get_byte_order(struct ctf_type* type);
enum ctf_int_base ctf_type_int_get_base(struct ctf_type* type);
enum ctf_int_encoding ctf_type_int_get_encoding(struct ctf_type* type);
int ctf_type_int_get_alignment(struct ctf_type* type);
int ctf_type_int_get_size(struct ctf_type* type);
int ctf_type_int_is_signed(struct ctf_type* type);

/* Operations for variant types */
int ctf_type_variant_has_tag(struct ctf_type* type);

/* Operations for array types (ctf_type_is_array() return non-zero). */
int ctf_type_array_get_n_elems(struct ctf_type* type);



//TODO: Are getters needed for other types?

/************************** CTF variable ******************************/

/* 
 * CTF Variable.
 * 
 * Unit on the constructed CTF metadata.
 * 
 * Have a type and corresponds to:
 *  -instantiated top-level type (simple or compound)
 *  -instantiated field of the instansiated type
 */

struct ctf_var;

/***************Common operations with variables***********************/

/* 
 * Note, that some of this operations doesn't require context,
 * in which them are defined.
 */

/* Return type corresponded to the variable */
struct ctf_type* ctf_var_get_type(struct ctf_var* var);

/* 
 * Return size of the variable(in bits).
 * 
 * If context is insufficient, return -1.
 */
int ctf_var_get_size(struct ctf_var* var,
    struct ctf_context* context);


/* 
 * Return alignment of the variable(in bits).
 * 
 * If context is insufficient, return -1.
 * 
 * Useful when create context for that variable.
 */
int ctf_var_get_alignment(struct ctf_var* var,
    struct ctf_context* context);


/* 
 * Check whether given variable exists in given context.
 * 
 * NOTE: This is not equal to "variable may be read in given context".
 * More precise description of function is:
 * "Variable will exist in any context, in which it MAY exists, and
 * which is costructed over(may be, indirectly) given context."
 */
int ctf_var_is_exist(struct ctf_var* var, struct ctf_context* context);

/*
 * Return pointer to the beginning of the variable in mapping.
 * 
 * After successfull call 'start_shift'(if not NULL),
 * contains bit-shift of the variable in the first byte
 * (for byte-aligned types this shift is always 0, except for bitfields).
 * 
 * Size of the mapping may be obtained via ctf_var_get_size().
 * 
 * Returned pointer is valid until context is exist or call of
 * ctf-function which need to extended context for read other variables.
 * Last situation may be evaded, e.g., using preliminary mapping of the
 * whole top-level variable.
 * 
 * NOTE: Note, that context for array element 
 * (see ctf_var_create_elem_context())
 * is really wrapper around context of array itself.
 * So, mapping of element may break if array context is extended.
 * 
 *
 * If context is insufficient for variable, or some error occures,
 * return NULL.
 * 
 * This function also may be used in form
 * if(ctf_var_get_map(var, NULL)) for
 * a) check, that context is sufficient, and
 * b) extend context for use type-specific interpretations
 */

const char* ctf_var_get_map(struct ctf_var* var,
    struct ctf_context* context, int* start_shift);


/****************** Operations for integer variables ******************/

/* 
 * Any of these operations, except first, may be applyed only to those
 * variables, which meta-type is 'int' or 'enum'.
 */

/* Check that variable has integer interpretation. */
int ctf_var_contains_int(struct ctf_var* var);

/* 
 * Copy variable to the given destination.
 * 
 * Context should be correctly extended before use this function.
 * 
 * 'dest' is expected to point variable, which
 * a) has size corresponded to variable's type(even when copy bitfield!),
 * b) has alignment suitable for type's alignment,
 * c) has native(for the given machine) byte order,
 * d) has signess corresponded to variables's type.
 */
void ctf_var_copy_int(void* dest, struct ctf_var* var,
    struct ctf_context* context);

/* 
 * Check that variable fit into 32-bit integer(signed or unsigned).
 */
int ctf_var_is_fit_int32(struct ctf_var* var);


/* 
 * Return 32-bit integer representation of variable.
 * 
 * May be used only when ctf_var_is_fit_int32() returns non-zero.
 */
uint32_t ctf_var_get_int32(struct ctf_var* var,
    struct ctf_context* context);

/* 
 * Check that variable fit into 64-bit integer.
 */
int ctf_var_is_fit_int64(struct ctf_var* var);


/* 
 * Return 64-bit integer value.
 * 
 * May be used only when ctf_var_is_fit_int64() returns non-zero.
 */

uint64_t ctf_var_get_int64(struct ctf_var* var,
    struct ctf_context* context);


/****************** Operations for enum variables ******************/
/* 
 * Operations special for enum variables.
 * Note, that for those variables integer operations are also applicable.
 */

/* Check that variable is enum. */
int ctf_var_is_enum(struct ctf_var* var);

/* 
 * Pointer to the string representation of enumeration value.
 * 
 * May be used only when ctf_var_is_enum() returns non-zero.
 */
const char* ctf_var_get_enum(struct ctf_var* var,
    struct ctf_context* context);


/****************** Operations for variant variables ******************/
/* 
 * Operations special for variables of type variant.
 */

/* Check that variable is enum. */
int ctf_var_is_variant(struct ctf_var* var);


/* 
 * Set 'current_field_p' to the field of variant variable, which exists
 * in the given context.
 * 
 * May be used only when ctf_var_is_variant() returns non-zero.
 * 
 * Return 0 on success, return -1 if context is insufficient for decide.
 * 
 * NOTE: 'current_field_p' may be set to NULL as indicator that
 * no variant's field corresponds to value of enumeration tag
 * (or tag itself does not exist in given context). This is not error
 * for trace.
 */
int ctf_var_get_variant(struct ctf_var* var,
    struct ctf_context* context, struct ctf_var** active_field_p);

/**************** Operations for arrays and sequences *****************/
/* 
 * Operations special for arrays and sequences and their elements.
 */

/* Check that variable may be interpret as array. */
int ctf_var_contains_array(struct ctf_var* var);

/* 
 * Returns number of element in the array of sequence.
 * 
 * When called for array, 'context' parameter is not used.
 * When called for sequence and context is insufficient to determine
 * number of elements in it, return -1.
 */
int ctf_var_get_n_elems(struct ctf_var* var,
    struct ctf_context* context);

/*
 * Check that variable is really an element of the array.
 * 
 * Such variables are created using names ended with "[]", so usually
 * it is known, whether given variable is really element of the array
 * or of the sequence, but nevertheless.
 */
int ctf_var_is_elem(struct ctf_var* var);


/* 
 * Create special context for arrays elements.
 * 
 * 'base_context' should be context of the array(sequence) itself.
 * 
 * Return NULL on error or when context is insufficient.
 * 
 * When read created this context, elements are expected to have
 * corresponded index.
 * 
 * If element with given index is not exist in array or sequence,
 * return 'end context' (see ctf_context_is_end()).
 */
struct ctf_context* ctf_var_elem_create_context(struct ctf_var* var,
    struct ctf_context* base_context, int element_index);

/* Return non-zero if context is created for array(sequence) element */
int ctf_context_is_elem(struct ctf_context* context);

/* 
 * Return 1 if element context points after last existing element.
 * 
 * The only function allowed for this context is ctf_context_destroy().
 */
int ctf_context_is_end(struct ctf_context* context);

/*
 * Return index of the element, currently set in the context.
 */
int ctf_context_get_element_index(struct ctf_context* context);


/*
 * Set index of the element in the context.
 * 
 * Return pointer to context itself.
 * 
 * If element with given index is not exist in array or sequence,
 * return 'end context' (see ctf_context_is_end()).
 * 
 * On unexpected error(like insufficient of memory) NULL is returned
 * and context is destroyed.
 */
struct ctf_context* ctf_context_set_element_index(
    struct ctf_context* context, int element_index);

/*
 * Move context element to the next one.
 * 
 * Return pointer to context itself.
 * 
 * If element with given index is not exist in array or sequence,
 * return 'end context' (see ctf_context_is_end()).
 * 
 * On unexpected error(like insufficient of memory) NULL is returned
 * and context is destroyed.
 */
struct ctf_context* ctf_context_set_element_next(
    struct ctf_context* context);

/******************* Construction of CTF metainfo *********************/
/* Returns last type which has been added but not freezed */
struct ctf_type* ctf_meta_get_current_type(struct ctf_meta* meta);

/* 
 * Search type with given name.
 * 
 * Possible scopes for search are detected automatically.
 */
struct ctf_type* ctf_meta_find_type(struct ctf_meta* meta,
    const char* name);
/* 
 * Search type with given name. Only current scope is searched.
 * 
 * Function usefull for determine, whether type with given name may be
 * defined in the current scope.
 */
struct ctf_type* ctf_meta_find_type_strict(struct ctf_meta* meta,
    const char* name);

/* 
 * Begin definition of integer type.
 * 
 * Note that integer type is always created internal,
 * that is not accessible via its name.
 */
int ctf_meta_int_begin(struct ctf_meta* meta,
    const char* name);

/* Begin inner scope of integer definition */
int ctf_meta_int_begin_scope(struct ctf_meta* meta);

/* 
 * Set corresponded parameter for the integer.
 * 
 * May be called only when current type is integer.
 */
int ctf_meta_int_set_signed(struct ctf_meta* meta, int is_signed);
int ctf_meta_int_set_size(struct ctf_meta* meta, int size);
int ctf_meta_int_set_align(struct ctf_meta* meta, int align);
int ctf_meta_int_set_byte_order(struct ctf_meta* meta,
    enum ctf_int_byte_order byte_order);
int ctf_meta_int_set_base(struct ctf_meta* meta,
    enum ctf_int_base base);
int ctf_meta_int_set_encoding(struct ctf_meta* meta,
    enum ctf_int_encoding encoding);

/* End inner scope of integer definition. */
void ctf_meta_int_end_scope(struct ctf_meta* meta);

/* Finish definition of integer type. Return type constructed. */
struct ctf_type* ctf_meta_int_end(struct ctf_meta* meta);

/* 
 * Begin definition of struct type.
 * 
 * 'is_internal' should be non-zero for create internal structure type,
 * which cannot be searched by name.
 */
int ctf_meta_struct_begin(struct ctf_meta* meta,
    const char* name, int is_internal);

/* Begin inner scope of the structure. */
int ctf_meta_struct_begin_scope(struct ctf_meta* meta);

/* 
 * Add field with given type to the structure.
 * 
 * Return 0 on success and negarive error code on fail.
 */
int ctf_meta_struct_add_field(struct ctf_meta* meta,
    const char* field_name,
    struct ctf_type* field_type);

/* 
 * Return not-zero if structure contain field with given name.
 */
int ctf_meta_struct_has_field(struct ctf_meta* meta,
    const char* field_name);


/* End inner scope of the structure. */
void ctf_meta_struct_end_scope(struct ctf_meta* meta);

/* Finish definition of structure. Return type constructed. */
struct ctf_type* ctf_meta_struct_end(struct ctf_meta* meta);

/* 
 * Begin definition of enumeration type.
 * 
 * 'is_internal' should be non-zero for create internal enumeration type,
 * which cannot be searched by name.
 */
int ctf_meta_enum_begin(struct ctf_meta* meta,
    const char* name,
    struct ctf_type* type_int,
    int is_internal);

/* Begin inner scope of the enumeration. */
int ctf_meta_enum_begin_scope(struct ctf_meta* meta);


/* 
 * Add value for enumeration type, which base type may be represented
 * as 32-bit integer.
 */
int ctf_meta_enum_add_value32(struct ctf_meta* meta, const char* val,
    int32_t start, int32_t end);

/* 
 * Add value for enumeration type, which base type may be represented
 * as 64-bit int type.
 */
int ctf_meta_enum_add_value64(struct ctf_meta* meta, const char* val,
    int64_t start, int64_t end);

/* End inner scope of the enumeration. */
void ctf_meta_enum_end_scope(struct ctf_meta* meta);


/* Finish definition of enumeration. Return type constructed. */
struct ctf_type* ctf_meta_enum_end(struct ctf_meta* meta);

/* 
 * Begins untagged variant.
 * 
 * 'is_internal' should be non-zero for create internal variant type,
 * which cannot be searched by name.
 * 
 * Tag may be set after type is constructed.
 */
int ctf_meta_variant_begin(struct ctf_meta* meta,
    const char* name,
    int is_internal);

/* Begin inner scope of variant. */
int ctf_meta_variant_begin_scope(struct ctf_meta* meta);

/*
 * Add field to the variant.
 */
int ctf_meta_variant_add_field(struct ctf_meta* meta,
    const char* field_name,
    struct ctf_type* field_type);

/* End inner scope of variant. */
void ctf_meta_variant_end_scope(struct ctf_meta* meta);

/* Finish definition of variant. Return type constructed. */
struct ctf_type* ctf_meta_variant_end(struct ctf_meta* meta);

/* Set tag for constructed variant. */
int ctf_meta_variant_set_tag(struct ctf_meta* meta, struct ctf_type* type,
    const char* str);


/* 
 * Create array. Return type constructed.
 * 
 * 'is_internal' should be non-zero for create internal array type,
 * which cannot be searched by name.
 */
struct ctf_type* ctf_meta_array_create(struct ctf_meta* meta,
    const char* name, int array_size, struct ctf_type* elem_type,
    int is_internal);


/* 
 * Create array. Return type constructed.
 * 
 * 'is_internal' should be non-zero for create internal sequence type,
 * which cannot be searched by name.
 * 
 * NOTE: unlike variant's tag, tag for sequence should be set
 * when sequence is created.
 */
struct ctf_type* ctf_meta_sequence_create(struct ctf_meta* meta,
    const char* name, const char* size_str, struct ctf_type* elem_type,
    int is_internal);

/* 
 * Create typedefed type. Return type constructed.
 * 
 * Normally, typedef may be searched by name.
 * But for adding tag to existing variant we need to create internal copy
 * of variant.
 */
struct ctf_type* ctf_meta_typedef_create(struct ctf_meta* meta,
    const char* name, struct ctf_type* type, int is_internal);

/* 
 * Begin top-level named scope, like "trace", "stream", "event", "env".
 * Also, subscopes of named scopes may be created.
 * 
 * Scopes connected to structures and unions don't reqiure user control:
 * them are automatically started with structure/union and ended with it.
 */
int ctf_meta_top_scope_begin(struct ctf_meta* meta,
    const char* scope_name);

/* 
 * Assign type to the some positions in the top scope.
 * 
 * Position is interpret as relative to current scope.
 * (E.g. "packet.context" in "stream" scope).
 */
int ctf_meta_assign_type(struct ctf_meta* meta,
    const char* position,
    struct ctf_type* type);

/* Ends named scope */
int ctf_meta_top_scope_end(struct ctf_meta* meta);

/* Add named parameter to the current scope */
int ctf_meta_add_param(struct ctf_meta* meta,
    const char* param_name, const char* param_value);

#endif /* CTF_META_H */