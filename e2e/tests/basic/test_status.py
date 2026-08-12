"""Test status interface: availability topic + status message with restarted flag."""

from pathlib import Path

import pytest

from conftest import assert_booted
from lib.device import Device
from lib.mqtt_client import MqttClient


def test_status_online_on_boot(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """After boot, availability='1' and status has restarted=true."""
    assert_booted(device, mqtt_client, restarted=True)
    assert mqtt_client.get_state("home/testDevice/available") == "1"