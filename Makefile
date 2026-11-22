obj-m += detector.o
detector-y := src/detector.o src/bpf.o src/yellow_card.o src/rs.o src/fork_detector.o
ccflags-y += -I$(src)/include

KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(CURDIR)/build

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) MO=$(BUILD_DIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) MO=$(BUILD_DIR) clean
