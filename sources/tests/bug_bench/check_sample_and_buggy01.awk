#!/bin/awk -f
# check_sample_and_buggy01.awk
# 
# This script checks the report about the data races found in the modules
# "kedr_sample_target" and "buggy01". 
# 
# Usage: 
#   awk -f check_sample_and_buggy01.awk < tsan_report_file
########################################################################

BEGIN {
	error_occurred = 0
	processing = 0
	start_rec = 0
	access = ""
	found = -1

	# loc[i] - location of a memory access in the code;
	# loc_alt[i] - location of another memory access involved;
	# acc[i] - access type of a memory access;
	# acc_alt[i] - access type of another memory access involved;
	# mod[i] - name of the target module.
	loc[0] = "cfake_open (cfake.c:82)"
	acc[0] = "read"
	loc_alt[0] = "cfake_open (cfake.c:84)"
	acc_alt[0] = "write"
	mod[0] = "kedr_sample_target"
	detected[0] = 0

	loc[1] = "sample_open (module.c:81)"
	acc[1] = "write"
	loc_alt[1] = "sample_init_module (module.c:209)"
	acc_alt[1] = "write"
	mod[1] = "buggy01"
	detected[1] = 0

	loc[2] = "sample_open (module.c:76)"
	acc[2] = "read"
	loc_alt[2] = "sample_open (module.c:80)"
	acc_alt[2] = "write"
	mod[2] = "buggy01"
	detected[2] = 0

	loc[3] = "sample_open (module.c:81)"
	acc[3] = "read"
	loc_alt[3] = "sample_init_module (module.c:203)"
	acc_alt[3] = "write"
	mod[3] = "buggy01"
	detected[3] = 0
}

/WARNING: Possible data race during (read|write) .* {{{/ {
	if (processing) {
		printf("Line %d: Unexpected start of a record.\n", NR) > "/dev/stderr"
		error_occurred = 1
		exit 1
	}

	access = $6
	if (access != "read" && access != "write") {
		printf("Line %d: Unknown access type: %s\n", NR, $6) > "/dev/stderr"
		error_occurred = 1
		exit 1
	}

	processing = 1
	start_rec = 1 # start of a record
}

/#0 / {
	if (!start_rec) {
		next
	}

	start_rec = 0
	found = -1
	code_loc = $2 " " $3

	for (idx in loc) {
		if ((code_loc == loc[idx] && access == acc[idx]) ||
		    (code_loc == loc_alt[idx] && access == acc_alt[idx])) {
			detected[idx] = 1
			found = idx
			break
		}
	}
	
	if (found == -1) {
		printf("Line %d: Unexpected race was reported.\n", NR) > "/dev/stderr"
		error_occurred = 1
		exit 1
	}

}

/Race verifier data:/ {
	split($4, parts, "[:,]")
	if (parts[1] != mod[found] || parts[3] != mod[found]) {
		printf("Line %d: Expected race verifier record for \"%s\".\n",
		       NR, mod[found]) > "/dev/stderr"
		error_occurred = 1
		exit 1
	}
}

/}}}/ {
	if (!processing) {
		printf("Line %d: Unexpected \"}}}\"\n", NR) > "/dev/stderr"
		error_occurred = 1
		exit 1
	}
	processing = 0
	start_rec = 0 # just in case
	access = ""
}

END {
	if (error_occurred) {
		exit 1
	}

	for (idx in detected) {
		if (detected[idx]) {
			continue
		}

		printf("The race in %s between %s at %s and %s at %s was not detected.\n",
			mod[idx], acc[idx], loc[idx], acc_alt[idx], loc_alt[idx])
		error_occurred = 1
	}

	if (error_occurred) {
		exit 1
	}
	
	exit 0
}

