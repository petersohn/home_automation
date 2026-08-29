"""Package-scoped fixtures for basic test suite."""

from pathlib import Path
from collections.abc import Iterator

import pytest

from conftest import assert_booted
from lib.device import Device
from lib.mqtt_client import MqttClient
from lib.gpio import Gpio
from lib.serial_capture import SerialCapture
from lib.pin_map import SERIAL_PORT, SERIAL_BAUD


config_name = "basic"


@pytest.fixture(scope="package")
def device_config(
    request: pytest.FixtureRequest,
    device: Device,
    global_config_path: Path,
) -> Iterator[Path]:
    """Upload config once per suite. No reset, no wait_for_boot."""
    config_dir = Path(__file__).parent.parent.parent / "configs" / config_name
    device.upload_config([config_dir, global_config_path])
    yield config_dir


@pytest.fixture(autouse=True)
def serial(request: pytest.FixtureRequest) -> Iterator[SerialCapture]:
    """Capture serial output to file per test (autouse: capture everything)."""
    cap = SerialCapture(
        port=SERIAL_PORT,
        baud=SERIAL_BAUD,
        log_dir=Path(__file__).parent.parent.parent / "artifacts",
    )
    cap.start(request.node.name)
    yield cap
    cap.stop()


@pytest.fixture
def reset_device(serial: SerialCapture, device: Device, mqtt_client: MqttClient, gpio: Gpio) -> None:
    """Reset device before each test, clear MQTT state. Runs after serial
    capture starts so boot output is logged."""
    device.reset()
    mqtt_client.clear_all()