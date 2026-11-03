# Snapshot format

Directory, ZIP, and TGZ snapshots use the same internal layout:

```text
snapshot/
├── manifest.json
├── 0/docs/report.txt
└── 1/docs/report.txt
```

Archives contain these entries without a wrapper directory. Source IDs are
zero-based positions in the captured backup plan. Empty selections retain their
source directory and manifest record. IDs are local to each snapshot.

## Manifest version 1

| Field | Value |
|---|---|
| `version` | `1` |
| `name` | Snapshot base name without the archive extension |
| `created_at_ns` | Creation time in nanoseconds since the Unix epoch |
| `hash_algorithm` | `sha256` |
| `sources` | Ordered array of source records |

Each source contains `id`, its original absolute `root` after environment
expansion, and an `entries` array. Entry paths are relative to that root; payload
paths are `<source ID>/<entry path>`.

Every entry contains `path`, `type`, POSIX `permissions`, numeric `owner` and
`group` IDs, and `modified_ns` in nanoseconds since the Unix epoch.

| Entry type | Additional fields |
|---|---|
| `file` | `size` in bytes and lowercase hexadecimal `sha256` |
| `directory` | None; empty directories are included |
| `symlink` | `target` containing the original link text |

Source container directories and `manifest.json` are excluded from the entry
inventory. Symlink targets are not followed or hashed. Original metadata remains
in the manifest even when copying the payload changes its filesystem metadata.

## Capture

The `Snapshotter` port returns `SnapshotMetadata` containing the artifact name,
location, typed `manifest`, and `manifestSha256`. Success requires the manifest
to be embedded and the output finalized. The snapshot store preserves the manifest
and digest when copying the artifact.

`manifestSha256` is the lowercase hexadecimal SHA-256 of the exact embedded
`manifest.json` bytes, including indentation and the final newline. It is returned
separately and is not included in the manifest file. The JSON is serialized once;
the same bytes are hashed and written to the snapshot.

Manifest data types live in `core/domain`. All snapshotters use the shared
`adapters/backup/SnapshotBuilder` to resolve source roots, assign source IDs,
traverse selections, stream files in 64 KiB chunks, and hash captured bytes.
`SnapshotManifestJson` serializes the manifest after every source succeeds.
The shared `adapters/crypto/Sha256` helper hashes file chunks and manifest bytes.
Directory metadata is applied through `kc::FileSystem` helpers.
Directory and archive writers handle their output formats. Read, write, close,
encoding, and detected file-size-change errors fail snapshot creation.
