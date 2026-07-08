# AstraphosVC Architecture Guide

AstraphosVC is organized as independently testable modules with documented public headers. Phase 1 implements `cli/`, `repository/`, `config/`, and `utils/`. Later phases add object storage, indexing, commits, branch operations, merges, diffs, transports, packfiles, security, signing, hooks, and plugins.

Core trade-off: native AstraphosVC repositories use `.avc/` so the project can evolve its own metadata safely. Git interoperability is implemented as an explicit compatibility layer instead of overloading native storage semantics.

Layering rules:

- CLI code may call public APIs but should not write repository files directly.
- Repository code owns `.avc/` layout creation and discovery.
- Object, index, ref, and commit modules will own their formats.
- Transport and protocol modules must treat remote data as untrusted.
- Utilities must remain dependency-light and reusable.

Phase 1 API surface:

- `repository/avc_repository.h`: initialize, open, discover, free repositories.
- `config/avc_config.h`: read, write, get, and set config values.
- `utils/avc_error.h`: structured status and diagnostic messages.
- `utils/avc_log.h`: environment-controlled logging.
- `utils/avc_fs.h`: filesystem helpers used by core modules.
