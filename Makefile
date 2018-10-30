CURRENT=$(shell uname -r )
KDIR = /lib/modules/$(CURRENT)/build
KMODDIR = /lib/modules/$(CURRENT)/kernel
PWD=$(shell pwd )

TARGET1 = 1ppsusbss-transmitter
TARGET2 = 1ppsusbss-receiver

obj-m += $(TARGET1).o $(TARGET2).o

ccflags-y = -Wno-declaration-after-statement -mpopcnt -fno-pie

default: all

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	@rm -f *.o .*.cmd .*.flags *.mod.c *.order
	@rm -f .*.*.cmd *~ *.*~ TODO.* 
	@rm -fR .tmp*
	@rm -rf .tmp_versions 
	@rm -f *.ko *.symvers

disclean: clean 
	@rm *.ko *.symvers
