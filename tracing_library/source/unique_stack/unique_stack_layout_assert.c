// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "unique_stack.h"
#include <stddef.h>

_Static_assert(0 == offsetof(unique_stack_header_t, version), "layout failed");
_Static_assert(8 == offsetof(unique_stack_header_t, mutex), "layout failed");
_Static_assert(72 == offsetof(unique_stack_header_t, _reserved_for_future_use), "layout failed");
_Static_assert(112 == offsetof(unique_stack_header_t, body_size), "layout failed");
_Static_assert(120 == sizeof(unique_stack_header_t), "layout failed");

_Static_assert(0 == offsetof(entry_head_t, md5_hash), "layout failed");
_Static_assert(16 == offsetof(entry_head_t, _reserved_for_future_use), "layout failed");
_Static_assert(24 == offsetof(entry_head_t, body_size), "layout failed");
_Static_assert(28 == offsetof(entry_head_t, crc), "layout failed");
_Static_assert(29 == sizeof(entry_head_t), "layout failed");