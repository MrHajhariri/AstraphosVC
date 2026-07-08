# Object Database Specification

Status: implemented in Phase 2.

AstraphosVC stores content-addressed objects below `.avc/objects/`. Object IDs are computed over a canonical object byte stream:

```text
<type> SP <size> NUL <payload>
```

Object types:

- `blob`: raw file bytes (e.g. file content).
- `tree`: directory entries with mode, name, and object ID.
- `commit`: tree ID, parent IDs, author, committer, timestamp, and message.
- `tag`: annotated tag target, type, tag name, tagger, and message.

## Storage Path

Loose objects use the Git-compatible flat layout:

```text
.avc/objects/<first-two-hex>/<remaining-38-hex>
```

Example for SHA-1 object `a9993e364706816aba3e25717850c26c9cd0d89d`:

```text
.avc/objects/a9/993e364706816aba3e25717850c26c9cd0d89d
```

The object type is embedded in the canonical content, not in the path. This provides content-addressable deduplication: identical content produces the same hash and only one copy is stored.

## Hash Algorithm

SHA-1 is the initial hash (20 bytes, 40 hex chars). The hashing API in `hashing/avc_hash.h` is designed for SHA-256 to be added later without changing the ODB API.

## Object Storage Format

Each object file on disk contains:

1. 4-byte big-endian uncompressed size prefix
2. zlib-deflated canonical object bytes

## Write Procedure

1. Build canonical bytes: `type SP decimal-size NUL payload`
2. Hash canonical bytes with SHA-1 → OID
3. Compress canonical bytes with zlib
4. Write to `.avc/objects/tmp/<random>` temp file
5. Create parent directory `.avc/objects/XX/` if needed
6. `rename()` temp file to `.avc/objects/XX/YYYY...`

## Read Procedure

1. Compute path from OID: `.avc/objects/XX/YYYY...`
2. Read compressed file from disk
3. Read 4-byte size prefix, decompress with zlib
4. Parse header: `<type> SP <size> NUL`
5. Verify payload byte count matches declared size
6. Return type, payload, and size

## Integrity

- Object content is verified by hash before the write completes.
- The on-disk path is derived from the hash; a file placed at path `XX/YYYY` is guaranteed by construction to have OID `XXYYYY...`.
- Decompression verifies the zlib checksum.
- Header parsing rejects unknown object types and size mismatches.
