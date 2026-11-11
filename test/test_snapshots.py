import json
import shutil
import subprocess
from uuid import UUID
from typing import Any

import pytest

from support.daemon import Daemon
from support.protocol import DaemonError
from support.workspace import Workspace


def run_cli(daemon: Daemon, *arguments: str) -> subprocess.CompletedProcess[str]:
    cli = daemon.binary.parent.parent / "cli" / "anchor"
    return subprocess.run(
        [str(cli), "--port", str(daemon.client.port), "snapshot", *arguments],
        capture_output=True,
        text=True,
        timeout=daemon.timeout,
        check=False,
    )


@pytest.mark.parametrize("archive", ["directory", "zip", "tgz"])
def test_snapshots_are_discovered_and_removed_after_restart(
    daemon: Daemon, workspace: Workspace, archive: str
) -> None:
    workspace.write_source("notes.txt", "backup data")
    workspace.write_plan("daily", archive=archive)
    workspace.write_plan("other", archive=archive)
    unrelated = workspace.destination / "unrelated.txt"
    unrelated.write_text("keep me", encoding="utf-8")
    with daemon:
        daemon.trigger_backup("daily")
        daemon.wait_for_run("daily")
        snapshots = daemon.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        )
        assert len(snapshots) == 1
        snapshot = snapshots[0]
        snapshot_id = snapshot["uuid"]
        assert str(UUID(snapshot_id)) == snapshot_id
        artifact = workspace.snapshots("daily")[0]
        assert snapshot["location"] == f"local://{artifact}"
        assert artifact.parent == workspace.destination / "daily"
        manifest = artifact.with_suffix(".json")
        assert json.loads(manifest.read_text())["uuid"] == snapshot_id
        assert json.loads(manifest.read_text())["format"] == archive
        assert len(list(artifact.parent.iterdir())) == 2
        result = run_cli(daemon, "list", "daily")
        assert result.returncode == 0, result.stderr
        assert snapshot_id in result.stdout
        assert snapshot["location"] in result.stdout
        with pytest.raises(DaemonError, match="Snapshot not found"):
            daemon.client.request("RemoveSnapshotRequest", {"name": "other", "uuid": snapshot_id})
        assert artifact.exists()
    restarted = Daemon(daemon.binary, workspace, timeout=daemon.timeout)
    with restarted:
        listed = restarted.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        )
        assert listed == snapshots
        removed = run_cli(restarted, "remove", "daily", snapshot_id.upper())
        assert removed.returncode == 0, removed.stderr
        assert removed.stdout == f"Snapshot '{snapshot_id.upper()}' removed.\n"
        assert not artifact.exists()
        assert not manifest.exists()
        assert restarted.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        ) == []
    assert unrelated.read_text() == "keep me"
    workspace.assert_runs_cleaned()


def test_removing_a_missing_payload_cleans_the_manifest_and_unknown_ids_fail(
    daemon: Daemon, workspace: Workspace
) -> None:
    workspace.write_source("notes.txt", "data")
    workspace.write_plan("daily")
    with daemon:
        daemon.trigger_backup("daily")
        daemon.wait_for_run("daily")
        snapshot = daemon.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        )[0]
        artifact = workspace.snapshots("daily")[0]
        manifest = artifact.with_suffix(".json")
        shutil.rmtree(artifact)
        assert daemon.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        ) == []
        result = run_cli(daemon, "remove", "daily", snapshot["uuid"])
        assert result.returncode == 0, result.stderr
        assert not manifest.exists()
        repeated = run_cli(daemon, "remove", "daily", snapshot["uuid"])
        assert repeated.returncode == 1
        assert repeated.stdout == ""
        assert "Snapshot not found" in repeated.stderr


def test_operations_only_use_the_current_plan_destination(
    daemon: Daemon, workspace: Workspace
) -> None:
    workspace.write_source("notes.txt", "data")
    workspace.write_plan("daily")
    with daemon:
        daemon.trigger_backup("daily")
        previous = daemon.wait_for_run("daily")
        old = daemon.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        )[0]
        old_payload = workspace.snapshots("daily")[0]
        new_destination = workspace.root / "new-destination"
        new_destination.mkdir()
        workspace.write_plan("daily", destination=new_destination)
        daemon.client.reload_plans()
        assert daemon.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        ) == []
        failed = run_cli(daemon, "remove", "daily", old["uuid"])
        assert failed.returncode == 1
        assert old_payload.exists()
        daemon.trigger_backup("daily")
        daemon.wait_for_run("daily", after_uuid=previous["uuid"])
        current = daemon.client.request(
            "GetSnapshotsRequest", {"name": "daily"}, response="GetSnapshotsResponse"
        )
        assert len(current) == 1
        assert current[0]["uuid"] != old["uuid"]
        assert str(new_destination / "daily") in current[0]["location"]


def test_inspection_remains_a_placeholder(daemon: Daemon) -> None:
    with daemon:
        result = run_cli(daemon, "inspect", "unknown", "12345678-1234-4234-8234-123456789abc")
        assert result.returncode == 0, result.stderr
        assert result.stdout == "No snapshot details available.\n"


def test_listing_reads_sidecars_without_opening_archives_and_handles_large_lists(
    daemon: Daemon, workspace: Workspace
) -> None:
    workspace.write_plan("daily")
    directory = workspace.destination / "daily"
    directory.mkdir()
    for index in range(300):
        snapshot_id = str(UUID(int=index + 1))
        manifest: dict[str, Any] = {
            "version": 1,
            "uuid": snapshot_id,
            "created_at_ns": 1_700_000_000_000_000_000 + index,
            "backup_name": "daily",
            "format": "zip",
            "hash_algorithm": "sha256",
            "sources": [],
        }
        (directory / f"{snapshot_id}.json").write_text(json.dumps(manifest))
        (directory / f"{snapshot_id}.zip").write_text("listing never opens this archive")
    with daemon:
        result = run_cli(daemon, "list", "daily")
        assert result.returncode == 0, result.stderr
        assert result.stdout.count("Snapshot: ") == 300
        assert result.stdout.startswith(f"Snapshot: {UUID(int=300)}\n")
