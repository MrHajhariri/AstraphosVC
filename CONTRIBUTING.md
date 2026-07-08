# Contributing to AstraphosVC

AstraphosVC changes must be small, documented, tested, and honest about compatibility.

Requirements:

- Document public APIs before implementation.
- Document on-disk formats before writing data in that format.
- Add unit tests and integration tests for every phase.
- Do not claim Git compatibility until the compatible behavior is implemented and tested.
- Keep global state minimal and localize ownership.
- Run `make test` or the equivalent CMake test flow before submitting changes.

Code should be C17-compatible unless a phase explicitly raises the minimum after build support is updated.
