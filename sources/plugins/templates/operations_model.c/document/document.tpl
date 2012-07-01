/* 
 * Sync model for callback operations on object.
 * 
 * Implemented as KEDR COI payload for interceptor for that object.
 */

#include <linux/module.h>

#include <kedr/kedr_mem/functions.h>
#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

<$header: join(\n)$>

<$implementation_header: join(\n)$>

#include <kedr-coi/operations_interception.h>
/* ====================================================================== */

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/* ====================================================================== */
/* 
 * Helpers for generate events.
 * 
 * (Really, them should be defined by the core).
 */

/* Pattern for handlers wrappers */
#define GENERATE_HANDLER_CALL(handler_name, ...) do {               \
    struct kedr_event_handlers *eh = kedr_get_event_handlers();     \
    if(eh && eh->handler_name) eh->handler_name(eh, ##__VA_ARGS__); \
}while(0)

static inline void generate_signal_pre(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_signal_pre, tid, pc, obj_id, type);
}

static inline void generate_signal_post(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_signal_post, tid, pc, obj_id, type);
}

static inline void generate_wait_pre(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_wait_pre, tid, pc, obj_id, type);
}

static inline void generate_wait_post(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_wait_post, tid, pc, obj_id, type);
}

static inline void generate_alloc_pre(unsigned long tid, unsigned long pc,
    unsigned long size)
{
    GENERATE_HANDLER_CALL(on_alloc_pre, tid, pc, size);
}

static inline void generate_alloc_post(unsigned long tid, unsigned long pc,
    unsigned long size, void* pointer)
{
    GENERATE_HANDLER_CALL(on_alloc_post, tid, pc, size, (unsigned long)pointer);
}

static inline void generate_free_pre(unsigned long tid, unsigned long pc,
    void* pointer)
{
    GENERATE_HANDLER_CALL(on_free_pre, tid, pc, (unsigned long)pointer);
}

static inline void generate_free_post(unsigned long tid, unsigned long pc,
    void* pointer)
{
    GENERATE_HANDLER_CALL(on_free_post, tid, pc, (unsigned long)pointer);
}

/* Derived events generation and identificators */

/* 
 * Model for refcount-like mechanizm.
 * Useful for implement "after all" relation.
 * 
 * 'ref_get' acquires reference on some object(reference address).
 * 'ref_put' releases reference,
 * 'ref_last' is executed after all other references are released.
 */
static inline void generate_ref_get(unsigned long tid,
    unsigned long pc, unsigned long ref_addr)
{
    (void)tid;
    (void)pc;
    (void)ref_addr;
    /* do nothing */
}

static inline void generate_ref_put(unsigned long tid,
    unsigned long pc, unsigned long ref_addr)
{
    generate_signal_pre(tid, pc, ref_addr, KEDR_SWT_COMMON);
    generate_signal_post(tid, pc, ref_addr, KEDR_SWT_COMMON);
}

static inline void generate_ref_last(unsigned long tid,
    unsigned long pc, unsigned long ref_addr)
{
    generate_wait_post(tid, pc, ref_addr, KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, ref_addr, KEDR_SWT_COMMON);
}

/* destructor should be called after all other object's callbacks*/
static inline unsigned long object_ref(<$object.type$>* obj) {return (unsigned long)obj;}
/* module_exit() should be called after all module references are put. */
static inline unsigned long module_ref(struct module* m) {return (unsigned long)m;}
/* constructor should be called before all other object callbacks */
static inline unsigned long object_created(<$object.type$>* obj) {return (unsigned long)&obj-><$object.operations_field$>;}

#define OBJECT_TYPE <$object.type$>

#define FIX_OWNER(tid, pc, obj) <$if operations.owner_field$>if(!(obj)-><$object.operations_field$>-><$operations.owner_field$>); else \
        generate_ref_get(tid, pc, module_ref((obj)-><$object.operations_field$>-><$operations.owner_field$>))<$endif$>
#define RELEASE_OWNER(tid, pc, obj) <$if operations.owner_field$>if(!(obj)-><$object.operations_field$>-><$operations.owner_field$>); else \
        generate_ref_put(tid, pc, module_ref((obj)-><$object.operations_field$>-><$operations.owner_field$>))<$endif$>


#define OPERATION_OFFSET(operation_name) offsetof(<$operations.type$>, operation_name)

/* Return 0 if operation has given type, otherwise generate error */
#define CHECK_OPERATION_TYPE(operation_name, type) \
BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(((<$operations.type$>*)0)->operation_name), type))


/* ====================================================================== */
/* Interception of object callbacks. */
<$block: join(\n)$>

static struct kedr_coi_pre_handler model_pre_handlers[] =
{
	<$pre: join(\n\t)$>
	kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler model_post_handlers[] =
{
	<$post: join(\n\t)$>
    kedr_coi_post_handler_end
};

static struct kedr_coi_payload model_payload =
{
	.pre_handlers = model_pre_handlers,
	.post_handlers = model_post_handlers,
};


int <$model.name$>_set(struct kedr_coi_interceptor* interceptor)
{
    return kedr_coi_payload_register(interceptor, &model_payload);
}

int <$model.name$>_unset(struct kedr_coi_interceptor* interceptor)
{
    return kedr_coi_payload_unregister(interceptor, &model_payload);
}

int <$model.name$>_connect(int (*payload_register)(struct kedr_coi_payload* payload))
{
    return payload_register(&model_payload);
}

int <$model.name$>_disconnect(int (*payload_unregister)(struct kedr_coi_payload* payload))
{
    return payload_unregister(&model_payload);
}

