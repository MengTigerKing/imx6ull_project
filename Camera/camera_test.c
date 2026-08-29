#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static void print_fourcc(unsigned int fmt)
{
    printf("%c%c%c%c",
           fmt & 0xff,
           (fmt >> 8) & 0xff,
           (fmt >> 16) & 0xff,
           (fmt >> 24) & 0xff);
}

int main(int argc, char const *argv[])
{
    int fd;
    int ret;

    if (argc != 2)
    {
        printf("Usage: %s </dev/video0,1...>\n", argv[0]);
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        perror("open video device failed");
        return -1;
    }

    /*================1.查询设备能力================*/
    struct v4l2_capability cap;
    // 清空结构体
    memset(&cap, 0, sizeof(struct v4l2_capability));

    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0)
    {
        perror("VIDIOC_QUERYCAP failed");
        close(fd);
        return -1;
    }

    printf("======设备能力======\n");
    printf("driver: %s\n", cap.driver);
    printf("card:%s\n", cap.card);
    printf("bus_info:%s\n", cap.bus_info);

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
    {
        printf("支持视频捕获\n");
    }
    if (cap.capabilities & V4L2_CAP_STREAMING)
    {
        printf("支持流式采集\n");
    }
    if (cap.capabilities & V4L2_CAP_READWRITE)
    {
        printf("支持read/write采集\n");
    }

    /*================2.枚举支持的像素格式================*/
    printf("======支持的像素格式和分辨率======\n");

    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));

    // 设置采集类型为视频采集,表示我要枚举的是"视频采集"类型的格式
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (int i = 0;; i++)
    {
        fmtdesc.index = i;

        ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);

        if (ret < 0)
        {
            break;
        }

        printf("\n格式 index = %d\n", fmtdesc.index);
        printf("description=%s\n", fmtdesc.description);
        printf("pixelformat= ");
        print_fourcc(fmtdesc.pixelformat);
        printf("\n");

        /*3.枚举当前格式支持的分辨率*/
        struct v4l2_frmsizeenum frmsize;
        memset(&frmsize, 0, sizeof(frmsize));

        // 设置采集类型,这句话的意思是我要描述(枚举)的是摄像头"视频采集"功能的格式
        /**
         * V4L2设备可能有很多类型
         * 视频采集
         * 视频输出
         * 元数据采集
         * 多平面采集
         */

        frmsize.pixel_format = fmtdesc.pixelformat;

        printf("支持分辨率:\n");

        for (int j = 0;; j++)
        {
            frmsize.index = j;

            ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
            if (ret < 0)
            {
                break;
            }

            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                printf("%d x %d\n",
                       frmsize.discrete.width,
                       frmsize.discrete.height);
            }
            else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
            {
                printf("stepwise:%dx%d到%d到x%d,step %d x %d\n",
                       frmsize.stepwise.min_width,
                       frmsize.stepwise.min_height,
                       frmsize.stepwise.max_width,
                       frmsize.stepwise.max_height,
                       frmsize.stepwise.step_width,
                       frmsize.stepwise.step_height);
            }
            else if (frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
            {
                printf("continuous:%d x %d 到 %d x %d\n",
                       frmsize.stepwise.min_width,
                       frmsize.stepwise.min_height,
                       frmsize.stepwise.max_width,
                       frmsize.stepwise.max_height);
            }
        }
    }

    /*4.查询当前流参数*/
    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd, VIDIOC_G_PARM, &streamparm);
    if (ret == 0)
    {
        int num = streamparm.parm.capture.timeperframe.numerator;
        int den = streamparm.parm.capture.timeperframe.denominator;

        printf("\n=======当前帧率参数======\n");
        printf("timeperframe =%d/%d 秒 \n", num, den);

        if (num != 0)
        {
            printf("当前 fps=%.2f\n", (double)den / num);
        }
    }

    else
    {
        perror("VIDIOC_G_PARM failed");
    }

    close(fd);
    return 0;
}
