"""Hardware acceptance tests for the signatureVerify interface.

These tests run on a real ESP (Raspberry Pi runner) against the production
BearSSL backend (see
.superpowers/sdd/2026-10-17-signing-prototype/task-3-brief.md). They never
replace the native unit tests.

Provisioning is isolated: a temporary config directory is built and uploaded
via the existing flash.py helper (Device.upload_config). The normal basic
config is never mutated in place; it is re-uploaded once after all certificate
scenarios so neighboring basic-suite fixtures see an unchanged device.

Fixtures come from e2e/signing_fixtures (see its README.md). Private keys
live in e2e/signing_fixtures/host and are NEVER uploaded to the device; only
certificates from e2e/signing_fixtures/device land in the SPIFFS image.

Expected results (stable interface contract):
    valid signature            -> "OK"
    wrong key / altered data   -> "FAIL invalid signature"
    malformed input            -> "ERROR <message>"
"""

import base64
import copy
import json
import shutil
import tempfile
from collections.abc import Iterator
from pathlib import Path
from typing import Any

import pytest

from conftest import assert_booted
from lib.device import Device
from lib.mqtt_client import MqttClient

E2E_DIR = Path(__file__).resolve().parents[2]
FIXTURES_DIR = E2E_DIR / "signing_fixtures"
VECTORS_PATH = FIXTURES_DIR / "host" / "vectors.json"
DEVICE_CERTS_DIR = FIXTURES_DIR / "device"

COMMAND_TOPIC = "home/testDevice/sv1/set"
RESULT_TOPIC = "home/testDevice/sv1/state"

VALID_CERT_NAME = "signing_test.pem"
WHITESPACE_CERT_NAME = "signing_test_whitespace.pem"
INVALID_CERT_CASES = [
    pytest.param("malformed_cert", "signing_test_malformed.pem", id="malformed"),
    pytest.param("der_cert", "signing_test_der.der", id="der"),
    pytest.param(
        "trailing_junk_cert", "signing_test_trailing_junk.pem", id="trailing-junk"
    ),
    pytest.param(
        "other_pem_block_cert",
        "signing_test_other_pem_block.pem",
        id="other-pem-block",
    ),
    pytest.param(
        "concatenated_cert", "signing_test_concatenated.pem", id="concatenated"
    ),
]

RESULT_TIMEOUT = 15.0


@pytest.fixture(scope="module")
def vectors() -> dict[str, Any]:
    return json.loads(VECTORS_PATH.read_text())


def envelope(vector: dict[str, Any]) -> str:
    """Build the wire envelope for a raw64 signature vector."""
    data = bytes.fromhex(vector["data_hex"])
    signature = bytes.fromhex(vector["signature_hex"])
    command = json.dumps(
        {
            "data": base64.b64encode(data).decode("ascii"),
            "signature": base64.b64encode(signature).decode("ascii"),
        },
        separators=(",", ":"),
    )
    # Prototype limit: a JSON command larger than 512 bytes is rejected, so
    # every generated vector must fit (the 256-byte payload does: 344 + 88 + 34).
    assert len(command.encode("ascii")) <= 512, "vector exceeds command limit"
    return command


def send_and_wait(mqtt_client: MqttClient, command: str) -> str:
    """Publish one signed-envelope command, return the published result."""
    mqtt_client.clear_state(RESULT_TOPIC)
    mqtt_client.publish(COMMAND_TOPIC, command)
    assert mqtt_client.wait_for_any_state(RESULT_TOPIC, timeout=RESULT_TIMEOUT), (
        f"no result published on {RESULT_TOPIC}"
    )
    return mqtt_client.get_state(RESULT_TOPIC)


def _make_temp_config(
    certificate_name: str, device_config_path: Path
) -> Path:
    """Create an isolated temp config dir: basic config + one signatureVerify
    interface + its publish action + the device-side certificate file."""
    config = json.loads(device_config_path.read_text())
    config = copy.deepcopy(config)
    config["interfaces"].append(
        {
            "type": "signatureVerify",
            "name": "sv1",
            "commandTopic": COMMAND_TOPIC,
            "certificate": f"/{certificate_name}",
        }
    )
    config["actions"].append(
        {
            "type": "publish",
            "interface": "sv1",
            "topic": RESULT_TOPIC,
            "template": "%1",
        }
    )

    temp_dir = Path(tempfile.mkdtemp(prefix="signing_config_"))
    (temp_dir / "device_config.json").write_text(
        json.dumps(config, indent=4) + "\n"
    )
    shutil.copy2(DEVICE_CERTS_DIR / certificate_name, temp_dir / certificate_name)
    return temp_dir


def _provision_certificate(
    certificate_name: str,
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
    return temp_dir


@pytest.fixture(scope="module", autouse=True)
def restore_basic_config(
    device_config: Path,
    device: Device,
    global_config_path: Path,
) -> Iterator[None]:
    """Restore basic config once after all certificate scenarios finish."""
    yield
    device.upload_config([device_config, global_config_path])


@pytest.fixture(scope="module")
def valid_signing_config(
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[None]:
    """Provision standard PEM certificate for signature behavior tests."""
    temp_dir = _provision_certificate(
        VALID_CERT_NAME, device_config, device, global_config_path, mqtt_client
    )
    try:
        yield
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture(scope="module")
def whitespace_signing_config(
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[None]:
    """Provision one PEM certificate with surrounding ASCII whitespace."""
    temp_dir = _provision_certificate(
        WHITESPACE_CERT_NAME,
        device_config,
        device,
        global_config_path,
        mqtt_client,
    )
    try:
        yield
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture(scope="module", params=INVALID_CERT_CASES)
def invalid_certificate_config(
    request: pytest.FixtureRequest,
    device_config: Path,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[str]:
    """Provision rejected certificate shapes without restoring between cases."""
    scenario, certificate_name = request.param
    temp_dir = _provision_certificate(
        certificate_name,
        device_config,
        device,
        global_config_path,
        mqtt_client,
    )
    try:
        yield scenario
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


def test_signed_empty_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Empty data + valid signature -> OK (empty data is a valid payload)."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(mqtt_client, envelope(vectors["vectors"]["empty"]))
    assert result == "OK"


def test_signed_binary_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Binary (0x00/0xff bytes) data + valid signature -> OK."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary"]))
    assert result == "OK"


def test_signed_max256_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Maximum 256-byte payload + valid signature -> OK (fits 512-byte limit)."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(
        mqtt_client, envelope(vectors["vectors"]["binary_max256"])
    )
    assert result == "OK"


def test_altered_data_fails(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Pinned-key signature over modified data -> FAIL invalid signature."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(
        mqtt_client, envelope(vectors["vectors"]["binary_altered_data"])
    )
    assert result == "FAIL invalid signature"


def test_wrong_key_fails(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Second same-curve (P-256) key's signature -> FAIL invalid signature."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(
        mqtt_client, envelope(vectors["vectors"]["binary_wrong_key"])
    )
    assert result == "FAIL invalid signature"


def test_malformed_json_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Malformed envelope -> ERROR (never OK)."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(mqtt_client, "not json at all")
    assert result.startswith("ERROR")


def test_wrong_signature_length_error(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """63-byte signature -> ERROR (exactly 64 bytes are required)."""
    assert_booted(device, mqtt_client, restarted=True)
    short_signature = bytes.fromhex(vectors["vectors"]["binary"]["signature_hex"])[:-1]
    command = json.dumps(
        {
            "data": base64.b64encode(bytes.fromhex(
                vectors["vectors"]["binary"]["data_hex"])).decode("ascii"),
            "signature": base64.b64encode(short_signature).decode("ascii"),
        },
        separators=(",", ":"),
    )
    result = send_and_wait(mqtt_client, command)
    assert result == "ERROR invalid signature length"


def test_whitespace_certificate_accepted(
    whitespace_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """One PEM certificate with surrounding whitespace remains valid."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(mqtt_client, envelope(vectors["vectors"]["binary"]))
    assert result == "OK"


def test_rejected_certificate_shapes_error(
    invalid_certificate_config: str,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    """Non-single-PEM certificate shape on SPIFFS -> invalid-certificate ERROR.

    The setup failure is retained; a well-formed envelope is answered with
    ERROR (never OK) even though its signature would be valid."""
    assert_booted(device, mqtt_client, restarted=True)
    result = send_and_wait(
        mqtt_client, envelope(vectors["vectors"]["binary"])
    )
    assert result == "ERROR invalid certificate", invalid_certificate_config
