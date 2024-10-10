// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "tracepoint.h"

#include <stddef.h>

_Static_assert(6 == offsetof(traceentry_head_t, pid), "layout failed");
_Static_assert(10 == offsetof(traceentry_head_t, tid), "layout failed");
_Static_assert(14 == offsetof(traceentry_head_t, timestamp_ns), "layout failed");
_Static_assert(22 == offsetof(traceentry_head_t, body), "layout failed");
