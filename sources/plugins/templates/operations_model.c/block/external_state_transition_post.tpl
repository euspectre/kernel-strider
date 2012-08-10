<$if operation.external.state.transition.post_value$><$if operation.external.state.transition.condition$><$if operation.external.state.transition.pre_value$>
	/* 
     * Restriction: After <$callback_name$>() state of object
     * '<$operation.external.state.transition.object$>' should be changed
     * into '<$operation.external.state.transition.post_value$>' if condition is true,
     * state should remain '<$operation.external.state.transition.pre_value$>' otherwise.
     */
    if(<$operation.external.state.transition.condition$>)
    {
        kedr_eh_on_signal(tid, pc, <$operation.external.state.transition.prefix$>_PRE_<$operation.external.state.transition.post_value$>(<$operation.external.state.transition.object$>),
            KEDR_SWT_COMMON);
    }
    else
    {
        kedr_eh_on_signal(tid, pc, <$operation.external.state.transition.prefix$>_PRE_<$operation.external.state.transition.pre_value$>(<$operation.external.state.transition.object$>),
            KEDR_SWT_COMMON);
    }
<$else$>
	/* 
     * Restriction: After <$callback_name$>() state of object
     * '<$operation.external.state.transition.object$>' should be changed
     * into '<$operation.external.state.transition.post_value$>' if condition is true.
     */
    if(<$operation.external.state.transition.condition$>)
    {
        kedr_eh_on_signal(tid, pc, <$operation.external.state.transition.prefix$>_PRE_<$operation.external.state.transition.post_value$>(<$operation.external.state.transition.object$>),
            KEDR_SWT_COMMON);
    }
<$endif$><$else$>
	/* 
     * Restriction: After <$callback_name$>() state of object
     * '<$operation.external.state.transition.object$>' should be changed
     * into '<$operation.external.state.transition.post_value$>'.
     */
    kedr_eh_on_signal(tid, pc, <$operation.external.state.transition.prefix$>_PRE_<$operation.external.state.transition.post_value$>(<$operation.external.state.transition.object$>),
        KEDR_SWT_COMMON);
<$endif$><$else$><$if operation.external.state.transition.condition$><$if operation.external.state.transition.pre_value$>
	/* 
     * Restriction: After <$callback_name$>() state of object
     * '<$operation.external.state.transition.object$>' should remain
     * '<$operation.external.state.transition.pre_value$>' if condition is false.
     */
    if(!(<$operation.external.state.transition.condition$>))
    {
        kedr_eh_on_signal(tid, pc, <$operation.external.state.transition.prefix$>_PRE_<$operation.external.state.transition.pre_value$>(<$operation.external.state.transition.object$>),
            KEDR_SWT_COMMON);
    }
<$endif$><$endif$><$endif$>