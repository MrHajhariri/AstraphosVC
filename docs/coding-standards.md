# Coding Standards

- Use C17-compatible code unless the project minimum is explicitly raised.
- Keep ownership clear: the function allocating memory documents or implies the function responsible for freeing it.
- Return `avc_status` for recoverable failures and fill `avc_error` with diagnostic context.
- Avoid hidden writes from lower-level utility functions.
- Prefer small functions when they clarify ownership or error handling.
- Do not copy source code from Git, Mercurial, Fossil, Jujutsu, or other VCS projects.
