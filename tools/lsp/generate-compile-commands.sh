#!/usr/bin/env bash
set -Eeuo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

if ! command -v bear >/dev/null 2>&1; then
    printf '%s\n' "error: bear is required to generate compile_commands.json" >&2
    printf '%s\n' "install it with your platform package manager, then rerun this script" >&2
    exit 1
fi

# -B ensures Bear observes every translation unit even when binaries already exist.
# Generate into a temporary file so a failed Bear invocation cannot destroy a
# previously usable database. The database is intentionally local because it
# contains machine-specific paths and is not a portable source artifact.
tmp_db=$(mktemp "${TMPDIR:-/tmp}/cadc-compile-commands.XXXXXX.json")
trap 'rm -f "$tmp_db"' EXIT

if ! bear --output "$tmp_db" -- make -B all; then
    printf '%s\n' "error: Bear could not generate compile_commands.json; existing database was preserved" >&2
    exit 1
fi

mv "$tmp_db" compile_commands.json

printf '%s\n' "Generated compile_commands.json for clangd and Helix."
