# Command-line client

The `anchor` executable runs one command and exits. Backup commands share the
`backup` group:

| Command | Behavior |
| --- | --- |
| `anchor backup list` | Lists backups alphabetically, including enabled state, latest run state, run ID, and failure message when available. A backup with no run is shown as idle. |
| `anchor backup trigger NAME` | Requests a run of the named backup and reports acceptance. `--name NAME` is also accepted. |
| `anchor backup reload` | Adds or updates plans from the daemon's configured plan directory. See [reload semantics](scheduling.md#plans-and-cron-expressions). |
| `anchor snapshot list BACKUP_NAME` | Returns an empty snapshot list for the named backup. |
| `anchor snapshot inspect BACKUP_NAME SNAPSHOT_UUID` | Returns no snapshot details. |
| `anchor snapshot remove BACKUP_NAME SNAPSHOT_UUID` | Acknowledges the request without removing anything. |
| `anchor diagnostics --details TEXT` | Submits diagnostic text to the daemon's log. |

`--port PORT` is a global option accepted before or after subcommands. Connections
use IPv4 loopback. `--help` and invalid arguments are handled before connecting.
Exactly one command is allowed per invocation. Success and help go to stdout;
runtime errors go to stderr with exit code 1. Argument errors use CLI11's exit
codes. A trigger acknowledgement means the run was accepted, and `backup list`
reports its current or most recent state.

Snapshot commands currently have placeholder daemon handlers. They do not look up
backups or snapshots, access snapshot files, or maintain a registry. Each command
requires a nonempty backup name; inspection and removal also require a valid UUID.
The CLI reports the empty results and removal acknowledgement without claiming a
snapshot was deleted.

## Command implementation

`Application` owns argument parsing, global options, and exit/error reporting.
Command classes register their leaf commands with `CommandContext::addCommand`.
Registration selects an action during parsing; the application executes it only
after parsing succeeds. Adding a command requires no change to dispatch logic.

`CommandContext::request<Response>` sends a protocol message and checks for
transport errors, daemon errors, and the expected response type before calling
the command's renderer. `acknowledge` handles commands that expect `AckResponse`.
Both helpers share the configured port and output stream. `BackupCommands`
contains backup request construction and list rendering; `DiagnosticsCommand`
uses the same registration and acknowledgement helpers.

`Client::request` connects to the daemon and exchanges newline-delimited JSON
messages, with a 32 KiB response limit. Each request starts a fresh connection.
The list response carries each backup's name, enabled flag, and optional latest
run containing a state, UUID, and optional error. Empty lists serialize as an
empty array. The decoder validates these fields and returns errors for malformed
responses.

[`ErrorResponse`](../src/lib/proto/SystemMessages.cpp) carries the error category,
numeric code, and message. Its decoder accepts Anchor errors and kcore's defined
codes, including `threadPoolClosed`; reserved kcore codes 1 and 2 and unknown codes
remain invalid. Encoding and decoding preserve the category and message.

CLI unit tests inject a client and output streams to exercise parsing, dispatch,
response validation, rendering, and exit codes without opening sockets. Protocol,
controller, and service tests cover the corresponding daemon request flow.

`proto/BackupMessages` groups backup operations; `proto/SnapshotMessages` groups
snapshot operations; `proto/SystemMessages` contains diagnostics, acknowledgements,
and errors. UUID validation uses `kc::isValidUuid` from `kcore/Random.hh`.

Snapshot protocol messages use `GetSnapshotsRequest` with a `name` field, and
`GetSnapshotRequest` or `RemoveSnapshotRequest` with `name` and `uuid` fields.
Their response bodies are respectively `GetSnapshotsResponse` with `[]`,
`GetSnapshotResponse` with `null`, and `AckResponse` with `null`. The response
decoders currently accept only these empty payloads.

## Logging

The CLI uses the `log` namespace alias exposed by
[`core/Core.hh`](../src/lib/core/Core.hh), alongside Anchor's foundational type
aliases. It delegates to kcore logging;
[`main.cpp`](../src/cli/main.cpp) disables logs before running the CLI so command
output remains on its designated streams.
