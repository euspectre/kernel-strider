	if (errh-><$function$> != NULL) {
		kedr_set_func_handlers(errh-><$function$>, 
			handle_pre_common, handle_post_common, 
			NULL, 0);
	}
