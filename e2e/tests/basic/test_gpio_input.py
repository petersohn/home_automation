"""Test GPIO input interfaces: ESP reads RPi-driven pin state, publishes via MQTT."""

import pytest

from conftest import assert_booted
from lib.pin_map import ESP_TO_RPI


@pytest.fixture
def i1_setup(gpio):
    """Set up RPi to drive ESP GPIO14 (i1)."""
    gpio.setup_output(14, value=False)
    yield
    gpio.set_boot_safe_state()


def test_input_toggle_on_off(device_config, reset_device, mqtt_client, gpio, i1_setup):
    """Drive i1 high -> MQTT '1'; drive low -> MQTT '0'."""
    assert_booted(device, mqtt_client, restarted=True)

    gpio.write(14, True)
    assert mqtt_client.wait_for_state("home/testDevice/i1/state", "1", timeout=10.0)

    gpio.write(14, False)
    assert mqtt_client.wait_for_state("home/testDevice/i1/state", "0", timeout=10.0)