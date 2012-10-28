[group]
# Name of the target function
function.name = kvasprintf

# Other definitions needed for the trigger function
trigger.add_before =>>
/* This is actually the code of kasprintf(). */
static char * 
trigger_kvasprintf_wrapper(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return p;
}
<< 

# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	char *buf = NULL;
	const char *str = "-16179-";
	
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		strcpy(&bytes[0], str);
		buf = trigger_kvasprintf_wrapper(GFP_KERNEL, "@%s@", 
			bytes);
	}
	kfree(bytes);
	kfree(buf);
<<
#######################################################################
