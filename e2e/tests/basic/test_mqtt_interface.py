"""Test MQTT interface: subscribe to topic, republish to another."""

from collections.abc import Iterator
from pathlib import Path
import time

from conftest import assert_booted
from lib.device import Device
from lib.gpio import Gpio
from lib.mqtt_client import MqttClient


def test_mqtt_passthrough(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Publish to m1 input topic -> m1/out receives same payload."""
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_client.clear_state("home/testDevice/m1/out")
    mqtt_client.publish("home/testDevice/m1/in", "hello")
    assert mqtt_client.wait_for_state(
        "home/testDevice/m1/out", "hello", timeout=10.0
    )


def test_mqtt_command_to_output(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    gpio: Gpio,
) -> None:
    """Publish to output command topic via MQTT -> GPIO output changes."""
    assert_booted(device, mqtt_client, restarted=True)

    gpio.setup_input(5)
    try:
        mqtt_client.publish("home/testDevice/o1/set", "on")
        time.sleep(0.5)
        assert gpio.read(5) == 1

        mqtt_client.publish("home/testDevice/o1/set", "off")
        time.sleep(0.5)
        assert gpio.read(5) == 0
    finally:
        gpio.set_boot_safe_state()