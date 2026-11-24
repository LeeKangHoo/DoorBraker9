.DEFAULT_GOAL := all
KDIR ?= /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR := $(MODULE_DIR)/build

include $(MODULE_DIR)/Kbuild

.PHONY: all clean

all:
	$(MAKE) -C "$(KDIR)" $(KBUILD_ARGS) modules

clean:
	$(MAKE) -C "$(KDIR)" $(KBUILD_ARGS) clean

ifneq ($(shell grep -qs 'KBUILD_EXTMOD_OUTPUT' "$(KDIR)/Makefile" && printf yes),)
KBUILD_ARGS := M="$(MODULE_DIR)" MO="$(BUILD_DIR)"
else
# Kbuild versions without MO write their outputs directly under M.
KBUILD_ARGS := M="$(BUILD_DIR)"

.PHONY: prepare-build
all clean: prepare-build

prepare-build:
	mkdir -p "$(BUILD_DIR)/src"
	ln -sfnT "$(MODULE_DIR)/Kbuild" "$(BUILD_DIR)/Kbuild"
	ln -sfnT "$(MODULE_DIR)/include" "$(BUILD_DIR)/include"
	for source in $(detector-y:.o=.c); do \
		ln -sfnT "$(MODULE_DIR)/$$source" "$(BUILD_DIR)/$$source" || exit 1; \
	done
endif
