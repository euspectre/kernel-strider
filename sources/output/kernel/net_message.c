#include "net_message.h"

#include <linux/slab.h> /* kmalloc */

/* 
 * Return (minimum)number which should be added to val
 * for satisfy to alignment.
*/
#define PAD_VAL(val, alignment) (alignment) - 1 - (((val) - 1) & ((alignment) - 1))
/* 
 * Return (minimum) number which is greater or equal to val
 * and satisfy to alignment.
 */
#define ALIGN_VAL(val, alignment) ((val) + PAD_VAL(val, alignment))


void msg_builder_init(struct msg_builder* builder,
    size_t msg_len_max)
{
    builder->vec.iov_base = NULL;
    builder->vec.iov_len = 0;
    builder->msg_len_max = msg_len_max;
}

int msg_builder_has_msg(struct msg_builder* builder)
{
    return builder->vec.iov_len > 0;
}

/*
 * Free message collected in the builder.
 * 
 * If no message has been collected, do nothing.
 */
void msg_builder_free_msg(struct msg_builder* builder)
{
    kfree(builder->vec.iov_base);
    builder->vec.iov_base = NULL;
    builder->vec.iov_len = 0;
}

void msg_builder_clean_msg(struct msg_builder* builder)
{
    builder->vec.iov_len = 0;
}


void msg_builder_destroy(struct msg_builder* builder)
{
    msg_builder_free_msg(builder);
}

size_t msg_builder_get_len(struct msg_builder* builder)
{
    return builder->vec.iov_len;
}
size_t msg_builder_get_max_len(struct msg_builder* builder)
{
    return builder->msg_len_max;
}
struct kvec* msg_builder_get_vec(struct msg_builder* builder)
{
    return &builder->vec;
}
size_t msg_builder_get_vec_len(struct msg_builder* builder)
{
    return 1;
}

ssize_t msg_builder_append_struct(struct msg_builder* builder,
    size_t struct_size, size_t struct_align, void** struct_p)
{
    size_t added_size = PAD_VAL(builder->vec.iov_len, struct_align)
        + struct_size;
    size_t new_size = builder->vec.iov_len + added_size;
    
    if(new_size > builder->msg_len_max)
        return -EFBIG;
    
    if(builder->vec.iov_base == NULL)
    {
        builder->vec.iov_base = kmalloc(builder->msg_len_max, GFP_KERNEL);
        if(builder->vec.iov_base == NULL)
        {
            pr_err("Failed to allocate message.");
            return -ENOMEM;
        }
    }

    builder->vec.iov_len = new_size;
    *struct_p = builder->vec.iov_base + new_size - struct_size;
    
    return added_size;
}

void msg_builder_trim_msg(struct msg_builder* builder,
    ssize_t new_size)
{
    if(new_size < 0) new_size = builder->vec.iov_len + new_size;
    /* If nothing to rollback */
    else if(builder->vec.iov_len == new_size) return;
    
    BUG_ON(builder->vec.iov_len < new_size);
    
    builder->vec.iov_len = new_size;
}
