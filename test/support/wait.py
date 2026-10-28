"""Bounded polling shared by process, run-state, and filesystem checks."""

import time
from collections.abc import Callable
from typing import TypeVar

T = TypeVar("T")


def wait_until(condition: Callable[[], T | None], *, timeout: float, description: str) -> T:
    deadline = time.monotonic() + timeout
    last_observed = None
    while True:
        last_observed = condition()
        if last_observed:
            return last_observed
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise AssertionError(
                f"Timed out after {timeout:g}s waiting for {description}; "
                f"last observed: {last_observed!r}"
            )
        time.sleep(min(0.05, remaining))
