void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long size;
	
	/* Prepare the arguments */ {
<$prepare_args.pre$>
	}
	if (size != 0)
		kedr_eh_on_alloc_pre(ls->tid, info->pc, size);
}

void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long size;
	unsigned long addr;
	
	/* Prepare the arguments */ {
<$prepare_args.post$>
	}
	if (size != 0 && addr != 0)
		kedr_eh_on_alloc_post(ls->tid, info->pc, size, addr);

	/* Additional operations */ {
<$aux_code.post$>
	}
}