[group]
# Name of the target function
function.name = vscnprintf

# Other definitions needed for the trigger function
trigger.add_before =>>
/* This is actually the code of scnprintf(). */
static int 
trigger_vscnprintf_wrapper(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}
<< 

# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	char *buf = NULL;
	const char *str = "-16179-";
	
	bytes = kmalloc(20, GFP_KERNEL);
	buf = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		strcpy(&bytes[0], str);
		trigger_vscnprintf_wrapper(buf, 20, "@%s@", bytes);
	}
	kfree(bytes);
	kfree(buf);
<<
#######################################################################
