obj-m		+= tx2mon_kmod.o
tx2mon_kmod-objs += tx2mon_base.o tx2errata_switch.o
DIR		:= $(PWD)
KERNELDIR	:= /lib/modules/$(shell uname -r)/build
all:
	make -C $(KERNELDIR) M=$(DIR) modules
clean:
	make -C $(KERNELDIR) M=$(DIR) clean
