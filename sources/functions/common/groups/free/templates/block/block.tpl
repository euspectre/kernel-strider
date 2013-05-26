static void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr;
	
	eh = kedr_get_event_handlers();
	if (eh->on_free_pre != NULL) {
<$prepare_args.pre$>
		if (!ZERO_OR_NULL_PTR((void *)addr))
			eh->on_free_pre(eh, ls->tid, info->pc, addr);
	}
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr;
	
	eh = kedr_get_event_handlers();
	if (eh->on_free_post != NULL) {
<$prepare_args.post$>
		if (!ZERO_OR_NULL_PTR((void *)addr))
			eh->on_free_post(eh, ls->tid, info->pc, addr);
	}
}

<$handlerStruct$>