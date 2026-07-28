#!/usr/bin/env python3
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Normalize a .clltk_trace file for golden byte comparison.

Two things in a trace file are not part of the format and vary without any
format change; the golden gate must ignore them so a plain byte compare answers
"did the format change?":

1. The library version in the file header (FileHead.version = CLLTK_VERSION),
   which changes on every release.
2. The two serialized pthread mutexes (one in the ringbuffer section, one in the
   stack section). These carry ephemeral synchronization state (glibc internal
   fields, futex/owner bits) that is not deterministic across runs or
   environments -- notably it varies under qemu emulation -- even though pid,
   tid, timestamp and span ids are frozen by frozen_info.c. It is never golden
   content, so it is masked.

Everything else (section format versions, definition version, payloads) is real
signal and is left untouched.

FileHead layout (see scripts/development_helper/clltk_trace.hexpat and
tracing_library/source/tracebuffer.h):

    offset  size  field
    0       16    file_magic
    16      8     version   (uint64_t = CLLTK_VERSION, native byte order) <- masked
    24      8     definition_offset
    32      8     ringbuffer_offset
    40      8     stack_offset
    48      7     padding
    55      1     crc8 over the header (depends on version) <- masked

Each of the ringbuffer and stack sections starts with a u64 version followed by
a 64-byte mutex, so the mutex sits at <section_offset> + 8.

The version is a native-order uint64_t, so its significant bytes land at offsets
16..18 on little-endian and 21..23 on big-endian. Masking the whole 8-byte field
is endianness-agnostic. The section offsets are also native-order uint64s; the
byte order is detected from where the version's significant bytes sit. If the
header layout changes, that IS a format change: the gate fires and these offsets
are updated alongside.
"""

import sys

FILE_VERSION_OFFSET = 16
FILE_VERSION_LEN = 8
FILE_HEAD_CRC_OFFSET = 55
RINGBUFFER_OFFSET_POS = 32
STACK_OFFSET_POS = 40
SECTION_MUTEX_REL = 8
MUTEX_LEN = 64


def _byteorder(data: bytes) -> str:
    # version's significant bytes are at 16..18 (little) or 21..23 (big)
    return "little" if data[16:19] != b"\x00\x00\x00" else "big"


def _zero(buf: bytearray, start: int, length: int) -> None:
    for i in range(start, min(start + length, len(buf))):
        buf[i] = 0


def normalize(data: bytes) -> bytes:
    buf = bytearray(data)
    if len(buf) <= FILE_HEAD_CRC_OFFSET:
        return bytes(buf)
    order = _byteorder(buf)
    # library version + header crc
    _zero(buf, FILE_VERSION_OFFSET, FILE_VERSION_LEN)
    buf[FILE_HEAD_CRC_OFFSET] = 0
    # the two section mutexes (ephemeral sync state)
    for off_pos in (RINGBUFFER_OFFSET_POS, STACK_OFFSET_POS):
        if len(buf) >= off_pos + 8:
            section = int.from_bytes(buf[off_pos : off_pos + 8], order)
            mutex = section + SECTION_MUTEX_REL
            if 0 < mutex <= len(buf):
                _zero(buf, mutex, MUTEX_LEN)
    return bytes(buf)


def main(argv: list) -> int:
    if len(argv) != 3:
        sys.stderr.write("usage: normalize.py <in.clltk_trace> <out.clltk_trace>\n")
        return 2
    with open(argv[1], "rb") as fh:
        data = fh.read()
    with open(argv[2], "wb") as fh:
        fh.write(normalize(data))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
