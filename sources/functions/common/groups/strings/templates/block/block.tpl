static void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{<$if code.pre$>
/* Prepare additional data for the post-handler if needed */ {
<$code.pre$>
	}<$endif$>
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{<$if code.post$>
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	/* Report memory accesses that have actually happened */ {
<$code.post$>
	}
<$endif$>
}

<$handlerStruct$>
<$if function.lookup$>
#define KEDR_FUNC_ADDR_LOOKUP_<$function.name$> 1
<$endif$>