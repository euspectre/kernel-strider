This example demonstrates how a plugin to the function handling subsystem
can be used.

In this case, the plugin allows to use KEDR-COI 
(http://code.google.com/p/kedr-callback-operations-interception/) 
with KernelStrider to establish several kinds of happens-before arcs and 
this way, to reduce the number of false positives reported by TSan offline.
============================================================================

Prerequisites:

- KernelStrider, should be installed to the default location (/usr/local).

- Header files provided by KernelStrider should be available in 
/usr/local/include/kedr/ and its appropriate subdirectories.

- KEDR-COI, should be installed to the default location (/usr/local).

- ThreadSanitizer offline - the executable should be in $PATH. See 
http://code.google.com/p/data-race-test/wiki/ThreadSanitizerOffline

- The helper scripts from devel/util/for_tsan/ in the repository of
KernelStrider should be available in $PATH.
============================================================================

Building the example:
	make
============================================================================

Usage:
	<TODO: describe>
============================================================================

Notes: 
	<TODO>
============================================================================
