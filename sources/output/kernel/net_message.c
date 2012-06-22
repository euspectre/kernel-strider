#include "net_message.h"

#include <linux/slab.h> /* kmalloc */

void msg_builder_init(struct msg_builder* builder,
    size_t msg_len_max)
{
    builder->vec[0].iov_len = kedr_message_header_size;
    builder->vec[0].iov_base = &builder->header;
    builder->vec[1].iov_base = NULL;
    builder->vec[1].iov_len = 0;
    builder->msg_len_max = msg_len_max;
}

int msg_builder_has_msg(struct msg_builder* builder)
{
    return builder->vec[1].iov_len > 0;
}

/*
 * Free message collected in the builder.
 *
 * If no message has been collected, do nothing.
 */
void msg_builder_free_msg(struct msg_builder* builder)
{
    kfree(builder->vec[1].iov_base);
    builder->vec[1].iov_base = NULL;
    builder->vec[1].iov_len = 0;
}

void msg_builder_clean_msg(struct msg_builder* builder)
{
    builder->vec[1].iov_len = 0;
}


void msg_builder_destroy(struct msg_builder* builder)
{
    msg_builder_free_msg(builder);
}

size_t msg_builder_get_len(struct msg_builder* builder)
{
    return builder->vec[1].iov_len;
}
size_t msg_builder_get_max_len(struct msg_builder* builder)
{
    return builder->msg_len_max;
}
struct kvec* msg_builder_get_vec(struct msg_builder* builder)
{
    return builder->vec;
}
size_t msg_builder_get_vec_len(struct msg_builder* builder)
{
    return 2;
}

ssize_t msg_builder_append_struct(struct msg_builder* builder,
    size_t struct_size, size_t struct_align, void** struct_p)
{
    size_t new_size = ALIGN_VAL(builder->vec[1].iov_len, struct_align)
        + struct_size;
    size_t added_size = new_size - builder->vec[1].iov_len;

    BUG_ON((new_size - struct_size) < builder->vec[1].iov_len);
    BUG_ON((new_size - struct_size) % struct_align);

    if(new_size > builder->msg_len_max)
        return -EFBIG;

    if(builder->vec[1].iov_base == NULL)
    {
        builder->vec[1].iov_base = kmalloc(builder->msg_len_max, GFP_KERNEL);
        if(builder->vec[1].iov_base == NULL)
        {
            pr_err("Failed to allocate message.");
            return -ENOMEM;
        }
    }

    builder->vec[1].iov_len = new_size;
    *struct_p = builder->vec[1].iov_base + (new_size - struct_size);

    return added_size;
}

void msg_builder_trim_msg(struct msg_builder* builder,
    ssize_t new_size)
{
    if(new_size < 0) new_size = builder->vec[1].iov_len + new_size;
    /* If nothing to rollback */
    else if(builder->vec[1].iov_len == new_size) return;

    BUG_ON(builder->vec[1].iov_len < new_size);

    builder->vec[1].iov_len = new_size;
}
