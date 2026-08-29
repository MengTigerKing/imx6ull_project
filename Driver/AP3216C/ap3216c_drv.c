#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>

// 设置主设备号
static int major;

static struct class *ap3216c_class;

// i2c特有的全局变量,作为客户端结构体
static struct i2c_client *ap3216c_client;

/*------open函数------*/
/**
 * 硬件逻辑:
 * 寄存器0 = 0x04 -> 软件复位
 * 寄存器0 = 0x03 -> 开启所有传感器功能
 */
static int ap3216c_open(struct inode *node, struct file *flip)
{
    /*硬件复位:向寄存器写入0x04*/
    i2c_smbus_write_byte_data(ap3216c_client, 0, 0x04);

    /*复位延时:传感器手册要求至少10ms,这里延时15ms保证稳定*/
    mdelay(15);

    /*传感器使能:向寄存器0写入0x03(开启红外+光强+距离检测*/
    i2c_smbus_write_byte_data(ap3216c_client, 0, 0x03);

    return 0;
}

/*------读函数-------*/
static ssize_t ap3216c_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    int var, err;
    char data[6]; // 存储6字节原始数据,1字节8位,每个数据占16位,占两个数组 组成:IR(2)+光强(2)+距离(2)

    if (size != 6) // 必须读取6个字节
    {
        return -EINVAL;
    }

    /*读取IR数据:寄存器0xA,读取16位数据*/
    var = i2c_smbus_read_word_data(ap3216c_client, 0xa);
    data[0] = (var >> 8) & 0xff; // 高8字节
    data[1] = var & 0xff;        // 低8字节

    /*读取光强数据:寄存器0xC*/
    var = i2c_smbus_read_word_data(ap3216c_client, 0xc);
    data[2] = (var >> 8) & 0xff; // 高8字节
    data[3] = var & 0xff;        // 低8字节

    /*读取距离数据:寄存器0xE*/
    var = i2c_smbus_read_word_data(ap3216c_client, 0xe);
    data[4] = (var >> 8) & 0xff; // 高8字节
    data[5] = var & 0xff;        // 低8字节

    /*内核态数据->用户态缓冲区拷贝*/
    err = copy_to_user(buf, data, size);
    if (err)
    {
        return EFAULT;
    }

    return size;
}

/*------设备数匹配表------*/
static const struct of_device_id ap3216c_dt_match[] = {
    {
        .compatible = "tiger,ap3216c", // 必须和设备树中的compatible属性完全一致
    },

    {} // 空结构体:表示匹配表结束
};

/*-------传统I2C设备匹配表------*/
static const struct i2c_device_id ap3216c_i2c_id[] = {
    {"ap3216c", 0}, // I2C设备名匹配
    {}};

/*------绑定函数------*/
static struct file_operations ap3216c_fops = {
    .owner = THIS_MODULE,
    .open = ap3216c_open,
    .read = ap3216c_read,
};

/*------探测驱动函数------*/
static int ap3216c_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct device *result;
    ap3216c_client = client;

    /*1.注册字符设备:0=内核自动分配主设备号*/
    major = register_chrdev(0, "ap3216c_chrdev", &ap3216c_fops);

    /*2.创建设备类*/
    ap3216c_class = class_create(THIS_MODULE, "my_ap3216c_class");
    if (IS_ERR(ap3216c_class))
    {
        dev_err(&client->dev, "Failed to create class\n");
        unregister_chrdev(major, "ap3216c_chrdev");
        return PTR_ERR(ap3216c_class);
    }

    /*创建设备节点*/
    result = device_create(ap3216c_class, NULL, MKDEV(major, 0), NULL, "ap3216c");
    if (IS_ERR(result))
    {
        printk("Failed to create a93216c device\n");
        class_destroy(ap3216c_class);               // 释放类
        unregister_chrdev(major, "ap3216c_chrdev"); // 释放字符设备
        return -ENODEV;
    }

    dev_info(&client->dev, "======ap3216c initialized successfully======\n");
    return 0;
}

static int ap3216c_i2c_remove(struct i2c_client *client)
{
    device_destroy(ap3216c_class, MKDEV(major, 0));
    class_destroy(ap3216c_class);
    unregister_chrdev(major, "ap3216c_chrdev");
    return 0;
}

static struct i2c_driver ap3216c_i2c_driver = {
    .probe = ap3216c_i2c_probe,
    .remove = ap3216c_i2c_remove,
    .id_table = ap3216c_i2c_id, // 传统I2C匹配表
    .driver = {
        .owner = THIS_MODULE,
        .name = "ap3216c",
        .of_match_table = ap3216c_dt_match,
    },
};

static int __init ap3216c_i2c_init(void)
{
    int ret;
    ret = i2c_add_driver(&ap3216c_i2c_driver); // 注册I2C驱动到内核
    if (ret != 0)
        pr_err("Failed to register ap3216c I2C driver: %d\n", ret);
    return 0;
}

static void __exit ap3216c_i2c_exit(void)
{
    i2c_del_driver(&ap3216c_i2c_driver); // 删除I2C驱动
}

module_init(ap3216c_i2c_init);
module_exit(ap3216c_i2c_exit);
MODULE_LICENSE("GPL");
