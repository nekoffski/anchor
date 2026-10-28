import pytest

from support.daemon import Daemon
from support.snapshot import assert_snapshot_files
from support.workspace import Workspace


@pytest.mark.parametrize("archive", ["directory", "zip", "tgz"])
def test_manual_backup_stores_selected_contents_and_cleans_its_workspace(
    daemon: Daemon, workspace: Workspace, archive: str
) -> None:
    workspace.write_source("notes.txt", "selected note")
    workspace.write_source("assets/nested/data.bin", b"\x00\x01\x7f\xff")
    workspace.write_source("ignored.txt", "not selected")
    workspace.write_plan("daily", archive=archive, includes=("notes.txt", "assets"))
    with daemon:
        daemon.trigger_backup("daily")
        run = daemon.wait_for_run("daily")

    assert run["state"] == "completed"
    snapshots = workspace.snapshots("daily")
    assert len(snapshots) == 1
    assert_snapshot_files(
        snapshots[0],
        {"notes.txt": b"selected note", "assets/nested/data.bin": b"\x00\x01\x7f\xff"},
    )
    workspace.assert_runs_cleaned()


def test_failed_backup_reports_the_error_and_cleans_its_workspace(
    daemon: Daemon, workspace: Workspace
) -> None:
    workspace.write_source("notes.txt", "cannot store this")
    workspace.write_plan("daily", destination=workspace.root / "missing/destination")
    with daemon:
        daemon.trigger_backup("daily")
        run = daemon.wait_for_run("daily", state="failed")

    assert "Failed to copy" in run["error"]["message"]
    assert workspace.snapshots("daily") == []
    workspace.assert_runs_cleaned()
