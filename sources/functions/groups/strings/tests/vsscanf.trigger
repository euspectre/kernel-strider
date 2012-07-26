[group]
# Name of the target function
function.name = vsscanf

# Other definitions needed for the trigger function
trigger.add_before =>>
/* This is actually the code of sscanf(). */
static int 
trigger_vsscanf_wrapper(const char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsscanf(buf, fmt, args);
	va_end(args);

	return i;
}
<<
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "1711 222 ";
	
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		int a;
		int b; 
		int ret;
		memcpy(&bytes[0], str, 15);
		ret = trigger_vsscanf_wrapper(&bytes[0], "%d %d", &a, &b);
		if (ret != 2 || a != b)
			bytes[0] = 0;
		kfree(bytes);
	}
<<
#######################################################################
