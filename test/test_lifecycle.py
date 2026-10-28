from pathlib import Path

import pytest

from support.daemon import Daemon
from support.workspace import Workspace


def test_sigterm_shuts_down_the_daemon_cleanly(daemon: Daemon) -> None:
    with daemon:
        daemon.assert_running()

    assert daemon.process is not None
    assert daemon.process.returncode == 0
    assert "daemon shutting down" in daemon.logs


def test_daemon_context_stops_the_process_when_the_body_raises(daemon: Daemon) -> None:
    with pytest.raises(RuntimeError, match="interrupted scenario"):
        with daemon:
            raise RuntimeError("interrupted scenario")

    assert daemon.process is not None
    assert daemon.process.returncode == 0


def test_workspace_context_removes_files_when_the_body_raises() -> None:
    root: Path | None = None
    with pytest.raises(RuntimeError, match="interrupted scenario"):
        with Workspace.temporary() as workspace:
            root = workspace.root
            workspace.write_source("notes.txt", "temporary content")
            raise RuntimeError("interrupted scenario")

    assert root is not None
    assert not root.exists()
