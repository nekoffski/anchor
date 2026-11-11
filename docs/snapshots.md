# Snapshot storage

Each backup has a directory below its configured destination. The destination
must already exist; the store creates the backup subdirectory. Snapshot manifests
and payloads are siblings, named by snapshot UUID:

```text
destination/backup-name/
├── UUID.json
├── UUID.zip
├── OTHER_UUID.json
└── OTHER_UUID.tgz
```

Directory-format payloads use `UUID/` instead of an archive file. Every payload
uses zero-based source directories, such as `0/docs/report.txt` and
`1/docs/report.txt`. Archives contain these entries without a wrapper directory.
The manifest is stored only in the JSON sidecar, outside the payload.

Source IDs are positions in the captured backup plan. Empty selections retain
their source directory and manifest record. IDs are local to each snapshot.

## Manifest version 1

| Field | Value |
|---|---|
| `version` | `1` |
| `uuid` | Snapshot UUID, independent of its run ID |
| `backup_name` | Originating backup name |
| `format` | `directory`, `zip`, or `tgz` |
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

Source container directories and the JSON sidecar are excluded from the entry
inventory. Symlink targets are not followed or hashed. Original metadata remains
in the manifest even when copying the payload changes its filesystem metadata.

## Capture and publication

The `Snapshotter` port returns `SnapshotMetadata` containing the payload name,
location, typed `manifest`, and `manifestSha256`. Success requires the payload to
be finalized and its sidecar written. The digest covers the exact JSON bytes,
including indentation and the final newline, and is returned separately.

Manifest data types live in `core/domain`. All snapshotters use the shared
`adapters/backup/SnapshotBuilder` to resolve source roots, assign source IDs,
traverse selections, stream files in 64 KiB chunks, and hash captured bytes.
`adapters/serialization/SnapshotJson` encodes manifests and list summaries and
parses manifest headers and list summaries. `adapters/crypto/Sha256` hashes
file chunks and manifest bytes. Directory metadata is applied through
`kc::FileSystem` helpers. Directory and archive writers handle their payload
formats. Read, write, close, encoding, and detected file-size-change errors fail
snapshot creation.

`SnapshotStore::put` checks the sidecar identity and returned digest, copies the
payload into a temporary path under the backup directory, then renames it to its
final UUID name. It publishes the exact sidecar bytes last using an atomic file
replacement. Existing UUID payloads or manifests are not overwritten. A run
completes after storage succeeds. An interrupted publication can leave a payload
without a manifest; these payloads are not listed.

## Discovery and removal

`SnapshotStore::list` enumerates JSON sidecars in the backup directory, reads
their headers, and checks that each payload exists with the declared type. It
does not open archives. The result contains UUIDs, creation times, and exact
payload locations, ordered newest first with UUIDs breaking timestamp ties.
Malformed manifest headers and filesystem errors are reported. Manifests with
missing payloads and payloads without manifests are excluded from the list.

`SnapshotStore::remove` resolves the UUID within the backup directory, reads its
manifest to identify the payload, deletes the payload, then removes its manifest.
An already missing payload is accepted. A missing manifest returns a not-found
error. Failed payload deletion preserves the manifest; failed manifest deletion
can be retried after the payload has gone.

Both operations use the named backup plan's current destination. After a
destination change, snapshots at the previous destination remain there and are
not included in queries to the new destination. Unknown backups return an error.
This layout does not discover older snapshots with embedded manifests automatically.
