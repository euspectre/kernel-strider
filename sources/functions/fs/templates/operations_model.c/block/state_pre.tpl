<$if operation.state.value$>
	/* 
     * Restriction: <$callback_name$>() should be called in state
     * '<$operation.state.value$>'.
     */
    kedr_eh_on_wait(tid, pc, SELF_STATE(PRE_<$operation.state.value$>)(<$operation.object$>),
        KEDR_SWT_COMMON);
<$endif$>