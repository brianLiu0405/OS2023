obj-m += kfetch_mod_311552067.o

PWD := $(CURDIR)
KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

load:
	sudo insmod kfetch_mod_311552067.ko

unload:
	sudo rmmod kfetch_mod_311552067

clean:
	make -C $(KDIR) M=$(PWD) clean
