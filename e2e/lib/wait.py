"""Polling wait helper shared by test-side wait loops."""

import time
from collections.abc import Callable


def wait_for(check: Callable[[], bool], timeout: float, interval: float = 0.1) -> bool:
    """Poll check() every interval seconds until True or timeout. Returns True if matched."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if check():
            return True
        time.sleep(interval)
    return check()