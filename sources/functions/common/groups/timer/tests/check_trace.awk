#!/bin/awk -f
# check_trace.awk
# 
# This script checks the event trace produces in the tests for handling
# of the timer API.
# 
# Usage: 
#   awk -f check_trace.awk < trace_file
#
# Expected sequence of events:
# 1. TID=T1 SIGNAL COMMON PRE pc=... id=id1
# 2. TID=T1 SIGNAL COMMON POST pc=... id=id1
# 3. TID=T2 WAIT COMMON PRE pc=... id=id1
# 4. TID=T2 WAIT COMMON POST pc=... id=id1
# 5. TID=T2 SIGNAL COMMON PRE pc=... id=id2
# 6. TID=T2 SIGNAL COMMON POST pc=... id=id2
# 7. TID=T1 WAIT COMMON PRE pc=... id=id2
# 8. TID=T1 WAIT COMMON POST pc=... id=id2
#
# Notes: id1 != id2, T1 may or may not equal T2.
########################################################################

function report_error(rec)
{
	printf("Line %d: Unexpected event: \"%s\"\n", NR, rec) > "/dev/stderr"
	error_occurred = 1
	exit 1
}

function do_record(rec)
{
	++pos;
	split(rec, parts, "[= \t]")
	id = parts[9]
}

BEGIN {
	error_occurred = 0
	id1 = ""
	id2 = ""
	id = ""
	pos = 0
	max_pos = 8
}

/SIGNAL COMMON PRE/ {
	do_record($0)

	if (pos == 1) {
		id1 = id
		next
	}
	else if (pos == 5) {
		id2 = id
		next
	}
	else {
		report_error($0)
	}
}

/SIGNAL COMMON POST/ {
	do_record($0)
	
	if ((pos == 2 && id == id1) || (pos == 6 && id == id2)) {
		next
	}
	else {
		report_error($0)
	}
}

/WAIT COMMON PRE/ {
	do_record($0)
	
	if ((pos == 3 && id == id1) || (pos == 7 && id == id2)) {
		next
	}
	else {
		report_error($0)
	}
}

/WAIT COMMON POST/ {
	do_record($0)
	
	if ((pos == 4 && id == id1) || (pos == 8 && id == id2)) {
		next
	}
	else {
		report_error($0)
	}
}

END {
	if (error_occurred) {
		exit 1
	}
	
	if (pos != max_pos) {
		printf("Expected %d event(s), got %d instead.\n", max_pos, pos) > "/dev/stderr"
		exit 1
	}
	
	exit 0
}

