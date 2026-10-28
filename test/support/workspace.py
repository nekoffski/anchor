"""Files and plans owned by a single test; no user configuration is used."""

from collections.abc import Generator
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Self

import yaml


@dataclass(frozen=True)
class Workspace:
    root: Path

    @classmethod
    @contextmanager
    def temporary(cls) -> Generator[Self, None, None]:
        with TemporaryDirectory(prefix="anchor-functional-") as root:
            workspace = cls(Path(root))
            workspace.initialize()
            yield workspace

    @property
    def sources(self) -> Path:
        return self.root / "sources"

    @property
    def plans(self) -> Path:
        return self.root / "plans"

    @property
    def runs(self) -> Path:
        return self.root / "runs"

    @property
    def destination(self) -> Path:
        return self.root / "destination"

    def initialize(self) -> None:
        for directory in (self.sources, self.plans, self.runs, self.destination):
            directory.mkdir()

    def write_source(self, relative: str, contents: str | bytes) -> Path:
        target = self.sources / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(contents.encode() if isinstance(contents, str) else contents)
        return target

    def write_plan(
        self,
        name: str,
        *,
        archive: str = "directory",
        includes: tuple[str, ...] = ("notes.txt",),
        destination: Path | None = None,
        cron: str | None = None,
    ) -> Path:
        settings = {
            "archive": archive,
            "destination": f"local://{destination or self.destination}",
        }
        if cron is not None:
            settings["cron"] = cron
        plan = {
            "settings": settings,
            "sources": [{"root": str(self.sources), "includes": list(includes)}],
        }
        target = self.plans / f"{name}.yaml"
        target.write_text(yaml.safe_dump(plan), encoding="utf-8")
        return target

    def write_config(self, port: int) -> Path:
        config = {
            "api": {"port": port},
            "paths": {"workspace": str(self.runs), "backup_plans": str(self.plans)},
        }
        target = self.root / "server.yaml"
        target.write_text(yaml.safe_dump(config), encoding="utf-8")
        return target

    def snapshots(self, name: str, *, destination: Path | None = None) -> list[Path]:
        return sorted((destination or self.destination).glob(f"{name}_*"))

    def assert_runs_cleaned(self) -> None:
        remaining = list(self.runs.iterdir())
        assert not remaining, f"Run workspaces were not cleaned up: {remaining}"
