kedr_load_test_prefixes()

set(functions_to_test
	"krealloc"
	"__krealloc"
)

# The regular expression the trace obtained in the test should match
# for the test to pass.
# In case of __krealloc, an additional call to "kfree()" should be used
# for the trace to match this expression.
set(TEST_MATCH_EXPR " FREE PRE.* FREE POST.* ALLOC PRE.* ALLOC POST")

add_tests_for_group(${GROUP_NAME} ${functions_to_test})

# [NB] Even if there were no functions here that we knew how to test 
# reliably, we should still call add_tests_for_group(). Something like
# "NO_FUNCTIONS" could be used as the last parameter.
########################################################################
