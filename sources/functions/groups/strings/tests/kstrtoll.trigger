[group]
# Name of the target function
function.name = kstrtoll
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "123456789";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		long long v;
		int ret;
		memcpy(&bytes[0], str, 15);
		ret = kstrtoll(&bytes[0], 10, &v);
		if (ret != 0)
			bytes[0] = 0;
		kfree(bytes);
	}
<<
#######################################################################
