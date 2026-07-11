#!/usr/bin/python3
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
Tests for the persisted lookup index inside the unique stack (since 1.7.0):
slabs are appended as tagged stack entries, their bodies are nibble-encoded
so pre-1.7.0 decoders can never misparse them, and a torn slab degrades to
rebuild-by-scan.
"""

import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.base import run_command, get_build_dir

REPO_ROOT = pathlib.Path(__file__).parent.parent
INDEX_TAG = b"CLLTKIDX"
TRACEPOINT_COUNT = 24  # above the publish threshold of 16


def setUpModule():
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk_tracing_shared")


def writer_source() -> str:
    lines = [
        '#include "CommonLowLevelTracingKit/tracing/tracing.h"',
        "CLLTK_TRACEBUFFER(IDX, 8192)",
        "int main(void)",
        "{",
    ]
    for i in range(TRACEPOINT_COUNT):
        lines.append(f'\tCLLTK_TRACEPOINT(IDX, "index test tracepoint {i} value %d", {i});')
    lines.append("\treturn 0;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def stack_entries(trace_file: pathlib.Path):
    """Walk the unique stack: yield (kind_tag, body_offset, body_bytes)."""
    raw = trace_file.read_bytes()
    stack_offset = struct.unpack_from("<Q", raw, 40)[0]
    body_size = struct.unpack_from("<Q", raw, stack_offset + 112)[0]
    body_start = stack_offset + 120
    offset = 0
    while offset + 29 <= body_size:
        head = body_start + offset
        kind_tag = raw[head + 16 : head + 24]
        entry_size = struct.unpack_from("<I", raw, head + 24)[0]
        body = raw[head + 29 : head + 29 + entry_size]
        yield kind_tag, head + 29, body
        offset += 29 + entry_size


class index_persistence(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.trace_dir = pathlib.Path(self.tmp.name)
        lib_dir = get_build_dir() / "tracing_library"
        src = self.trace_dir / "writer.c"
        src.write_text(writer_source())
        self.binary = self.trace_dir / "writer"
        run_command(
            f"gcc -std=c11 -O1 -I {REPO_ROOT}/tracing_library/include {src} "
            f"-L {lib_dir} -lclltk_tracing -Wl,-rpath,{lib_dir} -o {self.binary}"
        )

    def tearDown(self):
        self.tmp.cleanup()

    def run_writer(self):
        env = {k: v for k, v in os.environ.items() if "CLLTK" not in k}
        env["CLLTK_TRACING_PATH"] = str(self.trace_dir)
        subprocess.run([str(self.binary)], check=True, env=env)

    def decode_messages(self) -> list:
        decoder = REPO_ROOT / "decoder_tool" / "python" / "clltk_decoder.py"
        out = self.trace_dir / "out.csv"
        subprocess.run(
            [sys.executable, str(decoder), "-o", str(out), str(self.trace_dir / "IDX.clltk_trace")],
            check=True,
            capture_output=True,
        )
        import csv

        with open(out, newline="") as fh:
            rows = list(csv.DictReader(fh))
        return [
            r["formatted"].strip()
            for r in rows
            if not r["formatted"].strip().startswith('{"tracebuffer info')
        ]

    def slabs(self):
        trace = self.trace_dir / "IDX.clltk_trace"
        return [(off, body) for tag, off, body in stack_entries(trace) if tag == INDEX_TAG]

    def meta_entry_count(self):
        trace = self.trace_dir / "IDX.clltk_trace"
        return len([1 for tag, _, _ in stack_entries(trace) if tag != INDEX_TAG])

    def test_slab_written_and_safe_for_old_decoders(self):
        self.run_writer()
        slabs = self.slabs()
        self.assertGreaterEqual(len(slabs), 1, "expected a persisted index slab")
        for _, body in slabs:
            # the invariant that makes pre-1.7.0 decoders safe: no slab byte
            # can equal the meta magic '{' (0x7B); all bytes have the high
            # bit set by the nibble encoding
            self.assertTrue(all(b >= 0x80 for b in body))
        self.assertEqual(TRACEPOINT_COUNT, self.meta_entry_count())

    def test_second_run_deduplicates_through_slab(self):
        self.run_writer()
        metas_first = self.meta_entry_count()
        self.run_writer()
        # nothing new registered: the second process resolved every entry
        self.assertEqual(metas_first, self.meta_entry_count())
        messages = self.decode_messages()
        self.assertEqual(2 * TRACEPOINT_COUNT, len(messages))

    def test_torn_slab_rebuilds_by_scan(self):
        """Simulates the real crash model: the writer appends slab body and
        head first and commits the stack body_size last, so a crash mid-write
        leaves the slab beyond the committed size - invisible. Rewinding
        body_size to before the slab reproduces exactly that state."""
        self.run_writer()
        slabs = self.slabs()
        self.assertGreaterEqual(len(slabs), 1)

        trace = self.trace_dir / "IDX.clltk_trace"
        raw = bytearray(trace.read_bytes())
        stack_offset = struct.unpack_from("<Q", raw, 40)[0]
        body_start = stack_offset + 120
        slab_body_offset, _ = slabs[-1]
        slab_head_offset = slab_body_offset - 29
        rewound_body_size = slab_head_offset - body_start
        struct.pack_into("<Q", raw, stack_offset + 112, rewound_body_size)
        trace.write_bytes(raw)

        self.assertEqual([], self.slabs(), "torn slab must be invisible")
        metas_before = self.meta_entry_count()
        self.run_writer()  # must rebuild by scan and still deduplicate
        self.assertEqual(metas_before, self.meta_entry_count())
        messages = self.decode_messages()
        self.assertEqual(2 * TRACEPOINT_COUNT, len(messages))


if __name__ == "__main__":
    unittest.main()
