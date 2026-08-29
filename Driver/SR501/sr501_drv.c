#include <linux/module.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/gpio.h>

// 设置主设备号(如果给0,则内核自动分配)
static int major = 0;
// 设备类型,用于创建设备节点
static struct class *sr501_class;
static struct gpio_desc *sr501_gpio; // GPIO结构体操作指针int i;
static int sr501_irq;                // GPIO中断号
static wait_queue_head_t sr501_wq;   // 等待队列
struct fasync_struct *sr501_fasync;

// 创造环形缓冲区:解决"中断处理太快的问题"
#define BUF_LEN 128                     // 环形缓冲区的长度(128个整形)
static int g_sr501[BUF_LEN];            // 环形缓冲区
static int r, w;                        // 下标表示读和写的位置
#define NEXT_POS(x) ((x + 1) % BUF_LEN) // 找到环形缓冲区中,存放数据的下一个位置是什么,通过+1取模完成

// 判断缓冲区是否为空,1空/0非空
static int is_sr501_buf_empty(void)
{
    if (r == w)
        return 1;
    else
        return 0;
}

// 判断缓冲区是否为满,1满/0非满
static int is_sr501_buf_full(void)
{
    if (r == NEXT_POS(w))
        return 1;
    else
        return 0;
}

// 向缓冲区写数据(中断的时候需要调用这个函数)
static void put_sr501(int sr501_val)
{
    // 判断缓冲区放满了没
    if (is_sr501_buf_full() != 1)
    {
        // 往里面放
        g_sr501[w] = sr501_val;
        // w++
        w = NEXT_POS(w);
    }
}

// 向缓冲区读取数据(用户read时调用)
static int get_sr501(void)
{
    int sr501_val = 0;
    // 判断缓冲区是不是空
    if (is_sr501_buf_empty() != 1)
    {
        // 从读索引位置取数据
        sr501_val = g_sr501[r];
        // r++
        r = NEXT_POS(r);
    }
    return sr501_val;
}

// read函数
/**
 * static ssize_t dev_drv_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
 * @brief 驱动读函数
 * @param file:内核文件对象(与应用层open后得到的fd相对应)
 * @param buf:用户层缓冲区
 * @param size:用户请求读取的字节数量
 * @param offeset:偏移量,字符设备一般忽略,块设备需要处理
 * @return 成功返回读取到的字节数,失败返回负数
 */
static ssize_t gpio_drv_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    int err;
    int sr501_val; // 四个字节的缓冲区

    // file->f_fags表示是阻塞还是非阻塞状态
    // 非阻塞状态,没数据,不阻塞休眠,直接报错
    if (is_sr501_buf_empty() && (file->f_flags & O_NONBLOCK))
    {
        return -EAGAIN;
    }

    // 阻塞状态,用wait_event_interruptible判断缓冲区是否非空,如果空的就阻塞进程
    /**
     * int wait_event_interruptible(wait_queue_head_t wq, bool condition);
     * @param wq:等待队列头,是进程"休眠的容器"
     * @param condition:唤醒条件,bool类型,为ture的时候,进程才会正常唤醒并执行
     * @return 条件满足,进程被正常唤醒返回0,进程被信号中断返回非0
     */
    wait_event_interruptible(sr501_wq, !is_sr501_buf_empty()); // 检查缓冲区是否非空,如果TRUE,进程就不休眠,如果FALSE就休眠

    sr501_val = get_sr501();

    /**
     * unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
     * @brief Linux内核提供的核心函数,将数组从内核空间拷贝到用户空间
     * @param to: 目标地址
     * @param from:源地址
     * @param n:要拷贝的字节数
     * @return 拷贝成功返回0,拷贝失败返回未成功拷贝的字节数
     */
    err = copy_to_user(buf, &sr501_val, sizeof(sr501_val));
    return sizeof(sr501_val);
}

// poll
static unsigned int gpio_drv_poll(struct file *fp, poll_table *wait)
{
    /**
     * 1.函数会不断检查GPIO端口的状态,如果状态满足等待条件,则结束等待
     * 2.如果等待时间超过指定时间,则返回超时错误
     * wait:等待时间
     */
    // 把等待队列加入poll的wait表(让内核知道"这个进程在等数据")
    poll_wait(fp, &sr501_wq, wait);

    // 返回状态:如果缓冲区空非空,就告诉用户"可以读了"
    return is_sr501_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

// 异步通知
static int gpio_drv_fasync(int fd, struct file *file, int on)
{
    // 该函数用于注册或注销异布通知,如果注册成功,则返回0,否则返回-EIO
    if (fasync_helper(fd, file, on, &sr501_fasync) >= 0)
        return 0;
    else
        return -EIO;
}

// 定义file_operations结构体:告诉内核"用户调用不同应用层函数的时候,该调用对应的哪个驱动层函数"
static struct file_operations sr501_fops =
    {
        .owner = THIS_MODULE,      // 模块拥有者
        .read = gpio_drv_read,     // 这里gpio接口,所以gpio_drv_read
        .poll = gpio_drv_poll,     // 同上,gpio_drv_poll
        .fasync = gpio_drv_fasync, // 设置异步通知的时候调用
};

/**
 * sr501_isr
 * @brief GPIO sr501中断程序服务
 * @param irq:中断号
 * @param dev_id:设备表示,这里指向结构体gpio_desc
 * @return IRQ_HANDLED 表示中断已处理成功
 * 该函数处理GPIO下的sr501中断事件,当sr501触发中断时,此函数将会被调用
 * 它通过读取GPIO值来确定sr501的数值,并将sr501事件传递给环形缓冲区
 * 同时,它还会换行任何在等待这个时间的进程,并处理异步I/O的请求
 */
static irqreturn_t sr501_isr(int irq, void *dev_id)
{
    int sr501_val;

    // 打印发生中断的GPIO编号//
    printk("sr501_isr %d irq happened\n", sr501_irq);

    // 快速读取gpio的值
    sr501_val = gpiod_get_value(sr501_gpio);
    put_sr501(sr501_val); // 将rsr_501的值放入环形缓冲区

    /*------通知用户空间的进程有关设备状态的变化------*/

    // 唤醒任何在sr501_wq上等待的进程
    wake_up_interruptible(&sr501_wq);

    // 发送SIGIO信号给sr501_fasync队列,通知有异步事件发生
    kill_fasync(&sr501_fasync, SIGIO, POLL_IN);

    return IRQ_HANDLED;
}

/*------在入口函数------*/
static int sr501_probe(struct platform_device *pdev)
{
    int err;
    // 从设备树中获取引脚,只有一个引脚给0
    sr501_gpio = gpiod_get(&pdev->dev, "sr501", 0);
    if (IS_ERR(sr501_gpio))
    {
        dev_err(&pdev->dev, "Failed to get GPIO for sr501\n");
        return PTR_ERR(sr501_gpio);
    }
    gpiod_direction_input(sr501_gpio); // 设置为输入状态

    sr501_irq = gpiod_to_irq(sr501_gpio); // 为GPIO申请中断号

    /*注册中断处理程序*/
    err = request_irq(sr501_irq, sr501_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "sr501", NULL);
    if (err)
    {
        dev_err(&pdev->dev, "Failed to request IRQ for sr501\n");
        gpiod_put(sr501_gpio);
        return err;
    }

    /*注册file_operations*/
    major = register_chrdev(0, "sr501_chrdev", &sr501_fops);
    sr501_class = class_create(THIS_MODULE, "my_sr501_class");
    if (IS_ERR(sr501_class))
    {
        dev_err(&pdev->dev, "Failed to create class\n");
        unregister_chrdev(major, "sr501_chrdev");
        return PTR_ERR(sr501_class);
    }
    device_create(sr501_class, NULL, MKDEV(major, 0), NULL, "my_sr501_tree");
    dev_info(&pdev->dev, "sr501 initialized successfully\n");
    return err;
}

static int sr501_remove(struct platform_device *pdev)
{
    device_destroy(sr501_class, MKDEV(major, 0));
    class_destroy(sr501_class);
    unregister_chrdev(major, "sr501_chrdev");

    gpiod_put(sr501_gpio);
    free_irq(sr501_irq, NULL);
    return 0;
}

/*------定义设备树匹配表,用于识别和支持特定的驱动器------*/
static const struct of_device_id sr501_match_table[] = {
    /*匹配字符串"tiger,sr501"用于标识*/
    {.compatible = "tiger,sr501"},
    /*空项作为匹配表的结束标识*/
    {},
};

/*------定义platform_driver------*/
static struct platform_driver sr501_driver = {
    .probe = sr501_probe,   // 设置探测函数,当设备树被检查到的时候调用
    .remove = sr501_remove, // 设置移除函数,当设备被移除时调用
    /*设置<驱动程序的名称>和<设备树列表>*/
    .driver = {
        .name = "sr501",                     // 字符设备名,有设备树其实基本永不到这个
        .of_match_table = sr501_match_table, // 设置设备树匹配表,用于匹配设备
    },
};

/*------入口初始化函数,只需要执行一次,所以打上__init的标签------*/
static int __init gpio_drv_init(void)
{
    int err;
    init_waitqueue_head(&sr501_wq);
    err = platform_driver_register(&sr501_driver);
    return err;
}

/*-------卸载驱动程序------*/
static void __exit gpio_drv_exit(void)
{
    platform_driver_unregister(&sr501_driver);
    printk("=====exit=====\n");
}

/*------其他完善:提供设备信息,自动创建设备接节点------*/
module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

MODULE_LICENSE("GPL");
