"""Shared pytest fixtures; suites only arrange inputs and assert behavior."""

import math
import os
from collections.abc import Generator, Iterator
from pathlib import Path

import pytest

from support.daemon import Daemon
from support.workspace import Workspace

ROOT = Path(__file__).resolve().parents[1]
DAEMON = pytest.StashKey[Daemon]()


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("anchor")
    group.addoption(
        "--daemon-binary",
        type=Path,
        default=ROOT / "build/Debug/src/daemon/anchord",
        help="Path to the real daemon executable (default: Debug build)",
    )
    group.addoption(
        "--daemon-timeout",
        type=float,
        default=10.0,
        help="Daemon operation timeout in seconds",
    )


@pytest.fixture(scope="session")
def daemon_binary(pytestconfig: pytest.Config) -> Path:
    configured = pytestconfig.getoption("--daemon-binary")
    assert isinstance(configured, Path)
    binary = configured.resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise pytest.UsageError(
            f"Daemon executable not found: {binary}. Run make build-debug first."
        )
    return binary


@pytest.fixture
def workspace() -> Iterator[Workspace]:
    with Workspace.temporary() as workspace:
        yield workspace


@pytest.fixture
def daemon(
    daemon_binary: Path,
    workspace: Workspace,
    pytestconfig: pytest.Config,
    request: pytest.FixtureRequest,
) -> Daemon:
    timeout = pytestconfig.getoption("--daemon-timeout")
    assert isinstance(timeout, float)
    if not math.isfinite(timeout) or timeout <= 0:
        raise pytest.UsageError("--daemon-timeout must be positive")
    daemon = Daemon(daemon_binary, workspace, timeout=timeout)
    request.node.stash[DAEMON] = daemon
    return daemon


@pytest.hookimpl(wrapper=True)
def pytest_runtest_makereport(
    item: pytest.Item, call: pytest.CallInfo[None]
) -> Generator[None, pytest.TestReport, pytest.TestReport]:
    report = yield
    daemon = item.stash.get(DAEMON, None)
    if report.failed and daemon is not None:
        report.sections.append(("Daemon log", daemon.logs))
    return report
