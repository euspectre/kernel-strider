#!/bin/sh

# [NB] Source and binary directories of the project are passed in as
# environment variables CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR to avoid
# configuring the file.

TARGET_FUNCTION=$1
TEST_TRACE_FILE=$2

SRC_DIR=$(dirname $0)
CHECKER_SCRIPT="${CMAKE_SOURCE_DIR}/tests/util/check_signal_wait.awk"

# Group #1: allocation of wqs
for ff in \
	__alloc_workqueue_key \
	__create_workqueue_key \
	; do
	
	if test "${TARGET_FUNCTION}" = "${ff}"; then
		LC_ALL=C awk -f "${CHECKER_SCRIPT}" \
			-v expected_trace="${SRC_DIR}/expected_alloc_wq.trace" \
			"${TEST_TRACE_FILE}"
		exit $?
	fi
done

# Group #2: common scenario, user-provided wqs
for ff in \
	queue_work_on \
	queue_work \
	queue_delayed_work_on \
	queue_delayed_work \
	mod_delayed_work_on \
	mod_delayed_work \
	flush_work \
	cancel_work_sync \
	flush_delayed_work \
	cancel_delayed_work_sync \
	flush_workqueue \
	drain_workqueue \
	destroy_workqueue \
	; do
	
	if test "${TARGET_FUNCTION}" = "${ff}"; then
		LC_ALL=C awk -f "${CHECKER_SCRIPT}" \
			-v expected_trace="${SRC_DIR}/expected_common.trace" \
			"${TEST_TRACE_FILE}"
		exit $?
	fi
done

# Group #3: system wqs
for ff in \
	schedule_work_on \
	schedule_work \
	schedule_delayed_work_on \
	schedule_delayed_work \
	flush_scheduled_work \
	; do
	
	if test "${TARGET_FUNCTION}" = "${ff}"; then
		LC_ALL=C awk -f "${CHECKER_SCRIPT}" \
			-v expected_trace="${SRC_DIR}/expected_system_wq.trace" \
			"${TEST_TRACE_FILE}"
		exit $?
	fi
done

printf "Unknown target function: ${TARGET_FUNCTION}.\n"
exit 1
