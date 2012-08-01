<$if operation.state.transition.pre_value$>
	/* 
     * Restriction: <$callback_name$>() should be started in state
     * '<$operation.state.transition.pre_value$>'
     * (after all operations executed in that state).
     */
    kedr_eh_on_wait(tid, pc, SELF_STATE(POST_<$operation.state.transition.pre_value$>)(<$operation.object$>),
        KEDR_SWT_COMMON);
	/* 
     * Restriction: <$callback_name$>() should be started in state
     * '<$operation.state.transition.pre_value$>'
     * (after all operations performed transition into that state).
     */
    kedr_eh_on_wait(tid, pc, SELF_STATE(PRE_<$operation.state.transition.pre_value$>)(<$operation.object$>),
        KEDR_SWT_COMMON);
<$endif$>