# End-to-End Tests Design

## Goal

Create e2e tests that run on real hardware: Raspberry Pi 3 B+ controlling an
ESP12 (ESP8266) device via GPIO, MQTT, and serial. Tests run in CircleCI on a
self-hosted RPi runner. Git push triggers CI → build on cloud → download to RPi
→ upload to device → test.

## Scope

Core GPIO + MQTT interface types only for initial suite:

- `input` (GPIO binary input)
- `output` (GPIO binary output)
- `pwm` (PWM output via `analogWrite`)
- `mqtt` (MQTT subscribe interface)
- `status` (MQTT connection status)

Specialty suites (sensors, cover, powerSupply, keepalive, encoder, etc.) added
later with own configs.

## Architecture

### Hardware

- Raspberry Pi 3 B+ running Raspbian
- ESP12 (ESP8266) device
- RPi GPIO pins wired directly to ESP GPIO pins (both 3.3V, no level shifting)
- RPi configured as WiFi AP; ESP connects to it
- RPi GPIO pin wired to ESP reset pin (RST) for hardware reset
- Serial connection via RPi GPIO UART (BCM14=TXD, BCM15=RXD) directly to ESP GPIO3/1
- RPi UART dedicated to ESP serial; Bluetooth disabled
- RPi I2C and SPI disabled (pins used as GPIO for e2e)

### CI Flow

1. `git push` → CircleCI triggers
2. `build-firmware` job runs on cloud → compiles firmware → persists `.bin`
3. `e2e-tests` job runs on RPi self-hosted runner:
   - Downloads firmware artifact
   - Uploads firmware to ESP via `flash.py upload --image <firmware.bin>`
   - Runs `pytest e2e/tests/`
   - Each suite uploads its config via `flash.py upload-fs`
   - Each test resets device, captures serial, asserts
   - Serial logs stored as CircleCI artifacts

## Project Structure

```
e2e/
├── conftest.py              # session-scoped shared fixtures
├── pytest.ini
├── configs/
│   ├── global_config.json   # single, shared across all suites
│   └── basic/
│       └── device_config.json
├── lib/
│   ├── __init__.py
│   ├── device.py            # ESP control: reset, wait_for_boot, upload
│   ├── gpio.py              # RPi GPIO wrapper: drive, read, read_pwm
│   ├── mqtt_broker.py       # Mosquitto start/stop
│   ├── mqtt_client.py       # paho-mqtt wrapper: subscribe, publish, wait
│   ├── serial_capture.py    # pyserial: capture to file per test
│   ├── wifi_ap.py           # RPi AP control: up/down
│   └── pin_map.py           # ESP GPIO pin -> RPi BCM pin mapping
├── tests/
│   └── basic/
│       ├── __init__.py
│       ├── conftest.py      # package-scoped config fixture
│       ├── test_gpio_input.py
│       ├── test_gpio_output.py
│       ├── test_pwm.py
│       ├── test_mqtt_interface.py
│       ├── test_status.py
│       └── test_connectivity.py
├── ci/
│   └── .circleci/
│       └── config.yml
└── README.md
```

### Config organization

- `configs/global_config.json`: single shared global config (wifi SSID,
  password, MQTT servers). Same across all test suites.
- `configs/<suite_name>/device_config.json`: per-suite device config.
- `global_config.json` lives at `configs/` root; suite dirs only contain
  `device_config.json`.

### Test organization

Each suite is a directory under `e2e/tests/` with `__init__.py` (making it a
pytest package). A `conftest.py` in each suite dir declares `config_name` and
provides the package-scoped `device_config` fixture. Multiple `.py` test files
within the directory share one config upload.

Example:

```
e2e/tests/basic/
├── __init__.py
├── conftest.py          # config_name = "basic", package-scoped fixtures
├── test_gpio_input.py
├── test_gpio_output.py
├── test_pwm.py
├── test_mqtt_interface.py
├── test_status.py
└── test_connectivity.py
```

Specialty suites added later as sibling directories with own configs.

## Hardware Layer (`lib/`)

Each module thin, single responsibility. Hardware details isolated; tests do
not touch RPi.GPIO or pyserial directly.

### `pin_map.py`

Maps ESP GPIO pins to RPi BCM pin numbers. Optimized for physical alignment:
ESP-12F placed with GND near RPi pin 1 (left column), VCC near RPi pin 2
(right column). Same-side connections (ESP right→RPi left, ESP left→RPi right)
preferred for uninsulated wires. Config GPIOs distance ≤1 on same side. One
crossing (GPIO14→BCM22, dist 4, insulated) unavoidable.

```python
ESP_TO_RPI = {
    0: 4,    # ESP GPIO0  -> RPi BCM4  (pin 7)   boot strapping, direct
    2: 3,    # ESP GPIO2  -> RPi BCM3  (pin 5)   boot strapping, direct
    4: 17,   # ESP GPIO4  -> RPi BCM17 (pin 11)  input i2, same side, dist 1
    5: 27,   # ESP GPIO5  -> RPi BCM27 (pin 13)  output o1, same side, dist 1
    12: 24,  # ESP GPIO12 -> RPi BCM24 (pin 18)  pwm p1, same side, dist 6
    13: 25,  # ESP GPIO13 -> RPi BCM25 (pin 22)  pwm p2 (inverted), same side, dist 8
    14: 22,  # ESP GPIO14 -> RPi BCM22 (pin 15)  input i1, cross, dist 4 (insulated)
    15: 2,   # ESP GPIO15 -> RPi BCM2  (pin 3)   boot strapping, direct
    16: 18,  # ESP GPIO16 -> RPi BCM18 (pin 12)  output o2 (inverted), same side, dist 1
    # ESP GPIO1 (TXD) -> RPi BCM15 (RXD, pin 10), serial, cross, dist 4 (insulated)
    # ESP GPIO3 (RXD) -> RPi BCM14 (TXD, pin 8),  serial, cross, dist 4 (insulated)
    # GPIO6-11:  connected to flash chip, not usable
}

RESET_PIN = 23  # RPi BCM23 (pin 16) -> ESP RST, direct

SERIAL_PORT = "/dev/serial0"
SERIAL_BAUD = 115200
```

Note: RPi I2C (BCM2,3), SPI (BCM4,7,8,9,10,11), and Bluetooth (BCM14,15) all
disabled. RPi is dedicated to e2e testing only.

### Physical alignment

```
ESP right (bottom→top): GND  GPIO15  GPIO2  GPIO0  GPIO4  GPIO5  GPIO3  GPIO1
ESP left  (bottom→top): VCC  GPIO13  GPIO12  GPIO14  GPIO16  EN     ADC   RST
RPi left  (pins):          1    3       5      7      9      11     13     15
RPi right (pins):          2    4       6      8      10     12     14     16
```

ESP-12F placed with antenna facing away from RPi. GND near RPi pin 1 (left),
VCC near RPi pin 2 (right). Same-side connections (ESP right→RPi left column,
ESP left→RPi right column) use uninsulated wires. Crossings use insulated wires.

### Wiring table

| ESP pin | Function | RPi BCM | RPi pin | Side | Distance | Wire |
|---------|----------|---------|---------|------|----------|------|
| VCC | power | — | 1 | cross | 0 | direct (opposite) |
| GND | power | — | 9 | same | 4 | uninsulated |
| RST | reset | BCM23 | 16 | same | 0 | direct |
| GPIO15 | boot | BCM2 | 3 | same | 0 | direct |
| GPIO2 | boot | BCM3 | 5 | same | 0 | direct |
| GPIO0 | boot | BCM4 | 7 | same | 0 | direct |
| GPIO4 | i2 (input) | BCM17 | 11 | same | 1 | uninsulated |
| GPIO5 | o1 (output) | BCM27 | 13 | same | 1 | uninsulated |
| GPIO16 | o2 (output, inv) | BCM18 | 12 | same | 1 | uninsulated |
| GPIO14 | i1 (input) | BCM22 | 15 | cross | 4 | insulated |
| GPIO12 | p1 (pwm) | BCM24 | 18 | same | 6 | uninsulated |
| GPIO13 | p2 (pwm, inv) | BCM25 | 22 | same | 8 | uninsulated |
| GPIO1 | serial TXD | BCM15 | 10 | cross | 4 | insulated |
| GPIO3 | serial RXD | BCM14 | 8 | cross | 4 | insulated |

Totals: 3 direct (dist 0), 5 same-side uninsulated (dist 1-8), 3 insulated
crossings (dist 4), 1 same-side uninsulated power (dist 4).

### Config upload flow

ESP boot strapping pins: GPIO0 (must be high), GPIO2 (must be high), GPIO15
(must be low). All three wired to RPi. RPi sets all to input at boot (high-Z)
so ESP internal pull resistors handle strapping (GPIO0/2 pull-up, GPIO15
pull-down):

1. RPi: set GPIO0, GPIO2, GPIO15 to input (boot-safe state)
2. Reset ESP, upload config (via `flash.py upload-fs`)
3. RPi: set all pins to appropriate state for tests
4. Reset ESP

During normal reset (between tests): set GPIO0, GPIO2, GPIO15 back to input
before reset, then reset.

### `gpio.py`

RPi.GPIO wrapper.

```python
class Gpio:
    def setup_input(pin: int) -> None
    def setup_output(pin: int, value: bool = False) -> None
    def write(pin: int, value: bool) -> None     # drive ESP input
    def read(pin: int) -> bool                   # read ESP output
    def read_pwm(pin: int, sample_ms: int = 10) -> float
        # rapid sample for sample_ms, return duty cycle ratio 0.0-1.0
    def set_boot_safe_state() -> None
        # set GPIO0 (BCM4), GPIO2 (BCM3), GPIO15 (BCM2) to input
        # ESP internal pull resistors handle boot strapping
    def cleanup() -> None
```

`read_pwm` samples the pin rapidly in a tight loop for `sample_ms` milliseconds,
counts high vs low samples, returns ratio. Crude but enough to distinguish
0% (0.0), ~50% (0.5), 100% (1.0). Accurate enough for 0/128/255 values.

`set_boot_safe_state` sets RPi GPIO0 (BCM4), GPIO2 (BCM3), GPIO15 (BCM2) to
input (high-Z) so ESP internal pull resistors handle boot strapping (GPIO0/2
pull-up, GPIO15 pull-down). Called before ESP reset during config upload and
between tests.

### `device.py`

ESP device control. Uses `SERIAL_PORT` from `pin_map.py`.

```python
class Device:
    def __init__(self, serial_port: str, reset_pin: int, gpio: Gpio)
    def reset() -> None
        # set GPIO0/2/15 to input, pulse RST low 50ms
    def wait_for_boot(timeout: float = 30.0) -> bool
        # subscribe to statusTopic, wait for availability message
    def upload_firmware(firmware_bin: Path) -> int
        # gpio.set_boot_safe_state(); call: flash.py upload --image <path> --port <SERIAL_PORT>
    def upload_config(config_inputs: list[Path]) -> int
        # gpio.set_boot_safe_state(); call: flash.py upload-fs <inputs...> --port <SERIAL_PORT>
```

### `mqtt_broker.py`

Mosquitto control.

```python
class MqttBroker:
    def start() -> None       # systemctl start mosquitto
    def stop() -> None         # systemctl stop mosquitto
    def is_running() -> bool
```

### `mqtt_client.py`

paho-mqtt wrapper. Background thread receives all messages, keeps latest state
per topic. QoS 0.

```python
class MqttClient:
    def __init__(host, port, username, password)
    def subscribe(topic: str) -> None
    def publish(topic: str, payload: str, retain: bool = False) -> None
    def wait_for_state(topic: str, expected: str, timeout: float) -> bool
        # bg thread keeps latest state per topic; wait until matches expected
    def get_state(topic: str) -> str | None     # latest retained/received
    def clear_state(topic: str) -> None
    def clear_all() -> None
    def disconnect() -> None
    def reconnect() -> None
        # clear all state, resubscribe to all known topics
```

### Status/availability timing

Firmware rate-limits status/availability sends to 60s (`statusSendInterval`).
Both LWT "0" and availability "1" are published with `retain=true`.

On first boot connect: sends immediately (`nextStatusSend = now`).
On MQTT broker restart: ESP receives own LWT "0" (retained) → triggers
`refreshAvailability()` → immediate send on reconnect.
On WiFi AP restart: ESP doesn't receive LWT (not connected to broker) →
status send rate-limited → up to 60s delay for availability "1".

Since messages are retained, test client receives them on (re)subscribe
regardless of timing — no race condition on message delivery.

Test timeouts (account for ESP reconnect + rate limit, not race):
- Normal boot: `timeout=30.0` (immediate send)
- MQTT broker restart: `timeout=30.0` (immediate after LWT triggers refresh)
- WiFi AP restart: `timeout=90.0` (60s rate limit + reconnect margin)

### `serial_capture.py`

pyserial wrapper. One file per test. Uses `SERIAL_PORT` from `pin_map.py`.

```python
class SerialCapture:
    def __init__(port: str, baud: int, log_dir: Path)
    def start(test_name: str) -> None   # open serial, start bg thread writing to file
    def stop() -> Path                  # close serial, return log file path
```

### `wifi_ap.py`

RPi AP control.

```python
class WifiAp:
    def up() -> None      # start hostapd / bring up AP interface
    def down() -> None    # stop hostapd / bring down AP interface
    def is_up() -> bool
```

## `flash.py` Changes

Add `--image` flag to `upload` and `upload-fs` subcommands. If present, skip
build, upload given pre-built artifact. If absent, current behavior (build +
upload).

Add `--port` flag to override `flash.toml` `[build].port` for the serial
device. Used by e2e tests to specify `/dev/serial0`.

```
python3 flash.py upload --image firmware.bin --port /dev/serial0
python3 flash.py upload                                # build + upload firmware (current)
python3 flash.py upload-fs --image fs.spiffs.bin --port /dev/serial0
python3 flash.py upload-fs configs/basic --port /dev/serial0
```

`inspect-fs` unchanged (always builds from inputs, no upload).

For e2e: firmware built on cloud → downloaded → `upload --image --port /dev/serial0`. Config built
on RPi → `upload-fs` with config dirs and `--port /dev/serial0`.

## Fixtures (`conftest.py`)

### Session-scoped (once per pytest run)

```python
@pytest.fixture(scope="session")
def mqtt_broker():
    broker = MqttBroker()
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
        username="e2e",
        password="e2etest123",
    )
    yield client
    client.disconnect()

@pytest.fixture(scope="session")
def device(mqtt_client):
    dev = Device(serial_port=SERIAL_PORT, reset_pin=RESET_PIN)
    yield dev
```

### Package-scoped (once per test directory)

```python
# e2e/tests/basic/conftest.py
config_name = "basic"

@pytest.fixture(scope="package")
def device_config(request, device, global_config_path):
    config_dir = configs_dir / config_name
    device.upload_config([config_dir, global_config_path])
    yield config_dir
```

No reset, no wait_for_boot after config upload. Tests wait for boot themselves
when needed.

### Function-scoped (per test)

```python
@pytest.fixture
def reset_device(device, mqtt_client):
    device.reset()
    mqtt_client.clear_all()

@pytest.fixture
def serial(reset_device, request):
    cap = SerialCapture(port=SERIAL_PORT, baud=SERIAL_BAUD, log_dir=Path("artifacts"))
    cap.start(request.node.name)
    yield cap
    log_path = cap.stop()
```

Fixture order: `serial` depends on `reset_device` which depends on `device`
(session). Reset happens before each test, serial capture starts after reset.
Config persists across tests in suite (no re-upload).

## Basic Suite Config

`configs/global_config.json`:

```json
{
    "wifiSSID": "e2e-test",
    "wifiPassword": "testpassword",
    "servers": [
        {
            "address": "192.168.10.1",
            "port": 1883,
            "username": "e2e",
            "password": "e2etest123"
        }
    ]
}
```

`address` is the RPi's AP gateway IP (192.168.10.1, configured via hostapd +
dnsmasq). Username/password match the Mosquitto passwd file configured on the
RPi.

`configs/basic/device_config.json`:

```json
{
    "debug": true,
    "name": "testDevice",
    "availabilityTopic": "home/testDevice/available",
    "statusTopic": "home/testDevice/status",
    "debugTopic": "home/testDevice/debug",
    "interfaces": [
        {"type": "input", "name": "i1", "pin": 14},
        {"type": "input", "name": "i2", "pin": 4},
        {"type": "output", "name": "o1", "commandTopic": "home/testDevice/o1/set", "pin": 5},
        {"type": "output", "name": "o2", "commandTopic": "home/testDevice/o2/set", "pin": 16, "invert": true},
        {"type": "pwm", "name": "p1", "commandTopic": "home/testDevice/p1/set", "pin": 12},
        {"type": "pwm", "name": "p2", "commandTopic": "home/testDevice/p2/set", "pin": 13, "invert": true},
        {"type": "mqtt", "name": "m1", "topic": "home/testDevice/m1/in"},
        {"type": "status", "name": "s1"}
    ],
    "actions": [
        {"type": "publish", "interface": "i1", "topic": "home/testDevice/i1/state"},
        {"type": "publish", "interface": "i2", "topic": "home/testDevice/i2/state"},
        {"type": "publish", "interface": "m1", "topic": "home/testDevice/m1/out", "template": "%1"}
    ]
}
```

### Test aggregation

Aggregate related interactions into single tests to reduce reset overhead.
E.g. `test_input_toggle_on_off` drives pin high → assert "1" → drives low →
assert "0" in one test.

### Boot assertion

After reset + `wait_for_boot`, tests assert `restarted=true` in status message.
Helper:

```python
def assert_booted(device, mqtt_client, restarted=True):
    assert device.wait_for_boot()
    mqtt_client.wait_for_state("home/testDevice/status", ..., timeout=30.0)
    # parse status JSON, assert restarted field
```

Normal tests: `assert_booted(device, mqtt_client, restarted=True)`. Connectivity
tests after reconnect: check `restarted=False` (reconnect, not reboot).

## Test Suites

### `test_gpio_input.py`

Config: 2 input interfaces on GPIO 14, 4. Publish actions on state change.

Tests:
- `test_input_toggle_on_off`: RPi drives GPIO high → MQTT "1"; drives low → MQTT "0"
- `test_input_debounce`: rapid toggles below debounce window → single message
- `test_input_cycle_single`: multiple changes within cycle → one reported (needs cycle config, specialty suite)
- `test_input_cycle_multi`: multiple changes within cycle → all reported (needs cycle config, specialty suite)

Note: cycle tests require `cycle` field in input config. Move to specialty
suite `input_cycle/` with own config.

### `test_gpio_output.py`

Config: 2 output interfaces on GPIO 5 (standard), GPIO 16 (invert=true).
Command topics for each.

Tests:
- `test_output_on_off`: publish "on" → RPi reads GPIO high; publish "off" → low
- `test_output_toggle`: publish "toggle" → state flips
- `test_output_blink`: publish "blink 100 100" → RPi reads alternating high/low
- `test_output_default`: verify default value on boot
- `test_output_invert`: publish "on" to o2 (invert=true) → RPi reads low; publish "off" → high

### `test_pwm.py`

Config: PWM output on GPIO 12 (p1) and GPIO 13 (p2, inverted).

Tests:
- `test_pwm_values`: publish "off" → `read_pwm` ≈ 0.0; publish "on" → ≈ 1.0; publish "128" → in [0.4, 0.6]
- `test_pwm_increase_decrease`: publish "64" then "+64" → `read_pwm` in [0.4, 0.6]; publish "192" then "-64" → in [0.4, 0.6]
- `test_pwm_invert`: publish "64" to p1 → `read_pwm` ≈ 0.25; publish "64" to p2 → `read_pwm` ≈ 0.75

### `test_mqtt_interface.py`

Config: MQTT interface subscribing to topic, republishing to another.

Tests:
- `test_mqtt_passthrough`: publish to input topic → output topic receives same payload
- `test_mqtt_command_to_output`: publish to output command topic via MQTT interface → GPIO output changes

### `test_status.py`

Config: status interface + availability topic.

Tests:
- `test_status_online_on_boot`: availabilityTopic receives "1" after boot; status message has `restarted=true`
- `test_status_reconnect`: (see test_connectivity.py for MQTT reconnect; status reports `restarted=false`)

### `test_connectivity.py`

Single test per connectivity type:

```python
def test_wifi(reset_device, device, mqtt_client, wifi_ap, serial):
    assert_booted(device, mqtt_client, restarted=True)

    wifi_ap.down()
    # ESP loses wifi; MQTT connection drops, LWT fires
    mqtt_client.wait_for_state("home/testDevice/available", "0", timeout=60.0)

    wifi_ap.up()
    # ESP reconnects; status send rate-limited (up to 60s delay)
    mqtt_client.wait_for_state("home/testDevice/available", "1", timeout=90.0)

    msg = mqtt_client.wait_for_state("home/testDevice/status", ..., timeout=30.0)
    assert json.loads(msg)["restarted"] is False

def test_mqtt(reset_device, device, mqtt_client, mqtt_broker, serial):
    assert_booted(device, mqtt_client, restarted=True)

    mqtt_broker.stop()
    # LWT published by broker before stopping (retained "0")
    mqtt_broker.start()
    mqtt_client.reconnect()
    # ESP reconnects, receives own LWT "0" → triggers immediate status send
    mqtt_client.wait_for_state("home/testDevice/available", "0", timeout=30.0)
    mqtt_client.wait_for_state("home/testDevice/available", "1", timeout=30.0)

    msg = mqtt_client.wait_for_state("home/testDevice/status", ..., timeout=30.0)
    assert json.loads(msg)["restarted"] is False
```

Note: MQTT failover (multiple servers) requires separate config with multiple
servers in `global_config.json`. Not part of basic suite.

## CI Configuration

### `.circleci/config.yml`

```yaml
version: 2.1
jobs:
  build-firmware:
    docker:
      - image: arduino/cli:latest
    steps:
      - checkout
      - run: arduino-cli compile --fqbn esp8266:esp8266:generic --verify
      - persist_to_workspace:
          root: .
          paths: [build/*.bin]

  e2e-tests:
    machine: true
    resource_class: home_automation/rpi-runner
    steps:
      - checkout
      - attach_workspace:
          at: .
      - run:
          command: pytest e2e/tests/ --ci
          working_directory: e2e
      - store_artifacts:
          path: e2e/artifacts
          destination: serial-logs
```

### `pytest.ini`

```ini
[pytest]
addopts = --ci
testpaths = tests
markers =
    slow: long-running tests
```

`--ci` flag enables artifact storage path behavior.

### RPi setup (user does)

- Install: Python 3.11+, pip packages (pytest, paho-mqtt, pyserial, RPi.GPIO),
  mosquitto, hostapd, dnsmasq
- Configure: RPi as AP (hostapd + dnsmasq), GPIO wiring to ESP
- Serial UART setup:
  - Disable Bluetooth: add `dtoverlay=disable-bt` to `/boot/config.txt`
  - Enable UART: add `enable_uart=1` to `/boot/config.txt`
  - Disable serial console: `sudo raspi-config` → Interface Options → Serial
    Port → No login shell, Yes hardware serial
  - Disable BT service if present: `sudo systemctl disable hciuart` (or
    `bluetooth`/`btuart` — varies by OS version; skip if unit not found,
    `dtoverlay=disable-bt` handles it at hardware level)
- RPi I2C and SPI disabled: `sudo raspi-config` → Interface Options →
  disable I2C and SPI
- Mosquitto setup:
  - `sudo mosquitto_passwd -c /etc/mosquitto/passwd e2e` (password:
    `e2etest123`)
  - `/etc/mosquitto/conf.d/e2e.conf`:
    ```
    listener 1883
    password_file /etc/mosquitto/passwd
    ```
  - `sudo systemctl enable mosquitto`
- WiFi AP setup:
  - `/etc/dnsmasq.conf`:
    ```
    interface=wlan0
    dhcp-range=192.168.10.2,192.168.10.20,255.255.255.0,24h
    ```
  - `/etc/hostapd/hostapd.conf`:
    ```
    interface=wlan0
    driver=nl80211
    ssid=e2e-test
    hw_mode=g
    channel=7
    wmm_enabled=0
    macaddr_acl=0
    auth_algs=1
    ignore_broadcast_ssid=0
    wpa=2
    wpa_passphrase=testpassword
    wpa_key_mgmt=WPA-PSK
    rsn_pairwise=CCMP
    ```
  - `/etc/default/hostapd`: `DAEMON_CONF="/etc/hostapd/hostapd.conf"`
  - Static AP gateway IP. Modern Raspberry Pi OS uses NetworkManager (dhcpcd
    is not installed as a service, so `/etc/dhcpcd.conf` static IPs are
    never applied — this silently breaks DHCP and all e2e boot tests):
    - Mark wlan0 unmanaged by NetworkManager:
      `/etc/NetworkManager/conf.d/unmanage-wlan0.conf`:
      ```
      [keyfile]
      unmanaged-devices=interface-name:wlan0
      ```
      then `sudo systemctl reload NetworkManager`
    - Assign the IP at boot via a oneshot unit
      `/etc/systemd/system/e2e-ap-ip.service`:
      ```
      [Unit]
      Description=Static IP 192.168.10.1/24 on wlan0 for e2e WiFi AP
      After=hostapd.service network.target

      [Service]
      Type=oneshot
      RemainAfterExit=yes
      ExecStart=/usr/sbin/ip addr replace 192.168.10.1/24 dev wlan0

      [Install]
      WantedBy=multi-user.target
      ```
      then `sudo systemctl daemon-reload && sudo systemctl enable --now e2e-ap-ip`
  - `sudo systemctl enable hostapd dnsmasq`
- Wiring: RPi BCM14 (TXD, pin 8) → ESP GPIO3 (RXD), RPi BCM15 (RXD, pin 10)
  → ESP GPIO1 (TXD), GND shared
- CircleCI self-hosted runner agent configured with
  `resource_class: home_automation/rpi-runner`

## Error Handling

- `wait_for_boot` timeout → test fails with clear message, serial log attached
- MQTT message timeout → test fails, serial log shows device state
- GPIO read mismatch → test fails, serial log shows debug output
- Config upload failure → suite setup fails, all tests in suite skipped
- Firmware upload failure → CI job fails before tests run

## Known Firmware Bug (out of scope, fix later)

If MQTT broker restarts after unclean client disconnect, retained availability
"1" persists. ESP receives own stale retained "1" on reconnect, `initState == Done`,
no `refreshAvailability()` triggered. Fresh availability send rate-limited (up
to 60s).

Firmware fix: on reconnect with `initState == Done`, call `refreshAvailability()`
to force fresh publish regardless of retained state.

## Dependencies

### RPi (runtime)

- Python 3.11+
- pytest
- paho-mqtt
- pyserial
- RPi.GPIO
- mosquitto
- hostapd + dnsmasq (for AP)

### Cloud (build)

- arduino-cli
- ESP8266 core (`esp8266:esp8266`)