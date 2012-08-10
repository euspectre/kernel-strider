<$if concat(state_transition_pre)$><$state_transition_pre: join(/n)$>
<$endif$><$if concat(external_state_transition_pre)$><$external_state_transition_pre: join(\n)$>
<$endif$><$if concat(operation.code.pre.external)$>	{
<$operation.code.pre.external: join(\n)$>
	}
<$endif$><$if state_pre$><$state_pre$>
<$endif$><$if concat(external_state_pre)$><$external_state_pre: join(\n)$>
<$endif$><$if concat(operation.code.pre)$>    if(call_info->op_orig)
	{
<$operation.code.pre: join(\n)$>
	}
<$endif$>