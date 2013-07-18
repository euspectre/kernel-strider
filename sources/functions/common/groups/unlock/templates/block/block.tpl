static void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long lock_id; 
	enum kedr_lock_type lock_type;
<$if aux_code.pre$>	
	/* Additional operations */ {
<$aux_code.pre$>
	}
<$endif$>
	/* Prepare the arguments */ {
<$prepare_args.pre$>
	}
	kedr_fh_mark_unlocked(info->pc, lock_id);
	kedr_eh_on_unlock_pre(ls->tid, info->pc, lock_id, lock_type);
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long lock_id; 
	enum kedr_lock_type lock_type;
	
	/* Prepare the arguments */ {
<$prepare_args.post$>
	}
	kedr_eh_on_unlock_post(ls->tid, info->pc, lock_id, lock_type);
}

<$handlerStruct$>