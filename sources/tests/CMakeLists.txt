kedr_load_test_prefixes()

add_subdirectory(sample_target)
add_subdirectory(dummy_target)

if (TSAN_APP)
	# These tests use ThreadSanitizer and may assume the path to it is
	# ${TSAN_APP}.
	add_subdirectory(bug_bench)
endif ()
########################################################################
