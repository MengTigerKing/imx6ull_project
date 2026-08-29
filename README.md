# i.MX6ULL Linux 驱动与 V4L2 学习项目

这是一个面向 i.MX6ULL 开发板的 Linux 驱动学习仓库，包含设备树、字符设备驱动、驱动测试程序，以及几个 V4L2 摄像头示例。

本仓库当前使用的设备树文件名为 `100ask_imx6ull-14x14.dts`，主要适配百问网 i.MX6ULL 14x14 开发板。不同开发板的引脚连接、外设地址和启动方式可能不同，使用前请先根据自己的硬件修改设备树。

## 先弄清楚这些文件

初学者最容易混淆的是源码、内核模块和用户程序。它们的关系如下：

```text
Dts/xxx.dts
    └── 由内核构建系统编译为 xxx.dtb
        └── 放到开发板启动介质，由 U-Boot/内核在启动时使用

Driver/设备名/xxx_drv.c
    └── 使用 ARM 交叉编译工具链和目标内核源码编译
        └── 生成 xxx_drv.ko，复制到 i.MX6ULL 后使用 insmod 加载

Driver/设备名/xxx_test.c
    └── 使用 ARM 交叉编译器编译
        └── 生成无扩展名的 xxx_test 可执行文件，复制到 i.MX6ULL 后运行

Camera/xxx.c
    └── 使用 ARM 交叉编译器编译
        └── 生成同名的无扩展名可执行文件，复制到 i.MX6ULL 后运行
```

简单来说：

- `.dts` 是设备树源码，不能直接给开发板使用，必须先编译成 `.dtb`。
- `xxx_drv.c` 是内核驱动源码，交叉编译后得到 `xxx_drv.ko`。
- `.ko` 是 Linux 内核模块，不是普通应用程序；它应在 i.MX6ULL 上通过 `insmod` 加载，不能用 `./xxx_drv.ko` 运行。
- `xxx_test.c` 是用户空间测试源码，交叉编译后得到 `xxx_test` 可执行文件。
- Linux 可执行文件不要求带 `.exe` 后缀，所以 `xxx_test` 虽然没有扩展名，仍然是程序。
- 本仓库已有的 `.ko` 和无扩展名程序均为 32 位 ARM 产物，不能直接在常见的 x86/x86_64 Ubuntu 电脑上运行。

## 仓库结构

```text
imx6ull_project/
├── Dts/
│   └── 100ask_imx6ull-14x14.dts   # 设备树源码
├── Driver/
│   ├── AP3216C/                    # 红外、环境光、接近传感器
│   ├── DHT11/                      # 温湿度传感器
│   ├── OLED/                       # SSD1306 OLED 显示屏
│   ├── SG90/                       # PWM 舵机
│   ├── SR04/                       # 超声波测距模块
│   └── SR501/                      # 人体红外感应模块
├── Camera/
│   ├── camera_test.c               # 查询摄像头能力、格式、分辨率和帧率
│   ├── camera_config.c             # 设置并读取摄像头采集格式
│   ├── catch_picture.c             # 采集一帧 YUYV 数据并转换为 PNG
│   └── camera_show.c               # 将摄像头画面实时显示到 framebuffer
├── LICENSE
└── README.md
```

每个 `Driver/设备名/` 目录通常包含：

```text
Makefile        # 编译内核模块
xxx_drv.c       # 驱动源码
xxx_drv.ko      # 已交叉编译的 ARM 内核模块
xxx_test.c      # 用户空间测试源码
xxx_test        # 已交叉编译的 ARM 测试程序
```

## 开发环境

重新编译本项目通常需要：

- Linux 开发主机，例如 Ubuntu；
- ARM 交叉编译工具链；
- 与开发板当前运行内核完全对应的 Linux 内核源码及配置；
- i.MX6ULL 开发板、正确接线的外设和可用的串口或网络连接。

本仓库 Makefile 默认使用：

```text
ARCH=arm
CROSS_COMPILE=arm-buildroot-linux-gnueabihf-
KERN_DIR=/home/book/100ask_imx6ull-sdk/Linux-4.9.88
```

`KERN_DIR` 是作者电脑上的路径，在你的电脑上通常不同。请把它改为自己的内核源码绝对路径，或者在执行 `make` 时通过命令行传入。可以先检查工具链：

```bash
arm-buildroot-linux-gnueabihf-gcc --version
```

> 仓库中现有 `.ko` 的目标内核为 Linux 4.9.88。内核模块与目标板的内核版本、配置或符号版本不一致时，可能出现 `Invalid module format`。最稳妥的做法是使用开发板当前内核对应的源码重新编译。

## 编译和使用设备树

### 1. 编译 DTS 为 DTB

`Dts/100ask_imx6ull-14x14.dts` 引用了 `imx6ull.dtsi` 等内核文件，因此推荐通过 Linux 内核构建系统编译，而不是单独对它执行简单的 `dtc` 命令。

先将 DTS 放入或合并到目标内核源码的 `arch/arm/boot/dts/` 目录，并确认内核的设备树 Makefile 包含该 DTB 目标，然后执行：

```bash
make -C /你的内核源码路径 \
    ARCH=arm \
    CROSS_COMPILE=arm-buildroot-linux-gnueabihf- \
    dtbs
```

编译成功后，目标文件通常位于：

```text
/你的内核源码路径/arch/arm/boot/dts/100ask_imx6ull-14x14.dtb
```

注意：

- `.dts` 是文本源码，`.dtb` 才是开发板启动时读取的二进制设备树。
- 修改 `.dts` 后必须重新编译并更新开发板实际加载的 `.dtb`，只把 `.dts` 复制到开发板不会生效。
- 本项目中的驱动依赖设备树提供 GPIO、PWM 和 I2C 设备信息。设备树没有生效时，即使 `insmod` 成功，也可能不会生成预期设备节点。

### 2. 更新开发板上的 DTB

将新生成的 `.dtb` 放到开发板的启动分区，并确保 U-Boot 加载的正是这个文件。不同系统可能从 `/boot`、独立 FAT 分区或其他位置加载 DTB，实际位置请以开发板的 U-Boot 配置为准。

更新前务必备份原 DTB。错误的设备树可能导致开发板无法正常启动，建议保留串口终端和一种可恢复启动介质的方法。更新后重启开发板，新的硬件描述才会生效。

## 编译驱动模块

以 AP3216C 为例，在开发主机执行：

```bash
cd Driver/AP3216C

make KERN_DIR=/你的内核源码路径 \
     ARCH=arm \
     CROSS_COMPILE=arm-buildroot-linux-gnueabihf-
```

编译关系为：

```text
ap3216c_drv.c  →  ap3216c_drv.o  →  ap3216c_drv.ko
```

其他设备的规则相同：Makefile 中 `obj-m` 后面的名字必须与驱动源码的基本文件名一致。例如：

```makefile
obj-m += dht11_drv.o
```

会使用 `dht11_drv.c` 生成 `dht11_drv.ko`。

本仓库各驱动目录中的 `obj-m` 已与当前的 `*_drv.c` 文件名保持一致，可直接使用对应 Makefile 编译。

可以使用下面的命令清理某个驱动目录中的中间文件：

```bash
make KERN_DIR=/你的内核源码路径 clean
```

## 编译测试程序

测试程序是运行在用户空间的普通 ARM Linux 程序，不需要通过内核 Makefile 编译。以 AP3216C 为例：

```bash
cd Driver/AP3216C

arm-buildroot-linux-gnueabihf-gcc \
    -Wall -O2 \
    ap3216c_test.c \
    -o ap3216c_test
```

编译关系为：

```text
ap3216c_test.c  →  ap3216c_test
```

编译其他测试程序时，把 `ap3216c` 换成对应设备名即可。例如：

```bash
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 dht11_test.c -o dht11_test
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 oled_test.c  -o oled_test
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 sg90_test.c  -o sg90_test
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 sr04_test.c  -o sr04_test
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 sr501_test.c -o sr501_test
```

上面的命令应在各自源码所在目录执行。

## 在 i.MX6ULL 上加载和测试驱动

推荐按照以下顺序操作：

1. 让开发板使用由本项目 DTS 编译出的正确 DTB 启动。
2. 将对应的 `.ko` 和测试程序复制到开发板。
3. 在开发板上加载 `.ko`。
4. 检查设备节点和内核日志。
5. 运行测试程序。

以 AP3216C 为例，在 i.MX6ULL 开发板上执行：

```bash
chmod +x ap3216c_test
insmod ap3216c_drv.ko

dmesg | tail
ls -l /dev/ap3216c

./ap3216c_test /dev/ap3216c
```

先按 `Ctrl+C` 停止测试程序，再卸载模块：

```bash
rmmod ap3216c_drv
```

各驱动的默认设备节点和测试命令如下：

| 设备 | 驱动模块 | 设备节点 | 测试命令 |
| --- | --- | --- | --- |
| AP3216C | `ap3216c_drv.ko` | `/dev/ap3216c` | `./ap3216c_test /dev/ap3216c` |
| DHT11 | `dht11_drv.ko` | `/dev/mydht11` | `./dht11_test /dev/mydht11` |
| SSD1306 OLED | `oled_drv.ko` | `/dev/ssd1306` | `./oled_test /dev/ssd1306 "Hello"` |
| SG90 | `sg90_drv.ko` | `/dev/sg90` | `./sg90_test /dev/sg90 90` |
| HC-SR04 | `sr04_drv.ko` | `/dev/sr04` | `./sr04_test /dev/sr04` |
| HC-SR501 | `sr501_drv.ko` | `/dev/my_sr501_tree` | `./sr501_test /dev/my_sr501_tree` |

SG90 测试命令最后一个参数是角度，建议使用 `0` 到 `180`。OLED 示例中的字符串请使用驱动内置字库支持的字符。

执行 `insmod`、访问设备节点或操作硬件通常需要 root 权限。如果加载失败，请立即查看：

```bash
dmesg | tail -n 50
```

## Camera：V4L2 用户空间示例

`Camera/` 中的是 V4L2 用户空间程序，不是 `.ko` 内核驱动。它们要求开发板已经有可用的摄像头驱动，并已生成 `/dev/video0`、`/dev/video1` 等视频设备节点。

### 编译 Camera 程序

在开发主机的仓库根目录执行：

```bash
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 Camera/camera_test.c   -o Camera/camera_test
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 Camera/camera_config.c -o Camera/camera_config
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 Camera/catch_picture.c -o Camera/catch_picture
arm-buildroot-linux-gnueabihf-gcc -Wall -O2 Camera/camera_show.c    -o Camera/camera_show
```

### 在开发板上运行

先查看视频设备节点：

```bash
ls -l /dev/video*
```

然后根据实际节点运行：

```bash
# 查询设备能力、支持的像素格式、分辨率和当前帧率
./camera_test /dev/video0

# 尝试设置为 640×480、YUYV，并读取驱动最终采用的格式
./camera_config /dev/video0

# 抓取一帧并转换为 PNG
mkdir -p /root/camera_data
./catch_picture /dev/video0

# 将 640×480 YUYV 画面实时显示到 /dev/fb0，按 Ctrl+C 退出
./camera_show /dev/video0
```

`catch_picture` 会把文件写到：

```text
/root/camera_data/1.yuyv
/root/camera_data/1.png
```

该程序会在开发板上调用 `ffmpeg` 将 YUYV 裸数据转换成 PNG，因此开发板文件系统中需要提前安装或移植 `ffmpeg`。`camera_show` 还要求 `/dev/fb0` 可用，并且摄像头最终能够输出 640×480 的 YUYV 数据。

## 常见问题

### 1. `insmod: Invalid module format`

`.ko` 与开发板当前内核不匹配。请使用目标板当前运行内核对应的源码、配置和交叉工具链重新编译模块，并通过以下命令对比版本：

```bash
uname -r
modinfo xxx_drv.ko | grep vermagic
```

### 2. `insmod` 后没有出现 `/dev/xxx`

依次检查：

- 开发板是否真的使用了新编译的 DTB；
- DTS 中的节点、`compatible`、GPIO、PWM 和 I2C 地址是否正确；
- 外设接线和供电是否正确；
- `dmesg` 中是否有驱动 `probe` 失败信息。

### 3. 执行测试程序时出现 `Permission denied`

给程序增加可执行权限：

```bash
chmod +x xxx_test
```

### 4. 执行程序时出现 `not found`，但文件明明存在

常见原因是程序架构或动态链接器不匹配。可以检查：

```bash
file xxx_test
```

本项目程序应显示为 ARM 32 位可执行文件，并且开发板根文件系统需要提供与工具链匹配的 armhf 运行库和动态链接器。

### 5. 找不到 `/dev/video0`

这通常表示摄像头驱动尚未加载、设备树摄像头节点未生效、摄像头未正确连接，或者视频节点编号不是 `video0`。请结合 `dmesg` 和 `/dev/video*` 排查。

## 学习建议

第一次使用时，可以按以下顺序理解代码：

1. 从 DTS 中找到外设节点及其 `compatible`、GPIO、PWM 或 I2C 配置。
2. 在 `xxx_drv.c` 中找到相同的 `compatible`，观察驱动如何与设备树匹配。
3. 查看驱动创建的 `/dev/xxx` 设备节点，以及实现的 `open`、`read`、`write`、`ioctl` 等接口。
4. 查看 `xxx_test.c` 如何通过设备节点调用这些接口。
5. 最后修改代码、重新交叉编译，并在开发板上验证。

后续计划继续补充 Qt、MQTT 和上位机相关示例，欢迎一起学习、交流和改进。

## License

本项目使用 [MIT License](LICENSE)。
