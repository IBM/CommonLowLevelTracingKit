ifdef DEBUG
	ccflags-y += -DDEBUG=1
endif

KERNEL_SRC ?= /lib/modules/`uname -r`/build/
M ?= $(PWD)
INSTALL_MOD_PATH ?= /
INSTALL_HDR_PATH ?= $(INSTALL_MOD_PATH)/usr/include/kernel/

# Paths relative to kernel_tracing_library/src/
ROOT_DIR := $(M)/../..
VERSION_SCRIPT := $(ROOT_DIR)/cmake/gen_version_header.sh
VERSION_TEMPLATE := $(ROOT_DIR)/cmake/version.h.template
VERSION_OUTPUT := $(ROOT_DIR)/tracing_library/include/CommonLowLevelTracingKit/version.gen.h

all: modules

$(VERSION_OUTPUT): $(VERSION_TEMPLATE) $(ROOT_DIR)/VERSION.md
	$(VERSION_SCRIPT) -t $(VERSION_TEMPLATE) -o $(VERSION_OUTPUT)

modules: $(VERSION_OUTPUT)
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

header_install:
	install -d $(INSTALL_HDR_PATH)/CommonLowLevelTracingKit/tracing/
	install -Dm 0655 ../../tracing_library/include/CommonLowLevelTracingKit/tracing/* $(INSTALL_HDR_PATH)/CommonLowLevelTracingKit/tracing/

compile_commands.json:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) CC=clang compile_commands.json

.PHONY: old_compile_commands.json
