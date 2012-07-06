<$if operation.external.state.transition.pre_value$>
	/* 
     * Restriction: <$callback_name$>() should be started in state
     * '<$operation.external.state.transition.pre_value$>' of object
     * '<$operation.external.state.transition.object$>'.
     */
    generate_wait(tid, pc, <$operation.external.state.transition.prefix$>_POST_<$operation.external.state.transition.pre_value$>(<$operation.external.state.transition.object$>),
        KEDR_SWT_COMMON);
<$endif$>