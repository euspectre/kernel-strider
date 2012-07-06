<$if operation.state.transition.post_value$><$if operation.state.transition.condition$><$if operation.state.transition.pre_value$>
	/* 
     * Restriction: After <$callback_name$>() state should be changed
     * into '<$operation.state.transition.post_value$>' if condition is true,
     * state should remain '<$operation.state.transition.pre_value$>' otherwise.
     */
    if(<$operation.state.transition.condition$>)
    {
        generate_signal(tid, pc, SELF_STATE(PRE_<$operation.state.transition.post_value$>)(<$operation.object$>),
            KEDR_SWT_COMMON);
    }
    else
    {
        generate_signal(tid, pc, SELF_STATE(PRE_<$operation.state.transition.pre_value$>)(<$operation.object$>),
            KEDR_SWT_COMMON);
    }
<$else$>
	/* 
     * Restriction: After <$callback_name$>() state should be changed
     * into '<$operation.state.transition.post_value$>' if condition is true.
     */
    if(<$operation.state.transition.condition$>)
    {
        generate_signal(tid, pc, SELF_STATE(PRE_<$operation.state.transition.post_value$>)(<$operation.object$>),
            KEDR_SWT_COMMON);
    }
<$endif$><$else$>
	/* 
     * Restriction: After <$callback_name$>() state should be changed
     * into '<$operation.state.transition.post_value$>'.
     */
    generate_signal(tid, pc, SELF_STATE(PRE_<$operation.state.transition.post_value$>)(<$operation.object$>),
        KEDR_SWT_COMMON);
<$endif$><$else$><$if operation.state.transition.condition$><$if operation.state.transition.pre_value$>
	/* 
     * Restriction: After <$callback_name$>() state should remain
     * '<$operation.state.transition.pre_value$>' if condition is false.
     */
    if(!(<$operation.state.transition.condition$>))
    {
        generate_signal(tid, pc, SELF_STATE(PRE_<$operation.state.transition.pre_value$>)(<$operation.object$>),
            KEDR_SWT_COMMON);
    }
<$endif$><$endif$><$endif$>