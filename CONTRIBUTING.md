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
We encourage contributors to ensure that existing tests pass by running `./scripts/container.sh ./scripts/ci-cd/run.sh` before
submitting a pull request. Also it is highly appreciated if you implement unit-tests for new code.

## Coding style guidelines
Clang Format is enforced for code formatting. Failure to adhere to this standard will result in a CI (Continuous Integration) failure. You can format everything by running `./scripts/container.sh ./scripts/development_helper/format_everything.sh`, which uses a containerized environment to ensure the correct version of Clang Format is applied.

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
