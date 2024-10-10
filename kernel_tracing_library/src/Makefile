ifdef DEBUG
	ccflags-y += -DDEBUG=1
endif

KERNEL_SRC ?= /lib/modules/`uname -r`/build/
M ?= $(PWD)
INSTALL_MOD_PATH ?= /
INSTALL_HDR_PATH ?= $(INSTALL_MOD_PATH)/usr/include/kernel/

all: modules

modules: 
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

header_install:
	install -d $(INSTALL_HDR_PATH)/CommonLowLevelTracingKit/
	install -Dm 0655 ../../tracing_library/include/CommonLowLevelTracingKit/* $(INSTALL_HDR_PATH)/CommonLowLevelTracingKit/

compile_commands.json:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) CC=clang compile_commands.json

.PHONY: old_compile_commands.json
