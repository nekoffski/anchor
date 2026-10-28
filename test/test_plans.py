import pytest

from support.daemon import Daemon
from support.protocol import DaemonError
from support.snapshot import assert_snapshot_files
from support.workspace import Workspace


def test_daemon_starts_with_an_empty_plan_directory(daemon: Daemon) -> None:
    with daemon:
        assert daemon.list_backups() == []


def test_daemon_loads_plans_from_disk_at_startup(
    daemon: Daemon, workspace: Workspace
) -> None:
    workspace.write_plan("daily")
    with daemon:
        assert daemon.list_backups() == [{"plan": {"name": "daily"}, "enabled": True}]


def test_reload_adds_a_plan_without_restarting_the_daemon(
    daemon: Daemon, workspace: Workspace
) -> None:
    with daemon:
        workspace.write_plan("added")

        daemon.reload_plans()

        assert daemon.backup("added") == {"plan": {"name": "added"}, "enabled": True}


def test_reload_updates_the_plan_used_by_the_next_run(
    daemon: Daemon, workspace: Workspace
) -> None:
    workspace.write_source("notes.txt", "first destination")
    workspace.write_plan("daily")
    with daemon:
        daemon.trigger_backup("daily")
        first_run = daemon.wait_for_run("daily")

        new_destination = workspace.root / "new-destination"
        new_destination.mkdir()
        workspace.write_source("notes.txt", "updated plan")
        workspace.write_plan("daily", destination=new_destination)
        daemon.reload_plans()
        daemon.trigger_backup("daily")
        second_run = daemon.wait_for_run("daily", after_uuid=first_run["uuid"])

    assert second_run["uuid"] != first_run["uuid"]
    assert len(workspace.snapshots("daily")) == 1
    snapshots = workspace.snapshots("daily", destination=new_destination)
    assert len(snapshots) == 1
    assert_snapshot_files(snapshots[0], {"notes.txt": b"updated plan"})
    workspace.assert_runs_cleaned()


def test_triggering_an_unknown_backup_returns_an_error_and_keeps_daemon_usable(
    daemon: Daemon
) -> None:
    with daemon:
        with pytest.raises(DaemonError, match="Backup plan not found"):
            daemon.trigger_backup("missing")

        assert daemon.list_backups() == []
