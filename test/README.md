# Daemon functional tests

Suites live directly in `test/`; shared code lives in `test/support/`.
Pytest fixtures provide an isolated `workspace` and a prepared `daemon` for each
test. They use the real daemon binary, not a mock. The CLI is not invoked.

## Running

From the repository root:

```sh
make test-functional
```

This builds the Debug binaries, synchronizes the locked Python dependencies into
`.venv`, checks all test code with strict Pyright, and runs pytest. `make test`
continues to run the C++ unit tests. `make test-types` runs Python type checking
alone, and `make test-setup` only prepares the Python environment.

For subsequent runs against already-built binaries:

```sh
bin/test-functional -q
bin/test-functional test/test_plans.py -vv
bin/test-functional -k scheduled
bin/test-functional --daemon-binary build/Release/src/daemon/anchord
bin/test-functional --daemon-timeout 20
```

The default executable is `build/Debug/src/daemon/anchord`. The fast runner does
not rebuild C++; rebuild after changing daemon code. Every runner invocation
checks types before running tests.

## Writing a scenario

Arrange files and plans before entering the daemon context:

```python
def test_backup_contents(daemon: Daemon, workspace: Workspace) -> None:
    workspace.write_source("notes.txt", "important notes")
    workspace.write_plan("daily", archive="zip")

    with daemon:
        daemon.trigger_backup("daily")
        daemon.wait_for_run("daily")

    snapshots = workspace.snapshots("daily")
    assert len(snapshots) == 1
    assert_snapshot_files(snapshots[0], {"notes.txt": b"important notes"})
    workspace.assert_runs_cleaned()
```

Import `Daemon`, `Workspace`, and `assert_snapshot_files` from their modules in
`support`. Fixtures are declared in `conftest.py`; scenario functions remain
ordinary, typed pytest tests.

`with daemon:` starts the subprocess and waits for its readiness log and a real
protocol response. Exiting sends SIGTERM and waits for a clean exit. A stuck
process is killed and reaped, and the test fails. Failed startup and exceptions
inside the context also clean up the process. Each daemon instance is started
once. The workspace fixture uses `Workspace.temporary()` to remove all test files
afterward, on both success and failure. No user configuration or plans are used.

The daemon receives a fresh ephemeral TCP port and runs with `TZ=UTC`.
Connections and polling have timeouts; polling uses monotonic deadlines rather
than assuming completion after a fixed sleep. The default operation timeout is
10 seconds. Tests wait for a terminal run before checking results. Leaving the
daemon context drains work before assertions about run-workspace cleanup.

When retriggering a backup, pass the previous run UUID as `after_uuid` to
`wait_for_run` so an earlier completed run cannot satisfy the wait. `DaemonError`
from `support.protocol` exposes the daemon's error code and message for negative
scenarios. `daemon.client.request` supports new protocol messages without adding
transport code to tests.

Snapshot checks inspect files or archive members without extracting archives.
Daemon output goes to a file to avoid subprocess pipe backpressure. On failure,
pytest includes the daemon log; its contents remain available after temporary
files have been cleaned up. Extend support helpers when lifecycle, protocol, or
filesystem setup would otherwise be repeated across suites.
