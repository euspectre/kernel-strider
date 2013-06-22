static void 
cb_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	<$if is_probe$>handle_pre_probe<$else$>handle_pre_common<$endif$>(ls);
}

static void 
cb_<$function.name$>_post(struct kedr_local_storage *ls)
{
	<$if is_remove$>handle_post_remove<$else$>handle_post_common<$endif$>(ls);
}