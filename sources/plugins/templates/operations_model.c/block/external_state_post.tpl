<$if operations.external.state.value$>
	/* 
     * Restriction: <$callback_name$>() should be finished in state
     * '<$operation.external.state.value$>' of object <$operation.external.state.object$>.
     */
    generate_signal(tid, pc, <$operation.external.state.prefix$>_POST_<$operation.external.state.value$>(<$operation.external.state.object$>),
        KEDR_SWT_COMMON);
<$endif$>