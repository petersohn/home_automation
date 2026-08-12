"""Test PWM interfaces: ESP outputs PWM, RPi samples duty cycle."""

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
def pwm_setup(gpio: Gpio) -> Iterator[None]:
    """Set up RPi to read ESP PWM pins."""
    gpio.setup_input(12)
    gpio.setup_input(13)
    yield
    gpio.set_boot_safe_state()


def test_pwm_values(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    gpio: Gpio,
    pwm_setup: None,
) -> None:
    """off -> ~0.0, on -> ~1.0, 128 -> ~0.5."""
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_client.publish("home/testDevice/p1/set", "off")
    time.sleep(0.5)
    assert gpio.read_pwm(12) < 0.1

    mqtt_client.publish("home/testDevice/p1/set", "on")
    time.sleep(0.5)
    assert gpio.read_pwm(12) > 0.9

    mqtt_client.publish("home/testDevice/p1/set", "128")
    time.sleep(0.5)
    duty = gpio.read_pwm(12)
    assert 0.4 <= duty <= 0.6, f"Expected ~0.5, got {duty}"


def test_pwm_increase_decrease(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    gpio: Gpio,
    pwm_setup: None,
) -> None:
    """64 then +64 -> ~0.5; 192 then -64 -> ~0.5."""
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_client.publish("home/testDevice/p1/set", "64")
    time.sleep(0.5)
    mqtt_client.publish("home/testDevice/p1/set", "+64")
    time.sleep(0.5)
    duty = gpio.read_pwm(12)
    assert 0.4 <= duty <= 0.6, f"Expected ~0.5, got {duty}"

    mqtt_client.publish("home/testDevice/p1/set", "192")
    time.sleep(0.5)
    mqtt_client.publish("home/testDevice/p1/set", "-64")
    time.sleep(0.5)
    duty = gpio.read_pwm(12)
    assert 0.4 <= duty <= 0.6, f"Expected ~0.5, got {duty}"


def test_pwm_invert(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    gpio: Gpio,
    pwm_setup: None,
) -> None:
    """p1=64 -> ~0.25; p2(invert)=64 -> ~0.75."""
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_client.publish("home/testDevice/p1/set", "64")
    time.sleep(0.5)
    duty_p1 = gpio.read_pwm(12)
    assert 0.15 <= duty_p1 <= 0.35, f"Expected ~0.25, got {duty_p1}"

    mqtt_client.publish("home/testDevice/p2/set", "64")
    time.sleep(0.5)
    duty_p2 = gpio.read_pwm(13)
    assert 0.65 <= duty_p2 <= 0.85, f"Expected ~0.75, got {duty_p2}"