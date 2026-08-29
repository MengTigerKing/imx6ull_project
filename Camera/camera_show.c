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
#include <linux/fb.h>
#include <signal.h>

#define BUFFER_COUNT 4

int fd_fb = -1;
int fd_video = -1;

int screen_size;
int LCD_width;
int LCD_height;
unsigned char *fbbase = NULL;
unsigned long line_length;
unsigned int bpp;

static int running = 1;

/*Ctrl+C退出时*/
void signal_handler(int sig)
{
    running = 0;
}

/*限制RGB范围到0~255*/
static int clamp0_255(int val)
{
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return val;
}

static void yuv_to_rgb(unsigned char y,
                       unsigned char u,
                       unsigned char v,
                       unsigned char *r,
                       unsigned char *g,
                       unsigned char *b)
{
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;

    int r_tmp = (298 * c + 409 * e + 128) >> 8;
    int g_tmp = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b_tmp = (298 * c + 516 * d + 128) >> 8;

    *r = clamp0_255(r_tmp);
    *g = clamp0_255(g_tmp);
    *b = clamp0_255(b_tmp);
}

/* 初始化 LCD framebuffer */
int LCD_Init(void)
{
    // Linux framebuffer子系统里用来描述"可变显示参数"的结构体
    struct fb_var_screeninfo var;
    // 描述硬件固定属性的结构体
    struct fb_fix_screeninfo fix;

    fd_fb = open("/dev/fb0", O_RDWR);
    if (fd_fb < 0)
    {
        perror("打开 /dev/fb0 失败");
        return -1;
    }

    // 从ioctl,从Linux的framebuffer设备里面读取"可变屏幕信息",并保存到var这个结构体里.如果读取失败,就返回小于0
    if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &var) < 0)
    {
        perror("获取 LCD 可变信息失败");
        close(fd_fb);
        fd_fb = -1;
        return -1;
    }

    if (ioctl(fd_fb, FBIOGET_FSCREENINFO, &fix) < 0)
    {
        perror("获取 LCD 固定信息失败");
        close(fd_fb);
        fd_fb = -1;
        return -1;
    }

    LCD_width = var.xres;
    LCD_height = var.yres;
    bpp = var.bits_per_pixel;
    line_length = fix.line_length;

    /*
     * 不建议用 xres * yres * bpp / 8 算 screen_size，
     * 因为 framebuffer 每行可能有对齐填充。
     * 更稳妥的是：
     */
    screen_size = line_length * LCD_height;

    printf("LCD 分辨率: %d x %d\n", LCD_width, LCD_height);
    printf("LCD bpp: %d\n", bpp);
    printf("LCD line_length: %lu\n", line_length);

    fbbase = mmap(NULL,
                  screen_size,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd_fb,
                  0);

    if (fbbase == MAP_FAILED)
    {
        perror("LCD mmap 失败");
        close(fd_fb);
        fd_fb = -1;
        return -1;
    }

    /* 清屏为黑色 */
    memset(fbbase, 0x00, screen_size);

    return 0;
}

/* 画一个像素到 LCD */
static void LCD_Draw_Pixel(int x,
                           int y,
                           unsigned char r,
                           unsigned char g,
                           unsigned char b)
{
    unsigned char *p;

    if (x < 0 || x >= LCD_width || y < 0 || y >= LCD_height)
        return;

    p = fbbase + y * line_length + x * bpp / 8;

    if (bpp == 32)
    {
        /*
         * 大多数 Linux framebuffer 32bpp 是 B G R X 顺序
         */
        p[0] = b;
        p[1] = g;
        p[2] = r;
        p[3] = 0x00;
    }
    else if (bpp == 24)
    {
        p[0] = b;
        p[1] = g;
        p[2] = r;
    }
    else if (bpp == 16)
    {
        /*
         * RGB565
         */
        unsigned short color;

        color = ((r & 0xF8) << 8) |
                ((g & 0xFC) << 3) |
                ((b & 0xF8) >> 3);

        *(unsigned short *)p = color;
    }
}

/* 把一整帧 YUYV 图像显示到 LCD */
void LCD_YUYV_Show(unsigned char *yuyv_data, int width, int height)
{
    int show_width;
    int show_height;

    show_width = width < LCD_width ? width : LCD_width;
    show_height = height < LCD_height ? height : LCD_height;

    /*
     * YUYV 一像素平均 2 字节，
     * 所以一行数据长度是 width * 2
     */
    for (int y = 0; y < show_height; y++)
    {
        unsigned char *src = yuyv_data + y * width * 2;

        for (int x = 0; x < show_width; x += 2)
        {
            unsigned char y0;
            unsigned char u;
            unsigned char y1;
            unsigned char v;

            unsigned char r0, g0, b0;
            unsigned char r1, g1, b1;

            /*
             * YUYV 排列：
             *
             * src[0] = Y0
             * src[1] = U
             * src[2] = Y1
             * src[3] = V
             */
            y0 = src[0];
            u = src[1];
            y1 = src[2];
            v = src[3];

            yuv_to_rgb(y0, u, v, &r0, &g0, &b0);
            LCD_Draw_Pixel(x, y, r0, g0, b0);

            if (x + 1 < show_width)
            {
                yuv_to_rgb(y1, u, v, &r1, &g1, &b1);
                LCD_Draw_Pixel(x + 1, y, r1, g1, b1);
            }

            src += 4;
        }
    }
}

int main(int argc, char **argv)
{
    int ret;
    int type;
    int buffer_count;

    struct v4l2_format vfmt;
    struct v4l2_requestbuffers reqbuffer;
    struct v4l2_buffer mapbuffer;

    unsigned char *mmpaddr[BUFFER_COUNT];
    unsigned int addr_length[BUFFER_COUNT];

    memset(mmpaddr, 0, sizeof(mmpaddr));
    memset(addr_length, 0, sizeof(addr_length));

    signal(SIGINT, signal_handler);

    if (argc != 2)
    {
        printf("用法: %s </dev/video0 或 /dev/video1>\n", argv[0]);
        return -1;
    }

    /*==================1. 初始化 LCD==================*/
    if (LCD_Init() != 0)
    {
        return -1;
    }

    /*==================2. 打开摄像头设备==================*/
    fd_video = open(argv[1], O_RDWR);
    if (fd_video < 0)
    {
        perror("打开摄像头失败");
        goto error;
    }

    /*==================3. 设置摄像头格式为 YUYV==================*/
    memset(&vfmt, 0, sizeof(vfmt));

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /*
     * 你的摄像头最大分辨率是 640x480，
     * 所以这里直接设置成 640x480。
     */
    vfmt.fmt.pix.width = 640;
    vfmt.fmt.pix.height = 480;
    vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    vfmt.fmt.pix.field = V4L2_FIELD_ANY;

    ret = ioctl(fd_video, VIDIOC_S_FMT, &vfmt);
    if (ret < 0)
    {
        perror("设置摄像头采集格式失败");
        goto error;
    }

    /*==================4. 读取驱动最终采用的格式==================*/
    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd_video, VIDIOC_G_FMT, &vfmt);
    if (ret < 0)
    {
        perror("读取摄像头实际格式失败");
        goto error;
    }

    printf("摄像头实际 width  = %d\n", vfmt.fmt.pix.width);
    printf("摄像头实际 height = %d\n", vfmt.fmt.pix.height);

    unsigned char *p = (unsigned char *)&vfmt.fmt.pix.pixelformat;
    printf("摄像头实际 pixelformat = %c%c%c%c\n", p[0], p[1], p[2], p[3]);

    if (vfmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
    {
        printf("错误：摄像头最终输出格式不是 YUYV，程序退出\n");
        goto error;
    }

    /*==================5. 申请内核缓冲区==================*/
    memset(&reqbuffer, 0, sizeof(reqbuffer));

    reqbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuffer.count = BUFFER_COUNT;
    reqbuffer.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(fd_video, VIDIOC_REQBUFS, &reqbuffer);
    if (ret < 0)
    {
        perror("申请摄像头缓冲队列失败");
        goto error;
    }

    if (reqbuffer.count < 2)
    {
        printf("申请到的缓冲区太少: %d\n", reqbuffer.count);
        goto error;
    }

    buffer_count = reqbuffer.count;
    if (buffer_count > BUFFER_COUNT)
        buffer_count = BUFFER_COUNT;

    printf("实际使用缓冲区数量: %d\n", buffer_count);

    /*==================6. 查询、mmap、入队缓冲区==================*/
    for (int i = 0; i < buffer_count; i++)
    {
        memset(&mapbuffer, 0, sizeof(mapbuffer));

        mapbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mapbuffer.memory = V4L2_MEMORY_MMAP;
        mapbuffer.index = i;

        ret = ioctl(fd_video, VIDIOC_QUERYBUF, &mapbuffer);
        if (ret < 0)
        {
            perror("查询摄像头缓冲区失败");
            goto error;
        }

        mmpaddr[i] = mmap(NULL,
                          mapbuffer.length,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          fd_video,
                          mapbuffer.m.offset);

        if (mmpaddr[i] == MAP_FAILED)
        {
            perror("摄像头缓冲区 mmap 失败");
            mmpaddr[i] = NULL;
            goto error;
        }

        addr_length[i] = mapbuffer.length;

        ret = ioctl(fd_video, VIDIOC_QBUF, &mapbuffer);
        if (ret < 0)
        {
            perror("摄像头缓冲区入队失败");
            goto error;
        }
    }

    /*==================7. 开启视频流==================*/
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd_video, VIDIOC_STREAMON, &type);
    if (ret < 0)
    {
        perror("开启摄像头采集失败");
        goto error;
    }

    printf("开始实时显示，按 Ctrl+C 退出\n");

    /*==================8. 循环取帧并显示到 LCD==================*/
    while (running)
    {
        struct v4l2_buffer readbuffer;

        memset(&readbuffer, 0, sizeof(readbuffer));

        readbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        readbuffer.memory = V4L2_MEMORY_MMAP;

        /*
         * 从采集队列取出一帧数据
         */
        ret = ioctl(fd_video, VIDIOC_DQBUF, &readbuffer);
        if (ret < 0)
        {
            perror("获取摄像头数据失败");
            break;
        }

        if (readbuffer.index >= buffer_count)
        {
            printf("错误:readbuffer.index 越界: %d\n", readbuffer.index);
            break;
        }

        /*
         * 把 YUYV 数据转换成 RGB，然后写到 LCD framebuffer
         */
        LCD_YUYV_Show(mmpaddr[readbuffer.index],
                      vfmt.fmt.pix.width,
                      vfmt.fmt.pix.height);

        /*
         * 显示完之后，重新把缓冲区放回队列
         */
        ret = ioctl(fd_video, VIDIOC_QBUF, &readbuffer);
        if (ret < 0)
        {
            perror("摄像头缓冲区重新入队失败");
            break;
        }
    }

    /*==================9. 关闭视频流==================*/
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd_video, VIDIOC_STREAMOFF, &type);

    /*==================10. 取消 mmap==================*/
    for (int i = 0; i < buffer_count; i++)
    {
        if (mmpaddr[i] != NULL)
        {
            munmap(mmpaddr[i], addr_length[i]);
        }
    }

    /*==================11. 关闭设备==================*/
    if (fd_video >= 0)
        close(fd_video);

    if (fbbase != NULL && fbbase != MAP_FAILED)
        munmap(fbbase, screen_size);

    if (fd_fb >= 0)
        close(fd_fb);

    printf("程序退出\n");
    return 0;

error:
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (fd_video >= 0)
        ioctl(fd_video, VIDIOC_STREAMOFF, &type);

    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        if (mmpaddr[i] != NULL)
        {
            munmap(mmpaddr[i], addr_length[i]);
        }
    }

    if (fd_video >= 0)
        close(fd_video);

    if (fbbase != NULL && fbbase != MAP_FAILED)
        munmap(fbbase, screen_size);

    if (fd_fb >= 0)
        close(fd_fb);

    return -1;
}