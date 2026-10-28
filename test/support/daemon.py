"""Lifecycle and status polling for one real, isolated daemon subprocess."""

import os
import socket
import subprocess
from pathlib import Path
from types import TracebackType
from typing import Any, BinaryIO, Self

from .protocol import DaemonClient
from .wait import wait_until
from .workspace import Workspace


class Daemon:
    def __init__(self, binary: Path, workspace: Workspace, *, timeout: float):
        self.binary = binary
        self.workspace = workspace
        self.timeout = timeout
        self.process: subprocess.Popen[bytes] | None = None
        self._client: DaemonClient | None = None
        self._log_file: BinaryIO | None = None
        self._final_logs = ""
        self._stopped = False

    @property
    def logs(self) -> str:
        path = self.workspace.root / "daemon.log"
        if path.exists():
            return path.read_text(encoding="utf-8", errors="replace")
        return self._final_logs

    @property
    def client(self) -> DaemonClient:
        assert self._client is not None, "Start the daemon before sending requests"
        return self._client

    def __enter__(self) -> Self:
        try:
            self._start()
        except BaseException as error:
            self._cleanup_after_failure(error)
            raise
        return self

    def __exit__(
        self,
        _exception_type: type[BaseException] | None,
        exception: BaseException | None,
        _traceback: TracebackType | None,
    ) -> None:
        if exception is None:
            self.stop()
        else:
            self._cleanup_after_failure(exception)

    def _cleanup_after_failure(self, original: BaseException) -> None:
        try:
            self.stop()
        except Exception as cleanup_error:
            original.add_note(f"Daemon cleanup failed: {cleanup_error}")

    def _start(self) -> None:
        assert self.process is None, "This daemon instance has already been started"
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        config = self.workspace.write_config(port)
        self._client = DaemonClient(port, timeout=min(1.0, self.timeout))
        self._log_file = (self.workspace.root / "daemon.log").open("wb")
        self.process = subprocess.Popen(
            [str(self.binary), str(config)],
            cwd=self.workspace.root,
            env={**os.environ, "TZ": "UTC"},
            stdout=self._log_file,
            stderr=subprocess.STDOUT,
        )
        wait_until(self._ready, timeout=self.timeout, description="daemon readiness")

    def assert_running(self) -> None:
        assert self.process is not None, "Start the daemon before sending requests"
        assert self.process.poll() is None, (
            f"Daemon exited unexpectedly with code {self.process.returncode}\n{self.logs}"
        )

    def _ready(self) -> bool:
        self.assert_running()
        marker = f"API server listening on port {self.client.port}"
        if marker not in self.logs:
            return False
        try:
            self.list_backups()
        except OSError:
            return False
        self.assert_running()
        return True

    def list_backups(self) -> list[dict[str, Any]]:
        self.assert_running()
        return self.client.list_backups()

    def backup(self, name: str) -> dict[str, Any]:
        backups = self.list_backups()
        for backup in backups:
            if backup["plan"]["name"] == name:
                return backup
        raise AssertionError(f"Backup {name!r} not found in {backups!r}")

    def trigger_backup(self, name: str) -> None:
        self.assert_running()
        self.client.trigger_backup(name)

    def reload_plans(self) -> None:
        self.assert_running()
        self.client.reload_plans()

    def wait_for_run(
        self, name: str, *, state: str = "completed", after_uuid: str | None = None
    ) -> dict[str, Any]:
        observed: dict[str, Any] | None = None

        def matches() -> dict[str, Any] | None:
            nonlocal observed
            observed = self.backup(name).get("run")
            if not observed or observed["uuid"] == after_uuid:
                return None
            if state != "failed" and observed["state"] == "failed":
                raise AssertionError(f"Backup {name!r} failed: {observed!r}")
            return observed if observed["state"] == state else None

        try:
            return wait_until(
                matches,
                timeout=self.timeout,
                description=f"backup {name!r} to become {state}",
            )
        except AssertionError as error:
            error.add_note(f"Latest observed run: {observed!r}")
            raise

    def stop(self) -> None:
        if self._stopped:
            return
        try:
            if self.process is None:
                return
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=self.timeout)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=self.timeout)
                    raise AssertionError(
                        "Daemon did not shut down after SIGTERM; killed it"
                    ) from None
            assert self.process.returncode == 0, (
                f"Daemon exited with code {self.process.returncode}\n{self.logs}"
            )
        finally:
            self._stopped = True
            if self._log_file is not None:
                self._log_file.close()
            self._final_logs = self.logs
