#include <linux/module.h>
#include <linux/poll.h>
#include "linux/jiffies.h"
#include <linux/delay.h>

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
#include <linux/kthread.h>



/* 主设备号                                                                 */
static int major = 0;
static struct class *dht11_class;    //设备类
static struct gpio_desc *dht11_gpio_desc; // GPIO结构体操作指针

static void dht11_reset(void)
{
    //gpio先切换称输出模式给dht11发信息
    gpiod_direction_output(dht11_gpio_desc, 1);
}

static void dht11_start(void)
{
  // 不能使用GPIOD_OUT_HIGH，有问题，字节用0,1
  mdelay(30);
  gpiod_set_value(dht11_gpio_desc, 0);
  mdelay(20);
  gpiod_set_value(dht11_gpio_desc, 1);
  udelay(40);
  //切换称输入模式，接受dht11的信息
  gpiod_direction_input(dht11_gpio_desc);
  udelay(2);
}

static int dht11_wait_ready(void)
{
    int timeout_us = 20000;

    /*  等待低电平 - 或者直到超时*/
    timeout_us = 200;
    while (gpiod_get_value(dht11_gpio_desc) && --timeout_us)
    {
        udelay(1);
    }
    // 超时
    if (!timeout_us)
    {
        printk("----debug1-----\n");
        return -1;
    }

    /* 等待高电平 - 或者直到超时 */
    timeout_us = 200;
    while (!gpiod_get_value(dht11_gpio_desc) && --timeout_us)
    {
        udelay(1);
    }
    if (!timeout_us)
    {
        printk("----debug2-----\n");
        return -1;
    }

    /*  等待低电平 */
    timeout_us = 200;
    while (gpiod_get_value(dht11_gpio_desc) && --timeout_us)
    {
        udelay(1);
    }
    // 超时
    if (!timeout_us)
    {
        printk("----debug3-----\n");
        return -1;
    }

    return 0;
}

static int dht11_read_byte(unsigned char *data)
{
    int i = 0;
    unsigned char buffer = 0; //0-255
    int timeout_us = 400;
    for (i = 0; i < 8; i++)
    {

        /* 等待高电平  */
        timeout_us = 400;
        while (!gpiod_get_value(dht11_gpio_desc) && --timeout_us)
        {
            udelay(1);
        }
        if (!timeout_us)
        {
            return -1;
        }

        udelay(40);

        /* 延时在26-28us是0，70us是1，直接延时40us后，读到高电平则是1*/
        if (gpiod_get_value(dht11_gpio_desc))
        {
            buffer = (buffer << 1) | 1; //初始：0左移1位还是0,|1则为1

            /*  等待高电平结束 */
            timeout_us = 400;
            while (gpiod_get_value(dht11_gpio_desc) && --timeout_us)
            {
                udelay(1);
            }
            if (!timeout_us)
            {
                return -1;
            }
        }
        else
        {
            buffer = (buffer << 1) | 0;
        }
    }

    *data = buffer;
    return 0;
}

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
/**
 * 从DHT11传感器读取数据
 * 
 * 此函数用于从DHT11湿度和温度传感器读取数据。它首先发送一个高脉冲来启动DHT11，
 * 然后等待DHT11准备好。接着，它读取5个字节的数据，并根据校验码验证数据的完整性。
 * 最后，将数据返回给用户空间。
 * 
 * @param file 文件结构指针，通常为NULL
 * @param buf 用户空间的缓冲区指针，用于接收数据
 * @param size 要读取的数据大小
 * @param offset 当前文件位置指针，通常未使用
 * 
 * @return 成功时返回读取的字节数，失败时返回负错误码
 */
static ssize_t dht11_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    int i = 0;
    unsigned char kernel_buf[5];    // 存储从DHT11读取的数据
    unsigned long flags;            // 保存中断状态
    u64 pre, last;

    // 检查输入的大小是否正确，DHT11返回的数据应为4字节
    if(size != 4)
        return -EINVAL;

    printk("==== %s ====\n", __FUNCTION__);

    
    local_irq_save(flags);  //保存并禁用本地中断。

    pre = ktime_get_ns();
    udelay(40);
    last = ktime_get_ns();

    printk("udelay 40 times use ns: %lld\n", last - pre);

    // 发送高脉冲启动DHT11
    dht11_reset();
    dht11_start();

    // 等待DHT11就绪
    if (0 != dht11_wait_ready()) 
    {
        printk("设备未就绪\n");
        local_irq_restore(flags); // 恢复中断状态
        return -1;
    }

    // 读取5个字节
    for (i = 0; i < 5; i++)
    {
        if (dht11_read_byte(&kernel_buf[i]) != 0) //返回其他数据则错误
        {
            local_irq_restore(flags); // 恢复中断状态
            return -1;
        }
    }

    dht11_reset();            // 复位

    local_irq_restore(flags);  // 恢复中断状态

    // 根据校验码验证数据
    if (kernel_buf[4] != kernel_buf[0] + kernel_buf[1] + kernel_buf[2] + kernel_buf[3]) {
        printk("验证错误\n");
        local_irq_restore(flags);  // 恢复中断状态
        return -1;
    }

    // 将数据返回给用户空间
    size = copy_to_user(buf, kernel_buf, 4);

    return 4;
}




/* 定义自己的file_operations结构体                                              */
static struct file_operations dht11_drv = {
    .owner     = THIS_MODULE,
    .read    = dht11_read,
};

static int dht11_probe(struct platform_device *pdev)
{

    printk("====%s====\n", __FUNCTION__);

    // 获得硬件信息
    dht11_gpio_desc = gpiod_get(&pdev->dev, "dht11", GPIOD_OUT_HIGH); 
    if (IS_ERR(dht11_gpio_desc))
    {
        dev_err(&pdev->dev, "Failed to get GPIO for dht11\n");
        return PTR_ERR(dht11_gpio_desc);
    }
    /* 注册file_operations     */
    major = register_chrdev(0, "dht11_chrdev", &dht11_drv); /* /dev/gpio_desc */

    dht11_class = class_create(THIS_MODULE, "dht11_class");

    device_create(dht11_class, NULL, MKDEV(major, 0), NULL, "mydht11"); /* /dev/mydht11 */

    dev_info(&pdev->dev, "dht11 initialized successfully\n");
    return 0;
}

static int dht11_remove(struct platform_device *pdev)
{

    printk("======%s=======\n", __FUNCTION__);

    device_destroy(dht11_class, MKDEV(major, 0));
    class_destroy(dht11_class);
    unregister_chrdev(major, "dht11_chrdev");
    gpiod_put(dht11_gpio_desc);         // 释放 GPIO

    return 0;
}

/* 定义设备树匹配表，用于识别和支持特定的LED驱动器 */
static const struct of_device_id dht11_match_table[] = {
    /* 匹配字符串 "fire,dht11" 用于标识 */
    {.compatible = "fire,dht11"},
    /* 空项作为匹配表的结束标志 */
    {},
};



/*  定义platform_driver */
static struct platform_driver dht11_driver = {
    .probe = dht11_probe,   // 设置探测函数，当设备被探测到时调用
    .remove = dht11_remove, // 设置移除函数，当设备被移除时调用

    /* 设置<驱动程序的名称>和<设备树匹配表> */
    .driver = {
        .name = "dht11",                     // 字符设备名
        .of_match_table = dht11_match_table, // 设置设备树匹配表，用于设备的匹配
    },
};

/* 在入口函数 */
static int __init dht11_platform_driver_init(void)
{
    int ret = 0;
    printk("====%s====\n", __FUNCTION__);

    ret = platform_driver_register(&dht11_driver); // 注册驱动程序

    return ret;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit dht11_platform_driver_exit(void)
{
    printk("====%s====\n", __FUNCTION__);

     platform_driver_unregister(&dht11_driver); // 销毁设备信息

}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(dht11_platform_driver_init);
module_exit(dht11_platform_driver_exit);

MODULE_LICENSE("GPL");
