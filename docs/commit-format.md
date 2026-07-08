# Commit Format

Status: planned for Phase 2 and Phase 4.

Commit payloads will be canonical UTF-8 text with LF line endings:

```text
tree <object-id>
parent <object-id>
author <name> <email> <timestamp> <timezone>
committer <name> <email> <timestamp> <timezone>

<message>
```

Multiple `parent` lines represent merge commits. Signed commits will be added only after signing and canonicalization rules are specified.
