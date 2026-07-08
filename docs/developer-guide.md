# Developer Guide

Start with `make test` or the CMake workflow in `README.md`.

Module rules:

- Public APIs live in documented headers.
- Implementations should avoid unnecessary global state.
- Modules should be independently testable.
- On-disk format changes require documentation updates first.

Phase development order must follow `ROADMAP.md`.
