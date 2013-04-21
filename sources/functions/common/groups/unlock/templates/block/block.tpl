void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long lock_id; 
	enum kedr_lock_type lock_type;
	eh = kedr_get_event_handlers();
<$if aux_code.pre$>	
	/* Additional operations */ {
<$aux_code.pre$>
	}
<$endif$>
	if (eh->on_unlock_pre != NULL) {
<$prepare_args.pre$>
		eh->on_unlock_pre(eh, ls->tid, info->pc, lock_id, 
			lock_type);
	}
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long lock_id; 
	enum kedr_lock_type lock_type;
	
	eh = kedr_get_event_handlers();
	if (eh->on_unlock_post != NULL) {
<$prepare_args.post$>
		eh->on_unlock_post(eh, ls->tid, info->pc, lock_id, 
			lock_type);
	}
}