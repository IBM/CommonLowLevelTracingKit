# CLLTK - Common Lowlevel Tracing Kit

A blazingly fast and memory efficient tracing frontend.


## How it works

To explain the idea behind `common low level tracing` it is best to break down a small example:

```c
CLLTK_TRACEBUFFER(MyFirstTracebuffer, 1024);

int main()
{
  const char * name = "Max";
  const int age = 42;

  CLLTK_TRACEPOINT(MyFirstTracebuffer, "Hello %s, you are %d years old", name, age);

  return 0
}
```

The first line defines the tracebuffer with the name `MyFirstTracebuffer` and a size of 1024byte. Nothing else is needed to enable the tracing.
The name is used to associated tracepoint with this tracebuffer and is also the file name for this tracebuffer. In this case this would be `MyFirstTracebuffer.clltk_trace` with a ringbuffer body size of 1024byes.

The tracepoints `CLLTK_TRACEPOINT` defines first the target tracebuffer, than the format string, followed by the arguments.


### fmt-style format strings (C++20)

`CLLTK_TRACEPOINT_FMT` accepts std::format style `{}` placeholders, validated
against the argument types at compile time:

```cpp
CLLTK_TRACEPOINT_FMT(MyFirstTracebuffer, "loaded {} in {}ms", name, duration);
```

### Spans

Spans track scoped work with begin/end events and a carryable id:

```c
clltk_span_id_t request = CLLTK_SPAN_BEGIN(MyFirstTracebuffer, CLLTK_SPAN_NO_PARENT, "handle request");
clltk_span_id_t parsing = CLLTK_SPAN_BEGIN(MyFirstTracebuffer, request, "parsing");
// ...
CLLTK_SPAN_END(MyFirstTracebuffer, parsing);
CLLTK_SPAN_END(MyFirstTracebuffer, request);
```

The id is a plain `uint64` value: pass it as a function argument, hand it to
another thread, or embed it in an API to continue the span elsewhere - the
decoder correlates begin and end by id across all buffers of a decode set.
A span that never ends (for example because the process crashed) is reported
as still open, which is often exactly the interesting information.
See `examples/spans_c` and `examples/spans_cpp`.

## How to use it

1. Add repository to your project. By cloning, downloading or with CMake-FetchContent.
2. Link your target against `clltk_tracing_static` or `clltk_tracing_shared` depending if you want to like static or shared.
3. Define tracebuffers and tracepoints in your code
4. Build your target
5. Set environment variable if you want to trace to a specific location. Otherwise the location, from which you call the executable, is chosen.
6. Call your executable.
7. Decode traces:
    - decode traces while your executable is running or after it stopped by:
      `decoder_tool/python/clltk_decoder.py <path to tracebuffers>`
    - copy tracebuffer with `cp` or create a archive to create a snapshot. And decode it later with:
      `decoder_tool/python/clltk_decoder.py <path to tracebuffers>`
8. Read the decoded traces. By default they are written to `output.txt`; pass
   `-o <file>` to choose the destination, and a `.csv` extension to get CSV
   instead of the aligned text format.

Trace files are decoded on the machine you run the decoder on, regardless of
the endianness they were written with: a little-endian host can decode a
big-endian (for example s390x) trace file, and vice versa. This is an offline
capability - the writer always uses the host's native byte order.

### Visual timelines

`clltk export` converts trace files into Chrome/Perfetto trace event JSON:

```bash
clltk export /tmp/traces -o trace.json
```

Open the file in https://ui.perfetto.dev to see tracepoints as instant events
and spans as async begin/end pairs on a timeline - including spans that were
still open when the process ended.

## CLI Tool Commands

The `clltk` command-line tool provides several commands for working with tracebuffers:

### Tracebuffer Management

- **`tb` / `tracebuffer`** - Create a new tracebuffer with a given name and size
- **`clear`** - Clear all entries from a tracebuffer (keeps the file, empties the ringbuffer)

### Tracing

- **`tp` / `tracepoint`** - Write a dynamic tracepoint to a tracebuffer
- **`tracepipe`** - Pipe tracepoints from stdin or a file to a tracebuffer

### Decoding

- **`de` / `decode`** - Decode and format tracebuffer files
- **`live`** - Live streaming decoder for real-time trace monitoring

### Snapshots

- **`sp` / `snapshot`** - Take a snapshot of tracebuffers

## Security Considerations
**Tampering with Trace Files**: Any modification or tampering with the trace files can cause the library to **crash or potentially freeze** the system. Ensure the integrity of these files is maintained to avoid instability.

**Security and Access Rights**: If the access rights to the trace files are not properly configured, an attacker could exploit this vulnerability for a **denial-of-service (DoS) attack**. It is essential to set up correct permissions to prevent unauthorized access.

**Unencrypted Data**: The user is solely responsible for determining which information is traced and stored in these files. Please note that this information is **stored unencrypted**, so ensure that sensitive data is not included in the trace logs unless proper precautions are in place.

## Constrains

Due to the implementation, design decisions and compiler limitation there are some constrains with this tracing system.

- **It's is never possible to change the tracebuffer of a tracepoint** because the tracepoint is associated with the tracebuffer at compile-time.
  If you really want to do this, you may use `CLLTK_DYN_TRACEPOINT` but this could be magnitudes slower than `CLLTK_TRACEPOINT`.

- **A maximum of 10 arguments are supported**.
- **Format-string must be a string literal**.
- All arguments, pid, tid and timestamp together may not be bigger in size than UINT16_MAX - 8 bytes.

- To detect if a tracebuffer is defined you need an additional Macro, like:
  
  ```c
  #define My_Tracebuffer /* empty */
  CLLTK_TRACEBUFFER(My_Tracebuffer, <size>);

  #if defined(My_Tracebuffer)
    #message("now you could detect if tracebuffer is define");
  #endif
  ``` 


Tracing inside inline functions, function templates, and member functions
defined inside a class or struct body works as well; the tracepoint meta data
is discovered in a COMDAT-safe way.

## Build, Test and Packaging

You may use the repository with or without a container. To run any scripts, build, test, or package commands inside the recommended container, use: `./scripts/container.sh <your command + args>`. Alternatively, you can jump directly into the container with `./scripts/container.sh`.

It is also possible to cross compile with the container env by using of example: `CONTAINER_ARCH=arm64 ./scripts/container.sh`.

### Compiler requirements

The tracing library requires a recent compiler:

| Compiler | Minimum version |
| -------- | --------------- |
| GCC      | 14              |
| Clang    | 18              |

Older compilers reject the build with `impossible constraint in 'asm'` /
`invalid input constraint 'Ws'`: the tracepoint macros emit their metadata
sections via the `Ws` inline-asm constraint (a symbol reference with offset),
which is only supported from these versions onward. The `compiler-compat` CI
job builds and runs a minimal tracing example against every released GCC and
Clang from these floors upward on x86_64, plus a representative gcc/clang on
arm64 and s390x (big-endian) under QEMU.

### Build this repository

To build this repository for test purposes or development run:

```bash
./scripts/container.sh ./scripts/ci-cd/step_build.sh
```

Or using CMake presets directly:

```bash
cmake --preset unittests
cmake --build --preset unittests
```

### Run Tests

To run all tests (C++ and Python):

```bash
./scripts/container.sh ./scripts/ci-cd/step_test.sh
```

For C++ googletests are used covering internal functions and API functions. For Python, unittest is used covering tracing, decoding, and build validation.

### Run CI Locally

The CI pipeline is designed so that everything running on GitHub Actions can also be run locally. Each CI step is an independent script:

```bash
# Run the full CI pipeline (same as GitHub Actions)
./scripts/container.sh ./scripts/ci-cd/run_all.sh

# Or run individual steps:
./scripts/container.sh ./scripts/ci-cd/step_format.sh       # Format check
./scripts/container.sh ./scripts/ci-cd/step_build.sh        # Build (also: --preset unittests-nolto for the consumer-like non-LTO build)
./scripts/container.sh ./scripts/ci-cd/step_test.sh         # Tests
./scripts/container.sh ./scripts/ci-cd/step_memcheck.sh     # Valgrind memory check
./scripts/container.sh ./scripts/ci-cd/step_sanitizers.sh asan   # AddressSanitizer + LeakSanitizer
./scripts/container.sh ./scripts/ci-cd/step_sanitizers.sh ubsan  # UndefinedBehaviorSanitizer
./scripts/container.sh ./scripts/ci-cd/step_sanitizers.sh tsan   # ThreadSanitizer
./scripts/container.sh ./scripts/ci-cd/step_build_kernel_module.sh  # Kernel module build + link check
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --all  # Static analysis
./scripts/container.sh ./scripts/ci-cd/step_package.sh      # RPM packaging
```

### Static Analysis

Static analysis tools (clang-tidy, cppcheck) are integrated into the CI
pipeline. In CI, clang-tidy runs only on the lines changed by a pull request
(`--diff <base>`), while cppcheck runs on the whole tree; locally you can run
either against everything:

```bash
# Run all static analysis
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --all

# Run only clang-tidy
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --clang-tidy

# Run with auto-fix (clang-tidy only)
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --clang-tidy --fix

# Analyze specific component
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --clang-tidy --filter decoder_tool
```

You can also build with clang-tidy integrated into compilation:

```bash
cmake --preset static-analysis
cmake --build --preset static-analysis
```

### Package this repository

```bash
./scripts/container.sh ./scripts/ci-cd/step_package.sh

# Or directly:
cmake --workflow --preset rpms
```


### Minimal Build

By default all Features, Extensions and debugging tools are build. A minimal build, that only produces the dynamic and static library, can be run with:

```
mkdir build
cd build
cmake .. -DCLLTK_SNAPSHOT=OFF -DCLLTK_PYTHON_DECODER=OFF -DCLLTK_CPP_DECODER=OFF -DCLLTK_COMMAND_LINE_TOOL=OFF -DCLLTK_KERNEL_TRACING=OFF -DCLLTK_EXAMPLES=OFF -DCLLTK_TESTS=OFF
```

> [!IMPORTANT] Be aware that this disables the build of snapshot and decoding tool. Mixing versions and using this tools from older build is highly discouraged!

## Contributing
Have a lock in [CONTRIBUTING](./CONTRIBUTING.md)
