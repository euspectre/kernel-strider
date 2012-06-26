void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long pc;
	unsigned long tid;
	
	pc = info->pc;
	tid = ls->tid;
	eh = kedr_get_event_handlers();
	
	if (eh->on_call_pre != NULL)
		eh->on_call_pre(eh, tid, pc, info->target);
<$if code.pre$>
	/* Process the call */ {
<$code.pre$>
	}<$endif$>
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long pc;
	unsigned long tid;
	
	pc = info->pc;
	tid = ls->tid;
	eh = kedr_get_event_handlers();
<$if code.post$>
	/* Process the call */ {
<$code.post$>
	}
<$endif$>
	if (eh->on_call_post != NULL)
		eh->on_call_post(eh, tid, pc, info->target);
}