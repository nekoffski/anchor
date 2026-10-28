from support.daemon import Daemon
from support.snapshot import assert_snapshot_files
from support.workspace import Workspace


def test_reloading_a_schedule_runs_the_backup_without_a_manual_trigger(
    daemon: Daemon, workspace: Workspace
) -> None:
    workspace.write_source("notes.txt", "scheduled content")
    with daemon:
        workspace.write_plan("scheduled", cron="* * * * * *")

        daemon.reload_plans()
        daemon.wait_for_run("scheduled")

    snapshots = workspace.snapshots("scheduled")
    assert snapshots, "The scheduler did not produce a snapshot"
    assert_snapshot_files(snapshots[-1], {"notes.txt": b"scheduled content"})
    workspace.assert_runs_cleaned()
