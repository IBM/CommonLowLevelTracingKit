# Contributing
## Contributing In General
Our project welcomes external contributions. If you have an itch, please feel free to scratch it. Please ensure that you follow the [Code of Conduct](CODE_OF_CONDUCT.md) for any participation in the project.

To contribute code or documentation, please submit a [pull request](https://github.com/ibm/CommonLowLevelTracingKit/pulls).

A good way to familiarize yourself with the codebase and contribution process is
to look for and tackle low-hanging fruit in the [issue tracker](https://github.com/ibm/CommonLowLevelTracingKit/issues).
Before embarking on a more ambitious contribution, please quickly [get in touch](#communication) with us.

**Note: We appreciate your effort, and want to avoid a situation where a contribution
requires extensive rework (by you or by us), sits in backlog for a long time, or
cannot be accepted at all!**

### Proposing new features

If you would like to implement a new feature, please [raise an issue](https://github.com/ibm/CommonLowLevelTracingKit/issues)
before sending a pull request so the feature can be discussed. This is to avoid
you wasting your valuable time working on a feature that the project developers
are not interested in accepting into the code base.

### Fixing bugs

If you would like to fix a bug, please [raise an issue](https://github.com/ibm/CommonLowLevelTracingKit/issues) before sending a
pull request so it can be tracked.

### Merge approval

The project maintainers use LGTM (Looks Good To Me) in comments on the code
review to indicate acceptance. A change requires LGTMs from one of the
maintainers of each component affected.

For a list of the maintainers, see the [MAINTAINERS.md](MAINTAINERS.md) page.

## Communication
For feature requests, bug reports or technical discussions, please use the [issue tracker](https://github.com/ibm/CommonLowLevelTracingKit/issues).
Otherwise please contact one of the [maintainers](MAINTAINERS.md) directly for general questions or feedback.

## Testing

We encourage contributors to ensure that existing tests pass before submitting a pull request. 

### Running the Full CI Pipeline Locally

The CI pipeline is designed so that everything running on GitHub Actions can also be run locally:

```bash
# Run the full CI pipeline (same as GitHub Actions)
./scripts/container.sh ./scripts/ci-cd/run_all.sh
```

### Running Individual CI Steps

Each CI step is an independent script that can be run separately:

```bash
./scripts/container.sh ./scripts/ci-cd/step_format.sh           # Check code formatting
./scripts/container.sh ./scripts/ci-cd/step_build.sh            # Build the project
./scripts/container.sh ./scripts/ci-cd/step_test.sh             # Run all tests
./scripts/container.sh ./scripts/ci-cd/step_memcheck.sh         # Run Valgrind memory checks
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh  # Run static analysis
./scripts/container.sh ./scripts/ci-cd/step_package.sh          # Build RPM packages
```

It is highly appreciated if you implement unit-tests for new code.

## Code Quality

### Code Formatting

Clang Format is enforced for code formatting. Failure to adhere to this standard will result in a CI failure. 

```bash
# Check formatting (same as CI)
./scripts/container.sh ./scripts/ci-cd/step_format.sh

# Auto-format all files
./scripts/container.sh ./scripts/development_helper/format_everything.sh
```

### Static Analysis

We use clang-tidy and cppcheck for static analysis. While not currently blocking CI, we encourage fixing any issues found:

```bash
# Run all static analysis tools
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --all

# Run clang-tidy with auto-fix
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --clang-tidy --fix

# Analyze specific component
./scripts/container.sh ./scripts/ci-cd/step_static_analysis.sh --clang-tidy --filter decoder_tool
```

Configuration files:
- `.clang-tidy` - clang-tidy checks configuration
- `.clang-format` - Code formatting rules
- `.iwyu.imp` - Include-what-you-use mappings

### Directory structure
We are trying to stay close to _Canonical Project Structure_ as described [in this proposal](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1204r0.html).
All new code should follow the directory and naming conventions described here.

### Source code documentation
For documenting code, please use the [doxygen style](http://www.doxygen.nl/manual/docblocks.html) method 3.
For example to document a method:
```
/// Method for doing my stuff.
/// This method is doing my stuff by doing ....
myMethod()
...
```

## Copyright
Each source file must include a license header for the:

```
Copyright (c) <YEAR> <COPYRIGHT HOLDERS>
SPDX-License-Identifier: BSD-2-Clause-Patent
```

## Developer's Certificate of Origin

We have tried to make it as easy as possible to make contributions. This
applies to how we handle the legal aspects of contribution. We use the
same approach - the [Developer's Certificate of Origin 1.1 (DCO)](DCO1.1.txt) - that the Linux® Kernel [community](https://elinux.org/Developer_Certificate_Of_Origin)
uses to manage code contributions.

We simply ask that when submitting a patch for review, the developer
must include a sign-off statement in the commit message.

Here is an example Signed-off-by line, which indicates that the
submitter accepts the DCO:

```
Signed-off-by: John Doe <john.doe@example.com>
```

You can include this automatically when you commit a change to your
local git repository using the following command:

```
git commit -s
```
