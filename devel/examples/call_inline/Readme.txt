This example demonstrates "insertion" of function call into another function (statically, with the inline assembly block). See CFAKE_* macros before cfake_open() as well as my_sensor_func() and cfake_open() itself. 

Usage:
#>  ./kedr_sample_target load
#>  echo 123456 > /dev/cfake0

After that, see the messages in the system log.
