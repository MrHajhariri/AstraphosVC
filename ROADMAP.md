# AstraphosVC Roadmap

## Phase 1: Repository Foundation

Implemented: repository initialization, config system, repository discovery, object storage directories, CLI framework, logging, and error handling.

## Phase 2: Object Database

Implemented: SHA-1 hashing, zlib compression, blob/tree/commit/tag storage with integrity verification.

## Phase 3: Index

Implemented: Git-compatible v2 staging area, `add` and `status` commands, stat cache, change detection.

## Phase 4: Commit Engine

Implemented: commit creation, history traversal, branch pointers, and detached HEAD support.

## Phase 5: Branching

Implemented: branch creation, checkout, branch switching, and HEAD management.

## Phase 6: Merge Engine

Implemented: fast-forward merges, three-way merge, conflict detection, and conflict markers.

## Phase 7: Diff Engine

Implemented: file diffs, unified output, rename detection, and binary file handling.

## Phase 8: Remote Repositories

Implemented: clone, fetch, pull, push, and transport abstractions.

## Phase 9: Packfiles

Implemented: packfile reading, writing, and efficient object transfer.

## Phase 10: Git Compatibility Layer

Implemented: `.git/` repo detection, loose object I/O, ref reading (loose + packed-refs), config with subsection support, and Git packfile parser (PACK v2/v3 idx + pack with OFS_DELTA/REF_DELTA resolution).

## Phase 11: Advanced Features

Planned: tags, stash, cherry-pick, rebase, bisect, worktrees, hooks, signing, partial clone, sparse checkout, and Git LFS compatibility where practical.

## Phase 12: Plugin Architecture

Planned: stable plugin API with explicit capability boundaries.
