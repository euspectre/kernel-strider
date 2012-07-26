set(GROUP_NAME "strings")

kmodule_configure_kernel_functions(functions OPTIONAL
	"memset"
	"memcpy"
	"__memcpy"
	"memmove"
	"memcmp"
	"memscan"
	"memchr"
	"memchr_inv"

	"strcmp"
	"strncmp"	
	"strnicmp"
	"strcasecmp"
	"strncasecmp"

	"strcpy"
	"strncpy"
	"strlcpy"

	"strcat"
	"strncat"
	"strlcat"

	"strchr"
	"strrchr"
	"strnchr"

	"skip_spaces"
	"strim"
	"strstrip"

	"strlen"
	"strnlen"

	"strspn"
	"strcspn"
	"strpbrk"

	"strsep"

	"strstr"
	"strnstr"
	
	"sscanf"
	"vsscanf"

	"simple_strtoull"
	"simple_strtoul"
	"simple_strtol"
	"simple_strtoll"

	"strict_strtoull"
	"strict_strtoul"
	"strict_strtoll"
	"strict_strtol"

	"kstrtoull"
	"kstrtoll"
	"_kstrtoul"
	"_kstrtol"
	"kstrtouint"
	"kstrtoint"
	"kstrtou16"
	"kstrtos16"
	"kstrtou8"
	"kstrtos8"

	"vsnprintf"
	"snprintf"
	"vscnprintf"
	"scnprintf"
	"vsprintf"
	"sprintf"

	"kvasprintf"
	"kasprintf"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
