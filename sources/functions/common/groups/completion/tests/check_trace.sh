#!/bin/sh

# [NB] Source and binary directories of the project are passed in as
# environment variables CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR to avoid
# configuring the file.

TARGET_FUNCTION=$1
TEST_TRACE_FILE=$2

SRC_DIR=$(dirname $0)
CHECKER_SCRIPT="${CMAKE_SOURCE_DIR}/tests/util/check_signal_wait.awk"

LC_ALL=C awk -f "${CHECKER_SCRIPT}" \
	-v expected_trace="${SRC_DIR}/expected_common.trace" \
	"${TEST_TRACE_FILE}"
exit $?
