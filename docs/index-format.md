# Index Format

Status: implemented in Phase 3.

The index (`/avc/index`) is a binary staging area with Git-compatible v2 format.

## Header (12 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Signature: `DIRC` (0x44495243) |
| 4 | 4 | Version (2) |
| 8 | 4 | Number of index entries |

## Entry (variable, 62 bytes + path + padding)

All integer fields are big-endian.

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | ctime seconds |
| 4 | 4 | ctime nanoseconds (stored as zero) |
| 8 | 4 | mtime seconds |
| 12 | 4 | mtime nanoseconds (stored as zero) |
| 16 | 4 | dev |
| 20 | 4 | ino |
| 24 | 4 | mode (e.g. 0100644 for regular file) |
| 28 | 4 | uid |
| 32 | 4 | gid |
| 36 | 4 | file size |
| 40 | 20 | object ID (SHA-1) |
| 60 | 2 | flags (bit 15: assume valid, bits 0-11: path length) |
| 62 | variable | NUL-terminated path |
| after | padding | NUL padding to 8-byte alignment |

## Trailer (20 bytes)

SHA-1 checksum of all preceding bytes (header + entries + padding).

## Operations

- `add`: reads file content, creates blob object, adds/updates entry in sorted index.
- `status`: compares index entries against working tree stat cache, reports staged, modified (hash changed), and deleted files.
