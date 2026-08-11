"""Session-scoped fixtures for e2e tests."""

from pathlib import Path

import pytest

from lib.device import Device, MQTT_USERNAME, MQTT_PASSWORD
from lib.gpio import Gpio
from lib.mqtt_broker import MqttBroker
from lib.mqtt_client import MqttClient
from lib.serial_capture import SerialCapture
from lib.wifi_ap import WifiAp
from lib.pin_map import ESP_TO_RPI, RESET_PIN, SERIAL_PORT, SERIAL_BAUD


@pytest.fixture(scope="session")
def mqtt_broker():
    broker = MqttBroker()
    if not broker.is_running():
        broker.start()
    yield broker
    broker.stop()


@pytest.fixture(scope="session")
def wifi_ap():
    ap = WifiAp()
    ap.up()
    yield ap
    ap.down()


@pytest.fixture(scope="session")
def mqtt_client(mqtt_broker):
    client = MqttClient(
        host="localhost",
        port=1883,
        username=MQTT_USERNAME,
        password=MQTT_PASSWORD,
    )
    # Subscribe to all expected topics
    for topic in [
        "home/testDevice/available",
        "home/testDevice/status",
        "home/testDevice/i1/state",
        "home/testDevice/i2/state",
        "home/testDevice/m1/out",
    ]:
        client.subscribe(topic)
    yield client
    client.disconnect()


@pytest.fixture(scope="session")
def gpio():
    g = Gpio()
    yield g
    g.cleanup()


@pytest.fixture(scope="session")
def device(gpio, mqtt_client):
    dev = Device(
        serial_port=SERIAL_PORT,
        reset_pin=RESET_PIN,
        gpio=gpio,
    )
    dev.set_mqtt_client(mqtt_client)
    yield dev


@pytest.fixture(scope="session")
def global_config_path():
    return Path(__file__).parent / "configs" / "global_config.json"


def assert_booted(device, mqtt_client, restarted=True):
    """Helper: wait for boot, assert restarted flag in status."""
    assert device.wait_for_boot(), "Device did not boot in time"
    status = mqtt_client.get_state_json("home/testDevice/status")
    assert status is not None, "No status message received"
    assert status.get("restarted") is restarted, (
        f"Expected restarted={restarted}, got {status.get('restarted')}"
    )