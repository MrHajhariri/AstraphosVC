# Changelog

## 0.1.0-phase1

- Added Phase 1 architecture, repository format, object database design, and Git compatibility strategy documentation.
- Implemented `astraphosvc init`, `astraphosvc version`, and `astraphosvc help`.
- Added `.avc/` repository initialization with config, HEAD, refs, object directories, and logs.
- Added config, repository discovery, logging, filesystem, and error handling APIs.
- Added Make, CMake, CI, shell completions, and Phase 1 tests.

## Phase 2

- Implemented SHA-1 hashing module (RFC 3174, zero external crypto dependencies).
- Implemented zlib compression/decompression wrapper.
- Implemented object database (ODB) with Git-compatible canonical format.
- Added flat hash-addressed object storage layout: `.avc/objects/XX/YYYY...`.
- Added object type support: blob, tree, commit, tag.
- Added atomic object writes via temp file + rename.
- Added object existence checks and integrity verification.
- Added Phase 2 unit tests covering SHA-1 vectors, OID round-trips, compression round-trips, and object read/write for all types.
- Updated repository init to produce flat object layout.
- Added `objects_path` field to repository struct.
