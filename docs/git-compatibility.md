# Git Compatibility Guide

Status: planned; no Git repository compatibility is implemented in Phase 1.

Strategy:

- Keep `.avc/` as the native repository metadata directory.
- Add a compatibility layer that can open `.git/` repositories explicitly.
- Support Git object canonicalization for blob, tree, commit, and tag objects.
- Read Git refs and configs through dedicated parsers.
- Implement packfile parsing before claiming clone, fetch, pull, or push compatibility.

Compatibility claims require tests against repositories created by Git. Partial compatibility must document unsupported cases.
