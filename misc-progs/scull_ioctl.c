#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "scull_ioctl.h"

void get_values(int *quantum, int *qset, int *fd)
{
    int retval;
    retval = ioctl(*fd, SCULL_IOCGQUANTUM, quantum);
    printf("[SCULL_IOCGQUANTUM] quantum = %d, retval = %d\n", *quantum, retval < 0 ? errno : retval);

    retval = ioctl(*fd, SCULL_IOCGQSET, qset);
    printf("[SCULL_IOCGQSET] qset = %d, retval = %d\n", *qset, retval < 0 ? errno : retval);
}

void set_values(int *quantum, int *qset, int *fd)
{
    int retval;
    retval = ioctl(*fd, SCULL_IOCSQUANTUM, quantum);
    printf("[SCULL_IOCSQUANTUM] quantum = %d, retval = %d\n", *quantum, retval < 0 ? errno : retval);

    retval = ioctl(*fd, SCULL_IOCSQSET, qset);
    printf("[SCULL_IOCSQSET] qset = %d, retval = %d\n", *qset, retval < 0 ? errno : retval);
}

int main() {
    int fd = open("/dev/scull0", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    int quantum, qset;

    get_values(&quantum, &qset, &fd);

    quantum = 10;
    qset = 20;
    set_values(&quantum, &qset, &fd);
    
    get_values(&quantum, &qset, &fd);


    close(fd);

    return 0;
}
