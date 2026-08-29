"""Test GPIO output interfaces: ESP drives pin, RPi reads."""

from collections.abc import Iterator
from pathlib import Path
import time

import pytest

from conftest import assert_booted
from lib.device import Device
from lib.gpio import Gpio
from lib.mqtt_client import MqttClient

pytestmark = pytest.mark.skip(reason="disabled until test_status_online_on_boot passes")


@pytest.fixture
def o1_setup(gpio: Gpio) -> Iterator[None]:
    """Set up RPi to read ESP GPIO5 (o1)."""
    gpio.setup_input(5)
    yield
    gpio.set_boot_safe_state()


@pytest.fixture
def o2_setup(gpio: Gpio) -> Iterator[None]:
    """Set up RPi to read ESP GPIO16 (o2, inverted)."""
    gpio.setup_input(16)
    yield
    gpio.set_boot_safe_state()


def test_output_on_off(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    gpio: Gpio,
    o1_setup: None,
) -> None:
    """Publish 'on' -> RPi reads high; publish 'off' -> RPi reads low."""
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_client.publish("home/testDevice/o1/set", "on")
    time.sleep(0.5)
    assert gpio.read(5) == 1

    mqtt_client.publish("home/testDevice/o1/set", "off")
    time.sleep(0.5)
    assert gpio.read(5) == 0


def test_output_toggle(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    gpio: Gpio,
    o1_setup: None,
) -> None:
    """Publish 'toggle' -> state flips."""
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_client.publish("home/testDevice/o1/set", "on")
    time.sleep(0.5)
    initial = gpio.read(5)

    mqtt_client.publish("home/testDevice/o1/set", "toggle")
    time.sleep(0.5)
    assert gpio.read(5) == (1 - initial)

    mqtt_client.publish("home/testDevice/o1/set", "toggle")
    time.sleep(0.5)
    assert gpio.read(5) == initial


def test_output_invert(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    gpio: Gpio,
    o2_setup: None,
) -> None:
    """o2 has invert=true: 'on' -> low, 'off' -> high."""
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_client.publish("home/testDevice/o2/set", "on")
    time.sleep(0.5)
    assert gpio.read(16) == 0

    mqtt_client.publish("home/testDevice/o2/set", "off")
    time.sleep(0.5)
    assert gpio.read(16) == 1