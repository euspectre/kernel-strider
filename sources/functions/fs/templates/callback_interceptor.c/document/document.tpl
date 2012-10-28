/* Protect from header inclusion(otherwise typedefs will be redefined)*/
#define CALLBACK_INTERCEPTOR_<$interceptor.group.name$>_H
#include <kedr/kedr_mem/core_api.h>
#include "callback_interceptor.h"

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(implementation_header)$><$implementation_header: join(\n)$>

<$endif$><$block: join(\n)$>