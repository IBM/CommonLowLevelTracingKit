# Golden trace file fixtures

Committed `.clltk_trace` files used to test the decoders against known
inputs: different library versions, both byte orders, all argument types.

Old-version fixtures are generated from a git worktree of the respective
commit; `generator/gen.sh` adapts the writer to the header layout of the
checked-out tree. Every fixture was produced by `generator/writer.c` (one tracepoint per
argument type with fixed values, plus one never-fired tracepoint) via
`generator/gen.sh`. The values inside a fixture (pids, tids, timestamps)
are frozen in the committed file, so decoder output is fully deterministic.

The versions were chosen at real format boundaries: 1.2.39 is the oldest
buildable state and predates the dump-format change (1.2.40); 1.2.49
predates the definition-V2 format (1.2.50); 1.2.64 is the last version
before the metaptr discovery rework; 1.3.0 is current. The minor <= 1
layout (32-byte mutex) predates the public history and cannot be
regenerated from source.

| fixture | produced by |
| --- | --- |
| `golden-1.2.39-le-aarch64.clltk_trace` | library 1.2.39 (commit 312369e), aarch64 |
| `golden-1.2.49-le-aarch64.clltk_trace` | library 1.2.49 (commit c15038b), aarch64 |
| `golden-1.2.64-le-aarch64.clltk_trace` | library 1.2.64 (commit 1fa19d2), aarch64 |
| `golden-1.3.0-le-aarch64.clltk_trace` | library 1.3.0, aarch64 |
| `golden-1.3.0-be-s390x.clltk_trace` | library 1.3.0, s390x (big endian) |
| `golden-1.3.0-be-s390x.o` / `.so` | ELF objects compiled with library 1.3.0 headers on s390x |
| `golden-1.5.0-le-aarch64.clltk_trace` | library 1.5.0 (first with span events), aarch64 |
| `golden-1.5.0-be-s390x.clltk_trace` | library 1.5.0 (first with span events), s390x (big endian) |
| `golden-1.6.0-fmt-le-aarch64.clltk_trace` | library 1.6.0 (first with fmt tracepoints), aarch64 |
| `golden-1.6.0-fmt-be-s390x.clltk_trace` | library 1.6.0 (first with fmt tracepoints), s390x (big endian) |

`tests/test_golden.py` decodes every fixture and asserts the formatted
tracepoint messages. When the trace file format changes, add a new fixture
for the new version instead of replacing old ones — old files must stay
decodable.

## Regenerating / adding fixtures

```bash
mkdir out
podman run --rm -v "$(git rev-parse --show-toplevel):/src:ro" -v "$PWD/out:/out" \
    --arch <arch> registry.fedoraproject.org/fedora:43 bash /src/tests/golden/generator/gen.sh
```

For big-endian fixtures use `--arch s390x`; this needs qemu-user-static
binfmt support (on a podman machine VM, register the handler in the VM and
run rootful: `podman machine ssh sudo podman run ...`).
