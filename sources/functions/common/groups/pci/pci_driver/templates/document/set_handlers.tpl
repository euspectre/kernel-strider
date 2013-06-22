	if (drv-><$function.name$> != NULL) {
		kedr_set_func_handlers(drv-><$function.name$>, 
			cb_<$function.name$>_pre, cb_<$function.name$>_post, 
			NULL, 0);
	}
