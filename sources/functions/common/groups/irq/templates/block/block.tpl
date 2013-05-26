static void 
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{<$if code.pre$>
<$code.pre$><$endif$>
}

static void 
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{<$if code.post$>
<$code.post$><$endif$>
}

<$handlerStruct$>