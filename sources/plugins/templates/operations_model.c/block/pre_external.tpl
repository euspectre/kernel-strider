<$state_transition_pre$><$if concat(external_state_transition_pre)$>
<$external_state_transition_pre$><$endif$><$if concat(operation.code.pre.external)$>
{
<$operation.code.pre.external: join(\n)$>
}
<$endif$>