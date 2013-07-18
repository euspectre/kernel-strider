	if (ops-><$function.name$> != NULL) {
		kedr_set_func_handlers(ops-><$function.name$>,
			cb_<$function.name$>_pre, cb_<$function.name$>_post, 
			(void *)ops, 0);
	}
