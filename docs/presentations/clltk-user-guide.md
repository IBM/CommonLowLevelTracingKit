---
marp: true
theme: default
paginate: true
style: |
  section {
    transform: scale(0.9);
    transform-origin: top left;
  }
---

# CLLTK - Common Low Level Tracing Kit

**A fast, memory-efficient tracing library for C/C++ applications**

---

# What is CLLTK?

- **Printf-style tracing** with compile-time optimization
- **Ringbuffer storage** — fixed memory footprint, no allocations at runtime
- **Minimal overhead** — metadata captured at compile time, only values at runtime
- **File-based output** — each tracebuffer creates a `.clltk_trace` file

```
Your App → CLLTK_TRACEPOINT() → Ringbuffer → .clltk_trace file → clltk decode
```

---

# Quick Start: CMake Integration

**Option 1: FetchContent**
```cmake
include(FetchContent)
FetchContent_Declare(clltk
    GIT_REPOSITORY https://github.com/IBM/CommonLowLevelTracingKit.git
    GIT_TAG main
)
FetchContent_MakeAvailable(clltk)

target_link_libraries(your_target PRIVATE clltk_tracing)
```

**Option 2: Installed package**
```cmake
find_package(CLLTK REQUIRED)
target_link_libraries(your_target PRIVATE clltk::clltk_tracing_shared)
```

---

# Minimal Example

```c
#include "CommonLowLevelTracingKit/tracing/tracing.h"

CLLTK_TRACEBUFFER(MyApp, 4096);  // 4KB ringbuffer

int main(void)
{
    CLLTK_TRACEPOINT(MyApp, "Application started");
    CLLTK_TRACEPOINT(MyApp, "User: %s, ID: %d", "alice", 42);
    return 0;
}
```

**Output:** Creates `MyApp.clltk_trace` in current directory

---

# Step 1: Define a Tracebuffer

```c
CLLTK_TRACEBUFFER(BufferName, size_in_bytes);
```

- **BufferName** — valid C identifier, becomes the filename
- **Size** — ringbuffer capacity in bytes (older entries overwritten when full)

```c
// Examples
CLLTK_TRACEBUFFER(NetworkEvents, 8192);   // → NetworkEvents.clltk_trace
CLLTK_TRACEBUFFER(DebugLog, 1024);        // → DebugLog.clltk_trace
```

Place at **file scope** (not inside functions).

---

# Step 2: Add Tracepoints

```c
CLLTK_TRACEPOINT(BufferName, "format string", args...);
```

Printf-style format specifiers:

```c
CLLTK_TRACEPOINT(MyApp, "Simple message");
CLLTK_TRACEPOINT(MyApp, "Integer: %d, Unsigned: %u", -5, 10u);
CLLTK_TRACEPOINT(MyApp, "Float: %f, Double: %.2f", 3.14f, 2.718);
CLLTK_TRACEPOINT(MyApp, "String: %s", "hello");
CLLTK_TRACEPOINT(MyApp, "Hex: 0x%08x, Pointer: %p", 0xDEAD, ptr);
```

Each tracepoint automatically captures: **timestamp, PID, TID, file, line**

---

# Dump Binary Data

For raw memory dumps, use `CLLTK_TRACEPOINT_DUMP`:

```c
CLLTK_TRACEPOINT_DUMP(BufferName, "message", address, size);
```

```c
uint8_t packet[64];
receive_packet(packet, sizeof(packet));

CLLTK_TRACEPOINT_DUMP(NetworkEvents, "Received packet", packet, sizeof(packet));
```

**Note:** This is NOT printf-style — the message is a label, not a format string.

---

# Control Output Location

**Default:** Trace files created in current working directory.

**Option 1: Environment variable**
```bash
export CLLTK_TRACING_PATH=/var/log/traces
./my_application
```

**Option 2: Programmatic**
```c
#include "CommonLowLevelTracingKit/tracing/tracing.h"

int main(void)
{
    clltk_set_tracing_path("/var/log/traces");
    // Now all tracebuffers write to /var/log/traces/
    ...
}
```

---

# Decoding Traces

**Decode to stdout:**
```bash
clltk decode MyApp.clltk_trace
```

**Decode to file:**
```bash
clltk decode MyApp.clltk_trace -o traces.txt
```

**Decode entire directory:**
```bash
clltk decode /var/log/traces/
```

**Live monitoring (while app runs):**
```bash
clltk live MyApp.clltk_trace
```

---

# CLI Commands Reference

| Command | Purpose |
|---------|---------|
| `clltk decode <file>` | Decode trace file(s) to readable output |
| `clltk live <file>` | Real-time trace monitoring |
| `clltk snapshot -o snap.clltk` | Archive traces for later analysis |
| `clltk clear <buffer>` | Clear a tracebuffer without restart |
| `clltk tb <name> -s <size>` | Create tracebuffer from CLI |
| `clltk tp <buffer> <msg>` | Write tracepoint from CLI |

---

# Dynamic API (Runtime Control)

Create tracebuffers at runtime:
```c
clltk_dynamic_tracebuffer_creation("DynamicBuffer", 2048);
```

Clear a tracebuffer:
```c
clltk_dynamic_tracebuffer_clear("MyApp");
```

Dynamic tracepoints (buffer name as string):
```c
CLLTK_DYN_TRACEPOINT("DynamicBuffer", "value: %d", val);
```

**Note:** Dynamic tracepoints are slower than static ones.

---

# Constraints You Must Know

| Constraint | Details |
|------------|---------|
| **Max 10 arguments** | Compile error if exceeded |
| **Format string must be literal** | `const char* fmt = "..."; CLLTK_TRACEPOINT(buf, fmt)` fails |
| **Buffer binding is permanent** | A tracepoint cannot change its buffer at runtime |
| **No inline member functions** | Move implementation to `.cpp` file |

```cpp
// WRONG - will fail at link time
struct Foo {
    void bar() { CLLTK_TRACEPOINT(TB, "nope"); }
};

// CORRECT
void Foo::bar() { CLLTK_TRACEPOINT(TB, "works"); }
```

---

# Complete C Example

```c
#include "CommonLowLevelTracingKit/tracing/tracing.h"

CLLTK_TRACEBUFFER(Demo, 4096);

void process_request(int id, const char *data)
{
    CLLTK_TRACEPOINT(Demo, "Processing request %d", id);
    // ... do work ...
    CLLTK_TRACEPOINT(Demo, "Request %d complete: %s", id, data);
}

int main(void)
{
    CLLTK_TRACEPOINT(Demo, "Service starting");
    process_request(1, "success");
    process_request(2, "success");
    return 0;
}
```

---

# Complete C++ Example

```cpp
#include "CommonLowLevelTracingKit/tracing/tracing.h"

CLLTK_TRACEBUFFER(AppTrace, 8192);

class Service {
public:
    void start();  // Declaration only
    void stop();
};

void Service::start() {  // Definition in source file
    CLLTK_TRACEPOINT(AppTrace, "Service::start() called");
}

void Service::stop() {
    CLLTK_TRACEPOINT(AppTrace, "Service::stop() called");
}
```

---

# Workflow Summary

```
1. Add CLLTK to your CMake project
   └─ target_link_libraries(app PRIVATE clltk_tracing)

2. Define tracebuffer(s)
   └─ CLLTK_TRACEBUFFER(MyApp, 4096);

3. Add tracepoints in your code
   └─ CLLTK_TRACEPOINT(MyApp, "event: %s", name);

4. Build and run your application
   └─ Creates MyApp.clltk_trace

5. Decode traces
   └─ clltk decode MyApp.clltk_trace
```

---

# Resources

- **Repository:** github.com/IBM/CommonLowLevelTracingKit
- **API Docs:** `docs/readme.asciidoc`
- **Examples:** `examples/` directory
- **File Format:** `docs/file_specification.asciidoc`

**Questions?**
