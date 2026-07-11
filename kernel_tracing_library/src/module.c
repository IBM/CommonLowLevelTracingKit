// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <linux/module.h>
#include <linux/notifier.h>

#include "CommonLowLevelTracingKit/tracing/tracing.h"

// used in file.c
char *tracing_path = "/tmp/";
module_param(tracing_path, charp, 0000);
MODULE_PARM_DESC(tracing_path, "path where to write the traces");

// Registration of a traced module's tracebuffers/tracepoints runs from a C
// constructor injected into that module (see _kernel_tracing.h). The kernel
// only calls module constructors when CONFIG_CONSTRUCTORS is enabled, which is
// off in most production kernels - so on those the constructor never fires and
// tracing silently does nothing.
//
// A module notifier does the same work on any kernel and needs no cooperation
// from the traced module: the kernel calls it as each module is (un)loaded.
// Both paths are idempotent, so on a CONFIG_CONSTRUCTORS kernel the constructor
// and the notifier simply agree.
void _clltk_init_tracing_for_this_module(const struct mod_kallsyms *);
void _clltk_deinit_tracing_for_this_module(const struct mod_kallsyms *);

static int clltk_module_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	struct module *const mod = data;
	(void)nb;
	switch (action) {
	case MODULE_STATE_COMING: // linked, before the module runs its own init
		_clltk_init_tracing_for_this_module(mod->kallsyms);
		break;
	case MODULE_STATE_GOING: // being removed
		_clltk_deinit_tracing_for_this_module(mod->kallsyms);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block clltk_module_nb = {
	.notifier_call = clltk_module_notify,
};

static int __init init_clltk_kf(void)
{
	return register_module_notifier(&clltk_module_nb);
}

static void __exit exit_clltk_kf(void)
{
	unregister_module_notifier(&clltk_module_nb);
}

module_init(init_clltk_kf);
module_exit(exit_clltk_kf);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Common Low Level Tracing Kit (CLLTK) kernel tracing module");
