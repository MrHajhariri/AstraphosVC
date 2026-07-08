# Packfile Format

Status: planned for Phase 9.

Packfiles will group objects for efficient transfer and storage. Git packfile reading is a compatibility target, but native pack writing may use versioned AstraphosVC indexes if they are documented before implementation.

Security requirements:

- Bound object sizes and delta chain depth.
- Validate checksums before trusting contents.
- Reject malformed offsets and cyclic deltas.
