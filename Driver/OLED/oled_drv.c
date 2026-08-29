#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/printk.h>

#define SSD1306_MAX_SEG (128)
#define SSD1306_MAX_LINE (7)
#define SSD1306_DEF_FONT_SIZE (5)

// 全局变量：记录光标位置，用于记录写到屏幕哪里了
static int major;
static struct class *SSD1306_class;
static struct i2c_client *etx_i2c_client_oled = NULL;
static uint8_t SSD1306_LineNum = 0;
static uint8_t SSD1306_CursorPos = 0;

// 字符数库SSD1306_font
static const unsigned char SSD1306_font[][SSD1306_DEF_FONT_SIZE] =
    {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // space
        {0x00, 0x00, 0x2f, 0x00, 0x00}, // !
        {0x00, 0x07, 0x00, 0x07, 0x00}, // "
        {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
        {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
        {0x23, 0x13, 0x08, 0x64, 0x62}, // %
        {0x36, 0x49, 0x55, 0x22, 0x50}, // &
        {0x00, 0x05, 0x03, 0x00, 0x00}, // '
        {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
        {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
        {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
        {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
        {0x00, 0x00, 0xA0, 0x60, 0x00}, // ,
        {0x08, 0x08, 0x08, 0x08, 0x08}, // -
        {0x00, 0x60, 0x60, 0x00, 0x00}, // .
        {0x20, 0x10, 0x08, 0x04, 0x02}, // /
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
        {0x00, 0x36, 0x36, 0x00, 0x00}, // :
        {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
        {0x08, 0x14, 0x22, 0x41, 0x00}, // <
        {0x14, 0x14, 0x14, 0x14, 0x14}, // =
        {0x00, 0x41, 0x22, 0x14, 0x08}, // >
        {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
        {0x32, 0x49, 0x59, 0x51, 0x3E}, // @
        {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
        {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
        {0x55, 0xAA, 0x55, 0xAA, 0x55}, // Backslash (Checker pattern)
        {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
        {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
        {0x40, 0x40, 0x40, 0x40, 0x40}, // _
        {0x00, 0x03, 0x05, 0x00, 0x00}, // `
        {0x20, 0x54, 0x54, 0x54, 0x78}, // a
        {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
        {0x38, 0x44, 0x44, 0x44, 0x20}, // c
        {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
        {0x38, 0x54, 0x54, 0x54, 0x18}, // e
        {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
        {0x18, 0xA4, 0xA4, 0xA4, 0x7C}, // g
        {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
        {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
        {0x40, 0x80, 0x84, 0x7D, 0x00}, // j
        {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
        {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
        {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
        {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
        {0x38, 0x44, 0x44, 0x44, 0x38}, // o
        {0xFC, 0x24, 0x24, 0x24, 0x18}, // p
        {0x18, 0x24, 0x24, 0x18, 0xFC}, // q
        {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
        {0x48, 0x54, 0x54, 0x54, 0x20}, // s
        {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
        {0x44, 0x28, 0x10, 0x28, 0x44}, // x
        {0x1C, 0xA0, 0xA0, 0xA0, 0x7C}, // y
        {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
        {0x00, 0x10, 0x7C, 0x82, 0x00}, // {
        {0x00, 0x00, 0xFF, 0x00, 0x00}, // |
        {0x00, 0x82, 0x7C, 0x10, 0x00}, // }
        {0x00, 0x06, 0x09, 0x09, 0x06}  // ~ (Degrees)
};

// 将buf里的数据发送给I2C设备
static int I2C_Write(unsigned char *buf, unsigned int len)
{
    int ret = i2c_master_send(etx_i2c_client_oled, buf, len);
    if (ret != len)
    {
        printk("I2C_Write fail\n");
        return -EIO;
    }
    return ret;
}

// SSD1306写函数或者写命令
static void SSD1306_Write(bool is_cmd, unsigned char data)
{
    unsigned char buf[2] = {0};
    int ret;
    // 如果是true,后面这个发送的就是命令
    if (is_cmd == true)
    {
        buf[0] = 0x00;
    }
    // 如果是false,后面这个发送的就是oled屏幕上显示的数字
    else
    {
        buf[0] = 0x40;
    }
    buf[1] = data;

    ret = I2C_Write(buf, 2);
}

// 把OLED的写入位置移动到指定的page和列
static void SSD1306_SetCursor(uint8_t lineNo, uint8_t cursorPos)
{
    if ((lineNo <= SSD1306_MAX_LINE) && (cursorPos < SSD1306_MAX_SEG))
    {
        SSD1306_LineNum = lineNo;
        SSD1306_CursorPos = cursorPos;
        // 设置起始列
        SSD1306_Write(true, 0x21);
        SSD1306_Write(true, cursorPos);
        SSD1306_Write(true, SSD1306_MAX_SEG - 1);
        // 设置页地址范围
        SSD1306_Write(true, 0x22);
        SSD1306_Write(true, lineNo);
        SSD1306_Write(true, SSD1306_MAX_LINE);
    }
}

// 反显函数
static void SSD1306_InvertDisplay(bool need_to_invert)
{
    // 反色显示
    if (need_to_invert)
    {
        SSD1306_Write(true, 0xA7);
    }
    // 正常显示
    else
    {
        SSD1306_Write(true, 0XA6);
    }
}

// SSD1306_OLED屏上电初始化过程
static int SSD1306_DisplayInit(void)
{
    msleep(100);
    SSD1306_Write(true, 0xAE); // 先初始化关闭显示
    SSD1306_Write(true, 0xD5); // Set Display Clock Divide Ratio and Oscillator Frequency
    SSD1306_Write(true, 0x80); // Default Setting for Display Clock Divide Ratio and Oscillator Frequency that is recommended
    SSD1306_Write(true, 0xA8); // Set Multiplex Ratio
    SSD1306_Write(true, 0x3F); // 64 COM lines
    SSD1306_Write(true, 0xD3); // Set display offset
    SSD1306_Write(true, 0x00); // 0 offset
    SSD1306_Write(true, 0x40); // Set first line as the start line of the display
    SSD1306_Write(true, 0x8D); // Charge pump
    SSD1306_Write(true, 0x14); // Enable charge dump during display on
    SSD1306_Write(true, 0x20); // Set memory addressing mode
    SSD1306_Write(true, 0x00); // Horizontal addressing mode
    SSD1306_Write(true, 0xA1); // Set segment remap with column address 127 mapped to segment 0
    SSD1306_Write(true, 0xC8); // Set com output scan direction, scan from com63 to com 0
    SSD1306_Write(true, 0xDA); // Set com pins hardware configuration
    SSD1306_Write(true, 0x12); // Alternative com pin configuration, disable com left/right remap
    SSD1306_Write(true, 0x81); // Set contrast control
    SSD1306_Write(true, 0x80); // Set Contrast to 128
    SSD1306_Write(true, 0xD9); // Set pre-charge period
    SSD1306_Write(true, 0xF1); // Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK
    SSD1306_Write(true, 0xDB); // Set Vcomh deselect level
    SSD1306_Write(true, 0x20); // Vcomh deselect level ~ 0.77 Vcc
    SSD1306_Write(true, 0xA4); // Entire display ON, resume to RAM content display
    SSD1306_Write(true, 0xA6); // Set Display in Normal Mode, 1 = ON, 0 = OFF
    SSD1306_Write(true, 0x2E); // Deactivate scroll
    SSD1306_Write(true, 0xAF); // Display ON in normal mode
    return 0;
}

// 清屏/填充函数
static void SSD1306_Fill(unsigned char data)
{
    unsigned int total = 128 * 8;
    unsigned int i = 0;
    for (i = 0; i < total; i++)
    {
        SSD1306_Write(false, data);
    }
}

static void SSD1306_ShowChar(char ch)
{
    int i;
    if (ch < ' ' || ch > '~')
    {
        ch = ' ';
    }

    for (i = 0; i < SSD1306_DEF_FONT_SIZE; i++)
    {
        SSD1306_Write(false, SSD1306_font[ch - ' '][i]);
    }

    /*字符之间空一列*/
    SSD1306_CursorPos += SSD1306_DEF_FONT_SIZE + 1;

    if (SSD1306_CursorPos >= SSD1306_MAX_SEG - SSD1306_DEF_FONT_SIZE)
    {
        SSD1306_CursorPos = 0;
        SSD1306_LineNum++;

        if (SSD1306_LineNum > SSD1306_MAX_LINE)
        {
            SSD1306_LineNum = 0;
        }
        SSD1306_SetCursor(SSD1306_LineNum, SSD1306_CursorPos);
    }
}

static void SSD1306_ShowString(char *str)
{
    while (*str)
    {
        if (*str == '\n')
        {
            SSD1306_CursorPos = 0;
            SSD1306_LineNum++;

            if (SSD1306_LineNum > SSD1306_MAX_LINE)
            {
                SSD1306_LineNum = 0;
            }
            SSD1306_SetCursor(SSD1306_LineNum, SSD1306_CursorPos);
        }
        else
        {
            SSD1306_ShowChar(*str);
        }
        str++;
    }
}

static ssize_t SSD1306_drv_write(struct file *file, const char __user *buf, size_t count, loff_t *poss)
{
    char kbuf[128];
    size_t len;
    len = count;
    if (len > sizeof(kbuf) - 1)
    {
        len = sizeof(kbuf) - 1;
    }

    if (copy_from_user(kbuf, buf, len))
    {
        return -EFAULT;
    }

    kbuf[len] = '\0';

    SSD1306_ShowString(kbuf);

    return len;
}

static struct file_operations SSD1306_fops = {
    .owner = THIS_MODULE,
    .write = SSD1306_drv_write,
};

static int SSD1306_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct device *result;
    int i = 0;
    etx_i2c_client_oled = client;

    major = register_chrdev(0, "ssd1306_chrdev", &SSD1306_fops);
    SSD1306_class = class_create(THIS_MODULE, "ssd1306_chardev");
    if (IS_ERR(SSD1306_class))
    {
        dev_err(&client->dev, "Failed to create class\n");
        unregister_chrdev(major, "ssd1306_chrdev");
        return PTR_ERR(SSD1306_class);
    }

    result = device_create(SSD1306_class, NULL, MKDEV(major, 0), NULL, "ssd1306");
    if (IS_ERR(result))
    {
        printk("Failed to create device");
        class_destroy(SSD1306_class);
        unregister_chrdev(major, "ssd1306_chrdev");
        return -ENODEV;
    }

    SSD1306_DisplayInit();
    SSD1306_Fill(0x00);
    SSD1306_SetCursor(0, 0);

    // 列地址范围0~127
    SSD1306_Write(true, 0x21);
    SSD1306_Write(true, 0);
    SSD1306_Write(true, 127);

    // 页地址范围2~4
    SSD1306_Write(true, 0x22);
    SSD1306_Write(true, 2);
    SSD1306_Write(true, 4);

    // 连续显示数据
    for (i = 0; i < 138; i++)
    {
        // 这里连续写了138个0xFF,表示每列8个像素全亮,相当于花了一段"实心粗线"
        SSD1306_Write(false, 0xff);
    }

    for (i = 10; i < 128; i++)
    {
        // 这里写入表示二进制最低位的两位为1
        // 表示每个字节只有最底下的两个像素亮,这样可以画出比较细的横线
        SSD1306_Write(false, BIT(1) | BIT(0));
    }

    // 画一整页全亮
    for (i = 0; i < 128; i++)
    {
        SSD1306_Write(false, 0xff);
    }

    for (i = 0; i < 128; i++)
    {
        // 这个字节中,第7位亮,第6位亮,第0位亮
        SSD1306_Write(false, BIT(7) | BIT(6) | BIT(0));
    }

    dev_info(&client->dev, "======ssd1306 initialized successfully======\n");
    return 0;
}

static int SSD1306_remove(struct i2c_client *client)
{
    SSD1306_Fill(0x00);
    SSD1306_SetCursor(0, 0);

    device_destroy(SSD1306_class, MKDEV(major, 0));
    class_destroy(SSD1306_class);
    unregister_chrdev(major, "ssd1306_chrdev");
    return 0;
}

static const struct i2c_device_id SSD1306_id[] = {
    {"tiger,oled", 0}, // I2C设备名匹配
    {}};

// 设备树匹配表
static const struct of_device_id SSD1306_dt_match[] = {
    {
        .compatible = "tiger,oled",
    },
    {} // 空结构体:表示匹配表结束
};

static struct i2c_driver SSD1306_i2c_driver = {
    .probe = SSD1306_probe,
    .remove = SSD1306_remove,
    .id_table = SSD1306_id,
    .driver = {
        .owner = THIS_MODULE,
        .name = "ssd1306",
        .of_match_table = SSD1306_dt_match,
    },
};

static int __init SSD1306_i2c_init(void)
{
    int ret;
    ret = i2c_add_driver(&SSD1306_i2c_driver);
    if (ret != 0)
    {
        pr_err("Failed to register SSD1306 I2C driver:&d\n", ret);
    }
    return 0;
}

static void __exit SSD1306_i2c_exit(void)
{
    i2c_del_driver(&SSD1306_i2c_driver);
}

module_init(SSD1306_i2c_init);
module_exit(SSD1306_i2c_exit);
MODULE_LICENSE("GPL");
