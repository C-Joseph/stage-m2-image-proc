obj-m := hello.o

ccflags-y=-I$(PWD)
CFLAGS_hello.o := -DDEBUG

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symsvers modules.order
	rm -rf .tmp_versions Modules.symvers
