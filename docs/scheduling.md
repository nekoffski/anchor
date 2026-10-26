# Backup execution and scheduling

Domain terms are defined in [CONTEXT.md](../CONTEXT.md).

## Ownership and request flow

The daemon owns the repositories, snapshot adapters, executor, API server, and `BackupService`. The service receives references to those dependencies and owns its request queue, consumer thread, scheduler, and orchestrator.

```text
API controller --+
                 +--> BackupService queue --> validate/accept --> Orchestrator --> Run
Scheduler -------+
```

The API controller and scheduler submit requests through `RequestSender` handles. Only the service consumes the request queue. Its private handlers serialize plan lookup, validation, and run acceptance, so manual and scheduled requests share the same admission check. The API controller waits for the request response; a successful trigger acknowledges acceptance rather than backup completion.

The backup repository holds each backup's current plan, enabled flag, and latest run status. Each accepted `BackupRun` copies its plan. Later changes to the repository do not alter queued or executing work. The scheduler holds a reference to the repository and a min-heap of backup names and next deadlines. It stores no backup definitions or cron copies.

## Plans and cron expressions

Plans are loaded from YAML files when the service is constructed and when it handles a reload request. The filename supplies the backup name. An optional `settings.cron` enables automatic execution:

```yaml
settings:
  archive: directory
  destination: local:///backups/documents
  cron: '0 */5 * * * *'
sources:
  - root: /home/user/Documents
    includes: ['*.txt']
```

Cron expressions have six fields: second, minute, hour, day of month, month, and day of week. They use the daemon machine's local timezone. The example requests a backup every five minutes at second zero. Without `cron`, a plan runs only on manual requests. A malformed cron expression rejects that plan; the loader logs the error and continues loading other plans.

`anchor backup reload` sends a `ReloadPlansRequest` through the same service queue as list and trigger requests. The handler adds or updates valid plans and refreshes each loaded plan's scheduler deadline before acknowledging the request. Plan updates preserve enabled state and latest run status under the repository lock, including status changes from executing runs. Queued and executing runs retain their accepted plan copies.

Reload does not remove backups whose files were deleted. An invalid plan leaves its previous definition and schedule intact. Valid plans still load when other plans fail; the request returns the first loading error, while the daemon logs every failure. A directory listing failure returns an error without updating any plans. A successful reload with no files leaves the repository unchanged. Restarting the daemon reloads only the definitions currently on disk.

Repository contents and execution status are held in memory and do not survive a daemon restart. The enabled flag controls automatic scheduling; manual triggers ignore it.

## Scheduling

When the service loads a plan it registers the backup name with `BackupScheduler::add`. The scheduler calculates its next occurrence and stores the name and deadline in a heap-backed priority queue. The earliest deadline determines the condition-variable timeout; an empty heap waits indefinitely. There is no periodic scan of the repository.

Adding a schedule notifies the condition variable so an earlier deadline can replace the current wait. Re-registering a name replaces its existing deadline instead of creating duplicates; registering a backup without a cron removes its deadline. A revision counter ensures that a changed queue is observed before the worker resumes waiting. Shutdown also interrupts the wait immediately.

At a deadline, the scheduler retrieves the current backup, checks its enabled and active state, and submits a named request when eligible. It then calculates the next future deadline regardless of whether that occurrence ran or was skipped. Deadlines older than the current second are skipped, so sleep or delayed execution does not create catch-up work. Starting the worker rebuilds its deadlines from the current time. A backward clock adjustment detected when processing deadlines also rebuilds them; clock changes alone do not notify the condition variable.

Scheduling and cron evaluation use `std::chrono::sys_seconds`; native timestamps are converted only inside `Cron`. `Cron::nextAfter` uses `croncpp` to find a future occurrence and validates it with the adjacent-second matcher. If the library moves backwards at a missing DST time, the search resumes at the next local date. Local times that never occur do not trigger work; repeated local times can produce separate occurrences. Evaluation exceptions log a warning with the expression and reason. A schedule with no future occurrence is logged and receives no heap entry.

Scheduled requests carry their origin through the same queue used for manual requests. At acceptance, the service rechecks the current enabled flag and active status. A scheduled request for a disabled or busy backup is acknowledged and skipped; a manual request for a busy backup returns `alreadyRunning`.

## Execution and results

The orchestrator records a run's UUID and `queued` status before dispatch. A run then transitions through `starting`, `snapshotting`, and `uploading` to `completed`, or records `failed` with an error. Only the first four states block another run. Completed and failed results remain until another run is accepted and replaces them; no background scan clears them.

The daemon uses `InlineExecutor`, so queued backups execute sequentially on the orchestrator thread. Request handling and scheduling remain on their own threads. Each run uses a workspace named after its UUID and attempts cleanup after snapshotting or storage, including failure paths. Failure to create the workspace returns without attempting removal.

## Lifecycle

`BackupService::start` starts the orchestrator and request consumer before the scheduler. The API server starts after the service is ready. Callers must keep the service alive while its sender handles are in use.

On shutdown, the daemon stops the API server and calls `BackupService::stop`. The service stops and joins the scheduler, then closes its request queue. The consumer drains requests and resolves their promises before closing and draining the orchestrator queue. Joining the service waits for that queued execution to finish; service destruction also stops and joins its workers before their dependencies are destroyed.
