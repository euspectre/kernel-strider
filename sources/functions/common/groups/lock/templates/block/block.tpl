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
	kedr_eh_on_lock_pre(ls->tid, info->pc, lock_id, lock_type);
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long lock_id; 
	enum kedr_lock_type lock_type;
	
	/* When handling a try-lock operation, set this flag to 
	 * a non-zero value if the locking did not actually happen. */
	int lock_failed = 0;
	
	/* Prepare the arguments */ {
<$prepare_args.post$>
	}

	if (lock_failed)
		goto out;

	if (lock_type == KEDR_LT_RLOCK ||
	    kedr_fh_mark_locked(info->pc, lock_id) == 1) {
		kedr_eh_on_lock_post(ls->tid, info->pc, lock_id, lock_type);
	}

out:
<$if aux_code.post$>
	/* Additional operations */ {
<$aux_code.post$>
	}<$endif$>
	return;
}

<$handlerStruct$>