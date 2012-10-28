<$if concat(operation.code.post)$>    if(call_info->op_orig)
	{
<$operation.code.post: join(\n)$>
	}
<$endif$><$if concat(external_state_post)$><$external_state_post: join(\n)$>
<$endif$><$if state_post$><$state_post$>
<$endif$><$if concat(operation.code.post.external)$>	{
<$operation.code.post.external: join(\n)$>
	}
<$endif$><$if concat(external_state_transition_post)$><$external_state_transition_post: join(\n)$>
<$endif$><$if concat(state_transition_post)$><$state_transition_post: join(/n)$>
<$endif$>