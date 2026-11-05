import subprocess

from support.daemon import Daemon
from support.workspace import Workspace


def test_snapshot_commands_return_placeholders_without_modifying_files(
    daemon: Daemon, workspace: Workspace
) -> None:
    cli = daemon.binary.parent.parent / "cli" / "anchor"
    snapshot_id = "12345678-1234-4234-8234-123456789abc"
    artifact = workspace.destination / snapshot_id
    artifact.mkdir()
    payload = artifact / "data.txt"
    payload.write_text("preserve me", encoding="utf-8")
    with daemon:
        for arguments, expected in [
            (["list", "unknown"], "No snapshots found for backup 'unknown'.\n"),
            (
                ["inspect", "unknown", snapshot_id],
                "No snapshot details available.\n",
            ),
            (
                ["remove", "unknown", snapshot_id],
                "Snapshot removal request acknowledged.\n",
            ),
        ]:
            result = subprocess.run(
                [str(cli), "--port", str(daemon.client.port), "snapshot", *arguments],
                capture_output=True,
                text=True,
                timeout=daemon.timeout,
                check=False,
            )
            assert result.returncode == 0, result.stderr
            assert result.stdout == expected
            assert result.stderr == ""
        assert daemon.list_backups() == []
    assert payload.read_text(encoding="utf-8") == "preserve me"
    assert list(workspace.destination.iterdir()) == [artifact]
    workspace.assert_runs_cleaned()
