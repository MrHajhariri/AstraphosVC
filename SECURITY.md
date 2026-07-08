# Security Policy

AstraphosVC treats repository data as untrusted input.

Security goals:

- Validate paths before writing repository data.
- Verify object integrity before trusting object contents.
- Reject malformed config, index, packfile, and protocol inputs.
- Avoid path traversal from trees, patches, archives, hooks, plugins, and remotes.
- Keep hooks and plugins opt-in with clear trust boundaries.

Report vulnerabilities privately to the maintainers before public disclosure.
