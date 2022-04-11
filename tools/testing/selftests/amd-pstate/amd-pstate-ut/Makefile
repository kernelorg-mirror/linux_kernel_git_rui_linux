AMD_PSTATE_UT_DIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KDIR ?= $(abspath $(AMD_PSATE_UT_DIR)/../../../../..)

ifeq ($(V),1)
Q =
else
Q = @
endif

MODULES = amd-pstate-ut.ko

obj-m += amd-pstate-ut.o
CFLAGS_amd-pstate-ut.o = -I$(src)

all:
	+$(Q)make -C $(KDIR) M=$(AMD_PSTATE_UT_DIR) modules

clean:
	+$(Q)make -C $(KDIR) M=$(AMD_PSTATE_UT_DIR) clean

