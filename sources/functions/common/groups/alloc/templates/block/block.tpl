static void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long size;
	
	eh = kedr_get_event_handlers();
	if (eh->on_alloc_pre != NULL) {
<$prepare_args.pre$>
		if (size != 0)
			eh->on_alloc_pre(eh, ls->tid, info->pc, size);
	}
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long size;
	unsigned long addr;
	
	eh = kedr_get_event_handlers();
	if (eh->on_alloc_post != NULL) {
<$prepare_args.post$>
		if (size != 0 && addr != 0)
			eh->on_alloc_post(eh, ls->tid, info->pc, size, addr);
	}
}

<$handlerStruct$>