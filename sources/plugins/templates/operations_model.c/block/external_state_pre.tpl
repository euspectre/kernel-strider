<$if operation.external.state.value$>
	/* 
     * Restriction: <$callback_name$>() should be called in state
     * '<$operation.external.state.value$>' of object <$operation.external.state.object$>.
     */
    generate_wait(tid, pc, <$operation.external.state.prefix$>_PRE_<$operation.external.state.value$>(<$operation.external.state.object$>),
        KEDR_SWT_COMMON);
<$endif$>