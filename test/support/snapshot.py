"""Inspect actual snapshots without extracting archives onto the filesystem."""

import hashlib
import json
import stat
import tarfile
import zipfile
from pathlib import Path
from typing import Any


def assert_snapshot_files(
    snapshot: Path, expected: dict[str, bytes]
) -> dict[str, Any]:
    if snapshot.is_dir():
        actual = {
            path.relative_to(snapshot).as_posix(): path.read_bytes()
            for path in snapshot.rglob("*")
            if path.is_file() and not path.is_symlink()
        }
    elif snapshot.suffix == ".zip":
        with zipfile.ZipFile(snapshot) as archive:
            assert archive.testzip() is None, f"Corrupt ZIP snapshot: {snapshot}"
            actual = {
                member.filename: archive.read(member)
                for member in archive.infolist()
                if not member.is_dir() and not stat.S_ISLNK(member.external_attr >> 16)
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
    manifest_bytes = actual.pop("manifest.json", None)
    assert manifest_bytes is not None, f"Missing manifest in {snapshot}"
    manifest = json.loads(manifest_bytes)
    assert manifest["version"] == 1
    assert manifest["hash_algorithm"] == "sha256"
    recorded: set[str] = set()
    for index, source in enumerate(manifest["sources"]):
        assert source["id"] == index
        assert Path(source["root"]).is_absolute()
        paths: set[str] = set()
        for entry in source["entries"]:
            relative = Path(entry["path"])
            assert not relative.is_absolute() and ".." not in relative.parts
            assert entry["path"] not in paths
            paths.add(entry["path"])
            if entry["type"] != "file":
                assert "sha256" not in entry
                continue
            name = f"{index}/{entry['path']}"
            assert name in actual, f"Manifest references missing file {name}"
            assert entry["size"] == len(actual[name])
            assert entry["sha256"] == hashlib.sha256(actual[name]).hexdigest()
            recorded.add(name)
    assert recorded == set(actual), (
        f"Manifest inventory differs from payload in {snapshot}"
    )
    assert actual == expected, f"Unexpected contents in snapshot {snapshot}"
    return manifest
