KSRC := /usr/lib/modules/`uname -r`/build/
PWD := $(shell pwd)

obj-m += audiosync.o

all:
	make -C $(KSRC) M=$(PWD)
clean:
	rm -rf *.o *.ko *.mod* *.order *.symvers
