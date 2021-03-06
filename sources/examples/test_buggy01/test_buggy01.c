/* A simple application that works with module "buggy01" via its file in 
 * debugfs.
 * Usage:
 *	test_buggy01 [file_in_debugfs]
 * 'file_in_debugfs' is the path to the file in debugfs maintained by 
 * "buggy01" module. If omitted, "/sys/kernel/debug/buggy01/data" is 
 * assumed. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

static const char *test_file = "/sys/kernel/debug/buggy01/data";

static void
usage(void)
{
	fprintf(stderr, "Usage:\n\ttest_buggy01 [file_in_debugfs]\n");
}

static void
test_buggy01(void)
{
	int fd;
	char buf[64];
	
	memset(&buf[0], 0, sizeof(buf));
	
	errno = 0;
	fd = open(test_file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Failed to open %s: %s\n", test_file, 
			strerror(errno));
		_exit(EXIT_FAILURE);
	}
	
	if (read(fd, &buf[0], sizeof(buf) - 1) == -1) {
		perror("Failed to read data");
	}
	sleep(1); /* Give the other process some time. */
	close(fd);
}

int 
main(int argc, char *argv[])
{
	pid_t pid;
	pid_t p;
		
	if (argc > 2) {
		usage();
		return EXIT_FAILURE;
	}
	
	if (argc == 2)
		test_file = argv[1];
	
	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "fork() failed\n");
		return EXIT_FAILURE;
	}
	else if (pid == 0) {
		/* Child process.
		 * [NB] It is recommended to call _exit() rather than exit()
		 * in the child process, esp. if an error is detected. Details:
		 * http://www.unixguide.net/unix/programming/1.1.3.shtml */
		test_buggy01();
		_exit(EXIT_SUCCESS);
	}
	
	/* Parent process */
	test_buggy01();

	p = waitpid(pid, NULL, 0);
	if (p == -1) {
		fprintf(stderr,
			"Failed to wait for the child process to finish\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
