---
marp: true
theme: default
paginate: true
---

# CLLTK Internals
## A Deep Dive for Contributors

Understanding the implementation to modify, extend, or debug the library

---

# Source Code Organization

```
CommonLowLevelTracingKit/
├── tracing_library/           # C11/C17 core library
│   ├── include/.../tracing/   # Public headers
│   │   ├── tracing.h          # ONLY public entry point
│   │   └── _*.h               # Internal (error if included directly)
│   └── source/
│       ├── abstraction/       # Platform layer (Linux-only)
│       ├── ringbuffer/        # Variable-length ringbuffer
│       ├── unique_stack/      # Metadata stack
│       ├── definition/        # Tracebuffer definition
│       ├── crc8/, md5/        # Crypto primitives
│       └── tracebuffer.c      # Core buffer management
├── decoder_tool/              # C++20 decoder library
├── command_line_tool/         # clltk CLI (C++20)
└── kernel_tracing_library/    # Linux kernel module
```

---

# Language Standards & Compiler Rules

| Component | Standard | Notes |
|-----------|----------|-------|
| tracing_library core | C11 | `tracebuffer.c`, `tracepoint.c` |
| abstraction layer | C17 | Platform-specific code |
| decoder, CLI, tests | C++20 | Modern C++ features |

**Compiler restrictions:**
- **ONLY GCC and Clang** — CMake `FATAL_ERROR` for others
- **ONLY Linux** — No cross-platform support
- `-Werror` everywhere — warnings break the build
- `-fvisibility=hidden` — explicit symbol export required

---

# The ELF Section Trick

Metadata is gathered at compile time using custom ELF sections:

```c
// _macros.h:38
#define _CLLTK_PLACE_IN(_NAME_) \
    __attribute__((used, section("_clltk_" #_NAME_ "_meta")))
```

Each tracepoint creates static metadata placed in a buffer-specific section:

```c
static const struct { ... } _meta 
    __attribute__((used, section("_clltk_MyBuffer_meta"))) = {...};
```

**Access via linker-generated symbols:**
```c
extern const uint8_t *const __start__clltk_MyBuffer_meta;
extern const uint8_t *const __stop__clltk_MyBuffer_meta;
```

---

# ELF Section: How It Works

```
┌─────────────────────────────────────────────────────┐
│  Compilation Unit A                                 │
│  ┌─────────────────────────────────────────────┐   │
│  │ CLLTK_TRACEPOINT(Buf, "msg1 %d", x)         │   │
│  │   → static _meta in section _clltk_Buf_meta │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
                        ↓ linker
┌─────────────────────────────────────────────────────┐
│  ELF Section: _clltk_Buf_meta                       │
│  ┌──────────┬──────────┬──────────┬──────────┐     │
│  │ meta_A_1 │ meta_A_2 │ meta_B_1 │ meta_C_1 │     │
│  └──────────┴──────────┴──────────┴──────────┘     │
│  ↑                                           ↑     │
│  __start__clltk_Buf_meta    __stop__clltk_Buf_meta │
└─────────────────────────────────────────────────────┘
```

---

# Constructor/Destructor Priority

```c
// _user_tracing.h:36
__attribute__((constructor(101), used)) 
static void _clltk_constructor(void) {
    // Iterate all tracebuffer handlers
    for (handler_ptr = &__start__clltk_tracebuffer_handler_ptr;
         handler_ptr < &__stop__clltk_tracebuffer_handler_ptr; 
         handler_ptr++) {
        _clltk_tracebuffer_init(handler);
        // Add metadata blob to stack
        handler->runtime.file_offset = 
            _clltk_tracebuffer_add_to_stack(handler, ...);
    }
}
```

**Priority 101** = very early initialization.
User code with priority ≤ 101 **cannot use tracing**.

---

# Macro Magic: Argument Counting

```c
// _macros.h:70-75
#define _CLLTK_NARGS_SEQ(_1, _2, ..., _20, N, ...) N
#define _CLLTK_NARGS(...) \
    _CLLTK_NARGS_SEQ(__VA_OPT__(__VA_ARGS__, ) \
        20, 19, 18, ..., 2, 1, 0)
```

**How it works:**
- `_CLLTK_NARGS(a, b, c)` → expands to `3`
- Uses `__VA_OPT__` (C++20/C23/GCC extension)
- **Never replace with `##__VA_ARGS__`** — not equivalent!

**Limit enforced:**
```c
_CLLTK_STATIC_ASSERT(_CLLTK_NARGS(__VA_ARGS__) <= 10,
    "only supporting up to 10 arguments");
```

---

# Macro Magic: Type Deduction

**C path:** Uses `_Generic` selection

```c
// _arguments.h:77-99
#define _CLLTK_TYPE_TO_TYPE(_I_, _X_) \
    _Generic((_X_),                   \
        bool:     _clltk_argument_uint8,  \
        char:     _clltk_argument_sint8,  \
        uint32_t: _clltk_argument_uint32, \
        char *:   _clltk_argument_string, \
        default:  _clltk_argument_pointer)
```

**C++ path:** Uses `constexpr` template

```cpp
// _arguments.h:112-168
template <typename T>
constexpr _clltk_argument_t _CLLTK_TYPE_TO_TYPE_TEMP() {
    if constexpr (std::is_same<T, uint32_t>::value)
        return _clltk_argument_uint32;
    // ... handles enums, atomics, pointers
}
```

---

# Argument Type Encoding

```c
// _arguments.h:22-39
typedef enum __attribute__((__packed__)) {
    _clltk_argument_uint8   = 'c',  // 1 byte
    _clltk_argument_sint8   = 'C',  // 1 byte
    _clltk_argument_uint16  = 'w',  // 2 bytes
    _clltk_argument_uint32  = 'i',  // 4 bytes
    _clltk_argument_uint64  = 'l',  // 8 bytes
    _clltk_argument_float   = 'f',  // 4 bytes
    _clltk_argument_double  = 'd',  // 8 bytes
    _clltk_argument_string  = 's',  // 4 + flex
    _clltk_argument_pointer = 'p',  // 8 bytes
    _clltk_argument_dump    = 'x',  // 4 + flex
} _clltk_argument_t;
```

**Stored as printable chars** — useful for debugging:
```c
argument_type_array[] = {'i', 's', '\0'}  // int, string
```

---

# Ringbuffer Implementation

**Location:** `tracing_library/source/ringbuffer/ringbuffer.c`

```c
// ringbuffer.h:25-36
struct __attribute__((packed, aligned(8))) ringbuffer_head_t {
    uint64_t version;                      // offset 0
    sync_mutex_t mutex;                    // offset 8  (64 bytes!)
    uint64_t body_size;                    // offset 72
    uint64_t wrapped;                      // offset 80
    uint64_t dropped;                      // offset 88
    uint64_t entries;                      // offset 96
    uint64_t next_free;                    // offset 104
    uint64_t last_valid;                   // offset 112
    uint8_t _reserved_for_future_use[40];  // offset 120
    uint8_t body[];                        // offset 160 (flexible array)
};
```

**Total header: 160 bytes**

---

# Ringbuffer Entry Format

```c
// ringbuffer.h:41-49
typedef struct __attribute__((packed)) {
    uint8_t magic;                      // '~' (0x7E)
    ringbuffer_entry_body_size_t body_size;  // uint16_t
    uint8_t crc;                        // CRC8 of header
    uint8_t body[];
    // After body: 1 byte CRC8 of body (not in body_size)
} ringbuffer_entry_head_t;
```

```
┌───────┬────────────┬──────┬─────────────────┬──────────┐
│ magic │ body_size  │ crc  │      body       │ body_crc │
│  '~'  │  uint16    │ u8   │   (variable)    │    u8    │
│ 1 byte│  2 bytes   │1 byte│  body_size bytes│  1 byte  │
└───────┴────────────┴──────┴─────────────────┴──────────┘
```

---

# Ringbuffer: Write Path

```c
// ringbuffer.c:227-260
size_t ringbuffer_in(ringbuffer_head_t *dest, 
                     const void *source, size_t size) {
    // Build entry header
    ringbuffer_entry_head_t entry_header = {
        .magic = '~',
        .body_size = size,
        .crc = crc8_continue(0, &entry_header, 3)
    };
    
    // Make space by dropping oldest entries
    while (needed > ringbuffer_available(dest))
        drop_oldest_entry(dest);
    
    // Copy header, body, body_crc
    copy_in(dest, &entry_header, sizeof(entry_header));
    move_next_free(dest, sizeof(entry_header));
    copy_in(dest, source, size);
    move_next_free(dest, size);
    // ... body_crc
}
```

---

# Ringbuffer: Wrap-Around Copy

```c
// ringbuffer.c:108-125
static void copy_in(ringbuffer_head_t *dest, 
                    const void *source, size_t size) {
    // Two-block copy for wrap-around
    uint64_t space_until_wrap = dest->body_size - dest->next_free;
    uint64_t first_block = MIN(space_until_wrap, size);
    
    // Block 1: next_free → end of body
    memcpy_and_flush(&dest->body[dest->next_free], 
                     source, first_block);
    
    // Block 2: start of body → remaining
    uint64_t second_block = size - first_block;
    memcpy_and_flush(&dest->body[0], 
                     source + first_block, second_block);
}
```

**Note:** `memcpy_and_flush` — ARM requires explicit cache flush!

---

# Platform Abstraction Layer

**Location:** `tracing_library/source/abstraction/`

```
abstraction/
├── include/abstraction/
│   ├── error.h        # ERROR_AND_EXIT, ERROR_LOG
│   ├── file.h         # file_create, file_open, file_mmap
│   ├── info.h         # get_process_id, get_thread_id
│   ├── memory.h       # memcpy_and_flush
│   ├── optimization.h # LIKELY, UNLIKELY
│   └── sync.h         # sync_mutex_t, sync_memory_mutex_*
└── unix_user_space/
    ├── error.c
    ├── file.c
    ├── info.c
    ├── memory.c
    └── sync.c
```

**Adding a new platform:** Create new subdirectory, update CMakeLists.txt

---

# Mutex: Inter-Process Synchronization

```c
// sync.c:42-66
void sync_memory_mutex_init(sync_mutex_t *const ptr) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init((pthread_mutex_t *)ptr, &attr);
}
```

**Key attributes:**
- `PTHREAD_PROCESS_SHARED` — works across processes via mmap
- `PTHREAD_MUTEX_ROBUST` — handles `EOWNERDEAD` after process crash

```c
// sync.c:85-91
if (status == EOWNERDEAD) {
    if (pthread_mutex_consistent(mutex) == 0) {
        return CREATE_SYN_LOCK_OBJECT(ptr, true, "mutex recovered from dead owner");
    }
    return CREATE_SYN_LOCK_OBJECT(ptr, false, strerror(status));
}
```

---

# File Format: Overview

```
┌──────────────────────────────────────────────────────┐
│ File Header (56 bytes)                               │
│   magic: "?#$~tracebuffer\0" (16 bytes)              │
│   version: uint64                                    │
│   definition_offset, ringbuffer_offset, stack_offset │
│   crc8 at byte 55                                    │
├──────────────────────────────────────────────────────┤
│ Definition Section                                   │
│   body_size: uint64                                  │
│   name: null-terminated string                       │
│   [V2: extended magic, source_type, crc8]            │
├──────────────────────────────────────────────────────┤
│ Ringbuffer Section (160 + body_size bytes)           │
│   header + body (trace entries)                      │
├──────────────────────────────────────────────────────┤
│ Stack Section                                        │
│   header + entries (metadata blobs)                  │
└──────────────────────────────────────────────────────┘
```

---

# Trace Entry Format

**Location in ringbuffer body:**

```
┌────────────────────────────────────────────────────┐
│ Traceentry Header (22 bytes fixed)                 │
│   in_file_offset: 6 bytes (points to metaentry)    │
│   pid: 4 bytes                                     │
│   tid: 4 bytes                                     │
│   timestamp_ns: 8 bytes (UTC nanoseconds)          │
├────────────────────────────────────────────────────┤
│ Arguments (variable, defined by metaentry)         │
│   Fixed types: stored directly (1-16 bytes)        │
│   Flex types (string/dump):                        │
│     size: uint32 + data: size bytes                │
└────────────────────────────────────────────────────┘
```

---

# Meta Entry Format

**Stored in stack section, referenced by `in_file_offset`:**

```c
// _meta.h:30-49
struct __attribute__((packed)) {
    char magic;              // '{' (0x7B)
    uint32_t size;           // Total struct size
    _clltk_meta_enty_type type;  // 1=printf, 2=dump
    uint32_t line;           // Source line number
    uint8_t argument_count;  // 0-10
    char argument_type_array[N+1];  // Types + '\0'
    char file[];             // Null-terminated filename
    char str[];              // Format string
};
```

**Magic `{`** — allows validation when accessing by offset

---

# Build System

**CMake presets:** `default`, `unittests`, `rpm`, `static-analysis`

```bash
# Development build
cmake --preset unittests
cmake --build --preset unittests

# Run tests
ctest --test-dir build/ --output-on-failure
python3 -m unittest discover -v -s ./tests -p 'test_*.py'

# Full CI in container
./scripts/container.sh ./scripts/ci-cd/run_all.sh
```

**Key CMake options:**
- `CLLTK_TRACING`, `CLLTK_SNAPSHOT`, `CLLTK_COMMAND_LINE_TOOL`
- `ENABLE_ASAN`, `ENABLE_CLANG_TIDY`
- `STANDALONE_PROJECT=ON` — enables tests/examples

---

# Testing Infrastructure

**C++ tests:** Google Test 1.12.1 (FetchContent)
```
tests/
├── tracing.tests/    # Core library tests
├── decoder.tests/    # Decoder tests
└── cmd.tests/        # CLI tests
```

**Python tests:** `unittest.TestCase`
- Integration tests, live tracing tests
- Always set `CLLTK_TRACING_PATH` to temp dir

**Generated tests (DO NOT EDIT):**
```
tests/decoder.tests/args_cpp.py    → args_cpp.tests.cpp
tests/decoder.tests/format_cpp.py  → format_cpp.tests.cpp
```

**Test-only APIs:** Enabled by `-DUNITTEST` define

---

# Critical Gotchas for Contributors

1. **Tracepoint-buffer binding is permanent**
   - Static tracepoints bound at compile time
   - Use `CLLTK_DYN_TRACEPOINT` for runtime binding (slower)

2. **Max 10 arguments** — `static_assert` enforced

3. **Format string must be literal** — `__attribute__((format))` check

4. **No tracing in inline member functions** — linker errors

5. **API typo is intentional:**
   ```c
   clltk_unrecoverbale_error_callback  // DO NOT FIX
   ```

6. **`sync_mutex_t` must be exactly 64 bytes** — static assert

7. **Run `format_everything.sh` before commit** — CI rejects unformatted

---

# Error Handling Patterns

```c
// abstraction/error.h
ERROR_AND_EXIT("message %s", arg);  // Fatal, calls _exit()
ERROR_LOG("message %s", arg);       // Non-fatal, continues
```

**Why `_exit()` not `exit()`?**
- Avoids deadlock if error occurs while holding locks
- `exit()` runs atexit handlers which might try to acquire locks

**Unrecoverable error callback:**
```c
// Weak symbol — consumers can override
void clltk_unrecoverbale_error_callback(const char *msg);
```

---

# ARM-Specific: Cache Flushing

```c
// memory.c (ARM path)
void memcpy_and_flush(void *dest, const void *src, size_t n) {
    memcpy(dest, src, n);
    
    // Explicit cache flush for ARM
    for (addr = dest; addr < dest + n; addr += cache_line) {
        asm volatile("dc cvac, %0" :: "r"(addr));  // Clean
    }
    asm volatile("dsb ish");  // Data sync barrier
    asm volatile("isb");      // Instruction sync barrier
}
```

**Why?** ARM has weaker memory ordering than x86.
mmap'd regions need explicit cache management.

---

# Quick Reference: Key Files

| What | Where |
|------|-------|
| Public API | `tracing_library/include/.../tracing/tracing.h` |
| Macro magic | `_macros.h`, `_arguments.h`, `_meta.h` |
| Tracepoint impl | `_user_tracing.h`, `source/tracepoint.c` |
| Ringbuffer | `source/ringbuffer/ringbuffer.c` |
| Platform layer | `source/abstraction/unix_user_space/` |
| File format spec | `docs/file_specification.asciidoc` |
| Build config | `CMakeLists.txt`, `cmake/*.cmake` |
| CI scripts | `scripts/ci-cd/` |

**Before modifying:** Read `AGENTS.md` for full gotcha list

---

# Summary

- **ELF sections** gather metadata at compile time
- **Constructor priority 101** ensures early initialization
- **`_Generic` / `constexpr`** deduce types at compile time
- **Ringbuffer** handles wrap-around with two-block copies
- **Robust mutex** recovers from process crashes
- **Platform abstraction** isolates Linux-specific code
- **CRC8** validates all data structures

**Questions?** Check `docs/technical_documentation.asciidoc`
