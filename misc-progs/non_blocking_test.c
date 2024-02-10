#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#define BUFFER_SIZE 4096

char buffer[BUFFER_SIZE];

int main(int argc, char **argv)
{
    int fd = open("/dev/scullpipe0", O_RDONLY);
    if (fd == -1) {
        perror("Failed to open scull pipe device\n");
        exit(1);
    }

    // mark file descriptor as non-blocking
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	while (1) {
		int n = read(fd, buffer, BUFFER_SIZE);
		if (n >= 0)
            printf("[scull pipe read] %s\n", buffer);

        // break if error other than EAGAIN occurred
		if (n < 0  && errno != EAGAIN) {
            perror("Error occurred reading scull pipe device\n");
            close(fd);
            exit(1);
        } else {
            printf("Retry scull pipe read again\n");
        }

        // sleep for 1 second
		sleep(1);
	}
}
