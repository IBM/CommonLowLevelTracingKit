1.2.48

# Change log
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

