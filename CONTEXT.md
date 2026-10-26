# Anchor

Anchor creates snapshots of selected files and directories and stores them at a backup destination.

## Language

**Backup**:
A named backup definition together with its enabled state and execution status.

**Backup plan**:
The instructions for a backup: its sources, file selections, archive type, destination, and optional automatic schedule.

**Backup run**:
One accepted execution of a named backup, using the plan captured when that execution was accepted.
_Avoid_: Backup, snapshot

**Scheduled occurrence**:
A time matching a backup's automatic schedule. It requests a run only while automatic execution is enabled and the backup has no queued or executing run; skipped occurrences leave no pending retry.

**Enabled backup**:
A backup whose automatic schedule is allowed to request runs. Disabling automatic execution does not prevent an explicit manual run.

**Snapshot**:
The output produced by a backup run from its selected source files and directories.
_Avoid_: Backup run
