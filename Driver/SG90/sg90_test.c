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
    unsigned char buf[1];

    /*判断参数*/

    if (argc != 3)
    {
        printf("Usage :%s <dev>/xxx\n", argv[0]);
        return -1;
    }

    /*打开文件*/
    fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        printf("Can not open file %s\n", argv[1]);
        return -1;
    }

    buf[0] = atoi(argv[2]);
    write(fd, buf, 1);

    sleep(3);

    close(fd);

    return 0;
}
