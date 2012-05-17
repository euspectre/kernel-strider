#!/bin/sh

########################################################################
# Usage: 
#	sh kedr_show_target_symbols.sh <module_name>
#
# Get the names and addresses of all the ELF sections in the specified
# module loaded to the memory as well as of all the symbols in the module
# known to kallsyms subsystem. The collected data are printed to stdout 
# using the following format:
# 0x<%lx, addr> <%s, symbol_name>
#
# It is expected that this script is executed with root privileges.
########################################################################

########################################################################
# Error codes

# Incorrect command line arguments
EBADARGS=1

# No section/symbol data found for the target module
ENODATA=2

# Failed to read a file for a section  or the file does not contain a 
# section address in the expected format ("0x%x" or "0x%X")
EBADFILE=3
########################################################################

if test -z "$1"; then
	printf "Usage: sh $0 <module_name>\n"
	exit ${EBADARGS}
fi

TARGET_NAME=$1
SYSFS_MOD_DIR="/sys/module/${TARGET_NAME}"

if test ! -d "${SYSFS_MOD_DIR}"; then
	printf "Failed to find the directory in sysfs for ${TARGET_NAME}.\n"
	exit ${ENODATA}
fi

SECTIONS_DATA=""
for ss in ${SYSFS_MOD_DIR}/sections/* ${SYSFS_MOD_DIR}/sections/.*; do
	if test -f ${ss}; then
		# Get the contents of the section file. It is expected to be
		# a single hexadecimal value, possibly prefixed with "0x".
		# In addition, remove leading and trailing blanks.
		SADDR=$(cat ${ss} | sed -e 's/\(^[[:blank:]]*|[[:blank:]]*$\)//')
		if test -z "${SADDR}"; then
			printf "Failed to read ${ss} or the file does not contain a section address.\n"
			exit ${EBADFILE}
		fi
		
		SECTIONS_DATA="${SECTIONS_DATA}$(basename ${ss}) ${SADDR} "
		printf "${SADDR} $(basename ${ss})\n"
	fi
done

if test -z "${SECTIONS_DATA}"; then
	printf "No section data found for the target module.\n"
	exit ${ENODATA}
fi

grep "\[${TARGET_NAME}\]" /proc/kallsyms | awk '{printf("0x%s %s\n", $1, $3)}'
if test $? -ne 0; then
	printf "No symbol info found for the target module or an error occurred.\n"
	exit ${ENODATA}
fi
########################################################################

exit 0
