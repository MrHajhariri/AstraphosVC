# Merge Algorithm

Status: planned for Phase 6.

AstraphosVC will implement fast-forward detection first. Non-fast-forward merges will use merge-base selection and a three-way content merge. Conflict markers will be written only after paths are validated to avoid path traversal or accidental writes outside the worktree.

Correctness requirement: merge must never silently discard changes. Ambiguous rename and directory/file conflicts must be reported explicitly.
