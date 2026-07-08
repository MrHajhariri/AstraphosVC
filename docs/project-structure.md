# Project Directory Structure

```text
AstraphosVC/
├── api/             Public aggregate headers
├── cli/             Command-line frontend
├── repository/      Repository creation, opening, discovery, and layout ownership
├── objects/         Planned object database implementation
├── refs/            Planned reference storage and validation
├── index/           Planned staging area
├── commits/         Planned commit creation and history traversal
├── branches/        Planned branch operations
├── tags/            Planned tag operations
├── merge/           Planned merge engine
├── diff/            Planned diff engine
├── patch/           Planned patch parser and applier
├── protocol/        Planned wire protocol support
├── transport/       Planned local, SSH, and HTTP transports
├── packfiles/       Planned packfile parser and writer
├── hashing/         Planned hash algorithm abstraction
├── compression/     Planned compression abstraction
├── config/          Implemented config parser and writer
├── hooks/           Planned hook runner
├── signing/         Planned signature support
├── security/        Planned validation and policy helpers
├── plugins/         Planned plugin runtime
├── utils/           Implemented error, logging, and filesystem helpers
├── tests/           Unit and integration tests
├── benchmarks/      Planned performance benchmarks
├── examples/        Planned example repositories and workflows
├── docs/            Specifications and guides
├── scripts/         Shell completions and developer scripts
└── tools/           Planned maintenance tools
```

Only `api/`, `cli/`, `repository/`, `config/`, `utils/`, `tests/`, and `scripts/` contain implemented Phase 1 behavior.
