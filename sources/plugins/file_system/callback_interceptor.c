#include "callback_interceptor.h"

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h> /* kmalloc */

/* One element of object->callback mapping */
struct map_elem
{
    struct list_head list_elem;
    const void* object;
    void* callback;
};

struct callback_interceptor
{
    struct list_head map_elems;
    /* Protect elements list from concurrent access. */
    spinlock_t lock;
};

/* Create interceptor for callback function. */
struct callback_interceptor* callback_interceptor_create(void)
{
    struct callback_interceptor* interceptor =
        kmalloc(sizeof(*interceptor), GFP_KERNEL);
    if(interceptor == NULL)
    {
        pr_err("Failed to allocate interceptor structure.");
        return NULL;
    }
    INIT_LIST_HEAD(&interceptor->map_elems);
    spin_lock_init(&interceptor->lock);
    
    return interceptor;
}
/* Destroy interceptor for callback function. */
void callback_interceptor_destroy(
    struct callback_interceptor* interceptor,
    void (*trace_unforgotten_object)(const void* object))
{
    while(!list_empty(&interceptor->map_elems))
    {
        struct map_elem* elem = list_first_entry(&interceptor->map_elems,
            typeof(*elem), list_elem);
        if(trace_unforgotten_object)
            trace_unforgotten_object(elem->object);
        list_del(&elem->list_elem);
        kfree(elem);
    }
    
    kfree(interceptor);
}

/* 
 * Save given callback for given object.
 * 
 * Return 0 on success and negative error on fail.
 * 
 * If other callback has been mapped for that object, return -EEXIST.
 * 
 * NOTE: But attempt to map same callback for object, which already set,
 * will succeed.
 */
int callback_interceptor_map(struct callback_interceptor* interceptor,
    const void* object, void* callback)
{
    unsigned long flags;
    int result = 0;
    struct map_elem* elem;
    spin_lock_irqsave(&interceptor->lock, flags);
    list_for_each_entry(elem, &interceptor->map_elems, list_elem)
    {
        if(elem->object == object)
        {
            if(elem->callback != callback)
            {
                result = -EEXIST;
            }
            goto out;
        }
    }
    
    elem = kmalloc(sizeof(*elem), GFP_ATOMIC);
    if(elem == NULL)
    {
        pr_err("Failed to allocate interceptor mapping element structure.");
        result = -ENOMEM;
        goto out;
    }
    elem->object = object;
    elem->callback = callback;
    INIT_LIST_HEAD(&elem->list_elem);
    list_add_tail(&elem->list_elem, &interceptor->map_elems);
    
out:
    spin_unlock_irqrestore(&interceptor->lock, flags);
    
    return result;
}

/* 
 * Forget callback mapping for given object.
 * 
 * Return 0 if mapping has been erased for object, 1 if object hasn't
 * been mapped.
 */
int callback_interceptor_forget(struct callback_interceptor* interceptor,
    const void* object)
{
    unsigned long flags;
    int result = 1;
    struct map_elem* elem;
    spin_lock_irqsave(&interceptor->lock, flags);
    list_for_each_entry(elem, &interceptor->map_elems, list_elem)
    {
        if(elem->object == object)
        {
            list_del(&elem->list_elem);
            kfree(elem);
            result = 0;
            break;
        }
    }
    
    spin_unlock_irqrestore(&interceptor->lock, flags);
    
    return result;
}

/*
 * Extract callback which has been set for that object.
 * 
 * On success, return 0 and set 'callback' to point to that callback.
 * Otherwise return negative error code
 * (which normally is an unrecoverable error).
 */
int callback_interceptor_get_callback(struct callback_interceptor* interceptor,
    const void* object, void** callback)
{
    unsigned long flags;
    int result = -EINVAL;
    struct map_elem* elem;
    spin_lock_irqsave(&interceptor->lock, flags);
    list_for_each_entry(elem, &interceptor->map_elems, list_elem)
    {
        if(elem->object == object)
        {
            *callback = elem->callback;
            result = 0;
            break;
        }
    }
    
    spin_unlock_irqrestore(&interceptor->lock, flags);
    
    return result;
}