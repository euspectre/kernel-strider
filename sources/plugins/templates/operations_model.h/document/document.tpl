/* 
 * Sync model for callback operations <$operations.type$>.
 */

#include <kedr-coi/operations_interception.h>

/* Register model for interceptor, which should be of corresponded type. */
int <$model.name$>_register(struct kedr_coi_interceptor* interceptor);
/* Unregister model for interceptor */
int <$model.name$>_unregister(struct kedr_coi_interceptor* interceptor);

/* Same functionality, but for generated interceptors */
int <$model.name$>_connect(int (*payload_register)(struct kedr_coi_payload* payload));
int <$model.name$>_disconnect(int (*payload_unregister)(struct kedr_coi_payload* payload));
