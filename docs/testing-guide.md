# Testing Guide

Phase 1 tests include:

- Config round-trip unit coverage.
- Repository initialization unit coverage.
- Repository discovery unit coverage.
- CLI version, help, and init integration checks.

Run with:

```sh
make test
```

or:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Future phases must add compatibility tests against Git-created repositories before claiming Git compatibility.
