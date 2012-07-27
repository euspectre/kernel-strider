/* Callback interceptor '<$interceptor.name$>'. */
static struct callback_interceptor* <$interceptor.name$>_interceptor = NULL;

typedef <$if callback.returnType$><$callback.returnType$><$else$>void<$endif$>
(*<$callback.typename$>)(<$if concat(callback.arg.name)$><$callback.arg.type: join(, )$><$else$>void<$endif$>);

/* Initialize interceptor */
int <$interceptor.name$>_init(void)
{
	<$interceptor.name$>_interceptor = callback_interceptor_create();
	return (<$interceptor.name$>_interceptor != NULL) ? 0 : -EINVAL;
}
/* Destroy interceptor */
static void on_object_freed(const void* obj)
{
	pr_info("Unforgotten callback for object %p for interceptor %p.",
		obj, <$interceptor.name$>_interceptor);
}

void <$interceptor.name$>_destroy(void)
{
	if(<$interceptor.name$>_interceptor)
	{
		callback_interceptor_destroy(<$interceptor.name$>_interceptor,
			on_object_freed);
		<$interceptor.name$>_interceptor = NULL;
	}
}

static <$if callback.returnType$><$callback.returnType$><$else$>void<$endif$> 
<$interceptor.name$>_replacement(<$if concat(callback.arg.name)$><$arg_spec: join(, )$><$else$>void<$endif$>)
{
<$if callback.returnType$>    <$callback.returnType$> returnValue;
<$endif$>    <$callback.typename$> callback_orig;
	int result = callback_interceptor_get_callback(<$interceptor.name$>_interceptor,
			(void*)(<$callback.object$>), (void**)&callback_orig);
	BUG_ON(result);
	
<$if pre$>    {
		<$pre$>
	}
<$endif$>    <$if callback.returnType$>returnValue = <$endif$>callback_orig(
		<$if concat(callback.arg.name)$><$callback.arg.name: join(, )$><$endif$>);
<$if post$>    {
		<$post$>
	}
<$endif$>    <$if callback.returnType$>return returnValue;<$endif$>
}

/*
 * Return replacement callback and store obj -> old_callback mapping.
 *
 * On fail return 'old_callback'.
 *
 * That function may be used in call of the function which accept given callback.
 */
<$callback.typename$> <$interceptor.name$>_replace(<$callback.object_type$> obj,
	<$callback.typename$> old_callback)
{
	int result = callback_interceptor_map(<$interceptor.name$>_interceptor,
		(void*)obj, (void*)old_callback);
	if(result) return old_callback;
	
	return &<$interceptor.name$>_replacement;
}
int <$interceptor.name$>_forget(<$callback.object_type$> obj)
{
	return callback_interceptor_forget(<$interceptor.name$>_interceptor,
		(void*)obj);
}
