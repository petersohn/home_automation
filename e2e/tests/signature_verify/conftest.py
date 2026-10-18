from collections.abc import Iterator
from pathlib import Path

import pytest

from lib.device import Device
from lib.gpio import Gpio
from lib.mqtt_client import MqttClient
from lib.pin_map import SERIAL_BAUD, SERIAL_PORT
from lib.serial_capture import SerialCapture


@pytest.fixture(autouse=True)
def serial(
    request: pytest.FixtureRequest, provisioning_complete: None
) -> Iterator[SerialCapture]:
    capture = SerialCapture(
        port=SERIAL_PORT,
        baud=SERIAL_BAUD,
        log_dir=Path(__file__).parent.parent.parent / "artifacts",
    )
    capture.start(request.node.name)
    yield capture
    capture.stop()


@pytest.fixture
def reset_device(
    serial: SerialCapture,
    device: Device,
    mqtt_client: MqttClient,
    gpio: Gpio,
) -> None:
    device.reset()
    mqtt_client.clear_all()
