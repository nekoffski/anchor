# Anchor

Anchor is a C++23 backup engine with a command-line client and a background
daemon. It uses YAML manifests to select files and directories, build snapshots,
and copy them to a backup destination.

The project supports directory, ZIP, and gzip-compressed tar (TGZ) snapshots
with local storage.

## Build

Supported platforms: macOS and Linux.

Install these prerequisites and make sure they are available in `PATH`:

- Python 3 and Make
- CMake 3.23 or newer
- Conan 2 for dependency management
- A C++23 compiler: GCC 14+, Clang 18+, or Apple Clang 17+

```sh
make bootstrap  # select a compiler and create the local Conan profile
make build      # install dependencies and build Release binaries
```

Run `make bootstrap` once before the first build. It saves the compiler settings
in `.anchor/bootstrap.profile`.

Binaries are written to `build/Release/src/cli/anchor` and
`build/Release/src/daemon/anchord`.

For a Debug build or to run the unit tests:

```sh
make build-debug
make test         # build Debug and run the unit tests
```

Backup execution and scheduling are described in [docs/scheduling.md](docs/scheduling.md).

## Functional tests

The Python suite starts the real daemon in isolated temporary workspaces and
talks directly to its JSON/TCP interface. It covers basic plans, reloads,
directory/ZIP/TGZ backups, scheduling, errors, and shutdown. CLI functional tests
are outside this suite.

Install [uv](https://docs.astral.sh/uv/), then run:

```sh
make test-setup       # create .venv and install locked test dependencies
make test-types       # strict Pyright checks, using the Pylance configuration
make test-functional  # build Debug binaries, check Python types, run pytest
```

After building, use the faster runner to select suites or tests:

```sh
bin/test-functional -q
bin/test-functional test/test_backups.py -vv
bin/test-functional -k reload
```

Tests require Python 3.11 or newer. Pyright uses Node.js when available on
`PATH`; its Python launcher can provision a runtime otherwise. The `.venv`
interpreter and strict type-checking settings are declared in `pyproject.toml`.
Select `.venv/bin/python` in your editor for Pylance to resolve the same dependencies.
See [test/README.md](test/README.md) for fixture usage and binary overrides.

## CLI

With the daemon running:

```sh
anchor backup list
anchor backup trigger personal
anchor backup reload
```

`list` shows each backup's enabled flag, latest run state, run ID, and failure
details when present. A successful `trigger` acknowledges acceptance; use `list`
to check execution. `reload` adds or updates valid plans from the configured
directory, preserving enabled state and run status. Deleted or invalid plans
keep their previously loaded definitions. If any plan fails to load, valid
plans still update and the command reports an error.

Use `--port PORT` at any command level to select the local daemon port, and
`--help` for command options. The existing `backup trigger --name personal`
spelling is also supported. The CLI's command structure is described in
[docs/cli.md](docs/cli.md).

## Example backup manifest

Save this as `personal.yaml` in the directory configured by the daemon's
`paths.backup_plans` setting. Both `.yaml` and `.yml` files are supported; the
filename without its extension becomes the backup name (`personal`).

```yaml
settings:
  archive: directory  # directory, zip, or tgz
  destination: local://$HOME/backups/anchor
  cron: '0 0 2 * * *'  # optional: daily at 02:00 in the daemon's local timezone

sources:
  - root: $HOME
    includes:
      - Documents/*.pdf
      - Documents/notes.txt
      - Pictures
```

This selects PDF files directly inside `Documents`, the `notes.txt` file, and
the entire `Pictures` directory. Include paths and glob patterns are relative
to each source's `root`; selected directories are copied recursively. Relative
paths are preserved in the snapshot, such as `Documents/notes.txt`.

`$HOME` is expanded in source roots and local destination paths. The destination
receives a separate directory or archive file for each snapshot. Create the
destination directory before running a backup.

Use `archive: zip` or `archive: tgz` to store a compressed archive instead of a
directory. Archive members retain their paths relative to each source root.
Selected directories include their contents recursively; permissions,
modification times, and symbolic links are preserved. Archives use `.zip` and
`.tgz` filenames respectively. Symbolic links are stored as links without
following their targets.

Omit `cron` for a manual-only plan. Scheduled occurrences are skipped while the
backup is queued or running, and missed times are not replayed after downtime
or sleep. Plans are loaded at startup; run `anchor backup reload` after editing
or adding a plan.
