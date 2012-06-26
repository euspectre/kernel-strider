result = checker.check_submessage(<$submessage.args$>);
if(result)
{
	std::cerr << i + 1 << "-th message has incorrect subevent.\n";
	return -1;
}