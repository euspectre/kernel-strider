void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{<$if code.pre$>
/* Prepare additional data for the post-handler if needed */ {
<$code.pre$>
	}<$endif$>
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{<$if code.post$>
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	/* Report memory accesses that have actually happened */ {
<$code.post$>
	}
<$endif$>
}