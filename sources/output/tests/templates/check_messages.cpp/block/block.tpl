<$if concat(submessage.args)$>
{
	check_message_<$message.type$> checker(*iter, traceReader);
	
	result = checker.check_begin(<$message.args$>, <$submessage_count: join( + )$>);
	if(result)
	{
		std::cerr << i + 1 << "-th message has unexpected common parameters.\n";
		return -1;
	}
	
	<$check_submessage: join(\n\t)$>
}
<$else$>
result = check_message_<$message.type$>(*iter, traceReader, <$message.args$>);
if(result)
{
	std::cerr << i + 1 << "-th message is unexpected.\n";
	return -1;
}
<$endif$>
++iter; ++i;