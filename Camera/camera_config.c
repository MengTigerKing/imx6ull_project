#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s </dev/video0,1....>\n", argv[0]);
        return 1;
    }

    /*打开摄像头设备*/
    int fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        perror("open err");
        return -1;
    }

    /*设置摄像头采集格式*/
    /**
     * 采集格式包括:
     * 采集类型
     * 宽度
     * 高度
     * 像素格式
     * 扫描方式
     * 每行字节数
     * 一帧图像大小
     * 颜色空间等信息
     */
    struct v4l2_format vfmt; // 帧格式
    // 清零
    memset(&vfmt, 0, sizeof(vfmt));

    /**
     * V4L2设备可能有多种类型
     * 视频采集
     * 视频输出
     * 多平面采集
     * 元数据采集
     */
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 选择视频抓取,意思是我要配置摄像头采集画面的格式

    vfmt.fmt.pix.width = 640;
    vfmt.fmt.pix.height = 480;
    vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // 设置视频采集格式

    int ret = ioctl(fd, VIDIOC_S_FMT, &vfmt);
    if (ret < 0)
    {
        perror("设置采集格式错误");
        close(fd);
        return -1;
    }

    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd, VIDIOC_G_FMT, &vfmt);
    if (ret < 0)
    {
        perror("读取采集格式失败");
        close(fd);
        return -1;
    }
    printf("width=%d\n", vfmt.fmt.pix.width);
    printf("height=%d\n", vfmt.fmt.pix.height);

    unsigned char *p = (unsigned char *)&vfmt.fmt.pix.pixelformat;
    printf("pixelformat =%c%c%c%c \n", p[0], p[1], p[2], p[3]);
    close(fd);
    return 0;
}
