[group]
# Name of the target function
function.name = strim
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "   nnn   ";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		void *p;
		memcpy(&bytes[0], str, 9);
		p = strim(&bytes[0]);
		if (p != NULL)
			bytes[0] = *(char *)p;
		kfree(bytes);
	}
<<
#######################################################################
