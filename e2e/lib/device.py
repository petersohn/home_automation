"""ESP8266 device control: reset, wait for boot, upload firmware/config."""

import subprocess
import time
from pathlib import Path

import RPi.GPIO as GPIO_lib

from . import pin_map
from .gpio import Gpio

MQTT_USERNAME = "e2e"
MQTT_PASSWORD = "e2etest123"

# flash.py is in the repo root, two levels up from e2e/
FLASH_PY = Path(__file__).parent.parent.parent / "flash.py"


class Device:
    def __init__(self, serial_port: str, reset_pin: int, gpio: Gpio):
        self._serial_port = serial_port
        self._reset_pin = reset_pin
        self._gpio = gpio
        self._mqtt_client = None

    def set_mqtt_client(self, mqtt_client) -> None:
        self._mqtt_client = mqtt_client

    def reset(self) -> None:
        """Set boot pins to input, pulse RST low 50ms."""
        self._gpio.set_boot_safe_state()
        GPIO_lib.setup(self._reset_pin, GPIO_lib.OUT, initial=True)
        GPIO_lib.output(self._reset_pin, False)
        time.sleep(0.05)
        GPIO_lib.output(self._reset_pin, True)

    def wait_for_boot(self, timeout: float = 30.0) -> bool:
        """Wait for ESP to publish availability '1' on MQTT."""
        if not self._mqtt_client:
            raise RuntimeError("mqtt_client not set")
        return self._mqtt_client.wait_for_state(
            "home/testDevice/available", "1", timeout
        )

    def upload_firmware(self, firmware_bin: Path) -> int:
        """Upload pre-built firmware via flash.py."""
        return subprocess.run(
            [
                "python3", str(FLASH_PY), "-p", self._serial_port,
                "upload",
                "--image", str(firmware_bin),
            ],
            check=True,
        ).returncode

    def upload_config(self, config_inputs: list[Path]) -> int:
        """Build and upload SPIFFS config via flash.py."""
        cmd = ["python3", str(FLASH_PY), "-p", self._serial_port, "upload-fs"]
        cmd += [str(p) for p in config_inputs]
        return subprocess.run(cmd, check=True).returncode