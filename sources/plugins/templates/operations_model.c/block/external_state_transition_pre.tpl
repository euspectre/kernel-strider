<$if operation.external.state.transition.pre_value$>
	/* 
     * Restriction: <$callback_name$>() should be started in state
     * '<$operation.external.state.transition.pre_value$>' of object
     * '<$operation.external.state.transition.object$>'
     * (after all operations in that state finished).
     */
    kedr_eh_on_wait(tid, pc, <$operation.external.state.transition.prefix$>_POST_<$operation.external.state.transition.pre_value$>(<$operation.external.state.transition.object$>),
        KEDR_SWT_COMMON);
	/* 
     * Restriction: <$callback_name$>() should be started in state
     * '<$operation.external.state.transition.pre_value$>' of object
     * '<$operation.external.state.transition.object$>'
     * (after all operations performed transition into that state).
     */
    kedr_eh_on_wait(tid, pc, <$operation.external.state.transition.prefix$>_PRE_<$operation.external.state.transition.pre_value$>(<$operation.external.state.transition.object$>),
        KEDR_SWT_COMMON);
<$endif$>