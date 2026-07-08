# AstraphosVC

AstraphosVC is a modern distributed version control system written from scratch in C. It is part of the Astraphos ecosystem and uses `astraphosvc` as its primary binary and `.avc/` as its repository metadata directory.

Current status: Phase 5 is implemented. Phase 1 (repository init, config, discovery, CLI) + Phase 2 (SHA-1, zlib, object database) + Phase 3 (Git-compatible v2 index with `add` and `status`) + Phase 4 (commit engine, refs/HEAD, `commit -m`, `log`) + Phase 5 (branches, checkout, `branch`, `checkout`). Merge, packfiles, remotes, and Git compatibility are designed but not yet implemented.

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

CLI code calls repository APIs, repository APIs call filesystem and config APIs, and all failures propagate through `avc_status` plus `avc_error`. Phase 2 added SHA-1 hashing, zlib compression, and a content-addressed object database.

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
astraphosvc add <file>
astraphosvc status
astraphosvc commit -m "message"
astraphosvc log
astraphosvc branch
astraphosvc branch feature-x
astraphosvc checkout feature-x
```

Planned workflows such as `diff`, `clone`, `fetch`, and `push` are documented in `docs/cli-reference.md` but intentionally return an error until their phases are implemented.

## Compatibility Matrix

| Area | Status | Notes |
| --- | --- | --- |
| `.avc/` repositories | Implemented | Phase 1 repository creation and discovery. |
| Existing `.git/` repositories | Planned | Design documented; not implemented. |
| Object database | Implemented | SHA-1, zlib, flat hash-addressed storage, all 4 object types. |
| Git index | Implemented | Phase 3. |
| Commits & refs | Implemented | Phase 4. |
| Branches | Implemented | Phase 5. |
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
- SHA-1 hashing (RFC 3174, no external crypto)
- zlib compression/decompression
- Content-addressed object database (blob, tree, commit, tag)
- Atomic object writes and integrity verification
- Git-compatible v2 index (DIRC format)
- `astraphosvc add <path>` — stage files
- `astraphosvc status` — show staged/modified/deleted files
- Stat cache for fast status checks
- Commit engine (tree builder, commit creation, parent chain)
- Refs module (HEAD symbolic/detached, branch refs read/write/resolve)
- `astraphosvc commit -m <message>` — create commits
- `astraphosvc log` — display commit history
- `astraphosvc branch` — list branches
- `astraphosvc branch <name>` — create a branch at current HEAD
- `astraphosvc checkout <branch>` — switch to an existing branch
- Branch API (list, current, delete)
- unit and integration test targets

Planned:

- Merge, diff, remotes, packfiles, Git compatibility, hooks, signing, plugins.

## Roadmap

The detailed roadmap is in `ROADMAP.md`. Phase 3 added the index; Phase 4 added commits and refs; Phase 5 added branching and checkout.

## FAQ

Is AstraphosVC Git-compatible today?

Not yet. The object database uses a Git-compatible canonical format, but `.git/` repository opening is planned for Phase 10.

Why `.avc/` instead of `.git/`?

AstraphosVC owns its native metadata format. Git interoperability will be implemented through a compatibility layer rather than by pretending native repositories are Git repositories.

## Troubleshooting

`astraphosvc: command 'add' is not yet implemented` means the command is planned but not yet implemented.

If `make test` fails because no compiler exists, install a C17-capable compiler such as GCC or Clang.

## Contributing

Read `CONTRIBUTING.md` and `docs/coding-standards.md`. Each phase must compile, include tests, and document new on-disk data before implementation.
