void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	eh = kedr_get_event_handlers();
	/* Report memory accesses that have actually happened */ {
<$code.post$>
	}
}