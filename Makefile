KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m := rootkit.o
rootkit-objs := src/rootkit.o src/hooking.o src/ksym.o src/backdoor.o src/hide.o src/netcom.o

EXTRA_CFLAGS := -I$(PWD)/include

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f Module.symvers modules.order
