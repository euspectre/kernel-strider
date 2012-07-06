<$state_post$><$if concat(external_state_post)$>
<$external_state_post: join(\n)$><$endif$><$if concat(operation.code.post)$>
{
<$operation.code.post: join(\n)$>
}
<$endif$>