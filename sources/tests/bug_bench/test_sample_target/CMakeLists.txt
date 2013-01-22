# This application is used in the tests that involve "kedr_sample_target"
# module.
set(APP_NAME "test_sample_target")

add_executable("${APP_NAME}"
	main.c
)
kedr_test_add_target(${APP_NAME})
