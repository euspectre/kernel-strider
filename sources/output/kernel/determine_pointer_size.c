#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_DESCRIPTION("Determine size of pointer on given architecture.");
MODULE_LICENSE("GPL");

#if defined(EXPECTED_POINTER_SIZE_64)
#define EXPECTED_POINTER_SIZE 64
#elif defined(EXPECTED_POINTER_SIZE_32)
#define EXPECTED_POINTER_SIZE 32
#else 
#error EXPECTED_POINTER_SIZE_64 or EXPECTED_POINTER_SIZE_32 macros should be defined
#endif 

int test(void)
{
	BUILD_BUG_ON(EXPECTED_POINTER_SIZE != (sizeof(void*) * 8));
	return 0;
}