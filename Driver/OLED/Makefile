# 内核源码路径（固定）
KERN_DIR = /home/book/100ask_imx6ull-sdk/Linux-4.9.88

# 编译环境（固定）
ARCH ?= arm
CROSS_COMPILE ?= arm-buildroot-linux-gnueabihf-

# 编译驱动模块
all:
	make -C $(KERN_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(shell pwd) modules 

# 清理驱动产物（唯一的clean规则，无重复）
clean:
	make -C $(KERN_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(shell pwd) modules clean
	rm -rf *.o *.mod.o *.mod.c *.symvers *.order .*.cmd .tmp_versions/

# 驱动模块名（必须与驱动源码文件名一致）
obj-m += oled_drv.o
