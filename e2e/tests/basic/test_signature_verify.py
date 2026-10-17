"""Hardware acceptance tests for the signatureVerify interface.

These tests run on a real ESP against the production BearSSL backend. Provisioning
uses isolated temporary config directories; host private keys never enter SPIFFS.
"""

import base64
import copy
import json
import shutil
import tempfile
import time
from collections.abc import Iterator
from pathlib import Path
from typing import Any
from uuid import uuid4

import pytest

from conftest import assert_booted
from lib.device import Device
from lib.mqtt_broker import MqttBroker
from lib.mqtt_client import MqttClient

E2E_DIR = Path(__file__).resolve().parents[2]
FIXTURES_DIR = E2E_DIR / "signing_fixtures"
VECTORS_PATH = FIXTURES_DIR / "host" / "vectors.json"
DEVICE_CERTS_DIR = FIXTURES_DIR / "device"

COMMAND_TOPIC = "home/testDevice/sv1/set"
RESULT_TOPIC = "home/testDevice/sv1/state"
STATUS_ACTION_TOPIC = "home/testDevice/s1/state"
PASSTHROUGH_INPUT_TOPIC = "home/testDevice/m1/in"
PASSTHROUGH_OUTPUT_TOPIC = "home/testDevice/m1/out"
DEVICE_STATUS_TOPIC = "home/testDevice/status"

VALID_CERT_NAME = "signing_test.pem"
DEVICE_CERT_ALIAS = "signing_test.pem"
WHITESPACE_CERT_NAME = "signing_test_whitespace.pem"
INVALID_CERT_CASES = [
    pytest.param(
        (
            "malformed",
            "signing_test_malformed.pem",
            "ERROR invalid certificate",
        ),
        id="malformed",
    ),
    pytest.param(
        ("DER", "signing_test_der.der", "ERROR invalid certificate"), id="der"
    ),
    pytest.param(
        (
            "trailing junk",
            "signing_test_trailing_junk.pem",
            "ERROR invalid certificate",
        ),
        id="trailing-junk",
    ),
    pytest.param(
        (
            "other PEM block",
            "signing_test_other_pem_block.pem",
            "ERROR invalid certificate",
        ),
        id="other-pem-block",
    ),
    pytest.param(
        (
            "concatenated",
            "signing_test_concatenated.pem",
            "ERROR invalid certificate",
        ),
        id="concatenated",
    ),
    pytest.param(
        (
            "oversized",
            "signing_test_oversized.pem",
            "ERROR invalid certificate",
        ),
        id="oversized",
    ),
    pytest.param(
        (
            "unsupported curve",
            "signing_test_unsupported_curve.pem",
            "ERROR unsupported key",
        ),
        id="unsupported-curve",
    ),
    pytest.param(
        (
            "unsupported algorithm",
            "signing_test_unsupported_algorithm.pem",
            "ERROR unsupported key",
        ),
        id="unsupported-algorithm",
    ),
    pytest.param(
        ("missing", None, "ERROR certificate unavailable"), id="missing"
    ),
]

RESULT_TIMEOUT = 15.0
RECONNECT_RESULT_TIMEOUT = 5.0
RECONNECT_ATTEMPTS = 8


@pytest.fixture(scope="module")
def vectors() -> dict[str, Any]:
    return json.loads(VECTORS_PATH.read_text())


def envelope(vector: dict[str, Any]) -> str:
    """Build the wire envelope for one raw64 signature vector."""
    data = bytes.fromhex(vector["data_hex"])
    signature = bytes.fromhex(vector["signature_hex"])
    command = json.dumps(
        {
            "data": base64.b64encode(data).decode("ascii"),
            "signature": base64.b64encode(signature).decode("ascii"),
        },
        separators=(",", ":"),
    )
    assert len(command.encode("ascii")) <= 512, "vector exceeds command limit"
    return command


def send_and_wait(mqtt_client: MqttClient, command: str) -> str:
    """Publish one envelope and wait for a new result event, even if identical."""
    previous_event = mqtt_client.get_event_count(RESULT_TOPIC)
    mqtt_client.publish(COMMAND_TOPIC, command)
    result = mqtt_client.wait_for_next_state(
        RESULT_TOPIC, previous_event, timeout=RESULT_TIMEOUT
    )
    assert result is not None, f"no result published on {RESULT_TOPIC}"
    return result


def send_signed_command_until_ok(mqtt_client: MqttClient, command: str) -> None:
    """Retry only transient publish/no-response failures after broker restart."""
    for attempt in range(RECONNECT_ATTEMPTS):
        previous_event = mqtt_client.get_event_count(RESULT_TOPIC)
        try:
            mqtt_client.publish(COMMAND_TOPIC, command)
        except ConnectionError:
            result = None
        else:
            result = mqtt_client.wait_for_next_state(
                RESULT_TOPIC, previous_event, timeout=RECONNECT_RESULT_TIMEOUT
            )
        if result == "OK":
            return
        if result is not None:
            raise AssertionError(
                f"expected signed command result 'OK', got {result!r}"
            )
        if attempt + 1 < RECONNECT_ATTEMPTS:
            time.sleep(0.5)
    raise AssertionError("no signed-command result after broker restart")


def _make_temp_config(certificate_name: str | None, device_config_path: Path) -> Path:
    """Create isolated config with signature and MQTT-status test actions."""
    config = copy.deepcopy(json.loads(device_config_path.read_text()))
    certificate_filename = (
        DEVICE_CERT_ALIAS
        if certificate_name is not None
        else "signing_test_missing.pem"
    )
    certificate_path = f"/{certificate_filename}"
    config["interfaces"].append(
        {
            "type": "signatureVerify",
            "name": "sv1",
            "commandTopic": COMMAND_TOPIC,
            "certificate": certificate_path,
        }
    )
    config["actions"].extend(
        [
            {
                "type": "publish",
                "interface": "sv1",
                "topic": RESULT_TOPIC,
                "template": "%1",
            },
            {
                "type": "publish",
                "interface": "s1",
                "topic": STATUS_ACTION_TOPIC,
                "template": "%1",
            },
        ]
    )

    temp_dir = Path(tempfile.mkdtemp(prefix="signing_config_"))
    (temp_dir / "device_config.json").write_text(json.dumps(config, indent=4) + "\n")
    if certificate_name is not None:
        shutil.copy2(
            DEVICE_CERTS_DIR / certificate_name, temp_dir / DEVICE_CERT_ALIAS
        )
    return temp_dir


def _provision_certificate(
    certificate_name: str | None,
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Path:
    temp_dir = _make_temp_config(
        certificate_name, device_config / "device_config.json"
    )
    device.upload_config([temp_dir, global_config_path])
    mqtt_client.subscribe(RESULT_TOPIC)
    mqtt_client.subscribe(STATUS_ACTION_TOPIC)
    return temp_dir


@pytest.fixture(scope="module", autouse=True)
def restore_basic_config(
    device_config: Path,
    device: Device,
    global_config_path: Path,
) -> Iterator[None]:
    """Restore base configuration after signature scenarios finish."""
    yield
    device.upload_config([device_config, global_config_path])


@pytest.fixture
def provisioning_complete(
    request: pytest.FixtureRequest,
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> None:
    """Provision this test's signing config before serial capture starts."""
    for fixture_name in (
        "valid_signing_config",
        "whitespace_signing_config",
        "invalid_certificate_config",
    ):
        if fixture_name in request.fixturenames:
            request.getfixturevalue(fixture_name)
            break


@pytest.fixture
def valid_signing_config(
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[None]:
    temp_dir = _provision_certificate(
        VALID_CERT_NAME, device_config, device, global_config_path, mqtt_client
    )
    try:
        yield
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture
def whitespace_signing_config(
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[None]:
    temp_dir = _provision_certificate(
        WHITESPACE_CERT_NAME, device_config, device, global_config_path, mqtt_client
    )
    try:
        yield
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture(params=INVALID_CERT_CASES)
def invalid_certificate_config(
    request: pytest.FixtureRequest,
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[tuple[str, str]]:
    """Provision rejected certificate configurations without a device restore."""
    scenario, certificate_name, expected_result = request.param
    temp_dir = _provision_certificate(
        certificate_name, device_config, device, global_config_path, mqtt_client
    )
    try:
        yield scenario, expected_result
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


def test_signed_empty_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """An empty payload with a valid signature is accepted."""
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, envelope(vectors["vectors"]["empty"])) == "OK"


def test_signed_binary_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Binary payload bytes verify unchanged."""
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary"])) == "OK"


def test_signed_max256_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """The maximum 256-byte payload fits command limit and verifies."""
    assert_booted(device, mqtt_client, restarted=True)
    assert (
        send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary_max256"]))
        == "OK"
    )


def test_altered_data_fails(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """A signature paired with altered data fails verification."""
    assert_booted(device, mqtt_client, restarted=True)
    assert (
        send_and_wait(
            mqtt_client, envelope(vectors["vectors"]["binary_altered_data"])
        )
        == "FAIL invalid signature"
    )


def test_wrong_key_fails(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """A same-curve signature from a different key fails verification."""
    assert_booted(device, mqtt_client, restarted=True)
    assert (
        send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary_wrong_key"]))
        == "FAIL invalid signature"
    )


def test_malformed_json_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, "not json at all") == "ERROR invalid JSON"


@pytest.mark.parametrize("signature_size", [63, 65], ids=["short", "long"])
def test_wrong_signature_length_error(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    signature_size: int,
) -> None:
    """Only signatures of exactly 64 bytes are accepted."""
    assert_booted(device, mqtt_client, restarted=True)
    data = bytes.fromhex(vectors["vectors"]["binary"]["data_hex"])
    command = json.dumps(
        {
            "data": base64.b64encode(data).decode("ascii"),
            "signature": base64.b64encode(bytes(signature_size)).decode("ascii"),
        },
        separators=(",", ":"),
    )
    assert (
        send_and_wait(mqtt_client, command) == "ERROR invalid signature length"
    )


@pytest.mark.parametrize(
    "data_value,signature_value",
    [
        pytest.param(
            "!!!!", base64.b64encode(bytes(64)).decode("ascii"), id="bad-alphabet"
        ),
        pytest.param(
            "AB==",
            base64.b64encode(bytes(64)).decode("ascii"),
            id="noncanonical-pad-bits",
        ),
        pytest.param("", "?", id="invalid-signature-base64"),
    ],
)
def test_invalid_base64_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    data_value: str,
    signature_value: str,
) -> None:
    """Malformed and non-canonical base64 return exact error string."""
    assert_booted(device, mqtt_client, restarted=True)
    command = json.dumps(
        {"data": data_value, "signature": signature_value}, separators=(",", ":")
    )
    assert send_and_wait(mqtt_client, command) == "ERROR invalid base64"


@pytest.mark.parametrize(
    "command,expected",
    [
        pytest.param("x" * 513, "ERROR command too large", id="oversized-command"),
        pytest.param(
            json.dumps(
                {
                    "data": base64.b64encode(bytes(257)).decode("ascii"),
                    "signature": base64.b64encode(bytes(64)).decode("ascii"),
                },
                separators=(",", ":"),
            ),
            "ERROR data too large",
            id="oversized-data",
        ),
    ],
)
def test_size_limits_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    command: str,
    expected: str,
) -> None:
    """Commands and decoded payloads beyond protocol limits are rejected."""
    assert_booted(device, mqtt_client, restarted=True)
    assert len(command.encode("ascii")) <= 512 or expected == "ERROR command too large"
    assert send_and_wait(mqtt_client, command) == expected


@pytest.mark.parametrize(
    "shape", ["unknown", "duplicate", "missing-data", "missing-signature"]
)
def test_invalid_envelope_fields_error(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    shape: str,
) -> None:
    """Unknown, duplicate, and missing envelope fields are rejected."""
    assert_booted(device, mqtt_client, restarted=True)
    vector = vectors["vectors"]["binary"]
    data = base64.b64encode(bytes.fromhex(vector["data_hex"])).decode("ascii")
    signature = base64.b64encode(bytes.fromhex(vector["signature_hex"])).decode(
        "ascii"
    )
    if shape == "unknown":
        command = f'{{"data":"{data}","signature":"{signature}","extra":1}}'
    elif shape == "duplicate":
        command = f'{{"data":"{data}","data":"{data}","signature":"{signature}"}}'
    elif shape == "missing-data":
        command = f'{{"signature":"{signature}"}}'
    else:
        command = f'{{"data":"{data}"}}'
    assert send_and_wait(mqtt_client, command) == "ERROR invalid fields"


def test_whitespace_certificate_accepted(
    whitespace_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """A single PEM certificate with surrounding ASCII whitespace is accepted."""
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary"])) == "OK"


def test_bad_command_recovery(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """A malformed command does not block a subsequent valid verification."""
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, "not json") == "ERROR invalid JSON"
    assert send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary"])) == "OK"


def test_repeated_identical_ok_results_are_distinct_events(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Repeated identical result strings each produce a new MQTT event."""
    assert_booted(device, mqtt_client, restarted=True)
    first_count = mqtt_client.get_event_count(RESULT_TOPIC)
    command = envelope(vectors["vectors"]["binary"])
    assert send_and_wait(mqtt_client, command) == "OK"
    first_result_count = mqtt_client.get_event_count(RESULT_TOPIC)
    assert first_result_count > first_count
    assert send_and_wait(mqtt_client, command) == "OK"
    assert mqtt_client.get_event_count(RESULT_TOPIC) > first_result_count


def test_rejected_certificate_shapes_error(
    invalid_certificate_config: tuple[str, str],
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Invalid or unavailable certificate yields its exact error string."""
    assert_booted(device, mqtt_client, restarted=True)
    scenario, expected_result = invalid_certificate_config
    result = send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary"]))
    assert result == expected_result, scenario


def test_reconnect_then_verify(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    mqtt_broker: MqttBroker,
) -> None:
    """After MQTT broker reconnect, the device verifies a signed command."""
    assert_booted(device, mqtt_client, restarted=True)
    assert mqtt_client.wait_ready(timeout=10.0), (
        "MQTT observer not ready before restart"
    )
    previous_ready_generation = mqtt_client.get_ready_generation()
    try:
        mqtt_broker.stop()
        time.sleep(3.0)
        mqtt_broker.start()
        assert mqtt_client.wait_ready(
            timeout=60.0, after_generation=previous_ready_generation
        ), "MQTT observer did not reconnect and resubscribe"
        send_signed_command_until_ok(
            mqtt_client, envelope(vectors["vectors"]["binary"])
        )
    finally:
        mqtt_broker.start()


def test_signature_interface_preserves_passthrough_and_status(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Signed commands leave unrelated MQTT passthrough and boot status intact."""
    assert_booted(device, mqtt_client, restarted=True)
    assert mqtt_client.wait_for_state(
        STATUS_ACTION_TOPIC, "1", timeout=RESULT_TIMEOUT
    )
    status_before = mqtt_client.get_state_json(DEVICE_STATUS_TOPIC)
    assert status_before is not None
    assert status_before.get("restarted") is True
    assert mqtt_client.get_state(STATUS_ACTION_TOPIC) == "1"

    passthrough = f"signature-regression-{uuid4().hex}"
    previous_passthrough_event = mqtt_client.get_event_count(PASSTHROUGH_OUTPUT_TOPIC)
    mqtt_client.publish(PASSTHROUGH_INPUT_TOPIC, passthrough)
    assert mqtt_client.wait_for_next_state(
        PASSTHROUGH_OUTPUT_TOPIC,
        previous_passthrough_event,
        timeout=RESULT_TIMEOUT,
    ) == passthrough

    assert send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary"])) == "OK"

    passthrough_after = f"signature-regression-after-{uuid4().hex}"
    assert passthrough_after != passthrough
    previous_passthrough_event = mqtt_client.get_event_count(PASSTHROUGH_OUTPUT_TOPIC)
    mqtt_client.publish(PASSTHROUGH_INPUT_TOPIC, passthrough_after)
    assert mqtt_client.wait_for_next_state(
        PASSTHROUGH_OUTPUT_TOPIC,
        previous_passthrough_event,
        timeout=RESULT_TIMEOUT,
    ) == passthrough_after
    assert mqtt_client.get_state_json(DEVICE_STATUS_TOPIC) == status_before
