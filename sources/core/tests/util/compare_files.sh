#!/bin/sh

########################################################################
# Compare the files with the expected data (<expected_file>) and with 
# the actual data (<actual_file>) line by line, report success (exit 
# code 0) if they match, failure (exit code 1) otherwise.
# The comparison is case-insensitive.
#
# Usage:
#   sh compare_files.sh <expected_file> <actual_file>
#
# [NB] '~' will be used internally as a separator by this script, so the 
# input files should not contain such characters.
#
# It is reasonable to sort the lines in each of these files same order 
# before using this script.
########################################################################
SCRIPT_DIR=$(dirname $0)

if test $# -ne 2; then
    printf "Usage:\n\tsh $0 <expected_file> <actual_file>\n"
    exit 1
fi
   
EXPECTED_FILE="$1"
ACTUAL_FILE="$2"

if test ! -f "${EXPECTED_FILE}"; then
    printf "File does not exist: \"${EXPECTED_FILE}\"\n"
    exit 1
fi

if test ! -f "${ACTUAL_FILE}"; then
    printf "File does not exist: \"${ACTUAL_FILE}\"\n"
    exit 1
fi

# Glue the corresponding lines with '~' character and let the helper 
# awk script decide whether the lines match.
paste -d '~' "${EXPECTED_FILE}" "${ACTUAL_FILE}" | LC_ALL=C awk -F '~' -f "${SCRIPT_DIR}/compare_files.awk"
