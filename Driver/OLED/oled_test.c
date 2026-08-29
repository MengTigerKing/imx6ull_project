#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>

int main(int argc, char const *argv[])
{
    int fd;
    const char *str;

    if (argc != 3)
    {
        printf("Usage :%s <dev>/xxx\n", argv[0]);
        return -1;
    }

    fd = open(argv[1], O_RDWR);

    if (fd < 0)
    {
        printf("Can not open file %s\n", argv[1]);
        return -1;
    }

    str = argv[2];

    if (write(fd, str, strlen(str)) < 0)
    {
        perror("write failed");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}
