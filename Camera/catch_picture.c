#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/mman.h>

int main(int argc, char const *argv[])
{
    int ret;

    if (argc != 2)
    {
        printf("Usage: %s </dev/video0,1...>\n", argv[0]);
        return 1;
    }

    /*==================1.打开摄像头设备==================*/
    int fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        perror("open err");
        return -1;
    }

    /*===================2.设置摄像头采集格式==================*/
    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;      // 视频采集类型
    vfmt.fmt.pix.width = 640;                     // 设置宽度
    vfmt.fmt.pix.height = 480;                    // 设置高度
    vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // 设置为 YUYV 格式
    vfmt.fmt.pix.field = V4L2_FIELD_ANY;          // 扫描方式，交给驱动决定

    // 将vmft里设置好的采集格式参数赋值给摄像头
    ret = ioctl(fd, VIDIOC_S_FMT, &vfmt);
    if (ret < 0)
    {
        perror("设置设备采集格式错误");
        close(fd);
        return -1;
    }

    /*==================3.读取驱动最终采用的格式,虽然前面设置给了,但是不一定驱动就会用你的格式==================*/
    // 清空vfmt
    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // 这里再把设备的采集格式赋值给空的vfmt用于检查
    ret = ioctl(fd, VIDIOC_G_FMT, &vfmt);
    if (ret < 0)
    {
        perror("读取设备采集格式失败");
        close(fd);
        return -1;
    }

    printf("实际 width  = %d\n", vfmt.fmt.pix.width);
    printf("实际 height = %d\n", vfmt.fmt.pix.height);

    unsigned char *p = (unsigned char *)&vfmt.fmt.pix.pixelformat;
    printf("实际 pixelformat = %c%c%c%c\n", p[0], p[1], p[2], p[3]);

    /*==================4.申请内核缓冲队列==================*/
    /**摄像头采集道德数据会先放到内核缓冲区.
     *
     * VIDIOC_REQBUFS用来向驱动申请缓冲区
     * 这里申请4个缓冲区
     *
     * memory=V4L2_MEMORY_MMAP表示申请到的缓冲区准备使用mmap的方式映射到用户空间
     * 也就是把内核中的视频缓冲区映射到用户空间,
     * 用户程序可以直接访问采集到的数据
     */

    // 申请缓冲区结构体(只申请)
    struct v4l2_requestbuffers reqbuffer;
    memset(&reqbuffer, 0, sizeof(reqbuffer));

    // 依旧场景设置为视频采集
    reqbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // 申请四个缓冲区
    reqbuffer.count = 4;
    // 申请申请到的采集缓冲区,准备用mmap的方式映射到用户空间
    reqbuffer.memory = V4L2_MEMORY_MMAP;

    // 依旧配置给设备,把缓冲区申请要求提交给设备驱动
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuffer);
    if (ret < 0)
    {
        perror("申请缓冲队列失败");
        close(fd);
        return -1;
    }

    printf("实际申请到的缓冲区数量: %d\n", reqbuffer.count);

    /*==================5.查询并映射每一个缓冲区==================*/
    // 真正的映射缓冲区结构体
    struct v4l2_buffer mapbuffer;

    unsigned char *mmpaddr[4];   // 保存每个缓冲区映射后的用户空间地址
    unsigned int addr_length[4]; // 保存每个缓冲区的长度

    // 4个缓冲区,4个循环
    for (int i = 0; i < 4; i++)
    {
        // 清空
        memset(&mapbuffer, 0, sizeof(mapbuffer));

        // 依旧设置场景
        mapbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        // 依旧通过mmap的方式映射到用户层
        mapbuffer.memory = V4L2_MEMORY_MMAP;
        // 索引,用于查询每一个缓冲区的信息
        mapbuffer.index = i;

        /*=======查询第i个缓冲区的信息=======*/

        // 这个命令用于查找某个缓冲区的长度和映射信息
        ret = ioctl(fd, VIDIOC_QUERYBUF, &mapbuffer);
        if (ret < 0)
        {
            perror("查询缓存队列失败");
            close(fd);
            return -1;
        }

        /*把内核缓冲区映射到用户空间*/
        mmpaddr[i] = mmap(NULL,
                          mapbuffer.length,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          fd,
                          mapbuffer.m.offset);

        if (mmpaddr[i] == MAP_FAILED)
        {
            perror("mmap 映射失败");
            close(fd);
            return -1;
        }

        /* 记录映射长度，后面 munmap 时要用 */
        addr_length[i] = mapbuffer.length;

        /* 把缓冲区放入采集队列 */
        ret = ioctl(fd, VIDIOC_QBUF, &mapbuffer);
        if (ret < 0)
        {
            perror("放入队列失败");
            close(fd);
            return -1;
        }
    }

    /*==================6.启动摄像头采集==================*/

    // 依旧设置场景为视频采集
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // 命令用于通知设备开始视频流采集
    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret < 0)
    {
        perror("打开设备失败");
        close(fd);
        return -1;
    }

    printf("start capture ok\n");

    /*==================7.从队列中取出一帧图像==================*/

    // 设置一个读取缓冲区结构体
    struct v4l2_buffer readbuffer;
    memset(&readbuffer, 0, sizeof(readbuffer));

    // 依旧设置场景
    readbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    readbuffer.memory = V4L2_MEMORY_MMAP;

    // 表示从设备的采集队列中取出一个已经填充好数据的缓冲区
    ret = ioctl(fd, VIDIOC_DQBUF, &readbuffer);
    if (ret < 0)
    {
        perror("获取数据失败");
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        close(fd);
        return -1;
    }

    printf("获取到一帧数据\n");
    printf("buffer index = %d\n", readbuffer.index);
    printf("bytesused    = %d\n", readbuffer.bytesused);

    /*==================8.保存这一帧YUYV原始数据并转化为PNG图片==================*/
    /**
     *因为摄像头输出的是YUYV,不是什么图片格式,所以不能直接当成图片直接打开,是YUYV裸数据
     */

    FILE *file = fopen("/root/camera_data/1.yuyv", "wb");
    if (!file)
    {
        perror("fopen 1.yuyv 失败");
        ioctl(fd, VIDIOC_QBUF, &readbuffer);
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        close(fd);
        return -1;
    }

    fwrite(mmpaddr[readbuffer.index], readbuffer.bytesused, 1, file);
    fclose(file);

    printf("已保存一帧 YUYV 原始数据到 1.yuyv\n");

    // 转化为PNG图片
    char cmd[512];

    snprintf(cmd,
             sizeof(cmd),
             "ffmpeg -y -f rawvideo -pixel_format yuyv422 "
             "-video_size %dx%d "
             "-i /root/camera_data/1.yuyv "
             "/root/camera_data/1.png",
             vfmt.fmt.pix.width,
             vfmt.fmt.pix.height);

    printf("开始转换 PNG:\n%s\n", cmd);

    ret = system(cmd);
    if (ret != 0)
    {
        printf("YUYV 转 PNG 失败，请确认是否安装了 ffmpeg\n");
        printf("可以执行: sudo apt install ffmpeg\n");
    }
    else
    {
        printf("已转换 PNG 图片到 /root/camera_data/1.png\n");
    }

    /*==================9.用完后把缓冲区重新放回队列==================*/
    // 如果不放回去视频流就不完整了

    ret = ioctl(fd, VIDIOC_QBUF, &readbuffer);
    if (ret < 0)
    {
        perror("重新放入队列失败");
    }

    /*==================10.关闭视频流==================*/

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0)
    {
        perror("关闭设备失败");
    }

    /*==================11.取消内存映射==================*/
    for (int i = 0; i < 4; i++)
    {
        munmap(mmpaddr[i], addr_length[i]);
    }

    /*==================12.关闭设备文件==================*/
    close(fd);

    printf("程序结束\n");

    return 0;
}
