#!/bin/awk -f
#
# The script checks if the event trace matches the expected one. Only
# "SIGNAL" and "WAIT" events are checked here.
#
# Usage:
#	awk -f check_signal_wait.awk -v expected_trace=<...> trace_file
#
# The expected trace file contains the following kinds of records:
# "SIGNAL id", "WAIT id", one per line. Symbolic names can be used as IDs
# there. Blank lines and comments (the lines that start with '#') are
# ignored.
#
# The actual trace should have the format kedr_test_reporter uses.
############################################################################

function load_expected_trace( ret, str, parts, n)
{
	num_events = 0
	delete exp_type
	delete exp_id
	
	ret = 0
	do {
		str = ""
		ret = getline str < expected_trace

		sub("^[ \\t]+", "", str)
		if (length(str) == 0) {
			# Skip blanks.
			continue
		}
		
		n = split(str, parts, "[ \\t]+")

		if (substr(parts[1], 1, 1) == "#") {
			# Skip comments
			continue
		}

		++num_events

		if (n < 2) {
			printf("Invalid record in %s:\n%s\n", expected_trace, str) > "/dev/stderr"
			error_occurred = 1
			close(expected_trace)
			exit 1
		}
		
		exp_type[num_events] = parts[1]
		exp_id[num_events] = parts[2]
	}
	while (ret == 1)
	close(expected_trace)

	if (num_events == 0) {
		printf("Failed to read the expected trace from %s.\n", expected_trace) > "/dev/stderr"
		error_occurred = 1
		exit 1
	}
}


function report_error(rec)
{
	printf("Line %d: Unexpected event: \"%s\"\n", NR, rec) > "/dev/stderr"
	error_occurred = 1
	exit 1
}

function do_record(rec)
{
	split(rec, parts, "[= \t]")
	tid = parts[2]
	pc = parts[7]
	id = parts[9]
}

function check_ids(i, j)
{
	if (exp_id[i] != exp_id[j] || rid[i] == rid[j]) {
		return
	}

	printf("Expected:\n%d. %s %s\n%d. %s %s\n", 
			i, exp_type[i], exp_id[i], j, exp_type[j], exp_id[j]) > "/dev/stderr"

	printf("Got:\n%d. %s %s\n%d. %s %s\n",
			i, exp_type[i], rid[i], j, exp_type[j], rid[j]) > "/dev/stderr"
	exit 1
}

BEGIN {
	error_occurred = 0

	tid = ""
	id = ""
	pc = ""

	pre_tid = ""
	pre_id = ""
	pre_pc = ""

	spre = 0
	wpre = 0

	pos = 0
	num_events = 0

	delete rid

	if (expected_trace == "") {
		printf("'expected_trace' is not specified.\n") > "/dev/stderr"
		error_occurred = 1
		exit 1
	}

	load_expected_trace()
}

/(SIGNAL|WAIT) COMMON PRE/ {
	do_record($0)

	if ($2 == "SIGNAL") {
		if (spre) {
			report_error($0)
		}
		spre = 1
	}
	else {
		if (wpre) {
			report_error($0)
		}
		wpre = 1
	}

	pre_tid = tid
	pre_id = id
	pre_pc = pc

	next
}

/(SIGNAL|WAIT) COMMON POST/ {
	do_record($0)

	++pos

	if ($2 == "SIGNAL") {
		if (!spre) {
			report_error($0)
		}
		spre = 0
	}
	else {
		if (!wpre) {
			report_error($0)
		}
		wpre = 0
	}

	if (pre_tid != tid || pre_id != id || pre_pc != pc) {
		report_error($0)
	}
	
	if ($2 != exp_type[pos]) {
		printf("Expected %s, found %s\n", exp_type[pos], $2) > "/dev/stderr"
		report_error($0)
	}

	rid[pos] = id

	next
}

END {
	if (error_occurred) {
		exit 1
	}

	if (pos != num_events) {
		printf("Expected %d event(s), got %d instead.\n", num_events, pos) > "/dev/stderr"
		exit 1
	}

	for (i in exp_id) {
		for (j in exp_id) {
			if (i >= j) {
				continue
			}
			check_ids(i, j)
		}
	}
	
	exit 0
}
