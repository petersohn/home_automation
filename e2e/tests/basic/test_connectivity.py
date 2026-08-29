"""Test WiFi and MQTT connectivity: disconnect/reconnect behavior."""

from pathlib import Path
import time

from conftest import assert_booted
from lib.device import Device
from lib.mqtt_broker import MqttBroker
from lib.mqtt_client import MqttClient
from lib.wifi_ap import WifiAp


def test_wifi(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    wifi_ap: WifiAp,
) -> None:
    """AP down -> available=0; AP up -> available=1; restarted=false."""
    assert_booted(device, mqtt_client, restarted=True)

    try:
        wifi_ap.down()
        assert mqtt_client.wait_for_state(
            "home/testDevice/available", "0", timeout=60.0
        ), "Device did not go offline when WiFi down"

        mqtt_client.clear_state("home/testDevice/status")
        wifi_ap.up()
        assert mqtt_client.wait_for_state(
            "home/testDevice/available", "1", timeout=90.0
        ), "Device did not come back online when WiFi up"

        assert mqtt_client.wait_for_any_state(
            "home/testDevice/status", timeout=90.0
        ), "No status message after reconnect"
        status = mqtt_client.get_state_json("home/testDevice/status")
        assert status is not None
        assert status.get("restarted") is False, "Expected restarted=false after reconnect"
    finally:
        wifi_ap.up()
        # Drain the device's reconnect publishes so they cannot pollute
        # the next test's boot assertions.
        mqtt_client.wait_for_any_state("home/testDevice/status", timeout=60.0)


def test_mqtt(
    device_config: Path,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    mqtt_broker: MqttBroker,
) -> None:
    """Broker stop/start -> ESP reconnects, restarted=false."""
    assert_booted(device, mqtt_client, restarted=True)

    try:
        mqtt_client.clear_state("home/testDevice/status")
        mqtt_broker.stop()
        time.sleep(2)
        mqtt_broker.start()
        mqtt_client.reconnect()

        assert mqtt_client.wait_for_state(
            "home/testDevice/available", "1", timeout=90.0
        ), "Device did not reconnect to MQTT broker"

        assert mqtt_client.wait_for_any_state(
            "home/testDevice/status", timeout=90.0
        ), "No status message after MQTT reconnect"
        status = mqtt_client.get_state_json("home/testDevice/status")
        assert status is not None
        assert status.get("restarted") is False, "Expected restarted=false after MQTT reconnect"
    finally:
        mqtt_broker.start()
        mqtt_client.reconnect()
        mqtt_client.wait_for_any_state("home/testDevice/status", timeout=60.0)