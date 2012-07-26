[group]
# Name of the target function
function.name = kstrtos8
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "123456789";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		s8 v;
		int ret;
		memcpy(&bytes[0], str, 15);
		ret = kstrtos8(&bytes[0], 10, &v);
		if (ret != 0)
			bytes[0] = 0;
		kfree(bytes);
	}
<<
#######################################################################
