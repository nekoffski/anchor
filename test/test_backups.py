import pytest
import yaml

from support.daemon import Daemon
from support.snapshot import assert_snapshot_files
from support.workspace import Workspace


@pytest.mark.parametrize("archive", ["directory", "zip", "tgz"])
def test_manual_backup_stores_selected_contents_and_cleans_its_workspace(
    daemon: Daemon, workspace: Workspace, archive: str
) -> None:
    binary = bytes(range(256)) * 1024 + b"\x00"
    workspace.write_source("notes.txt", "selected note")
    workspace.write_source("assets/nested/data.bin", binary)
    (workspace.sources / "assets/nested/link").symlink_to("data.bin")
    (workspace.sources / "assets/nested/broken").symlink_to("missing")
    workspace.write_source("ignored.txt", "not selected")
    workspace.write_plan("daily", archive=archive, includes=("notes.txt", "assets"))
    with daemon:
        daemon.trigger_backup("daily")
        run = daemon.wait_for_run("daily")

    assert run["state"] == "completed"
    snapshots = workspace.snapshots("daily")
    assert len(snapshots) == 1
    manifest = assert_snapshot_files(
        snapshots[0],
        {"0/notes.txt": b"selected note", "0/assets/nested/data.bin": binary},
    )
    links = {
        entry["path"]: entry["target"]
        for entry in manifest["sources"][0]["entries"]
        if entry["type"] == "symlink"
    }
    assert links == {
        "assets/nested/link": "data.bin", "assets/nested/broken": "missing"
    }
    workspace.assert_runs_cleaned()


@pytest.mark.parametrize("archive", ["directory", "zip", "tgz"])
def test_manifest_keeps_colliding_sources_and_an_empty_source_distinct(
    daemon: Daemon, workspace: Workspace, archive: str
) -> None:
    workspace.write_source("first/report.txt", "first source")
    workspace.write_source("second/report.txt", "second source")
    workspace.write_source("first/manifest.json", "user data")
    workspace.write_source("second/empty", b"")
    plan = workspace.write_plan("daily", archive=archive)
    data = yaml.safe_load(plan.read_text())
    data["sources"] = [
        {"root": str(workspace.sources / "missing"), "includes": ["absent"]},
        {
            "root": str(workspace.sources / "first"),
            "includes": ["report.txt", "manifest.json"],
        },
        {
            "root": str(workspace.sources / "second"),
            "includes": ["report.txt", "empty"],
        },
    ]
    plan.write_text(yaml.safe_dump(data), encoding="utf-8")
    with daemon:
        daemon.trigger_backup("daily")
        daemon.wait_for_run("daily")
    snapshot = workspace.snapshots("daily")[0]
    manifest = assert_snapshot_files(
        snapshot,
        {
            "1/report.txt": b"first source",
            "1/manifest.json": b"user data",
            "2/report.txt": b"second source",
            "2/empty": b"",
        },
    )
    assert [source["root"] for source in manifest["sources"]] == [
        str(workspace.sources / name) for name in ("missing", "first", "second")
    ]
    assert manifest["sources"][0]["entries"] == []
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
