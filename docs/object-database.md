# Object Database Specification

Status: designed, not implemented in Phase 1.

AstraphosVC will store content-addressed objects below `.avc/objects/`. Object IDs will be computed over a canonical object byte stream:

```text
<type> SP <size> NUL <payload>
```

Object types:

- `blob`: raw file bytes.
- `tree`: directory entries with mode, name, and object ID.
- `commit`: tree ID, parent IDs, author, committer, timestamp, and message.
- `tag`: annotated tag target, type, tag name, tagger, and message.

Planned loose-object path:

```text
.avc/objects/<type>/<algorithm>/<first-two-hex>/<remaining-hex>
```

The algorithm directory allows SHA-1 compatibility and stronger native hashes such as SHA-256 without guessing object ID length.

Storage rules:

- Write objects to `.avc/objects/tmp/` first.
- Verify the computed object ID before publishing.
- Rename atomically into the final object path when supported by the platform.
- Reject object types, sizes, and encodings that do not match the specification.

Compression is planned for Phase 2 and must preserve integrity verification over the uncompressed canonical object stream.
