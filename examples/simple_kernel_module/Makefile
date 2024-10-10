ifdef DEBUG
	ccflags-y += -DDEBUG=1
endif

KERNEL_SRC ?= /lib/modules/`uname -r`/build/
M ?= $(PWD)
INSTALL_MOD_PATH ?= /

all: modules

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(M)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

old_compile_commands.json:
	$(M)/../../scripts/development_helper/gen_compile_commands.py -d $(KERNEL_SRC) $(M)

compile_commands.json:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) CC=clang compile_commands.json

.PHONY: old_compile_commands.json
