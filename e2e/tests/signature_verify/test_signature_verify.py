"""Hardware acceptance tests; host private keys never enter SPIFFS."""

import base64
import hashlib
import json
import shutil
import tempfile
from collections.abc import Iterator
from pathlib import Path
from typing import Any
from uuid import uuid4

import pytest

from conftest import assert_booted
from lib.device import Device
from lib.mqtt_broker import MqttBroker
from lib.mqtt_client import MqttClient
from lib.signature_stream import (
    StreamTarget,
    send_stream,
    verify_broker_restart_invalidation,
)

E2E_DIR = Path(__file__).resolve().parents[2]
FIXTURES_DIR = E2E_DIR / "signing_fixtures"
VECTORS_PATH = FIXTURES_DIR / "host" / "vectors.json"
DEVICE_CERTS_DIR = FIXTURES_DIR / "device"
DEVICE_CONFIG_PATH = E2E_DIR / "configs" / "signature_verify" / "device_config.json"

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
        ("malformed", "signing_test_malformed.pem", "ERROR invalid certificate"),
        id="malformed",
    ),
    pytest.param(("DER", "signing_test_der.der", "ERROR invalid certificate"), id="der"),
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
    pytest.param(("missing", None, "ERROR certificate unavailable"), id="missing"),
]

RESULT_TIMEOUT = 15.0


@pytest.fixture(scope="module")
def vectors() -> dict[str, Any]:
    return json.loads(VECTORS_PATH.read_text())


def send_and_wait(mqtt_client: MqttClient, command: str) -> list[str]:
    previous_event = mqtt_client.get_event_count(RESULT_TOPIC)
    mqtt_client.publish(COMMAND_TOPIC, command, retain=False, qos=1)
    result = mqtt_client.wait_for_next_state(
        RESULT_TOPIC, previous_event, timeout=RESULT_TIMEOUT
    )
    assert result is not None, f"no result published on {RESULT_TOPIC}"
    report = json.loads(result)
    assert isinstance(report, list) and len(report) == 3
    assert all(isinstance(value, str) for value in report)
    return report


def _command(command_type: str, payload_hash: str, **fields: Any) -> str:
    return json.dumps(
        {"type": command_type, "hash": payload_hash, **fields},
        separators=(",", ":"),
    )


def _begin(data: bytes, payload_hash: str | None = None) -> str:
    return _command(
        "begin",
        payload_hash or hashlib.sha256(data).hexdigest(),
        size=len(data),
    )


def _chunk(data: bytes, payload_hash: str, offset: int) -> str:
    return _command(
        "chunk",
        payload_hash,
        offset=offset,
        data=base64.b64encode(data).decode("ascii"),
    )


def _finish(payload_hash: str, signature: bytes) -> str:
    return _command(
        "finish",
        payload_hash,
        signature=base64.b64encode(signature).decode("ascii"),
    )


def _send_valid_stream(
    mqtt_client: MqttClient, data: bytes, signature: bytes
) -> None:
    outcome = send_stream(
        mqtt_client,
        COMMAND_TOPIC,
        [StreamTarget("testDevice", RESULT_TOPIC)],
        data,
        signature,
    )
    assert outcome.verified == {"testDevice"}
    assert outcome.dropped == {}


def _vector(vectors: dict[str, Any], name: str) -> tuple[bytes, bytes]:
    item = vectors["vectors"][name]
    return bytes.fromhex(item["data_hex"]), bytes.fromhex(item["signature_hex"])


def _make_temp_config(certificate_name: str | None) -> Path:
    temp_dir = Path(tempfile.mkdtemp(prefix="signing_config_"))
    try:
        shutil.copy2(DEVICE_CONFIG_PATH, temp_dir / "device_config.json")
        if certificate_name is not None:
            shutil.copy2(
                DEVICE_CERTS_DIR / certificate_name, temp_dir / DEVICE_CERT_ALIAS
            )
    except BaseException:
        shutil.rmtree(temp_dir, ignore_errors=True)
        raise
    return temp_dir


def _provision_certificate(
    certificate_name: str | None,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Path:
    temp_dir = _make_temp_config(certificate_name)
    try:
        device.upload_config([temp_dir, global_config_path])
        mqtt_client.subscribe(RESULT_TOPIC)
        mqtt_client.subscribe(STATUS_ACTION_TOPIC)
    except BaseException:
        shutil.rmtree(temp_dir, ignore_errors=True)
        raise
    return temp_dir


@pytest.fixture
def provisioning_complete(
    request: pytest.FixtureRequest,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> None:
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
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[None]:
    temp_dir = _provision_certificate(
        VALID_CERT_NAME, device, global_config_path, mqtt_client
    )
    try:
        yield
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture
def whitespace_signing_config(
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[None]:
    temp_dir = _provision_certificate(
        WHITESPACE_CERT_NAME, device, global_config_path, mqtt_client
    )
    try:
        yield
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture(params=INVALID_CERT_CASES)
def invalid_certificate_config(
    request: pytest.FixtureRequest,
    device: Device,
    global_config_path: Path,
    mqtt_client: MqttClient,
) -> Iterator[tuple[str, str]]:
    scenario, certificate_name, expected_result = request.param
    temp_dir = _provision_certificate(
        certificate_name, device, global_config_path, mqtt_client
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
    assert_booted(device, mqtt_client, restarted=True)
    _send_valid_stream(mqtt_client, *_vector(vectors, "empty"))


def test_signed_binary_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    _send_valid_stream(mqtt_client, *_vector(vectors, "binary"))


def test_signed_max256_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    _send_valid_stream(mqtt_client, *_vector(vectors, "binary_max256"))


def test_signed_65536_byte_payload_ok(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    data, signature = _vector(vectors, "binary_stream65536")
    assert len(data) == 65536
    _send_valid_stream(mqtt_client, data, signature)


@pytest.mark.parametrize(
    "vector_name", ["binary_altered_data", "binary_wrong_key"]
)
def test_invalid_signature_fails(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    vector_name: str,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    outcome = send_stream(
        mqtt_client,
        COMMAND_TOPIC,
        [StreamTarget("testDevice", RESULT_TOPIC)],
        *_vector(vectors, vector_name),
    )
    assert outcome.verified == set()
    assert outcome.dropped == {"testDevice": "FAIL invalid signature"}


def test_advertised_hash_mismatch_is_distinct_from_invalid_signature(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    data, signature = _vector(vectors, "binary")
    real_hash = hashlib.sha256(data).hexdigest()
    advertised_hash = ("0" if real_hash[0] != "0" else "1") + real_hash[1:]

    assert send_and_wait(mqtt_client, _begin(data, advertised_hash)) == [
        "PROGRESS",
        "0",
        advertised_hash,
    ]
    assert send_and_wait(mqtt_client, _chunk(data, advertised_hash, 0)) == [
        "PROGRESS",
        str(len(data)),
        advertised_hash,
    ]
    assert send_and_wait(mqtt_client, _finish(advertised_hash, signature)) == [
        "FAIL hash mismatch",
        str(len(data)),
        advertised_hash,
    ]


def test_malformed_json_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, "not json at all") == ["ERROR invalid JSON", "0", ""]


@pytest.mark.parametrize("signature_size", [63, 65], ids=["short", "long"])
def test_wrong_signature_length_error(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    signature_size: int,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    data, _ = _vector(vectors, "binary")
    payload_hash = hashlib.sha256(data).hexdigest()
    report = send_and_wait(
        mqtt_client,
        _finish(payload_hash, bytes(signature_size)),
    )
    assert report == ["ERROR invalid signature length", "0", ""]


@pytest.mark.parametrize(
    "command",
    [
        '{"type":"chunk","hash":"' + "a" * 64 + '","offset":0,"data":"!!!!"}',
        '{"type":"chunk","hash":"' + "a" * 64 + '","offset":0,"data":"AB=="}',
        '{"type":"finish","hash":"' + "a" * 64 + '","signature":"?"}',
    ],
    ids=["chunk-data", "noncanonical-pad-bits", "signature"],
)
def test_invalid_base64_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    command: str,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, command) == ["ERROR invalid base64", "0", ""]


@pytest.mark.parametrize(
    "field,value",
    [
        pytest.param("size", True, id="size-boolean"),
        pytest.param("size", -1, id="size-negative"),
        pytest.param("size", 1.0, id="size-fraction"),
        pytest.param("size", 4294967296, id="size-overflow"),
        pytest.param("offset", True, id="offset-boolean"),
        pytest.param("offset", -1, id="offset-negative"),
        pytest.param("offset", 1.0, id="offset-fraction"),
        pytest.param("offset", 4294967296, id="offset-overflow"),
    ],
)
def test_invalid_stream_count_numbers_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    field: str,
    value: bool | int | float,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    if field == "size":
        command = json.dumps(
            {"type": "begin", "hash": "a" * 64, "size": value},
            separators=(",", ":"),
        )
    else:
        command = json.dumps(
            {
                "type": "chunk",
                "hash": "a" * 64,
                "offset": value,
                "data": base64.b64encode(b"x").decode("ascii"),
            },
            separators=(",", ":"),
        )
    assert send_and_wait(mqtt_client, command) == ["ERROR invalid fields", "0", ""]


@pytest.mark.parametrize(
    "command,expected",
    [
        pytest.param("x" * 513, "ERROR command too large", id="oversized-command"),
        pytest.param(
            _command(
                "chunk",
                "a" * 64,
                offset=0,
                data=base64.b64encode(bytes(257)).decode("ascii"),
            ),
            "ERROR data too large",
            id="oversized-chunk",
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
    assert_booted(device, mqtt_client, restarted=True)
    report = send_and_wait(mqtt_client, command)
    assert report[0] == expected


@pytest.mark.parametrize(
    "command",
    [
        '{"type":"begin","hash":"' + "a" * 64 + '","size":0,"extra":1}',
        '{"type":"begin","hash":"' + "a" * 64 + '"}',
    ],
    ids=["unknown-field", "missing-size"],
)
def test_invalid_stream_fields_error(
    valid_signing_config: None,
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    command: str,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, command)[0] == "ERROR invalid fields"


def test_duplicate_begin_and_chunk_re_ack_without_corrupting_digest(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    data, signature = _vector(vectors, "binary")
    payload_hash = hashlib.sha256(data).hexdigest()
    begin = _begin(data, payload_hash)
    chunk = _chunk(data, payload_hash, 0)

    expected_zero = ["PROGRESS", "0", payload_hash]
    expected_full = ["PROGRESS", str(len(data)), payload_hash]
    assert send_and_wait(mqtt_client, begin) == expected_zero
    assert send_and_wait(mqtt_client, begin) == expected_zero
    assert send_and_wait(mqtt_client, chunk) == expected_full
    assert send_and_wait(mqtt_client, begin) == expected_full
    assert send_and_wait(mqtt_client, chunk) == expected_full
    assert send_and_wait(mqtt_client, _finish(payload_hash, signature)) == [
        "OK",
        str(len(data)),
        payload_hash,
    ]


def test_duplicate_json_fields_use_last_value(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    data, signature = _vector(vectors, "binary")
    payload_hash = hashlib.sha256(data).hexdigest()
    correct_data = base64.b64encode(data).decode("ascii")
    wrong_data = base64.b64encode(b"wrong first value").decode("ascii")

    assert send_and_wait(mqtt_client, _begin(data, payload_hash)) == [
        "PROGRESS",
        "0",
        payload_hash,
    ]
    duplicate_data = (
        f'{{"type":"chunk","hash":"{payload_hash}","offset":0,'
        f'"data":"{wrong_data}","data":"{correct_data}"}}'
    )
    assert send_and_wait(mqtt_client, duplicate_data) == [
        "PROGRESS",
        str(len(data)),
        payload_hash,
    ]
    assert send_and_wait(mqtt_client, _finish(payload_hash, signature)) == [
        "OK",
        str(len(data)),
        payload_hash,
    ]


def test_gap_error_recovers_after_fresh_begin(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    payload = bytes((index * 19 + 7) % 256 for index in range(300))
    payload_hash = hashlib.sha256(payload).hexdigest()

    assert send_and_wait(mqtt_client, _begin(payload, payload_hash)) == [
        "PROGRESS",
        "0",
        payload_hash,
    ]
    assert send_and_wait(mqtt_client, _chunk(payload[:1], payload_hash, 1)) == [
        "ERROR invalid offset",
        "0",
        payload_hash,
    ]
    _send_valid_stream(mqtt_client, *_vector(vectors, "binary"))


def test_whitespace_certificate_accepted(
    whitespace_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    _send_valid_stream(mqtt_client, *_vector(vectors, "binary"))


def test_bad_command_recovery(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    assert send_and_wait(mqtt_client, "not json") == ["ERROR invalid JSON", "0", ""]
    _send_valid_stream(mqtt_client, *_vector(vectors, "binary"))


def test_repeated_identical_ok_results_are_distinct_events(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    first_count = mqtt_client.get_event_count(RESULT_TOPIC)
    _send_valid_stream(mqtt_client, *_vector(vectors, "binary"))
    first_result_count = mqtt_client.get_event_count(RESULT_TOPIC)
    assert first_result_count > first_count
    _send_valid_stream(mqtt_client, *_vector(vectors, "binary"))
    assert mqtt_client.get_event_count(RESULT_TOPIC) > first_result_count


def test_rejected_certificate_shapes_error(
    invalid_certificate_config: tuple[str, str],
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    scenario, expected_result = invalid_certificate_config
    data, _ = _vector(vectors, "binary")
    report = send_and_wait(mqtt_client, _begin(data))
    assert report[0] == expected_result, scenario


def test_broker_restart_aborts_transfer_and_requires_full_retransmission(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
    mqtt_broker: MqttBroker,
) -> None:
    assert_booted(device, mqtt_client, restarted=True)
    data, signature = _vector(vectors, "binary_max256")
    payload_hash = hashlib.sha256(data).hexdigest()
    assert send_and_wait(mqtt_client, _begin(data)) == [
        "PROGRESS",
        "0",
        payload_hash,
    ]
    try:
        verify_broker_restart_invalidation(
            mqtt_client,
            stop_broker=mqtt_broker.stop,
            start_broker=mqtt_broker.start,
            command_topic=COMMAND_TOPIC,
            result_topic=RESULT_TOPIC,
            partial_command=_chunk(data[:128], payload_hash, 0),
            probe_command=_chunk(data[128:], payload_hash, 128),
            payload_hash=payload_hash,
            partial_count=128,
        )
        _send_valid_stream(mqtt_client, data, signature)
    finally:
        mqtt_broker.start()


def test_signature_interface_preserves_passthrough_and_status(
    valid_signing_config: None,
    vectors: dict[str, Any],
    device: Device,
    reset_device: None,
    mqtt_client: MqttClient,
) -> None:
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

    _send_valid_stream(mqtt_client, *_vector(vectors, "binary"))

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
