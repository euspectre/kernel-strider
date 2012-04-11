/*
 * Core library for read CTF trace in abstract format.
 */

#ifndef CTF_READER_H
#define CTF_READER_H

#include <stdint.h>

#include <kedr/ctf_reader/ctf_meta_types.h>

/* Abstract structure for read one CTF trace */
struct ctf_reader;

/* Create CTF reader from file contained CTF metadata */
struct ctf_reader* ctf_reader_create_from_file(
    const char* metadata_filename);

void ctf_reader_destroy(struct ctf_reader* reader);

/* Structure determine CTF variable for given trace. */
struct ctf_var;

/* 
 * Return string value for global variable (which is given via string
 * like "var = value;" in CTF metadata).
 */
const char* ctf_get_var_global(struct ctf_reader* reader,
    const char* name);

/* 
 * Return variable with given name.
 * 
 * If 'var_scope' is not NULL, 'name' is assumed to be relative to
 * the given variable(as field inside structure).
 * 
 * Example of full name:
 * "event.fields.lock.type".
 * 
 * Same name but relative to "event.fields":
 * "lock.type".
 * 
 * For access element in the array, notation "array_name[]" should be
 * used.
 * E.g, variable with name "trace.packet.header.uuid[]" may be used
 * for access particular bytes in trace UUID.
 */

struct ctf_var* ctf_reader_find_var(struct ctf_reader* reader,
    const char* name, struct ctf_var* var_scope);


/* Concrete types(with parameters) of CTF variables */
struct ctf_type;

/* Return type of the variable */
struct ctf_type* ctf_var_get_type(struct ctf_var* var);

//TODO: getters for type properties may be here

/* Iterator through different CTF elements: packets, events, arrays */
struct ctf_iter;

/* 
 * Create iterator through packets in the file.
 * 
 * When created, iterator points to the first packet in the file.
 */
struct ctf_iter* ctf_reader_create_packet_iter(const char* filename);

/*
 * Create iterator through events inside packet.
 * 
 * When created, iterator points to the first event in the packet.
 */
struct ctf_iter* ctf_reader_create_event_iter(struct ctf_iter* packet_iter);

/* 
 * Copy iterator.
 * 
 * This may be used, e.g., when need to lookup some futher elements
 * for process current one.
 */
struct ctf_iter* ctf_iter_copy(struct ctf_iter* iter);

/* 
 * Advance iterator to the next element.
 */
void ctf_iter_next(struct ctf_iter* iter);

//TODO: EOF indicator

/* Destroy iterator, releasing all used resources */
void ctf_iter_destroy(struct ctf_iter* iter);

/***************Common operations with variables***********************/

/*
 * Return pointer to the beginning of the variable in mapping.
 * 
 * After successfull call 'start_shift'(if not NULL),
 * contains bit-shift of the variable in the first byte
 * (for byte-aligned types this shift is always 0).
 * 
 * Size of the mapping may be obtained via ctf_var_get_size().
 * 
 * Returned pointer is valid until iterator is changed (via
 * ctf_iter_next, or ctf_iter_set_index) or destroyed.
 *
 * Return NULL if variable is not mapped for that iter.
 */

const char* ctf_iter_get_map(struct ctf_iter* iter, struct ctf_var* var,
    int* start_shift);


/* 
 * Return size of the variable(in bits).
 * 
 * Return -1 if variable is not mapped for that iter.
 */
int ctf_iter_get_size(struct ctf_iter* iter, struct ctf_var* var);

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
 * 'dest' is expected to point variable, which
 * a) has size corresponded to variable's type(even when copy bitfield!),
 * b) has alignment suitable for type's alignment,
 * c) has native(for the reader) byte order,
 * d) has signess corresponded to variables's type.
 */
void ctf_iter_copy_int(struct ctf_iter* iter, struct ctf_var* var,
    void* dest);

/* 
 * Check that variable fit into "C" native type 'int'.
 */
int ctf_var_is_fit_int(struct ctf_var* var);


/* 
 * Return "C" native integer value.
 * 
 * May be used only when ctf_var_is_fit_int() returns 1.
 */
unsigned int ctf_iter_get_int(struct ctf_iter* iter, struct ctf_var* var);

/* 
 * Check that variable fit into 64-bit type 'int'.
 */
int ctf_var_is_fit_int64(struct ctf_var* var);


/* 
 * Return 64-bit integer value.
 * 
 * May be used only when ctf_var_is_fit_int64() returns 1.
 */

uint64_t ctf_iter_get_int64(struct ctf_iter* iter, struct ctf_var* var);


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
 * May be used only when ctf_var_is_enum() returns not 0.
 */
const char* ctf_iter_get_enum(struct ctf_iter* iter, struct ctf_var* var);


/****************** Operations for variant variables ******************/
/* 
 * Operations special for variables of type variant.
 */

/* Check that variable is enum. */
int ctf_var_is_variant(struct ctf_var* var);

/* 
 * Return variant's field, which is exist in the given context.
 * 
 * May be used only when ctf_var_is_variant() returns not 0.
 */
struct ctf_var* ctf_iter_get_variant(struct ctf_iter* iter,
    struct ctf_var* var);

/**************** Operations for arrays and sequences *****************/
/* 
 * Operations special for arrays and sequences and their elements.
 */

/* Check that variable may be interpret as array. */
int ctf_var_contains_array(struct ctf_var* var);


/* 
 * Returns number of element in the array or sequence.
 */
int ctf_iter_get_n_elems(struct ctf_iter* iter, struct ctf_var* var);

/*
 * Check that variable is really an element of the array.
 * 
 * Such variables are created using names ended with "[]", so usually
 * it is known, whether given variable is really element of the array
 * or of the sequence, but nevertheless.
 */
int ctf_var_is_elem(struct ctf_var* var);


/* 
 * Create special iterator for arrays elements.
 * 
 * 'base_iter' should be iterator for array(sequence) itself.
 * 
 * Return NULL on error.
 * 
 * When created, iterator points to the first element in the array
 * (sequence).
 */
struct ctf_iter* ctf_var_elem_create_iter(struct ctf_var* var,
    struct ctf_iter* base_iter);

/*
 * Return index of the element, currently set in the iterator.
 * 
 * Mainly for use for array element iterators, but also may be used
 * for packet iterator or event iterator.
 */
int ctf_iter_get_element_index(struct ctf_iter* iter);


/*
 * Set index of the element in the iterator.
 * 
 * Mainly for use for array element iterators, but also may be used
 * for packet iterator or event iterator.
 * Note, that if used for packet iterator, may take a lot of time
 * for execution. Same for event iterator.
 * 
 * Return 0 on success or negarive error code on fail.
 */
int ctf_iter_set_element_index(struct ctf_iter* iter,
    int element_index);


//TODO: other types may be here. Strings, arrays...
#endif /* CTF_READER_H */