"""One newline-delimited JSON request per connection to the real daemon."""

import json
import socket
from typing import Any

MAX_FRAME_BYTES = 32 * 1024


class DaemonError(RuntimeError):
    def __init__(self, body: dict[str, Any]):
        self.code = body["code"]
        self.message = body["message"]
        super().__init__(f"Daemon error {self.code}: {self.message}")


class DaemonClient:
    def __init__(self, port: int, *, timeout: float = 1.0):
        self.port = port
        self.timeout = timeout

    def request(
        self, name: str, body: Any = None, *, response: str = "AckResponse"
    ) -> Any:
        frame = json.dumps({"name": name, "body": body}).encode() + b"\n"
        with socket.create_connection(("127.0.0.1", self.port), self.timeout) as peer:
            peer.sendall(frame)
            with peer.makefile("rb") as stream:
                reply = stream.readline(MAX_FRAME_BYTES + 1)
        assert reply.endswith(b"\n") and len(reply) <= MAX_FRAME_BYTES, (
            f"Missing, incomplete, or oversized response to {name}: {reply!r}"
        )
        envelope = json.loads(reply)
        if envelope["name"] == "ErrorResponse":
            raise DaemonError(envelope["body"])
        assert envelope["name"] == response, (
            f"Expected {response} for {name}, received {envelope!r}"
        )
        return envelope["body"]

    def list_backups(self) -> list[dict[str, Any]]:
        return self.request("GetBackupsRequest", response="GetBackupsResponse")

    def trigger_backup(self, name: str) -> None:
        self.request("TriggerBackupRequest", {"name": name})

    def reload_plans(self) -> None:
        self.request("ReloadPlansRequest")
