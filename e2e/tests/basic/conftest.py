"""Package-scoped fixtures for basic test suite."""

from pathlib import Path

import pytest

from conftest import assert_booted
from lib.serial_capture import SerialCapture
from lib.pin_map import SERIAL_PORT, SERIAL_BAUD


config_name = "basic"


@pytest.fixture(scope="package")
def device_config(request, device, global_config_path):
    """Upload config once per suite. No reset, no wait_for_boot."""
    config_dir = Path(__file__).parent.parent.parent / "configs" / config_name
    device.upload_config([config_dir, global_config_path])
    yield config_dir


@pytest.fixture
def reset_device(device, mqtt_client, gpio):
    """Reset device before each test, clear MQTT state."""
    device.reset()
    mqtt_client.clear_all()


@pytest.fixture
def serial(reset_device, request):
    """Capture serial output to file per test."""
    cap = SerialCapture(
        port=SERIAL_PORT,
        baud=SERIAL_BAUD,
        log_dir=Path(__file__).parent.parent.parent / "artifacts",
    )
    cap.start(request.node.name)
    yield cap
    cap.stop()