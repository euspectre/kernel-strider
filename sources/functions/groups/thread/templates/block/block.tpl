<$if concat(arg.name)$>enum <$function.name$>_args
{
    <$arg_enum: join(,\n\t)$>
};

<$endif$>void
func_drd_<$function.name$>_pre(struct kedr_local_storage *ls)
{
<$if concat(pre)$><$if concat(arg.name)$><$arg_def: join(\n)$>
<$endif$>#define tid (ls->tid)
#define pc (((struct kedr_call_info *)ls->info)->pc)

    <$pre : join(\n)$>

<$if concat(arg.name)$><$arg_undef: join(\n)$>
<$endif$><$endif$>}

void
func_drd_<$function.name$>_post(struct kedr_local_storage *ls)
{
<$if concat(post)$><$if concat(arg.name)$><$arg_def: join(\n)$>
<$endif$><$if returnType$>#define returnValue ((<$returnType$>)KEDR_LS_RET_VAL(ls))
<$endif$>#define tid (ls->tid)
#define pc (((struct kedr_call_info *)ls->info)->pc)

    <$post : join(\n)$>

<$if concat(arg.name)$><$arg_undef: join(\n)$>
<$endif$><$if returnType$>#undef returnValue
<$endif$><$endif$>}
