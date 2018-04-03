CURRENT=$(shell uname -r )
KDIR = /lib/modules/$(CURRENT)/build
KMODDIR = /lib/modules/$(CURRENT)/kernel
PWD=$(shell pwd )

TARGET1 = pps-cfx3
TARGET2 = pps-cfx3-recv
obj-m += $(TARGET1).o $(TARGET2).o

ccflags-y = -Wno-declaration-after-statement -mpopcnt -I/usr/lib/gcc/x86_64-pc-linux-gnu/7.3.1/include

default: all

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	@rm -f *.o .*.cmd .*.flags *.mod.c *.order
	@rm -f .*.*.cmd *~ *.*~ TODO.* 
	@rm -fR .tmp*
	@rm -rf .tmp_versions 

disclean: clean 
	@rm *.ko *.symvers
