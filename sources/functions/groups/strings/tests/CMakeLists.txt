kedr_load_test_prefixes()

# Make sure the functions in question access 10 bytes in each test.
# This is to distinguish the memory accesses made by these functions 
# from other memory accesses (reading globals, etc.).
set(TEST_MATCH_EXPR " (READ|WRITE|UPDATE).*addr=.*size=(10|8)")

add_tests_for_group(${GROUP_NAME} ${functions})
########################################################################
