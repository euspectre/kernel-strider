static void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr;
	
	/* Prepare the arguments */ {
<$prepare_args.pre$>
	}
	if (!ZERO_OR_NULL_PTR((void *)addr))
		kedr_eh_on_free_pre(ls->tid, info->pc, addr);
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr;
	
	/* Prepare the arguments */ {
<$prepare_args.post$>
	}
	if (!ZERO_OR_NULL_PTR((void *)addr))
		kedr_eh_on_free_post(ls->tid, info->pc, addr);
}

<$handlerStruct$>