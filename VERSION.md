1.2.54

# Change log
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
