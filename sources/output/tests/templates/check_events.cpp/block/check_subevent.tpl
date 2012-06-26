result = checker.check_subevent(<$subevent.args$>);
if(result)
{
	std::cerr << i + 1 << "-th event has incorrect subevent.\n";
	return -1;
}