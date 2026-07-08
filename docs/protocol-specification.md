# Protocol Specification

Status: planned for Phase 8.

Transport will be separated from protocol framing. Initial targets are local filesystem remotes and Git-compatible smart protocol support where practical.

Protocol implementations must validate remote advertisements, object IDs, packfile lengths, and reference names before changing local state.
