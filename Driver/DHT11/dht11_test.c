#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>


/*
 * ./dht11_test /dev/dht11
 *
 */
int main(int argc, char **argv)
{
    int fd;
    int ret = 0;
    unsigned char data[4];

    /* 1. 判断参数 */
    if (argc != 2)
    {
        printf("Usage: %s /dev/xxx\n", argv[0]);
        return -1;
    }

    /* 2. 打开文件 */
    fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        printf("can not open file %s\n", argv[1]);
        return -1;
    }

    while (1)
    {

        ret = read(fd, data, 4);
        if (ret < 0)
        {
            perror("read error");
            return -1;
        }
        printf("temperature: %d.%d\t", data[0], data[1]);
        printf("humidity: %d.%d\n", data[2], data[3]);

        sleep(1);
    }
    // close(fd);

    return 0;
}