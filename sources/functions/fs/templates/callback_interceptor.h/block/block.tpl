/* Callback interceptor '<$interceptor.name$>'. */
typedef <$if callback.returnType$><$callback.returnType$><$else$>void<$endif$>
(*<$callback.typename$>)(<$if concat(callback.arg.name)$><$callback.arg.type: join(, )$><$else$>void<$endif$>);

/* Initialize interceptor */
int <$interceptor.name$>_init(void);
/* Destroy interceptor */
void <$interceptor.name$>_destroy(void);
/*
 * Return replacement callback and store obj -> old_callback mapping.
 *
 * On fail return 'old_callback'.
 *
 * That function may be used in call of the function which accept given callback.
 */
<$callback.typename$> <$interceptor.name$>_replace(<$callback.object_type$> obj,
	<$callback.typename$> old_callback);
int <$interceptor.name$>_forget(<$callback.object_type$> obj);
