static <$if returnType$><$returnType$><$else$>void<$endif$> <$function.name$>_repl(<$if concat(arg.name)$><$arg_spec:join(, )$><$else$>void<$endif$>)
{
    /* 
     * PC should be converted into call address when trace will
     * be processed.
     */
#define pc 0
#define tid (kedr_get_thread_id())
<$if returnType$>    <$returnType$> returnValue;
<$endif$><$if pre$><$pre$>
<$endif$>    <$if returnType$>returnValue = <$endif$><$function.name$>(<$if concat(arg.name)$><$arg.name: join(,)$><$endif$>);
<$if post$><$post$>
<$endif$><$if returnType$>return returnValue;
<$endif$>#undef pc
#undef tid
}
