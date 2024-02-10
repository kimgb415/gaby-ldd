#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#define TIMEOUT_MS 5000


int main(int argc, char **argv)
{
    int ret_val = 1;
    struct pollfd *poll_fd = malloc(sizeof(struct pollfd));
    if (poll_fd == NULL) {
        perror("Failed to allocate memory");
        goto exit;
    }

    // make sure it's opened as non-blokcing mode
    poll_fd->fd = open("/dev/scullpipe0", O_RDONLY | O_NONBLOCK);
    if (poll_fd->fd == -1) {
        perror("Failed to open scull pipe device");
        goto exit;
    }
    poll_fd->events = POLLIN;

    int poll_result = poll(poll_fd, 1, TIMEOUT_MS);
    if (poll_result == -1) {
        perror("Failed to poll scull pipe");
    } else if (poll_result == 0) {
        perror("Timeout polling scull pipe");
    } else {
        if (poll_fd->revents & POLLIN) {
            char buffer[1024];
            ssize_t cnt = read(poll_fd->fd, buffer, sizeof(buffer) - 1);

            if (cnt == -1) {
                perror("Failed to read scull pipe");
                goto exit;
            }

            buffer[cnt] = '\0';
            printf("[scull pipe read] %s\n", buffer);
            ret_val = 0;
        }
    }

exit:
    if (poll_fd) {
        if (poll_fd->fd != -1)
            close(poll_fd->fd);
        free(poll_fd);
    }

    return ret_val;
}