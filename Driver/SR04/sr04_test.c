#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>

#define CMD_TRIG 100

static int fd;

/*
 * ./sr04_test /dev/sr04
 *
 */
int main(int argc, char **argv)
{
    int val;
    struct pollfd fds[1];       // poll 监听的文件描述符

    int i;
    
    /* 1. 判断参数 */
    if (argc != 2) 
    {
        printf("Usage: %s <dev>\n", argv[0]);
        return -1;
    }

    /* 2. 打开文件 */
    fd = open(argv[1], O_RDWR );
    if (fd == -1)
    {
        printf("can not open file %s\n", argv[1]);
        return -1;
    }

    while(1)
    {
        /* 发出触发信号,启动定时器，如果超时就返回错误 */
        ioctl(fd, CMD_TRIG);  

        /* poll */
        fds[0].fd = fd;
        fds[0].events = POLLIN;

        if (1 == poll(fds, 1, 500)) // 监督fds事件，1个时间，5000ms,如果定时器超时或者有事假发生，则被唤醒。
        {
            /* 读4字节数据 */
            if (read(fd, &val, 4) == 4)
                printf("get distance: %d\n", val * 17 / 1000000);
            else
                printf("get distance: err\n");
        }
        else
        {
            printf("get distance poll timeout\n");
        }
        
        /* sr04必须控制一定延迟 */
        sleep(1); 
    }
    close(fd);
    
    return 0;
}