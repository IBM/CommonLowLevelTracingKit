// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ringbuffer.h"
#include <stddef.h>
#include <stdint.h>

_Static_assert(0 == offsetof(ringbuffer_head_t, version), "layout failed");
_Static_assert(8 == offsetof(ringbuffer_head_t, mutex), "layout failed");
_Static_assert(72 == offsetof(ringbuffer_head_t, body_size), "layout failed");
_Static_assert(80 == offsetof(ringbuffer_head_t, wrapped), "layout failed");
_Static_assert(88 == offsetof(ringbuffer_head_t, dropped), "layout failed");
_Static_assert(96 == offsetof(ringbuffer_head_t, entries), "layout failed");
_Static_assert(104 == offsetof(ringbuffer_head_t, next_free), "layout failed");
_Static_assert(112 == offsetof(ringbuffer_head_t, last_valid), "layout failed");
_Static_assert(120 == offsetof(ringbuffer_head_t, _reserved_for_future_use), "layout failed");
_Static_assert(160 == offsetof(ringbuffer_head_t, body), "layout failed");
_Static_assert(160 == sizeof(ringbuffer_head_t), "layout failed");

_Static_assert(1 == sizeof(ringbuffer_entry_magic), "layout failed");
_Static_assert(2 == sizeof(ringbuffer_entry_body_size_t), "layout failed");

_Static_assert(0 == offsetof(ringbuffer_entry_head_t, magic), "layout failed");
_Static_assert(1 == offsetof(ringbuffer_entry_head_t, body_size), "layout failed");
_Static_assert(3 == offsetof(ringbuffer_entry_head_t, crc), "layout failed");
_Static_assert(4 == offsetof(ringbuffer_entry_head_t, body), "layout failed");
_Static_assert(4 == sizeof(ringbuffer_entry_head_t), "layout failed");
