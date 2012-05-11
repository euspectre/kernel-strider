#!/bin/awk -f
#
# This script converts the raw addresses in the stack frames of the
# TSan report to to form "symbol+0xoffset". The rest of the report
# remains unchanged. The resulting report is output to stdout.
#
# [NB] This script is for testing purposes only. For simplicity, it uses 
# a rather inefficient way of finding the symbol a given address belongs
# to. So its operation may take some time.
#
# Usage:
#	awk -f symbolize_tsan_report.awk \
#		-v symbols_file=<symbols_file>
#		<tsan_report_file> > <out_file>
#
# The symbol information should be present in the file located at 
# "<symbols_file>" path. That file should contain only the records having
# the following format (as well as empty lines):
# 	<symbol address (hex)> <symbol name>
########################################################################

function load_symbols(   str, addr)
{
	if (symbols_file == "") {
		printf("The path to the file with symbol info is not specified\n") > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	
	delete saddr
	ret = 0
	do {
		str = ""
		ret = getline str < symbols_file
		if (str == "") {
			break
		}
		sub("^[ \\t\\[]+", "", str)
		split(str, parts, "[ ]+")
		sub("^0x", "", parts[1])
		if (parts[1] == "" || parts[2] == "") {
			printf("Unexpected line in %s: \"%s\"\n", symbols_file, str) > "/dev/stderr"
			close(symbols_file)
			error_occurred = 1
			exit 1
		}
		
		addr = tolower(parts[1])
		if (addr !~ /^[a-f0-9]+$/) {
			printf("Invalid address (\"%s\") in %s\n", parts[1], symbols_file) > "/dev/stderr"
			close(symbols_file)
			error_occurred = 1
			exit 1
		}
		
		saddr[parts[2]] = addr;
	} 
	while (ret == 1)
	close(symbols_file) 
}

# Returns 1 if the hex number 'arg1' is greater than the hex number 'arg2',
# 0 otherwise. The strings representing these numbers should be lowercase
# and should not have "0x" prefix.
function hex_greater(arg1, arg2,   n, s)
{
	if ((arg1 !~ /^[a-f0-9]+$/) || (arg2 !~ /^[a-f0-9]+$/)) {
		printf("hex_greater(): invalid arguments: %s, %s\n", 
			arg1, arg2) > "/dev/stderr"
			error_occurred = 1
			exit 1
	}
	
	s = "test $((0x" arg1 ")) -gt $((0x" arg2 "))"
	n = system(s)
	
	if (n == 0) {
		return 1
	}
	else if (n == 1) {
		return 0
	}
	else {
		printf("hex_greater(): failed to execute command \"%s\"\n", 
			s) > "/dev/stderr"
			error_occurred = 1
			exit 1
	}
}

# Tries to resolve the address 'addr' according to the loaded symbol
# information. If successful, prints "<symbol_name>+0x<offset> (<addr>)", 
# otherwise prints 'addr'.
function print_address(addr,   gt, sym, a, n, s)
{
	a = tolower(addr)
	sub("^0x", "", a)
		
	gt = ""
	for (sym in saddr) {
		if (gt == "") {
			if (hex_greater(a, saddr[sym])) {
				gt = sym
			}
		}
		else {
			if (hex_greater(saddr[sym], saddr[gt]) && 
				hex_greater(a, saddr[sym])) {
				gt = sym
			}
		}
	}
	
	if (gt == "") {
		printf("%s", addr)
	}
	else {
		s = "printf \"" gt "+0x%x (%s)\" $((0x" a "-0x" saddr[gt] ")) " addr
		n = system(s)
		
		if (n != 0) {
			printf("print_address(): failed to execute command \"%s\"\n", 
				s) > "/dev/stderr"
				error_occurred = 1
				exit 1
		}
	}
}

BEGIN {
	error_occured = 0
	load_symbols()
}

/ \43[0-9]+[ \t]+(0x)?[0-9a-f]+:/ { 
	addr = $2
	sub(":$", "", addr)
	printf("    %s  ", $1)
	print_address(addr)
	printf("\n")
	next
}

# For all other lines, just output them unchanged
{
	print
}

END {
	if (error_occurred != 0) {
		exit 1
	}
}
