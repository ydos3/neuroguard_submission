/* SPDX-License-Identifier: GPL-2.0 */
obj-m += neuroguard.o

neuroguard-objs := src/main.o     \
                   src/chardev.o  \
                   src/events.o   \
                   src/anomaly.o  \
                   src/poller.o   \
                   src/sysfs.o    \
                   src/procfs.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

ccflags-y := -I$(PWD)/include -Wall -Wno-unused-function

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f tools/neuroguard-ctl tests/test_anomaly

tools: tools/neuroguard-ctl.c
	gcc -Wall -O2 -Iinclude -o tools/neuroguard-ctl tools/neuroguard-ctl.c

tests: tests/test_anomaly.c
	gcc -Wall -O2 -o tests/test_anomaly tests/test_anomaly.c

.PHONY: all clean tools tests
