# Git Compatibility Guide

Status: Phase 10 is implemented. The compat module (`compat/avc_git_compat.h/c`, `compat/avc_git_pack.h/c`) provides Git repository detection, loose object I/O, ref reading (loose + packed-refs), config parsing with subsection support, and Git packfile parsing (PACK v2/v3 with OFS_DELTA/REF_DELTA).

Strategy:

- Keep `.avc/` as the native repository metadata directory.
- Add a compatibility layer that can open `.git/` repositories explicitly.
- Support Git object canonicalization for blob, tree, commit, and tag objects.
- Read Git refs and configs through dedicated parsers.
- Implement packfile parsing before claiming clone, fetch, pull, or push compatibility.

Compatibility claims require tests against repositories created by Git. Partial compatibility must document unsupported cases.

## What works

- `.git/` repo detection via `avc_git_is_git_repo()`
- `.git/` repo opening via `avc_git_repo_open()`
- Loose object reading from `.git/objects/`
- Loose and packed refs reading (`refs/heads/*`, `packed-refs`)
- Symbolic ref resolution (ref: refs/heads/...)
- Detached HEAD resolution
- Git config files with subsection headers (`[remote "origin"]`)
- Git packfile and idx parsing according to the v2 format
- OFS_DELTA and REF_DELTA object resolution
