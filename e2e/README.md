# E2E Tests

End-to-end tests for the ESP8266 firmware, running on real hardware
(Raspberry Pi + ESP-12F).

See `docs/superpowers/specs/2026-08-04-e2e-tests-design.md` for full design.

## Running

    cd e2e
    python3 -m pytest tests/basic/test_gpio_input.py -v

## Hardware setup

See spec for RPi wiring, OS config, and software install instructions.

## Runner provisioning

The e2e tests (and the CircleCI `e2e-tests` job) run on a self-hosted
Raspberry Pi runner that must be provisioned once, outside this repo:

- System packages: `mosquitto`, `hostapd`, `dnsmasq`, `rpi-lgpio`,
  `python3-serial`, and pip packages `pytest`, `pytest-timeout`, `paho-mqtt`,
  `pyserial`.
- The user running the tests needs passwordless sudo for
  `systemctl start/stop hostapd dnsmasq` (used by `e2e/lib/wifi_ap.py`)
  and `systemctl start/stop mosquitto` (used by `e2e/lib/mqtt_broker.py`).
- Serial access: user in the `dialout` group (or equivalent access to
  `/dev/serial0`).
- SPI must be disabled (`dtparam=spi=off` in `/boot/firmware/config.txt`): the test
  wiring uses BCM8 (SPI CE0) as the ESP enable pin.
- WiFi AP is preconfigured (hostapd + dnsmasq config for the bench SSID);
  the tests only start/stop the services.
- On first CI run, arduino-cli and the ESP8266 core are installed by the
  guarded steps in `.circleci/config.yml`; the persistent runner filesystem
  keeps them for later runs.