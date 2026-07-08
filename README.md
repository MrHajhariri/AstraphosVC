# AstraphosVC

AstraphosVC is a modern distributed version control system written from scratch in C. It is part of the Astraphos ecosystem and uses `astraphosvc` as its primary binary and `.avc/` as its repository metadata directory.

Current status: Phase 1 is implemented. Repository initialization, repository discovery, configuration parsing and writing, logging, error handling, and the `init`, `version`, and `help` commands are present. Object storage, index operations, commits, packfiles, remotes, and Git compatibility are designed but not implemented yet.

## Goals

- Build a production-quality DVCS with modular internals.
- Preserve correctness before performance optimization.
- Support Git-compatible repositories wherever technically feasible.
- Document on-disk formats before implementing them.
- Keep implemented, experimental, and planned features clearly separated.

## Architecture

```text
             +-----------------------+
             | cli/ astraphosvc      |
             +-----------+-----------+
                         |
                         v
 +-----------+   +-------+--------+   +-----------+
 | config/   |<->| repository/    |<->| refs/     |
 +-----------+   +-------+--------+   +-----------+
                         |
                         v
       +-----------------+-----------------+
       | objects/ hashing/ compression/    |
       +-----------------+-----------------+
                         |
                         v
       +-----------------+-----------------+
       | index/ commits/ diff/ merge/      |
       +-----------------+-----------------+
                         |
                         v
       +-----------------+-----------------+
       | protocol/ transport/ packfiles/   |
       +-----------------------------------+
```

Phase 1 keeps the core small: CLI code calls repository APIs, repository APIs call filesystem and config APIs, and all failures propagate through `avc_status` plus `avc_error`.

## Build

Preferred C standard: C23. Minimum supported standard for Phase 1: C17.

Using Make:

```sh
make test
```

Using CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Installation

```sh
make install PREFIX=/usr/local
```

Shell completions are in `scripts/` for Bash, Zsh, and Fish.

## Quick Start

```sh
astraphosvc init my-repo
cd my-repo
astraphosvc version
astraphosvc help
```

This creates `my-repo/.avc/` with `HEAD`, `config`, object directories, refs, and logs.

## Example Workflows

Implemented today:

```sh
astraphosvc init
astraphosvc help
astraphosvc version
```

Planned workflows such as `add`, `commit`, `status`, `diff`, `clone`, `fetch`, and `push` are documented in `docs/cli-reference.md` but intentionally return an error until their phases are implemented.

## Compatibility Matrix

| Area | Status | Notes |
| --- | --- | --- |
| `.avc/` repositories | Implemented | Phase 1 repository creation and discovery. |
| Existing `.git/` repositories | Planned | Design documented; not implemented. |
| Git loose objects | Planned | Object format selected for future SHA-1 and SHA-256 compatibility. |
| Git index | Planned | Phase 3 target. |
| Git packfiles | Planned | Phase 9 target. |
| Git protocol | Planned | Phase 8 and Phase 10 target. |

## Current Implementation Status

Implemented:

- `astraphosvc init [path]`
- `astraphosvc version`
- `astraphosvc help`
- `.avc/` repository metadata creation
- config load/write for simple INI-style files
- upward repository discovery API
- unit and integration test targets

Planned:

- Object database, index, commits, branches, merge, diff, remotes, packfiles, Git compatibility, hooks, signing, plugins.

## Roadmap

The detailed roadmap is in `ROADMAP.md`. The next phase is the object database: blob, tree, commit, and tag storage with compression and integrity verification.

## FAQ

Is AstraphosVC Git-compatible today?

No. Phase 1 does not open `.git/` repositories. Git compatibility is a design goal and is documented as planned work.

Why `.avc/` instead of `.git/`?

AstraphosVC owns its native metadata format. Git interoperability will be implemented through a compatibility layer rather than by pretending native repositories are Git repositories.

## Troubleshooting

`astraphosvc: command 'status' is not implemented in Phase 1` means the command is planned but not yet implemented.

If `make test` fails because no compiler exists, install a C17-capable compiler such as GCC or Clang.

## Contributing

Read `CONTRIBUTING.md` and `docs/coding-standards.md`. Each phase must compile, include tests, and document new on-disk data before implementation.
