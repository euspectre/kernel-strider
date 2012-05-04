static struct kedr_func_drd_handlers handlers_<$function.name$> = {
	.func = <$if function.lookup$>0<$else$>(unsigned long)&<$function.name$><$endif$>,
	.pre_handler = &func_drd_<$function.name$>_pre,
	.post_handler = &func_drd_<$function.name$>_post
};