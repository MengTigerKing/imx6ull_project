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
#include <linux/kthread.h>
#include <linux/pwm.h>
#include <linux/uaccess.h>

static int major = 0;
static struct class *sg90_class;           // 设备类别
static struct pwm_device *sg90_pwm_device; // pwm结构体指针

static ssize_t sg90_open(struct inode *inode, struct file *file)
{
    pwm_config(sg90_pwm_device, 500000, 20000000);          // 设置pwm参数:初始角度,频率,单位ns
    pwm_set_polarity(sg90_pwm_device, PWM_POLARITY_NORMAL); // 设置pwm极性
    pwm_enable(sg90_pwm_device);
    return 0;
}

static ssize_t sg90_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    // printk("====%s====\n", __FUNCTION__);
    return 0;
}

static ssize_t sg90_write(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
    int ret;
    unsigned char data[1];
    ret = copy_from_user(data, buf, size);
    pwm_config(sg90_pwm_device, 500000 + data[0] * 100000 / 9, 20000000);
    return 0;
}

static int sg90_release(struct inode *inode, struct file *filp)
{
    pwm_free(sg90_pwm_device);
    return 0;
}

static struct file_operations sg90_drv = {
    .owner = THIS_MODULE,
    .open = sg90_open,
    .read = sg90_read,
    .write = sg90_write,
    .release = sg90_release,
};

static int sg90_probe(struct platform_device *pdev)
{
    /*从设备树获得硬件信息,获取pwm引脚*/
    sg90_pwm_device = devm_of_pwm_get(&pdev->dev, pdev->dev.of_node, NULL);
    if (IS_ERR(sg90_pwm_device))
    {
        dev_err(&pdev->dev, "Faild to get PWM for sg90\n");
        return PTR_ERR(sg90_pwm_device);
    }

    /*注册file_operations*/
    major = register_chrdev(0, "sg90_chrdev", &sg90_drv);
    sg90_class = class_create(THIS_MODULE, "sg90_class");
    device_create(sg90_class, NULL, MKDEV(major, 0), NULL, "sg90");

    dev_info(&pdev->dev, "=======sg90 initialized successfully======\n");
    return 0;
}

static int sg90_remove(struct platform_device *pdev)
{
    device_destroy(sg90_class, MKDEV(major, 0));
    class_destroy(sg90_class);
    unregister_chrdev(major, "sg90_chrdev");

    return 0;
}

static const struct of_device_id sg90_match_table[] = {
    /*匹配字符串"tiger,sr04"用于标识*/
    {.compatible = "tiger,sg90"},
    /*空项作为匹配表的结束标识*/
    {},
};

/*------定义platform driver------*/
static struct platform_driver sg90_driver = {
    .probe = sg90_probe,
    .remove = sg90_remove,
    .driver = {
        .name = "sg90",
        .of_match_table = sg90_match_table,
    },
};

/*------入口初始化函数------*/
static int __init sg90_platform_driver_init(void)
{
    int ret = 0;
    ret = platform_driver_register(&sg90_driver); // 注册设备信息
    return ret;
}

static void __exit sg90_platform_driver_exit(void)
{
    platform_driver_unregister(&sg90_driver); // 销毁设备信息
}

module_init(sg90_platform_driver_init);
module_exit(sg90_platform_driver_exit);

MODULE_LICENSE("GPL");