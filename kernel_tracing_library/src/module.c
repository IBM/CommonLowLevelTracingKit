// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <linux/module.h>

#include "CommonLowLevelTracingKit/tracing.h"

// used in file.c
char *tracing_path = "/tmp/";
module_param(tracing_path, charp, 0000);
MODULE_PARM_DESC(tracing_path, "path where to write the traces");

static int __init init_clltk_kf(void)
{
	return 0;
}

static void __exit exit_clltk_kf(void) {}

module_init(init_clltk_kf);
module_exit(exit_clltk_kf);
MODULE_LICENSE("Dual BSD/GPL");
