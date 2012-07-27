kedr_load_test_prefixes()

# Make sure the functions in question access 10 bytes in each test.
# This is to distinguish the memory accesses made by these functions 
# from other memory accesses (reading globals, etc.).
#
# [NB] I would rather check for memory accesses with size == 10 only, but,
# alas, I currently know no acceptable way to call the non-inline variants
# of the functions like 'memcpy' on some systems (e.g. Debian 6, 32-bit). 
# GCC seems to inline them no matter what their arguments are. As it breaks
# the operation in two, for the first 8 bytes and for the last 2, I use
# (10|8) in the match expression.
set(TEST_MATCH_EXPR " (READ|WRITE|UPDATE).*addr=.*size=(10|8)")

add_tests_for_group(${GROUP_NAME} ${functions})
########################################################################
