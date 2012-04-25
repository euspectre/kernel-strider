void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long lock_id; 
	enum kedr_lock_type lock_type;
	
	eh = kedr_get_event_handlers();
	if (eh->on_call_pre != NULL)
		eh->on_call_pre(eh, ls->tid, info->pc, info->target);
	
	if (eh->on_lock_pre != NULL) {
<$prepare_args.pre$>
		eh->on_lock_pre(eh, ls->tid, info->pc, lock_id, lock_type);
	}
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long lock_id; 
	enum kedr_lock_type lock_type;
	
	/* When handling a try-lock operation, set this flag to 
	 * a non-zero value if the locking did not actually happen. */
	int lock_failed = 0;
	
	eh = kedr_get_event_handlers();
	if (eh->on_lock_post != NULL) {
<$prepare_args.post$>
		if (!lock_failed)
			eh->on_lock_post(eh, ls->tid, info->pc, lock_id,
				lock_type);
	}
	
	if (eh->on_call_post != NULL)
		eh->on_call_post(eh, ls->tid, info->pc, info->target);
}