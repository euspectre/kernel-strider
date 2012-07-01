<$if operation.role.new$>
	/* Allocation of object structure has done. */
    generate_alloc_pre(tid, pc, sizeof(<$object.type$>));
    generate_alloc_post(tid, pc, sizeof(<$object.type$>), <$operation.object$>);
<$if object.operations.owner_field$>/* Owner of operations cannot be unloaded now. */
    if(<$operation.object$>-><$object.operations_field$>-><$object.operations.owner_field$>)
        generate_ref_get(tid, pc, module_ref(<$operation.object$>-><$object.operations_field$>-><$object.operations.owner_field$>));
<$endif$><$endif$>
