# AstraphosVC Roadmap

## Phase 1: Repository Foundation

Implemented: repository initialization, config system, repository discovery, object storage directories, CLI framework, logging, and error handling.

## Phase 2: Object Database

Implemented: SHA-1 hashing, zlib compression, blob/tree/commit/tag storage with integrity verification.

## Phase 3: Index

Implemented: Git-compatible v2 staging area, `add` and `status` commands, stat cache, change detection.

## Phase 4: Commit Engine

Planned: commit creation, history traversal, branch pointers, and detached HEAD support.

## Phase 4: Commit Engine

Planned: commit creation, history traversal, branch pointers, and detached HEAD support.

## Phase 5: Branching

Planned: branch creation, checkout, branch switching, and HEAD management.

## Phase 6: Merge Engine

Planned: fast-forward merges, three-way merge, conflict detection, and conflict markers.

## Phase 7: Diff Engine

Planned: file diffs, unified output, rename detection, and binary file handling.

## Phase 8: Remote Repositories

Planned: clone, fetch, pull, push, and transport abstractions.

## Phase 9: Packfiles

Planned: packfile reading, writing, delta compression, and efficient object transfer.

## Phase 10: Git Compatibility Layer

Planned: `.git/` layout, Git object IDs, Git refs, packfiles, config files, and remotes where technically feasible.

## Phase 11: Advanced Features

Planned: tags, stash, cherry-pick, rebase, bisect, worktrees, hooks, signing, partial clone, sparse checkout, and Git LFS compatibility where practical.

## Phase 12: Plugin Architecture

Planned: stable plugin API with explicit capability boundaries.
