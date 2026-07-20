1.7.6

# Change log
## 1.7.6
- fix: clang builds add -Wno-unknown-warning-option so older clang (which does not
  know -Wno-pre-c11-compat) no longer fails under -Werror.
- ci: kernel-module build step refreshes dnf metadata (like the kernel-runtime step)
  so it no longer 404s on a pruned kernel-devel package mid-week.
- ci: compiler-compatibility matrix builds, links and runs a minimal tracing example
  on every released GCC (>=14) and clang (>=18) on x86_64, plus a representative
  gcc/clang on arm64 and s390x (big-endian) under QEMU; the minimum supported
  compilers (set by the "Ws" inline-asm constraint) are now documented in the README.

## 1.7.5
- ci: kernel-module runtime gate. Boots the modules in QEMU on the stock distro kernel
  (built without CONFIG_CONSTRUCTORS) and verifies tracing actually registers and produces
  trace files - the regression gate for the module-notifier init path (1.7.4). No kernel
  build: uses the distro kernel-core image (bzImage on x86_64; the raw Image extracted from
  the EFI zboot payload on aarch64).

## 1.7.4
- fix: kernel tracing registers via a module notifier, so it works on kernels built
  without CONFIG_CONSTRUCTORS (the default for most production kernels). Registration
  previously ran only from a module constructor, which such kernels never call - the
  module loaded but tracing silently did nothing. The constructor path is kept for
  CONFIG_CONSTRUCTORS kernels; both are idempotent.

## 1.7.3
- docs: README fixes and additions - remove the (now lifted) in-class-member-function
  tracing limitation, correct the minimal-build feature flags and the decoder output
  description, and document the sanitizer, kernel-module and non-LTO CI steps, the
  clang-tidy diff mode, and offline cross-endian decoding.
- ci: UndefinedBehaviorSanitizer runs as its own leg (split out of the AddressSanitizer
  leg) so its findings are unambiguous.
- test: regression test that a tracepoint inside an in-class member function builds and
  links.

## 1.7.2
- fix: data race on a tracepoint's shared static argument-type info. The one-time
  lazy initialization (`first_time_check`) is now serialized under the global lock and
  published with an acquire/release flag, so concurrent first callers no longer race
  (ThreadSanitizer). Idempotent before, so no behavior change; patch level.
- test: widen the live-stress drain window and drain-after-join in the parallel stress
  tests so their tail checks are deterministic under CI load.

## 1.7.1
- fix: unaligned access on the byte-packed trace format in the writer and both decoders
  (memcpy instead of typed-pointer loads/stores) - undefined behavior that faults on
  strict-alignment targets such as s390x; no format change.
- fix: the unique-stack lookup index is freed on close (was leaked via the raw API).
- fix: the per-call-site offset cache is accessed with relaxed atomics (data race on
  concurrent first use). Behavior preserved throughout; patch-level.

## 1.7.0
- perf: registration lookups use a persisted open-addressing index instead of scanning the
  stack linearly. The index lives in memory per tracebuffer and is periodically persisted
  as tagged entries inside the stack; slabs are nibble-encoded so pre-1.7.0 decoders can
  never misparse them, the last valid slab wins, and a missing or torn slab degrades to
  rebuild-by-scan (the append-only stack stays the source of truth). Stack entry heads now
  carry a kind tag in the previously reserved bytes: file layout change, hence the minor
  bump.
## 1.6.0
- feat: fmt-style tracepoints (C++20 only). `CLLTK_TRACEPOINT_FMT(buffer, "loaded {} in {}ms",
  name, ms)` uses std::format {} placeholders, validated against the argument types at compile
  time via std::format_string. Argument encoding is unchanged; both decoders render the new
  meta entry type 5 with their native {} formatters. A char* argument is always recorded as a
  string (no %p/%s ambiguity). New meta type = file layout change, hence the minor bump.
  Kernel: not applicable (C API).
## 1.5.1
- feat: `clltk export` converts trace files into Chrome/Perfetto trace event JSON
  (open in ui.perfetto.dev): tracepoints as instant events, spans as async begin/end
  pairs correlated by id, open spans visible. The decoder library gains a typed
  `span_info()` accessor on tracepoints.
## 1.5.0
- feat: span tracking with carryable ids. `CLLTK_SPAN_BEGIN(buffer, parent, name)` evaluates
  to a plain uint64 span id that can be passed as a function argument, across threads, or
  through APIs; `CLLTK_SPAN_END(buffer, id)` records the end. Both decoders pair begin/end
  by id, resolve parent relations, and report spans that never ended (e.g. after a crash)
  as still open. Available in userspace (C and C++) and kernel modules. New meta entry
  types 3 (span begin) and 4 (span end): file layout change, hence the minor bump.
## 1.4.0
- feat: cross-endian decoding. Trace files and ELF binaries written on a machine with the
  opposite byte order (e.g. s390x files read on x86_64/aarch64 and vice versa) are
  byte-swapped while reading; the byte order is detected from the file magic / ELF
  identification. Works through the decoder library, the clltk CLI, and the python decoder
  (which additionally fixes float/double arguments from foreign-endian files).
- test: golden trace file fixtures generated by real library builds at every format
  boundary in the public history (1.2.39, 1.2.49, 1.2.64, 1.3.0) plus big-endian s390x
  trace file and ELF fixtures; decoded through both decoders on every test run.
## 1.3.0
- fix: tracepoints in inline functions, templates, and class members no longer fail with
  "causes a section type conflict" on GCC >= 15.2. Meta entries are now ordinary statics;
  discovery works through {meta, offset-cache} pointer pairs emitted into
  `_clltk_<BUFFER>_metaptr` sections via assembler data directives (COMDAT-safe, validated
  for x86_64/aarch64/s390x with -fPIC).
- perf: startup registration writes each call site's file offset into its cache, so the
  first execution of a tracepoint needs no lookup.
- feat: decoder/CLI read both the new `_metaptr` pointer sections and legacy inline `_meta`
  sections from ELF binaries.
- BREAKING (link-time): objects compiled with older headers cannot be mixed with objects
  compiled with these headers in one binary; rebuild all translation units.
- ci: add non-LTO build leg (`unittests-nolto` preset); LTO had masked the section conflict.
- feat: `clltk meta` reads tracepoint metadata from relocatable objects (.o) via relocation
  records.
- fix: handlers detach from the tracebuffer on every deinit; tracepoints firing after
  teardown no longer touch freed memory.
- perf: batched startup registration with a single stack scan and a single file write.
- ci: fix container.sh quoting (argument array instead of eval); weekly uncached container
  rebuild; RPM test cache invalidation on version change.
- fix: python decoder accepts newer minor file versions (gate on major only).
## 1.2.64
- feat: add explicit dependency checks for optional components
- feat: disable automatic source RPM generation
- fix: prevent cryptic build failures when dependencies are missing
## 1.2.63
- fix: kernel Makefile now generates version header automatically
## 1.2.62
- fix: kernel module build warnings
## 1.2.61
- fix: release workflow copies SRPM to workspace before creating checksums
## 1.2.60
- fix: release workflow now finds packages in correct container output path
## 1.2.59
- ci: use ccache with GitHub cache to speed up builds
## 1.2.58
- cli: show features, git hash, license, and URL in --version output
- ci: auto-publish SRPM to GitHub releases when PRs are merged
## 1.2.57
- refactor: move linux userspace abstraction to unix userspace abstraction
- fix: prevent complex_cpp to run if encounter known bad compiler
- fix: illumos wrap gettid for illumos
- fix: illumos do not generate _Generic case for iint8_t on non linux
- fix: illumos change file api to use posix instead of linux extension
## 1.2.56
- cmd: fix gzip-to-stdout closing fd 1 on destruction (dup before gzdopen)
- cmd: fix silent truncation of gzip output lines longer than 8 KB
- cmd: fix --no-recursive ignored by snapshot when no --filter is given
- cmd: fix clltk clear with no arguments giving a confusing empty-name error
- cmd: add -o/--output and -z/--compress to list and meta commands
- snapshot: fix fd 0 treated as invalid (m_fd check was > 0, should be >= 0)
- decoder: fix write_digits producing garbage characters for negative year values
## 1.2.55
- packaging: add RPM subpackages (tracing, decoder, snapshot, devel, cmd, python-decoder)
- packaging: add CMake package config (find_package(CLLTK)) and pkg-config support
- packaging: add proper SRPM via git archive + rpmbuild -bs with Fedora-style spec
- packaging: add comprehensive packaging test suite (89 tests)
- packaging: merge static libraries into clltk-devel, rename clltk-tools to clltk-cmd
## 1.2.54
- docs: add AGENTS.md for AI coding agent instructions
- cli: add backwards-compatible -C flag alias for tracing path
- ci: auto-detect non-interactive terminal in container script
- ci: add missing pytz dependency to container
- test: add CLLTK_ASAN_ENABLED CMake define and simplify ASAN guards
- test: skip stdbuf in live tests when ASAN is enabled
- fix: memory leak in vector test (missing vector_free)
- cmake: fix broken OBJECT_LIBRARY type check in command_line_tool
- cmake: replace no-op directory properties with proper CMake variables
- cmake: create CompileWarnings.cmake interface library for warning flags
- cmake: replace directory-scoped compile options with interface libraries
- cmake: add BUILD_INTERFACE/INSTALL_INTERFACE generator expressions
- cmake: replace raw pthread/Boost variables with imported targets
- cmake: add CONFIGURE_DEPENDS to all file(GLOB) calls
- cmake: remove no-op target_link_directories from command_line_tool
## 1.2.53
- ci: use GitHub Container Registry for faster CI container caching
- ci: consolidate Python tests into single directory structure
- cli: add gzip compression option to decode and live commands
- cli: add confirmation prompt to clear command with -y/--yes skip option
- cli: add meta command for inspecting tracepoint definitions
- cli: make recursive the default for all directory operations
- refactor: unify tar and gzip implementations in snapshot library
- refactor: decoder utilities into public API with header/source separation
- fix: live command error handling and exit codes
- test: add integration tests for CLI commands
## 1.2.52
- replace nlohman json with RapidJSON
- standardize tracebuffer filter style across commands
## 1.2.51
- perf: optimize CRC validation and formatting
- perf: reduce lock contention in OrderedBuffer
- perf: batch output flush calls
- perf: cache timestamp prefixes
- perf: use fast UTC date calculation
- fix: prevent use-after-free in tracebuffer name
- ci: refactor kernel module build and test
- cli: improve command naming and add tracebuffer list
- cli: add global verbose/quiet flags and signal handling
- refactor: clean up includes with IWYU
- fix: address clang-tidy warnings
- ci: modularize CI and add static analysis
## 1.2.50
- add live streaming decoder subcommand with ordered output buffer
- add lock-free memory pool for tracepoint allocation
- add advanced time filtering (min/max/now anchors, duration suffixes)
- add filter options: --pid, --tid, --msg, --file with regex support
- add --stdout option to decoder
- add kernel trace indicator (*) to decoder output
- add user vs kernel space handling in definition format
- fix multiple bugs in decoder and tracing libraries
- fix python decoder to handle definition V2 format
- fix critical bugs: python self reference and pointer arithmetic portability
- refactor: replace robot framework tests with python unittest
- refactor: move cmd implementations to .cpp files and add unit tests
- add extensive unit tests for cmd components (filter, timespec, ordered_buffer)
- add extreme stress tests for live decoder
## 1.2.49
- fix typos
## 1.2.48
refactor: reorganize include structure and improve project layout
- Renamed project from CommoneLowLevelTracingKit to CommonLowLevelTracingKit (correcting typo).
- Reorganized header paths under CommonLowLevelTracingKit/tracing/, CommonLowLevelTracingKit/decoder/, and CommonLowLevelTracingKit/snapshot/ for better namespace clarity.
- Updated all #include directives to use the new consistent path structure (e.g., tracing.h → tracing/tracing.h).
- Updated CMake targets to use OUTPUT_NAME consistently and improved library linking with proper visibility and standard settings.
- Restructured decoder_tool and snapshot_library to use consistent directory layout and public headers.
- Fixed include paths in all examples, tests, and kernel module code to reflect new structure.
- Removed redundant CMake logic and improved target properties.
- Enable optimization
## 1.2.47
- fix macros
- add more cmake options
## 1.2.46
- decoder in c++
- improve the setup based on ASAN
## 1.2.45
- CI changes
## 1.2.44
- update tracebuffer init at runtime in kernel tracing to match user space 
## 1.2.43
- make tracing thread safe
## 1.2.41
- fix potential memory leak in snapshot
## 1.2.40
- change dump format
## 1.2.39
- release to open source
