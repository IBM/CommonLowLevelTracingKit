# AGENTS.md

Instructions for AI coding agents working on CommonLowLevelTracingKit (CLLTK).

## Build Commands

```bash
# Configure + build (with tests and examples)
cmake --preset unittests
cmake --build --preset unittests

# Run C++ unit tests (Google Test via CTest)
ctest --test-dir build/ --output-on-failure

# Run Python integration tests
python3 -m unittest discover -v -s ./tests -p 'test_*.py'

# Full CI pipeline in container (same as GitHub Actions)
./scripts/container.sh ./scripts/ci-cd/run_all.sh

# Individual CI steps in container
./scripts/container.sh ./scripts/ci-cd/step_format.sh           # clang-format check
./scripts/container.sh ./scripts/ci-cd/step_build.sh            # build
./scripts/container.sh ./scripts/ci-cd/step_test.sh             # all tests
./scripts/container.sh ./scripts/ci-cd/step_memcheck.sh         # Valgrind
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh  # clang-tidy + cppcheck

# Auto-format all code
./scripts/container.sh ./scripts/development_helper/format_everything.sh
```

CMake presets: `default`, `unittests` (dev), `rpm` (packaging), `static-analysis` (clang-tidy).

## Project Architecture

```
tracing_library/    C library (C11). Core tracebuffer, tracepoint, ringbuffer.
                    Platform abstraction in source/abstraction/ (Linux-only).
decoder_tool/       C++ decoder library (C++20) + Python decoder script.
command_line_tool/  clltk CLI (C++20). Subcommands in commands/ as OBJECT libs.
snapshot_library/   Archive/snapshot library (C++20).
kernel_tracing_library/  Linux kernel module (Kbuild). Separate abstraction layer.
tests/              C++ Google Test (tracing.tests/, decoder.tests/, cmd.tests/)
                    + Python unittest (test_*.py) with helpers/ utilities.
examples/           C and C++ usage examples.
cmake/              CMake modules (toolchain, ASAN, coverage, packaging, static analysis).
scripts/            CI/CD scripts, container config, development helpers.
docs/               AsciiDoc documentation, file format spec, diagrams.
```

## Language Standards and Compiler Rules

- C11 (tracing library core), C17 (abstraction layer), C++20 (decoder, CLI, tests)
- ONLY GCC and Clang are supported. Other compilers trigger `FATAL_ERROR`.
- ONLY Linux is supported. CMake fatally errors on other platforms.
- `-Werror` is enabled everywhere. Any new warning is a build-breaking change.
- `-Werror=format`, `-Wformat=2`, `-Wformat-security` are PUBLIC flags that propagate to consumers.
- `-fvisibility=hidden` is default. Only explicitly marked symbols are exported.

## Code Style

- Enforced by `.clang-format`: LLVM-based, 100 column limit, tabs for indentation (width 4), Linux brace style.
- MUST run `format_everything.sh` before committing. CI will reject unformatted code.
- Naming: `clltk_` prefix for public C API, `_clltk_` for internal macros. PascalCase for C++ classes, snake_case for C++ low-level code.
- Include guards: `#ifndef _CLLTK_xxx_H_` for public headers. `#pragma once` for internal/abstraction headers. Never mix styles in one file.
- Documentation: Doxygen method 3 (`///` triple-slash comments).
- Copyright header required on every source file:
  ```
  Copyright (c) <YEAR> <COPYRIGHT HOLDERS>
  SPDX-License-Identifier: BSD-2-Clause-Patent
  ```
- Directory structure follows Canonical Project Structure (P1204R0).

## Critical Gotchas

### Tracing Macros
- `CLLTK_TRACEPOINT` supports max 10 arguments. More triggers a `static_assert`.
- Format string MUST be a string literal (enforced by `__attribute__((format(printf,...)))`).
- The buffer argument must be defined via `CLLTK_TRACEBUFFER_MACRO_VALUE()`. It is an identity macro, but `_CLLTK_STATIC_TRACEPOINT` internally references `&_clltk_##_BUFFER_`, so the name must match an existing `CLLTK_TRACEBUFFER` declaration or you get linker errors.
- Each `CLLTK_TRACEPOINT` call site creates static variables. A call site is permanently bound to one tracebuffer offset. You cannot dynamically reassign it.
- Macros use `__VA_OPT__` (C++20/C23/GCC extension). NEVER replace with `##__VA_ARGS__` -- they are not equivalent.
- `CLLTK_TRACEPOINT_DUMP` takes `(buffer, message, address, size)` -- NOT a printf format. Do not confuse with `CLLTK_TRACEPOINT`.
- `CLLTK_DYN_TRACEPOINT` takes a string buffer name (not an identifier) and binds at runtime. Slower than static tracepoints.

### ELF Sections
- Metadata is placed in custom ELF sections (`_clltk_<BUFFER_NAME>_meta`) via `__attribute__((section(...)))`. Buffer names MUST be valid C identifiers.
- Constructor/destructor priority is 101 (very early). User code with priority <= 101 cannot use tracing.
- `_CLLTK_INTERNAL` define gates macro expansion. The library itself compiles with `-D_CLLTK_INTERNAL`; consumer code must NOT define it.

### API Typo -- Do NOT Fix
- `clltk_unrecoverbale_error_callback` has a typo ("unrecoverbale"). This IS the public API. Fixing the spelling would be a breaking change.

### Error Handling
- C library: `ERROR_AND_EXIT()` is always fatal (calls `_exit()`, not `exit()`, to avoid deadlock). `ERROR_LOG()` is non-fatal.
- Memory allocation and mutex lock failures are always fatal.
- The unrecoverable error callback is a weak symbol that consumers can override.
- Error messages contain ANSI color escape codes.

### Files and Paths
- Trace files use `.clltk_trace` extension. Path resolution: `clltk_set_tracing_path()` > `CLLTK_TRACING_PATH` env var > current directory.
- File creation uses `linkat()` for atomic idempotent creation, NOT `rename()`.
- Memory mutex uses `PTHREAD_PROCESS_SHARED` + `PTHREAD_MUTEX_ROBUST` on mmapped regions. Handles `EOWNERDEAD` after process death.
- ARM (`__aarch64__`) uses explicit cache flushing (`dc cvac` + `dsb ish` + `isb`) in `memcpy_and_flush`.

### Platform Abstraction
- Six interface files in `tracing_library/source/abstraction/include/abstraction/` (file, sync, memory, error, info, optimization).
- `sync_mutex_t` MUST be exactly 64 bytes (static assert enforced).
- Adding a new platform requires a new subdirectory under `abstraction/` and updating `abstraction/CMakeLists.txt`.

## Testing

- C++ tests: Google Test 1.12.1 (fetched via CMake FetchContent). Tests compile with `-O0` and `-DUNITTEST`.
- Python tests: `unittest.TestCase`. Always set `CLLTK_TRACING_PATH` to temp dirs in `setUp()`.
- Test directory name IS the test executable name (e.g., `api.tests/` produces `api.tests` binary).
- All `.c`/`.cpp` files in a test directory are globbed automatically.
- `UNITTEST` define unlocks test-only APIs (e.g., `file_reset()`).

### Generated Test Files -- Do NOT Edit
- `tests/decoder.tests/args_cpp.py` generates `args_cpp.tests.cpp` at build time.
- `tests/decoder.tests/format_cpp.py` generates `format_cpp.tests.cpp` at build time.
- Edit the `.py` generators, not the generated `.cpp` files.

## Headers -- Public API

- Users MUST only include `CommonLowLevelTracingKit/tracing/tracing.h`. Never include `_`-prefixed headers directly; they enforce this with `#error`.
- C++ decoder headers: `CommonLowLevelTracingKit/decoder/Tracebuffer.hpp`, `Tracepoint.hpp`, `Meta.hpp`, `Pool.hpp`, `ToString.hpp`, `Common.hpp`.
- Snapshot: `CommonLowLevelTracingKit/snapshot/snapshot.hpp`.
- All public headers use `_CLLTK_EXTERN_C_BEGIN`/`END` for C++ compatibility.

## CMake Conventions

- Target naming: `clltk_<component>_static`, `clltk_<component>_shared`, `clltk_<component>` (alias).
- Shared library output name omits `_shared` suffix (e.g., `libclltk_tracing.so`).
- Tests/examples only build in standalone mode (`STANDALONE_PROJECT=ON`). Disabled when imported via `add_subdirectory`.
- Key options: `CLLTK_TRACING`, `CLLTK_SNAPSHOT`, `CLLTK_COMMAND_LINE_TOOL`, `CLLTK_CPP_DECODER`, `CLLTK_PYTHON_DECODER`, `CLLTK_KERNEL_TRACING`, `CLLTK_EXAMPLES`, `CLLTK_TESTS`, `ENABLE_ASAN`, `ENABLE_CLANG_TIDY`.

## Version Management

- Version is the first line of `VERSION.md` (format: `MAJOR.MINOR.PATCH`).
- `cmake/gen_version_header.sh` generates `version.gen.h` at build time. Do NOT edit the `.gen.h` file.
- Version bump: edit first line of `VERSION.md`, add changelog entry below.

## Git Conventions

- Commits MUST include `Signed-off-by:` (DCO). Use `git commit -s`.
- Branch model: `main` (protected) receives merges from development branches.
- Merge approval: LGTM from one maintainer per affected component.

## References

- @CONTRIBUTING.md for contribution process
- @README.md for usage overview
- @docs/readme.asciidoc for detailed API documentation
- @docs/file_specification.asciidoc for binary file format spec
- @docs/technical_documentation.asciidoc for implementation details
