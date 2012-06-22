<$if concat(subevent.args)$>
{
	check_event_<$event.type$> checker(*iter, traceReader);
	
	result = checker.check_begin(<$event.args$>, <$subevent_count: join( + )$>);
	if(result)
	{
		std::cerr << i + 1 << "-th event has unexpected common parameters.\n";
		return -1;
	}
	
	<$check_subevent: join(\n\t)$>
}
<$else$>
result = check_event_<$event.type$>(*iter, traceReader, <$event.args$>);
if(result)
{
	std::cerr << i + 1 << "-th event is unexpected.\n";
	return -1;
}
<$endif$>
++iter; ++i;