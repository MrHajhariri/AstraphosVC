# CLI Reference

Implemented:

```sh
astraphosvc init [path]
astraphosvc add <path>...
astraphosvc status
astraphosvc version
astraphosvc help
```

- `init [path]` — creates a native `.avc/` repository (default: current directory).
- `add <path>...` — stages file contents into the object database and records them in the index.
- `status` — shows staged files, modified files (stat/size/hash change), and deleted files.
- `version` — prints version information.
- `help` — prints available commands.

Planned commands:

```sh
astraphosvc clone
astraphosvc commit
astraphosvc diff
astraphosvc log
astraphosvc branch
astraphosvc switch
astraphosvc checkout
astraphosvc merge
astraphosvc rebase
astraphosvc tag
astraphosvc stash
astraphosvc push
astraphosvc pull
astraphosvc fetch
astraphosvc remote
astraphosvc gc
astraphosvc fsck
astraphosvc doctor
```

Planned commands intentionally return an error until implemented and tested.
