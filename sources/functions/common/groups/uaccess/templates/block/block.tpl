void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	/* Report memory accesses that have actually happened */ {
<$code.post$>
	}
}