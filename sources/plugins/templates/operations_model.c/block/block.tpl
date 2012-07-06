<$if concat(operation.protection)$>#if <$protection: join( && )$>
<$endif$><$if pre_internal$>static void 
model_<$operation.name$>_pre_internal(<$args$>,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
<$pre_internal$>}

#define model_<$operation.name$>_pre_handler_internal \
{ \
    .operation_offset = OPERATION_OFFSET(<$callback_name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_pre_internal), \
    .external = 0 \
},

<$else$>/* Operation <$operation.name$>() doesn't need internal pre handler */
#define model_<$operation.name$>_pre_handler_internal
<$endif$>

<$if pre_external$>static void 
model_<$operation.name$>_pre_external(<$args$>,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
<$pre_external$>}

#define model_<$operation.name$>_pre_handler_external \
{ \
    .operation_offset = OPERATION_OFFSET(<$callback_name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_pre_external), \
    .external = 1 \
},

<$else$>/* Operation <$operation.name$>() doesn't need external pre handler */
#define model_<$operation.name$>_pre_handler_external
<$endif$>


<$if post_internal$>static void 
model_<$operation.name$>_post_internal(<$args$>,
    <$if operation.returnType$><$operation.returnType$> returnValue,
    <$endif$>struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
<$post_internal$>}

#define model_<$operation.name$>_post_handler_internal \
{ \
    .operation_offset = OPERATION_OFFSET(<$callback_name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_post_internal), \
    .external = 0 \
},

<$else$>/* Operation <$operation.name$>() doesn't need internal post handler */
#define model_<$operation.name$>_post_handler_internal
<$endif$>

<$if post_external$>static void 
model_<$operation.name$>_post_external(<$args$>,
    <$if operation.returnType$><$operation.returnType$> returnValue,
    <$endif$>struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
<$post_external$>}

#define model_<$operation.name$>_post_handler_external \
{ \
    .operation_offset = OPERATION_OFFSET(<$callback_name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_post_external), \
    .external = 1 \
},

<$else$>/* Operation <$operation.name$>() doesn't need external post handler */
#define model_<$operation.name$>_post_handler_external
<$endif$>

<$if concat(operation.protection)$>#else /* Operation <$operation.name$> is not exist */
#define model_<$operation.name$>_pre_handler_internal
#define model_<$operation.name$>_pre_handler_external
#define model_<$operation.name$>_post_handler_internal
#define model_<$operation.name$>_post_handler_external
#endif /* Protection for <$operation.name$> */
<$endif$>

