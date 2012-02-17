/*
 * Utilities for build message for send via net.
 */

#ifndef NET_MESSAGE_H
#define NET_MESSAGE_H

#include "trace_definition.h" /* CTF_STRUCT_SIZE */
#include <linux/types.h> /* size_t */
#include <linux/uio.h> /* struct kvec */

/*
 * Object used for build one message.
 * 
 * When message is built and sent, it may be cleaned, and next message
 * may be built.
 * 
 * Mesage is constructed from pieces, each piece may have its own
 * alignment.
 * 
 * NOTE: Alignment of any piece should't exceed sizeof(padding).
 * 
 * There is maximum length for message, set when builder is created.
 * If appending piece to the message would exceed this length, nothing
 * is done and -EFBIG is returned as indicator.
 */

struct msg_builder
{
    /* All message as one kvec */
    struct kvec vec;
     /* 
     * Maximum length of the message.
     */
    size_t msg_len_max;
};


/* 
 * Initialize builder and set maximum length of the message which
 * it may create.
 * 
 * NOTE: There is no message at this stage.
 */
void msg_builder_init(struct msg_builder* builder,
    size_t msg_len_max);

/* 
 * Destroy builder.
 * 
 * If there is a message built at that moment, destroy it also.
 */
void msg_builder_destroy(struct msg_builder* builder);

/*
 * Append struct with given size and alignment to the message.
 * 
 * If message is not created, create it.
 *
 * On success function returns number of appended characters
 * and set 'struct_p' to allocated buffer for structure.
 * On error return negative error code.
 * 
 * If appending of message would exceed maximum message lentgh,
 * nothing is done and -EFBIG is returned as a signal of that situation.
 */
ssize_t msg_builder_append_struct(struct msg_builder* builder,
    size_t struct_size, size_t struct_align, void** struct_p);

/*
 * Return non-zero if msg_builder currently build message, 0 otherwise.
 */
int msg_builder_has_msg(struct msg_builder* builder);

/*
 * Free message collected in the builder.
 * 
 * If no message has been collected, do nothing.
 */
void msg_builder_free_msg(struct msg_builder* builder);

/*
 * Clean message collected in the builder.
 * 
 * As opposite to the previouse function, do not free all resources
 * concerned with message for them reusing in new message.
 */
void msg_builder_clean_msg(struct msg_builder* builder);

/* 
 * Different getters.
 * 
 * All except max_len should be executed when msg_builder_is_msg()
 * is true.
 */
size_t msg_builder_get_len(struct msg_builder* builder);
size_t msg_builder_get_max_len(struct msg_builder* builder);
struct kvec* msg_builder_get_vec(struct msg_builder* builder);
size_t msg_builder_get_vec_len(struct msg_builder* builder);


/* Return alignment for the type */
#define ALIGN_OF(TYPE) ({struct {char c; TYPE t;} s; offsetof(typeof(s), t);})

/* 
 * Shortcat of msg_builder_append_struct() for CTF structures.
 * 
 * 'var' should be of type TYPE* and after successfull call it will
 * contain pointer to the allocated structure of type TYPE.
*/
#define msg_builder_append(builder, var) \
msg_builder_append_struct(builder, CTF_STRUCT_SIZE(typeof(*var)), \
ALIGN_OF(typeof(*var)), (void**)&var)

/* Same for array */
#define msg_builder_append_array(builder, n_elems, var) \
msg_builder_append_struct(builder, CTF_ARRAY_SIZE(typeof(*var), n_elems), \
ALIGN_OF(typeof(*var)), (void**)&var)

/*
 * Trim message to the given size.
 * 
 * Useful for implement atomic addition of several structures.
 * 
 * Trimming to 0 is equivalent to msg_builder_clean_msg().
 * 
 * Negative 'new_size' means msg_len - abs(new_size).
 */
void msg_builder_trim_msg(struct msg_builder* builder, ssize_t new_size);

#endif /* NET_MESSAGE_H */