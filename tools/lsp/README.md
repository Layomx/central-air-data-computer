# Helix and clangd

This project uses `clangd` for C language intelligence in Helix. The repository
contains `.clangd` and a generator for the compilation database, but the
generated `compile_commands.json` is intentionally local and ignored by Git.

## Setup

Install `clangd`, `make`, `gcc`, and Bear. From the repository root, run:

```bash
./tools/lsp/generate-compile-commands.sh
```

Then restart the language server in Helix with:

```text
:lsp-restart
```

The compilation database is authoritative for per-file flags. This matters here
because the application uses `-I.` while test targets use `-Isrc`.

## Diagnostics

Check a translation unit outside Helix with:

```bash
clangd --check=src/airdata.c
clangd --check=src/cadc.c
clangd --check=main.c
```

If navigation still fails, confirm that `compile_commands.json` exists at the
repository root and that Helix has selected `clangd` as the C language server.
