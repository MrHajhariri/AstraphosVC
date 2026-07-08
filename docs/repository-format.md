# Repository Format

Native AstraphosVC repositories store metadata under `.avc/`.

Phase 1 layout:

```text
.avc/
├── HEAD
├── config
├── logs/
├── objects/
│   ├── blobs/
│   ├── commits/
│   ├── tags/
│   ├── trees/
│   └── tmp/
└── refs/
    ├── heads/
    └── tags/
```

`HEAD` contains either a symbolic reference or, in a future phase, a detached object ID. Phase 1 writes:

```text
ref: refs/heads/main
```

`config` is an INI-style text file. Phase 1 writes:

```ini
[core]
repositoryformatversion = 1
filemode = true
bare = false
primarymetadata = .avc
```

Format version `1` means the repository uses the documented `.avc/` layout. Future incompatible changes must increase the version and define migration behavior.
