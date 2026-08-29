#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>

static int fd;

int main(int argc, char const *argv[])
{
    int val;
    int flags;
    /*判断参数*/
    if (argc != 2)
    {
        printf("Usage:%s<dev>\n", argv[0]);
        return -1;
    }

    /*打开文件*/
    fd = open(argv[1], O_RDWR);
    if (fd == -1)
    {
        printf("Can not open file %s\n", argv[1]);
        return -1;
    }

    while (1)
    {
        if (read(fd, &val, 4) == 4)
        {
            printf("get sr501:%d", val);
        }
        else
        {
            printf("get sr501:NO DATA!!!\n");
        }
    }

    
    /*如果发生意外就阻塞程序*/
    // 获取文件描述符号fd当前的打开i啊方式,并将结果保存到flags中
    flags = fcntl(fd, F_GETFL);

    /*设置文件打开的方式*/
    // flags & ~O_NONBLOCK:将flags中的非阻塞模式标志位清楚,即关闭非阻塞模式

    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    return 0;
}
