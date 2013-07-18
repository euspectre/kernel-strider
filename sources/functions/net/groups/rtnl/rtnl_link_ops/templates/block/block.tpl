static void 
cb_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	<$if is_locked$>handle_pre_locked<$else$>handle_pre_common<$endif$>(ls);
}

static void 
cb_<$function.name$>_post(struct kedr_local_storage *ls)
{
	<$if is_locked$>handle_post_locked<$else$>handle_post_common<$endif$>(ls);
}