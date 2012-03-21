#include <linux/kbuild.h>

#include "functions.h"
#include "local_storage.h"
/* ====================================================================== */

void kedr_offsets_holder(void)
{
	COMMENT("kedr_local_storage");
	OFFSET(LSTORAGE_temp, kedr_local_storage, temp);
	OFFSET(LSTORAGE_ret_val, kedr_local_storage, ret_val);
	OFFSET(LSTORAGE_tid, kedr_local_storage, tid);
	BLANK();
	
	COMMENT("kedr_call_info");
	COMMENT("TODO");
}
/* ====================================================================== */
