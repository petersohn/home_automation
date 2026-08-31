"""Test PWM interfaces: ESP outputs PWM, RPi samples duty cycle."""

from collections.abc import Iterator
from pathlib import Path

import pytest

from conftest import assert_booted
from lib.device import Device
from lib.gpio import Gpio
from lib.mqtt_client import MqttClient


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
    assert gpio.wait_for_pwm(12, 0.0, 0.1)

    mqtt_client.publish("home/testDevice/p1/set", "on")
    assert gpio.wait_for_pwm(12, 0.9, 1.0)

    mqtt_client.publish("home/testDevice/p1/set", "128")
    assert gpio.wait_for_pwm(12, 0.4, 0.6), f"Duty out of range: {gpio.read_pwm(12)}"


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
    assert gpio.wait_for_pwm(12, 0.15, 0.35)
    mqtt_client.publish("home/testDevice/p1/set", "+64")
    assert gpio.wait_for_pwm(12, 0.4, 0.6), f"Duty out of range: {gpio.read_pwm(12)}"

    mqtt_client.publish("home/testDevice/p1/set", "192")
    assert gpio.wait_for_pwm(12, 0.65, 0.85)
    mqtt_client.publish("home/testDevice/p1/set", "-64")
    assert gpio.wait_for_pwm(12, 0.4, 0.6), f"Duty out of range: {gpio.read_pwm(12)}"


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
    assert gpio.wait_for_pwm(12, 0.15, 0.35), f"Duty p1 out of range: {gpio.read_pwm(12)}"

    mqtt_client.publish("home/testDevice/p2/set", "64")
    assert gpio.wait_for_pwm(13, 0.65, 0.85), f"Duty p2 out of range: {gpio.read_pwm(13)}"