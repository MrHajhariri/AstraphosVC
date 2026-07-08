# Index Format

Status: planned for Phase 3.

The index will be a binary staging area optimized for fast status checks. It will include a signature, version, entry count, sorted path entries, stat cache data, object IDs, mode bits, and extension blocks.

Git compatibility is a goal for index semantics, but the exact native `.avc/index` format is not implemented in Phase 1.
