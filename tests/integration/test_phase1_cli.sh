#!/usr/bin/env sh
set -eu

BIN="$1"
TMPDIR="${TMPDIR:-/tmp}/astraphosvc-cli-$$"
mkdir -p "$TMPDIR"
trap 'rm -rf "$TMPDIR"' EXIT INT TERM

"$BIN" version | grep 'AstraphosVC 0.1.0' >/dev/null
"$BIN" init "$TMPDIR/repo" >/dev/null
test -d "$TMPDIR/repo/.avc/objects/blobs"
test -f "$TMPDIR/repo/.avc/config"
test "$(cat "$TMPDIR/repo/.avc/HEAD")" = 'ref: refs/heads/main'
