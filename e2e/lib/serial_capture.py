"""pyserial wrapper: capture ESP serial output to one file per test."""

import threading
from pathlib import Path

import serial


class SerialCapture:
    def __init__(self, port: str, baud: int, log_dir: Path):
        self._port = port
        self._baud = baud
        self._log_dir = log_dir
        self._serial: serial.Serial | None = None
        self._thread: threading.Thread | None = None
        self._running = False
        self._log_path: Path | None = None

    def start(self, test_name: str) -> None:
        self._log_dir.mkdir(parents=True, exist_ok=True)
        self._log_path = self._log_dir / f"{test_name}.log"
        self._serial = serial.Serial(self._port, self._baud, timeout=0.1)
        self._running = True
        self._thread = threading.Thread(target=self._capture_loop, daemon=True)
        self._thread.start()

    def _capture_loop(self) -> None:
        with open(self._log_path, "w") as f:
            while self._running and self._serial and self._serial.is_open:
                data = self._serial.read(1024)
                if data:
                    f.write(data.decode("utf-8", errors="replace"))
                    f.flush()

    def stop(self) -> Path:
        self._running = False
        if self._thread:
            self._thread.join(timeout=2.0)
        if self._serial:
            self._serial.close()
        return self._log_path