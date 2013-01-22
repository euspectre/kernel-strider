/* A simple application that works with module "kedr_sample_target" via its
 * device file(s), /dev/cfake*.
 * Usage:
 *	test_sample_target
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

static void
usage(void)
{
	fprintf(stderr, "Usage:\n\ttest_sample_target\n");
}

static void
test_module(void)
{
	int fd;
	static const char *test_file = "/dev/cfake0";
	
	errno = 0;
	fd = open(test_file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Failed to open %s: %s\n", test_file, 
			strerror(errno));
		_exit(EXIT_FAILURE);
	}
	
	sleep(1); /* Give the other process some time. */
	close(fd);
}

/* Try to open the same device file from two processes. This should allow
 * the race detector to reveal the race(s) in cfake_open(). */
int 
main(int argc, char *argv[])
{
	pid_t pid;
	pid_t p;
		
	if (argc > 1) {
		usage();
		return EXIT_FAILURE;
	}
	
	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "fork() failed\n");
		return EXIT_FAILURE;
	}
	else if (pid == 0) {
		/* Child process */
		test_module();
		_exit(EXIT_SUCCESS);
	}
	
	/* Parent process */
	test_module();

	p = waitpid(pid, NULL, 0);
	if (p == -1) {
		fprintf(stderr,
			"Failed to wait for the child process to finish\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
