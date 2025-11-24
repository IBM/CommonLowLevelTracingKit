// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"

CLLTK_TRACEBUFFER(template_class, 4096);

namespace template_class
{

template <typename T> class template_class
{
  public:
	void run(void) { CLLTK_TRACEPOINT(template_class, "template_class %s", "template_class"); }
};
} // namespace template_class
