#!/bin/awk -f
#
# This script converts the trace prepared by the test reporter to the 
# format that the offline version of ThreadSanitizer can process. The 
# resulting trace is output to stdout.
#
# Usage:
#	awk -f convert_trace_to_tsan.awk \
#		<test_reporter_trace_file> > <tsan_trace_file>
#
# [TODO] TSan offline expects the trace in a format different from what
# this script currently outputs. The expected format: "%s %x %lx %lx %lx".
########################################################################

# Processes the part of the record specifying the "real" thread ID
# ("TID=....") and extracts that ID as a string. If no event with the same
# "real" thread ID has been observed before, assigns and index to it,
# stores this mapping (TID, index) in 'threads' map and sets 'tidx' to be
# equal to index. 
function process_tid_part(s,   parts, t)
{
	if (s !~ /^TID=(0x)?[a-f0-9]+/) {
		printf("Invalid TID part of record #%d: \"%s\"\n", NR, s) > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	split(s, parts, "=")
	t = parts[2]
	
	if (t in threads) {
		tidx = threads[t]
	}
	else {
		tidx = num_threads
		++num_threads
		threads[t] = tidx
		
		printf("# \"Start\" thread T%d (real TID is %s)\nTHR_START %d 0 0 0\n",
			tidx, t, tidx)
	}
}

# Extracts the value of PC from the string 's' ("pc=...").
function get_pc(s,   parts)
{
	if (s !~ /^pc=(0x)?[a-f0-9]+/) {
		printf("Invalid PC part of record #%d: \"%s\"\n", NR, s) > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	split(s, parts, "=")
	sub("^0x", "", parts[2])
	
	return parts[2]
}

# Extracts the address from the string 's' ("addr=...").
function get_addr(s,   parts)
{
	if (s !~ /^addr=(0x)?[a-f0-9]+/) {
		printf("Invalid address part of record #%d: \"%s\"\n", NR, s) > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	split(s, parts, "=")
	sub("^0x", "", parts[2])
	
	return parts[2]
}

# Extracts the value of 'size' from the string 's' ("size=...").
function get_size(s,   parts)
{
	if (s !~ /^size=(0x)?[a-f0-9]+/) {
		printf("Invalid 'size' part of record #%d: \"%s\"\n", NR, s) > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	split(s, parts, "=")
	return parts[2]
}

# Extracts the object's ID from the string 's' ("addr=...").
function get_obj_id(s,   parts)
{
	if (s !~ /^id=(0x)?[a-f0-9]+/) {
		printf("Invalid \"object ID\" part of record #%d: \"%s\"\n", NR, s) > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	split(s, parts, "=")
	sub("^0x", "", parts[2])
	
	return parts[2]
}

function report_mem_op(orig_type, pc, addr, size,   atype)
{
	atype = ""
	if (orig_type == "READ") {
		atype = "READ"
	}
	else if (orig_type == "WRITE" || orig_type == "UPDATE") {
		atype = "WRITE"
	}
	else {
		printf("Record #%d: unknown type of operation (\"%s\")\n", 
			NR, orig_type) > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	
	printf("%s %d %s %s %s\n", atype, tidx, pc, addr, size)
}

BEGIN {
	error_occured = 0
	
	# The index of the current thread. TSan expects 0-based index as the 
	# thread ID (0..num_threads-1) rather than a thread ID as KernelStrider
	# considers it.
	# The fake main thread that is used only to "create" all other threads
	# has an index of 0.
	tidx = 0
	
	# Number of threads observed so far, including the fake main thread.
	num_threads = 1
	
	printf("# A fake \"main\" thread, T0\nTHR_START 0 0 0 0\n")
}

/^TID=/ {
	process_tid_part($1)
}

# TID=<tid,0x%lx> BLOCK_ENTER pc=<pc,%p>
/ BLOCK_ENTER / { 
	pc = get_pc($3)
	printf("SBLOCK_ENTER %d %s 0 0\n", tidx, pc)
	next
}

# TID=<tid,0x%lx> CALL_PRE pc=<pc,%p>
/ CALL_PRE / {
	pc = get_pc($3)
	printf("RTN_CALL %d %s 0 0\n", tidx, pc)
	next
}

# TID=<tid,0x%lx> CALL_POST pc=<pc,%p>
/ CALL_POST / {
	printf("RTN_EXIT %d 0 0 0\n", tidx)
	next
}

# TID=<tid,0x%lx> <type> pc=<pc,%p> addr=<addr,%p> size=<%lu>
/^TID=[^ ]+ (READ|WRITE|UPDATE) / {
	report_mem_op($2, get_pc($3), get_addr($4), get_size($5))
	next
}

# TID=<tid,0x%lx> LOCKED|IO_MEM <type> pc=<pc,%p> addr=<addr,%p> size=<%lu>
/^TID=[^ ]+ (LOCKED|IO_MEM) (READ|WRITE|UPDATE) / {
	report_mem_op($3, get_pc($4), get_addr($5), get_size($6))
	next
}

# TID=<tid,0x%lx> ALLOC POST pc=<pc,%p> addr=<%p> size=<%lu>
/ ALLOC POST / {
	printf("MALLOC %d %s %s %s\n", tidx, get_pc($4), get_addr($5), get_size($6))
	next
}

# TID=<tid,0x%lx> FREE PRE pc=<pc,%p> addr=<%p>
/ FREE PRE / {
	printf("FREE %d %s %s 0\n", tidx, get_pc($4), get_addr($5))
}

# TID=<tid,0x%lx> SIGNAL <otype> PRE pc=<pc,%p> id=<obj_id,0x%lx>
/ SIGNAL [^ ]+ PRE / {
	printf("SIGNAL %d %s %s 0\n", tidx, get_pc($5), get_obj_id($6))
}

# TID=<tid,0x%lx> WAIT <otype> POST pc=<pc,%p> id=<obj_id,0x%lx>
/ WAIT [^ ]+ POST / {
	printf("WAIT %d %s %s 0\n", tidx, get_pc($5), get_obj_id($6))
}

# TID=<tid,0x%lx> LOCK <ltype> POST pc=<pc,%p> id=<lock_id,0x%lx>
/ LOCK [^ ]+ POST / {
	t = $3
	etype = ""
	if (t == "RLOCK") {
		etype = "READER_LOCK"
	}
	else if (t == "MUTEX" || t == "SPINLOCK" || t == "WLOCK") {
		etype = "WRITER_LOCK"
	}
	else {
		printf("Record #%d: unknown type of lock (\"%s\")\n", 
			NR, t) > "/dev/stderr"
		error_occured = 1
		exit 1
	}
	printf("%s %d %s %s 0\n", etype, tidx, get_pc($5), get_obj_id($6))
}

# TID=<tid,0x%lx> UNLOCK <ltype> PRE pc=<pc,%p> id=<lock_id,0x%lx>
/ UNLOCK [^ ]+ PRE / {
	printf("UNLOCK %d %s %s 0\n", tidx, get_pc($5), get_obj_id($6))
}

END {
	if (error_occurred != 0) {
		exit 1
	}
}
