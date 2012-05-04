[group]
# Name of the target function
function.name = memscan
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		void *p;
		memcpy(&bytes[0], str, 15);
		p = memscan(&bytes[0], 0xcc, 10 * value_one);
		bytes[0] = *(char *)p;
		kfree(bytes);
	}
<<
#######################################################################
