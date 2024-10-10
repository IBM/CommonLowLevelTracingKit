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
8. View your traces in `output.csv`

## Requirements

This project requires:

- cmake (>=3.18)
- gcc (>=10.0)
- g++ (>=10.0)

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


- **It's not possible** to tracing inside a member function defined inside a class or struct definition, like the following:

  ```c++
  // header
  struct A
  {
    void foo(void)
    {
      CLLTK_TRACEPOINT(My_TB, "it is not possible to trace here"); // fill fail at link-time
    }
  }
  ```

  To still be able to trace in change this to:

  ```c++
  // header
  struct A
  {
    void foo(void);
  }

  // source
  void A::foo(void)
  {
    CLLTK_TRACEPOINT(My_TB, "now it is possible to trace here");
  }
  ```

## Build, Test and Packaging
You may use the repository with or without a container. To run any scripts, build, test, or package commands inside the recommended container, use: `./scripts/container.sh <your command + args>`. Alternatively, you can jump directly into the container with `./scripts/container.sh`.

It is also possible to cross compile with the container env by using of example: `CONTAINER_ARCH=arm64 ./scripts/container.sh`.

### Build this repository

To build this repository for test purposes or development run:

```bash
./scripts/ci-cd/build_userspace.shs
```

### Tests tracing standalone

To run all test build the whole project with cmake and than run:

```bash
./scripts/ci-cd/test_userspace.shs
```

This will run all c++ and python tests.

For c++ googletests are used and there are covering many internal function and some api functions. To test internal function access to these function is required. There for a static linked version of `clltk_tracing` is used.
 
For python test unittest is used and these test covers the tracing and decoding of the examples and building test cases with valid and invalid use of the tracing api.

### Run CI locally
```bash
./scripts/ci-cd/run.sh
```

### package this repository
```bash
cmake --workflow --preset rpms
```

## Contributing
Have a lock in [CONTRIBUTING](./CONTRIBUTING.md)
