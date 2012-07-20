/* 
 * Sync model for callback operations on object.
 * 
 * Implemented as KEDR COI payload for interceptor for that object.
 */

#include <linux/module.h>

#include <kedr/kedr_mem/functions.h>
#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

/* Protection against including header for itself, which define states */
#define <$model.name$>_INCLUDE_H

<$header: join(\n)$>

<$implementation_header: join(\n)$>

#include <kedr-coi/operations_interception.h>
/* ====================================================================== */

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/* ====================================================================== */
#define OBJECT_TYPE <$object.type$>

#define OPERATION_OFFSET(operation_name) offsetof(<$operations_type$>, operation_name)

/* Return 0 if operation has given type, otherwise generate error */
#define CHECK_OPERATION_TYPE(operation_name, type) \
BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(((<$operations_type$>*)0)->operation_name), type))


<$if concat(object.state.value)$><$state_ids: join(\n)$>
<$endif$>

#define SELF_STATE(name) <$sw_id.prefix$>_##name

/* ====================================================================== */
/* Interception of object callbacks. */
<$block: join(\n)$>

static struct kedr_coi_pre_handler model_pre_handlers[] =
{
<$pre: join(\n)$>
	kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler model_post_handlers[] =
{
<$post: join(\n)$>
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
