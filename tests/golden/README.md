# Golden trace file fixtures

Committed `.clltk_trace` files used to test the decoders against known
inputs: different library versions, both byte orders, all argument types.

## What the fixtures are for

`tests/test_golden.py` decodes every fixture with **both** decoders (the Python
`clltk_decoder.py` and the C++ `clltk decode` CLI) and asserts the formatted
tracepoint messages. Agreement between the two decoders on every committed
fixture is the correctness oracle: old files must stay decodable, so when the
trace-file format changes, **add** a new fixture for the new version instead of
replacing old ones.

Which format each version established:

Fixtures live one folder per library version, `<version>/<arch>[-flavor].<ext>`.
Every trace fixture carries both byte orders (LE aarch64 + BE s390x); the old
big-endian traces were backfilled from a git worktree of each version's commit
via the era-adaptive generator (see below). The remaining per-folder asymmetry
is deliberate: 1.6.0 carries only the `fmt-` flavor it introduced, and the ELF
metadata objects (`.o`/`.so`) exist only where they anchor a `clltk meta`
format era, not per version.

| path | produced by |
| --- | --- |
| `1.2.39/{le-aarch64,be-s390x}.clltk_trace` | library 1.2.39 (commit 312369e), both byte orders |
| `1.2.49/{le-aarch64,be-s390x}.clltk_trace` | library 1.2.49 (commit c15038b), both byte orders |
| `1.2.64/{le-aarch64,be-s390x}.clltk_trace` | library 1.2.64 (commit 1fa19d2), both byte orders |
| `1.3.0/le-aarch64.clltk_trace`, `1.3.0/be-s390x.clltk_trace` | library 1.3.0, both byte orders |
| `1.3.0/be-s390x.o` / `.so` | ELF metadata objects for `clltk meta`, library 1.3.0 headers on s390x -- historical backward-compat anchor |
| `1.5.0/le-aarch64.clltk_trace`, `1.5.0/be-s390x.clltk_trace` | library 1.5.0 (first with span events) |
| `1.6.0/fmt-le-aarch64.clltk_trace`, `1.6.0/fmt-be-s390x.clltk_trace` | library 1.6.0 (first with fmt tracepoints) |
| `1.7.7/le-aarch64.clltk_trace`, `1.7.7/be-s390x.clltk_trace` | deterministic baseline, both byte orders (see below) |
| `1.7.7/{le-aarch64,be-s390x}.{o,so}` | `writer.cpp` ELF metadata for `clltk meta`: le = native-endian, be = cross-endian (`.o` relocation records, `.so` virtual addresses) |

Fixtures at 1.6.0 and earlier were produced by a historical era-adaptive
generator with real pids/tids/timestamps frozen into the committed file. From
1.7.7 on, generation is byte-deterministic by construction (see below) and is
the baseline the format gate compares against.

## Deterministic generation

`generator/frozen_info.c` defines the three abstraction symbols the library uses
to stamp entries — `info_get_timestamp_ns`, `info_get_process_id`,
`info_get_thread_id` — with fixed constants. The `golden_writer` cmake target
(`generator/writer.cpp`) links it ahead of the tracing static archive, so the
archive's `info.o` is never pulled and every timestamp, pid, tid, and (through
the span-id salt derived from timestamp ^ pid) span id is frozen. A freshly
generated fixture is therefore **byte-identical run to run**. The core library
is unchanged; only the golden writer links the shim.

One C++ writer exercises every tracepoint kind — printf, dump, dynamic, spans,
and fmt — into a single `GOLDEN` buffer. It is C++ because fmt tracepoints are
C++20-only; the other kinds compile identically from C, and the on-disk format
is language-independent, so one writer covers the whole decoder surface.

## The format gate

`scripts/ci-cd/step_golden.sh` regenerates the fixtures at HEAD for both byte
orders and byte-compares them against the newest committed golden, masking the
library-version field and the header crc that depends on it (`generator/normalize.py`,
see the mask offsets there). Identical → the existing corpus already covers this format. Different →
the trace format changed and a new fixture must be committed. It runs in CI (the
`golden` job) and locally via `run_all.sh` (skip with `--skip-golden`).

## Regenerating / adding fixtures

When the gate reports a format change, **bump `VERSION.md` first** (the fixture is
named by version and old fixtures must stay decodable), then regenerate:

```bash
./scripts/development_helper/regenerate_golden.sh --arch aarch64 --arch s390x
```

This writes one fixture per arch, `<version>/{le-aarch64,be-s390x}.clltk_trace`,
into `tests/golden/`. It refuses to overwrite an existing same-version fixture
(so a format change can't silently drop the old baseline). Add the new names to
the fixture lists in `tests/test_golden.py`, then commit. Old fixtures stay.

Under the hood each arch runs the `golden` cmake preset in a Fedora container:

```bash
./tests/golden/generator/generate.sh --src "$(git rev-parse --show-toplevel)" \
    --out ./out --arch aarch64        # or s390x
```

Add `--elf` to also emit `elfm.o` and `elfm.so` (`writer.cpp` compiled as a
relocatable object and a shared object). Both byte orders are committed
(`<version>/{le-aarch64,be-s390x}.{o,so}`) for the `clltk meta`
ELF-metadata test -- LE for the native-endian path, BE for the cross-endian
path. To refresh them, run generation with `--elf` and copy `elfm.{o,so}` over.
The format gate never builds these, so an ELF build issue can't trip it.

### Detecting ELF-metadata changes

The ELF objects are **not** byte-gated like the trace fixtures. Their bytes are
compiler-dependent (a toolchain bump changes codegen with no metadata change),
so a byte compare would false-fire. The metadata that matters is what
`clltk meta` extracts, which is source-defined and compiler-independent.

Change detection therefore works through two paths, not a byte gate:

1. The tracepoint metadata comes from `writer.cpp` -- the same source as the
   trace fixture. Any change to it that matters is caught by the **trace format
   gate**; when you bump the version and regenerate, refresh the ELF objects
   (`--elf`) in the same step so they stay in sync.
2. `golden_elf_meta` runs `clltk meta` on the committed (frozen) objects every
   test run, so a change to the metadata **section format** or the meta parser
   that broke reading the old objects fails the test.

A dedicated forward gate for a library ELF-metadata-format change that leaves
the trace wire format untouched (regenerate the objects and diff `clltk meta`
output against the committed golden) is possible but not wired up -- that class
of change is rare and needs `clltk` built in the gate job. Add it if it starts
to bite.

Big-endian (`--arch s390x`) needs qemu-user-static binfmt. On a Linux CI runner
`docker/setup-qemu-action` provides it. On a macOS podman machine, rootless
podman does not pick up the handler — register it in the VM and run rootful:

```bash
podman machine ssh 'sudo podman run --rm --platform linux/s390x \
    -v <repo>:/src:ro -v <outdir>:/out:z \
    registry.fedoraproject.org/fedora:43 bash /src/tests/golden/generator/gen.sh'
```

Set `CLLTK_CONTAINER_CMD=docker` to use docker instead of podman.

### Backfilling an old version (era-adaptive)

The generator above only builds HEAD. Old-version fixtures (the pre-1.7.7
folders) were produced from a git worktree of each version's commit with an
era-adaptive generator that compiles a minimal writer against that release's
headers. This is a one-time historical process, not part of the normal flow;
the recipe is: `git worktree add <wt> <commit>`, then run the era-adaptive
`gen.sh` (mounting the worktree as `/src` and a directory holding the minimal
`writer.c` as `/fix`) under the target arch, and copy `GOLDEN.clltk_trace` into
`<version>/<arch>.clltk_trace`. Old fixtures use real (unfrozen) identity, like
the rest of the historical corpus; only HEAD fixtures are byte-deterministic.
