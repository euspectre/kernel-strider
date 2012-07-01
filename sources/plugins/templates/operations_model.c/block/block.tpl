static void 
model_<$operation.name$>_pre(<$args$>,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

<$if operation.role.new$>    /* Allocation of object structure has done. */
    generate_alloc_pre(tid, pc, sizeof(OBJECT_TYPE));
    generate_alloc_post(tid, pc, sizeof(OBJECT_TYPE), <$operation.object$>);
    /* Owner of operations cannot be unloaded now. */
    FIX_OWNER(tid, pc, <$operation.object$>);
<$endif$><$if operation.role.constructor$><$else$>    /* Relation: "<$operation.name$>() may be called only after constructor returns. */
    generate_wait_pre(tid, pc, object_created(<$operation.object$>), KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, object_created(<$operation.object$>), KEDR_SWT_COMMON);
<$endif$><$if operation.role.destructor$>    /* Relation: "All callbacks should be finished at destructor call. */
    generate_ref_last(tid, pc, object_ref(<$operation.object$>));
<$else$><$if operation.role.constructor$><$else$>    /* Acquire reference on object(the object cannot be destroyed now)*/
    generate_ref_get(tid, pc, object_ref(<$operation.object$>));
<$endif$><$endif$>}

static void 
model_<$operation.name$>_post(<$args$>, <$operation.returnType$> returnValue,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

<$if operation.role.new$><$if operation.success_condition$>if(!(<$operation.success_condition$>))
    {
        /* Owner of operations can be unloaded now. */
        RELEASE_OWNER(tid, pc, <$operation.object$>);
        /* Cancel allocation of object structure. */
        generate_free_pre(tid, pc, <$operation.object$>);
        generate_free_post(tid, pc, <$operation.object$>);
    }
<$endif$><$endif$><$if operation.role.constructor$><$if operation.success_condition$>if(<$operation.success_condition$>){
<$endif$>    /* The first 'reference' on object is acquired. */
        generate_ref_get(tid, pc, object_ref(<$operation.object$>));

        /* Relation: "Callbacks may be called only after constructor returns. */
        generate_signal_pre(tid, pc, object_created(<$operation.object$>), KEDR_SWT_COMMON);
        generate_signal_post(tid, pc, object_created(<$operation.object$>), KEDR_SWT_COMMON);
<$if operation.success_condition$>    }
<$endif$><$else$><$if operation.role.destructor$><$else$>    /* Release reference on object(the object can be destroyed now)*/
    generate_ref_put(tid, pc, object_ref(<$operation.object$>));
<$endif$><$endif$><$if operation.role.delete$><$if operation.success_condition$>if(<$operation.success_condition$>){
<$endif$>    /* Owner of operations can be unloaded now. */
        RELEASE_OWNER(tid, pc, <$operation.object$>);
        /* Free object structure. */
        generate_free_pre(tid, pc, <$operation.object$>);
        generate_free_post(tid, pc, <$operation.object$>);
<$if operation.success_condition$>    }
<$endif$><$endif$>}

#define model_<$operation.name$>_pre_handler \
{ \
    .operation_offset = OPERATION_OFFSET(<$operation.name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_pre), \
    .external = <$if operation.role.new$>1<$else$><$if operation.role.destructor$>1<$else$>0<$endif$><$endif$> \
},

#define model_<$operation.name$>_post_handler \
{ \
    .operation_offset = OPERATION_OFFSET(<$operation.name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_post), \
    .external = <$if operation.role.new$>1<$else$><$if operation.role.delete$>1<$else$><$if operation.role.constructor$>1<$else$>0<$endif$><$endif$><$endif$> \
},
