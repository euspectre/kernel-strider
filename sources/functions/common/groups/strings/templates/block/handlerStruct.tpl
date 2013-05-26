static struct kedr_fh_handlers handlers_<$function.name$> = {
	.orig = <$if function.lookup$>NULL<$else$>&<$function.name$><$endif$>,
	.pre = &func_drd_<$function.name$>_pre,
	.post = &func_drd_<$function.name$>_post,
	.repl = NULL
};