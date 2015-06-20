static void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
<$code.pre$>
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
<$code.post$>
}

static struct kedr_fh_handlers handlers_<$function.name$> = {
	.orig = &<$function.name$>,
	.pre = &func_drd_<$function.name$>_pre,
	.post = &func_drd_<$function.name$>_post,
	.repl = NULL
};
