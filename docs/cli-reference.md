# CLI Reference

Implemented in Phase 1:

```sh
astraphosvc init [path]
astraphosvc version
astraphosvc help
```

`init` creates a native `.avc/` repository. If no path is provided, the current directory is used.

Planned commands:

```sh
astraphosvc clone
astraphosvc add
astraphosvc commit
astraphosvc status
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
