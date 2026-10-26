"""Read serial log markers relative to a captured file offset."""

from pathlib import Path


def serial_log_offset(path: Path) -> int:
    try:
        return path.stat().st_size
    except FileNotFoundError:
        return 0


def serial_log_contains_marker_after(path: Path, marker: str, offset: int) -> bool:
    try:
        with path.open("rb") as serial_log:
            serial_log.seek(offset)
            return marker.encode("utf-8") in serial_log.read()
    except FileNotFoundError:
        return False
