[group]
# Name of the target function
function.name = memchr_inv
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "tttttttttatttttttttttttt";
	
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		void *p;
		memcpy(&bytes[0], str, 15);
		p = memchr_inv(&bytes[0], 't', 14 * value_one);
		if (p != NULL)
			bytes[0] = *(char *)p;
		kfree(bytes);
	}
<<
#######################################################################
