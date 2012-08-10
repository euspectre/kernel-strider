<$if concat(operation.protection)$>#if <$protection: join( && )$>
<$endif$><$if pre_external$>static void 
model_<$operation.name$>_pre(<$args$>,
	struct kedr_coi_operation_call_info* call_info)
{
#define pc <$pc_value$>
#define tid (kedr_get_thread_id())
<$if is_external$><$pre_external$><$else$><$pre_internal$><$endif$>
#undef pc
#undef tid
}

#define model_<$operation.name$>_pre_handler \
{ \
    .operation_offset = OPERATION_OFFSET(<$callback_name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_pre), \
<$if is_external$>    .external = 1, \
<$endif$>},

<$else$>/* Operation <$operation.name$> doesn't need pre handler */
#define model_<$operation.name$>_pre_handler
<$endif$>


<$if post_external$>static void 
model_<$operation.name$>_post(<$args$>,
    <$if operation.returnType$><$operation.returnType$> returnValue,
    <$endif$>struct kedr_coi_operation_call_info* call_info)
{
#define pc <$pc_value$>
#define tid (kedr_get_thread_id())
<$if is_external$><$post_external$><$else$><$post_internal$><$endif$>
#undef pc
#undef tid
}

#define model_<$operation.name$>_post_handler \
{ \
    .operation_offset = OPERATION_OFFSET(<$callback_name$>), \
    .func = (void*)(<$checkOperationType$> + &model_<$operation.name$>_post), \
<$if is_external$>    .external = 1, \
<$endif$>},

<$else$>/* Operation <$operation.name$> doesn't need post handler */
#define model_<$operation.name$>_post_handler
<$endif$>

<$if concat(operation.protection)$>#else /* Operation <$operation.name$> is not exist */
#define model_<$operation.name$>_pre_handler
#define model_<$operation.name$>_post_handler
#endif /* Protection for <$operation.name$> */
<$endif$>