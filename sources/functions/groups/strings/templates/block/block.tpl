void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	eh = kedr_get_event_handlers();
	if (eh->on_call_pre != NULL)
		eh->on_call_pre(eh, ls->tid, info->pc, info->target);
	
<$if code.pre$>/* Prepare additional data for the post-handler if needed */ {
<$code.pre$>
	}<$endif$>
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	eh = kedr_get_event_handlers();
<$if code.post$>/* Report memory accesses that have actually happened */ {
<$code.post$>
	}
<$endif$>
	if (eh->on_call_post != NULL)
		eh->on_call_post(eh, ls->tid, info->pc, info->target);
}