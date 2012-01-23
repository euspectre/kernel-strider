# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/ins_rm.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ins_rm.sh"
  @ONLY
)

# A simple test checking insertion and removal of the module.
kedr_test_add_script (core.basics.01 
    ins_rm.sh
)
