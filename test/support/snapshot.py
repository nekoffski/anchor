"""Inspect actual snapshots without extracting archives onto the filesystem."""

import tarfile
import zipfile
from pathlib import Path


def assert_snapshot_files(snapshot: Path, expected: dict[str, bytes]) -> None:
    if snapshot.is_dir():
        actual = {
            path.relative_to(snapshot).as_posix(): path.read_bytes()
            for path in snapshot.rglob("*")
            if path.is_file()
        }
    elif snapshot.suffix == ".zip":
        with zipfile.ZipFile(snapshot) as archive:
            assert archive.testzip() is None, f"Corrupt ZIP snapshot: {snapshot}"
            actual = {
                member.filename: archive.read(member)
                for member in archive.infolist()
                if not member.is_dir()
            }
    elif snapshot.suffix == ".tgz":
        with tarfile.open(snapshot, "r:gz") as archive:
            actual = {}
            for member in archive.getmembers():
                if member.isfile():
                    source = archive.extractfile(member)
                    assert source is not None, f"Could not read member {member.name}"
                    with source:
                        actual[member.name] = source.read()
    else:
        raise AssertionError(f"Unrecognized snapshot format: {snapshot}")
    assert actual == expected, f"Unexpected contents in snapshot {snapshot}"
