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
#include <linux/delay.h>

#define CMD_TRIG 100


struct gpio_desc{
    int gpio;
    int irq;
    char *name;
    int key;
    struct timer_list key_timer;
} ;

// ===================== 仅修改这里的引脚编号 =====================
static struct gpio_desc gpios[2] = {
    {116, 0, "trig", },  // GPIO4_20
    {117, 0, "echo", },  // GPIO4_21
};
// ===============================================================

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_sr04[BUF_LEN];   // 缓冲区数组
static int r, w;  // 读写索引

struct fasync_struct *Sr04_fasync;  // 异步通知结构体

#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_sr04_buf_empty(void)
{
    return (r == w);
}

static int is_sr04_buf_full(void)
{
    return (r == NEXT_POS(w));
}

static void put_sr04(int sr04_val)
{
    if (!is_sr04_buf_full())
    {
        g_sr04[w] = sr04_val;
        w = NEXT_POS(w);
    }
}

static int get_sr04(void)
{
    int sr04_val = 0;
    if (!is_sr04_buf_empty())
    {
        sr04_val = g_sr04[r];
        r = NEXT_POS(r);
    }
    return sr04_val;
}

// 创建等待队列
static DECLARE_WAIT_QUEUE_HEAD(gpio_wait);




/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t sr04_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    /* 在读函数和中断里面不能添加打印 */
    int err;
    int sr04;

    
    if (is_sr04_buf_empty() && (file->f_flags & O_NONBLOCK))
        return -EAGAIN;

    /* 等待缓冲区非空 */
    wait_event_interruptible(gpio_wait, !is_sr04_buf_empty());

    /* 从缓冲区读取数据 */
    sr04 = get_sr04(); 
    if (sr04 == -1)
    {
        return -EAGAIN;
    }

    /* 向用户程序返回数据 */
    err = copy_to_user(buf, &sr04, sizeof(sr04));  

    return sizeof(sr04);
}




/* poll轮询 */
static unsigned int sr04_poll(struct file *fp, poll_table * wait)
{
    /* 将当前进程加入到等待队列gpio_wait中，直到传感器缓冲区非空 */
    poll_wait(fp, &gpio_wait, wait);    //wait: 超时时间

    /* 返回POLLIN | POLLRDNORM，表示可以进行读取操作。*/
    return is_sr04_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}


/* 异步通知 */
static int sr04_fasync(int fd, struct file *file, int on)
{
    // 添加异步通知
    if (fasync_helper(fd, file, on, &Sr04_fasync) >= 0)
        return 0;
    else
        return -EIO;
}

/* ioctl函数 */
static long sr04_ioctl(struct file *filp, unsigned int command, unsigned long arg)
{
    //send trig
    switch(command)
    {
        case CMD_TRIG:
        {
            // 发送触发信号
            gpio_set_value(gpios[0].gpio, 1);
            udelay(20);
            gpio_set_value(gpios[0].gpio, 0);
            
            /* 启动定时器 -设置超时时间,避免丢失信号 */
            mod_timer(&gpios[1].key_timer, jiffies + msecs_to_jiffies(50));
        }
    }
    return 0;
}

/* 定义自己的file_operations结构体                                              */
static struct file_operations sr04_drv = {
    .owner     = THIS_MODULE,
    .read    = sr04_read,
    .poll    = sr04_poll,
    .fasync  = sr04_fasync,
    .unlocked_ioctl = sr04_ioctl,
};  


static irqreturn_t sr04_isr(int irq, void *dev_id)
{
    struct gpio_desc *gpio_desc = dev_id;
    int val;
    static u64 rising_time = 0;  
    u64 time;
    /* 读取gpio值 */
    val = gpio_get_value(gpio_desc->gpio);
    if(val)
    {
        /* 上升沿记录起始时间 */
        rising_time = ktime_get_ns();
    }
    else
    {
        if(rising_time == 0)
        {
            return IRQ_HANDLED;
        }
        /* 下降沿记录结束时间，并计算时间间隔*/
        //停止定时器
        del_timer(&gpios[1].key_timer);

        //计算时间间隔
        time = ktime_get_ns() - rising_time;
        rising_time = 0;

        //放入缓冲区
        put_sr04(time);

        /* 通知等待队列可以读数据 */
        wake_up_interruptible(&gpio_wait);
        kill_fasync(&Sr04_fasync, SIGIO, POLL_IN); 
    }
    return IRQ_HANDLED;
}

//定时器函数
static void sr04_timer_func(unsigned long arg)
{
    /* 将-1放入缓冲区 */
    put_sr04(-1);
    //唤醒等待队列
    wake_up_interruptible(&gpio_wait);
    // 发送一个SIGIO信号给与sr04_fasync相关的异步I/O操作，告知它们有新的数据可读取。这里的POLL_IN表示可读事件。
    kill_fasync(&Sr04_fasync, SIGIO, POLL_IN);
}

/* 在入口函数 */
static int __init sr04_init(void)
{
    int err;

    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    
    // trig pin
    err = gpio_request(gpios[0].gpio, gpios[0].name);   //申请获得GPIO引脚
    gpio_direction_output(gpios[0].gpio, 0);            //设置为输出引脚

    // echo pin
    {
        /* 获取gpio对应的中断号 */
        gpios[1].irq = gpio_to_irq(gpios[1].gpio); 

        /* 申请中断 */ 
        err = request_irq(gpios[1].irq, sr04_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpios[1].name, &gpios[1]);

        /* 设置定时器 */
        setup_timer(&gpios[1].key_timer, sr04_timer_func, (unsigned long)&gpios[1]);
    }

    /* 注册file_operations     */
    major = register_chrdev(0, "100ask_sr04", &sr04_drv);  /* /dev/gpio_desc */

    gpio_class = class_create(THIS_MODULE, "100ask_sr04_class");
    if (IS_ERR(gpio_class)) {
        printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
        unregister_chrdev(major, "100ask_sr04");
        return PTR_ERR(gpio_class);
    }

    device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "sr04"); /* /dev/sr04 */
    
    return err;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit sr04_exit(void)
{

    
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    device_destroy(gpio_class, MKDEV(major, 0));
    class_destroy(gpio_class);
    unregister_chrdev(major, "100ask_sr04");

    // trig pin
    gpio_free(gpios[0].gpio);

    // echo pin
    {
        free_irq(gpios[1].irq, &gpios[1]);
        del_timer(&gpios[1].key_timer);
    }
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(sr04_init);
module_exit(sr04_exit);

MODULE_LICENSE("GPL");