
#CFILES = teemonitor.c tee_client_api.c teec_trace.c
ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
#CARGS = -I/home/atmark/linux-5.10-5.10.161-r0/drivers/tee/optee

obj-m := driver.o
driver-objs := teemonitordev.o tee_client_api.o seccomp.o
ccflags-y := -Idrivers/tee/optee

all:
	make -Wall -C /home/atmark/linux-5.10-5.10.161-r0 M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	
