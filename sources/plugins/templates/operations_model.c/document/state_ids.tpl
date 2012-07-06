<$if object.state.pre_id$>static inline void* <$sw_id.prefix$>_PRE_<$object.state.value$>(<$object.type$>* obj)
{
	return <$object.state.pre_id$>;
}
<$endif$><$if object.state.post_id$>static inline void* <$sw_id.prefix$>_POST_<$object.state.value$>(<$object.type$>* obj)
{
	return <$object.state.post_id$>;
}
<$endif$>
