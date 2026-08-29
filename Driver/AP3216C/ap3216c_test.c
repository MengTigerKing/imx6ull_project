#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char const *argv[])
{
    char data[6];
    int fd;

    /*1.判断参数*/
    if (argc != 2)
    {
        printf("Usage: %s /dev/ap3216c\n", argv[0]);
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        printf("open /dev/ap3216c failed\n");
        return -1;
    }

    while (1)
    {
        read(fd, data, 6);
        printf("IR =%d,light=%d,dis=%d\n", (data[0] << 8) | data[1],
               (data[2] << 8) | data[3],
               (data[4] << 8) | data[5]);
        sleep(1);
    }
    return 0;
}
