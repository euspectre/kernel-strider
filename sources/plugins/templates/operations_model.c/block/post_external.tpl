<$state_transition_post$><$if concat(external_state_transition_post)$>
<$external_state_transition_post: join(\n)$><$endif$><$if concat(operation.code.post.external)$>
{
<$operation.code.post.external: join(\n)$>
}
<$endif$>