import base64
import hashlib
import importlib.util
import json
import subprocess
import sys
import types
from collections.abc import Callable
from pathlib import Path
from types import SimpleNamespace
from typing import Any, cast

import pytest
from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils

from lib import device_config_ota, signature_stream
from lib.device_config_ota import (
    build_device_config_payload,
    decode_ota_device_status,
    decode_ota_stream_report,
    send_device_config_update,
    sign_device_config_payload,
    wait_for_device_lwt_then_clear,
)
from lib.serial_log import serial_log_contains_marker_after, serial_log_offset
from lib.signature_stream import StreamOutcome, StreamReport, StreamTarget, send_stream


ROOT = Path(__file__).parents[2]
HOST_KEY = ROOT / "signing_fixtures/host/signing_test_private_key.pem"
DEVICE_CERT = ROOT / "signing_fixtures/device/signing_test.pem"
TARGET_MAC = "AA:BB:CC:DD:EE:FF"
CONFIG = {"configVersion": 12, "label": "café"}


class FakeMqttClient:
    def __init__(self) -> None:
        self.subscriptions: list[str] = []
        self.publishes: list[tuple[str, str, bool, int]] = []
        self.states: dict[str, str] = {}
        self.event_counts: dict[str, int] = {}
        self.ready = False
        self.ready_generation = 0
        self.wait_ready_timeouts: list[float] = []
        self.on_publish = None

    def subscribe(self, topic: str) -> None:
        self.subscriptions.append(topic)

    def wait_ready(self, timeout: float) -> bool:
        assert timeout > 0
        self.wait_ready_timeouts.append(timeout)
        if not self.ready:
            self.ready = True
            self.ready_generation += 1
        return True

    def get_readiness_snapshot(self) -> tuple[bool, int]:
        return self.ready, self.ready_generation

    def get_event_count(self, topic: str) -> int:
        return self.event_counts.get(topic, 0)

    def get_state(self, topic: str) -> str | None:
        return self.states.get(topic)

    def publish(
        self, topic: str, payload: str, *, retain: bool, qos: int
    ) -> None:
        self.publishes.append((topic, payload, retain, qos))
        if self.on_publish is not None:
            self.on_publish(topic, payload)

    def report(self, topic: str, payload: str) -> None:
        self.states[topic] = payload
        self.event_counts[topic] = self.event_counts.get(topic, 0) + 1


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0

    def monotonic(self) -> float:
        return self.now

    def sleep(self, duration: float) -> None:
        self.now += duration


def _command(payload: str) -> dict[str, object]:
    return json.loads(payload)


def _ota_report(
    status: str,
    transferred: int,
    payload_hash: str,
    *,
    version: int | None = None,
    reason: str = "",
) -> str:
    return json.dumps(
        {
            "status": status,
            "transferred": transferred,
            "hash": payload_hash,
            "version": version,
            "reason": reason,
        },
        separators=(",", ":"),
    )


def test_payload_is_deterministic_compact_utf8_json() -> None:
    payload = build_device_config_payload(TARGET_MAC, CONFIG)

    assert payload == (
        '{"type":"device-config","target":"AA:BB:CC:DD:EE:FF",'
        '"config":{"configVersion":12,"label":"café"}}'
    ).encode("utf-8")
    assert build_device_config_payload(TARGET_MAC, CONFIG) == payload


@pytest.mark.parametrize(
    "target_mac",
    [
        "device-name",
        "AA-BB-CC-DD-EE-FF",
        "AA:BB:CC:DD:EE",
        "AA:BB:CC:DD:EE:GG",
        "AA:BB:CC:DD:EE:FF:00",
    ],
)
def test_payload_rejects_non_mac_targets(target_mac: str) -> None:
    with pytest.raises(ValueError):
        build_device_config_payload(target_mac, CONFIG)


def test_signing_returns_verifiable_raw64_p256_signature() -> None:
    payload = build_device_config_payload(TARGET_MAC, CONFIG)
    signature = sign_device_config_payload(payload, HOST_KEY)

    assert isinstance(signature, bytes)
    assert len(signature) == 64
    der_signature = utils.encode_dss_signature(
        int.from_bytes(signature[:32], "big"),
        int.from_bytes(signature[32:], "big"),
    )
    certificate = x509.load_pem_x509_certificate(DEVICE_CERT.read_bytes())
    public_key = certificate.public_key()
    assert isinstance(public_key, ec.EllipticCurvePublicKey)
    public_key.verify(der_signature, payload, ec.ECDSA(hashes.SHA256()))


def test_signing_is_independently_verifiable_with_openssl(tmp_path: Path) -> None:
    payload = build_device_config_payload(TARGET_MAC, CONFIG)
    signature = sign_device_config_payload(payload, HOST_KEY)
    public_key_path = tmp_path / "device_public_key.pem"
    payload_path = tmp_path / "payload.bin"
    signature_path = tmp_path / "signature.der"
    der_signature = utils.encode_dss_signature(
        int.from_bytes(signature[:32], "big"),
        int.from_bytes(signature[32:], "big"),
    )
    payload_path.write_bytes(payload)
    signature_path.write_bytes(der_signature)
    subprocess.run(
        [
            "openssl",
            "x509",
            "-in",
            str(DEVICE_CERT),
            "-pubkey",
            "-noout",
            "-out",
            str(public_key_path),
        ],
        check=True,
        capture_output=True,
    )

    result = subprocess.run(
        [
            "openssl",
            "dgst",
            "-sha256",
            "-verify",
            str(public_key_path),
            "-signature",
            str(signature_path),
            str(payload_path),
        ],
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stdout + result.stderr


def test_ota_result_decodes_staged_and_combines_failure_reason() -> None:
    payload_hash = "a" * 64

    assert decode_ota_stream_report(
        _ota_report("STAGED", 17, payload_hash, version=12)
    ) == StreamReport("STAGED", 17, payload_hash)
    assert decode_ota_stream_report(
        _ota_report("FAIL", 17, payload_hash, reason="invalid-signature")
    ) == StreamReport("FAIL invalid-signature", 17, payload_hash)
    assert decode_ota_stream_report(
        _ota_report("ERROR", 17, payload_hash, reason="updates-disabled")
    ) == StreamReport("ERROR updates-disabled", 17, payload_hash)


def test_exhausted_version_failure_correlates_before_dropping_target(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    clock = FakeClock()
    monkeypatch.setattr(signature_stream.time, "monotonic", clock.monotonic)
    monkeypatch.setattr(signature_stream.time, "sleep", clock.sleep)
    client = FakeMqttClient()
    target = StreamTarget("device", "result/device")
    data = b"payload"
    payload_hash = hashlib.sha256(data).hexdigest()
    begin_count = 0

    exhausted_report = _ota_report(
        "ERROR", 0, payload_hash, reason="version-range-exhausted"
    )
    assert decode_ota_stream_report(exhausted_report) == StreamReport(
        "ERROR version-range-exhausted", 0, payload_hash
    )

    def on_publish(_topic: str, payload: str) -> None:
        nonlocal begin_count
        assert _command(payload)["type"] == "begin"
        begin_count += 1
        report_hash = "b" * 64 if begin_count == 1 else payload_hash
        client.report(
            target.result_topic,
            _ota_report(
                "ERROR", 0, report_hash, reason="version-range-exhausted"
            ),
        )

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [target],
        data,
        b"s" * 64,
        timeout=1.1,
        retry_interval=0.1,
        report_decoder=decode_ota_stream_report,
    )

    assert outcome == StreamOutcome(
        set(), {"device": "ERROR version-range-exhausted"}
    )
    assert begin_count == 2
    assert [
        _command(payload)["type"] for _, payload, _, _ in client.publishes
    ] == ["begin", "begin"]


@pytest.mark.parametrize("version", [True, 1.0, -1, 0x100000000, None])
def test_ota_result_decoder_rejects_invalid_lifecycle_versions(
    version: object,
) -> None:
    payload = json.dumps(
        {
            "status": "CONFIRMED",
            "transferred": 17,
            "hash": "a" * 64,
            "version": version,
            "reason": "",
        },
        separators=(",", ":"),
    )

    assert decode_ota_stream_report(payload) is None


def test_ota_result_decoder_accepts_uint32_version_bounds_and_optional_null() -> None:
    payload_hash = "a" * 64

    assert decode_ota_stream_report(
        _ota_report("STAGED", 17, payload_hash, version=0)
    ) == StreamReport("STAGED", 17, payload_hash)
    assert decode_ota_stream_report(
        _ota_report("STAGED", 17, payload_hash, version=0xFFFFFFFF)
    ) == StreamReport("STAGED", 17, payload_hash)
    assert decode_ota_stream_report(
        _ota_report("PROGRESS", 17, payload_hash, version=None)
    ) == StreamReport("PROGRESS", 17, payload_hash)


def test_ota_device_status_accepts_uint32_bounds_and_null_exhaustion() -> None:
    exhausted_status = {
        "mac": TARGET_MAC,
        "configVersion": 0xFFFFFFFF,
        "minimumAcceptedConfigVersion": None,
        "trialState": "idle",
        "lastConfigUpdate": None,
        "active": True,
    }
    boundary_status = {**exhausted_status, "minimumAcceptedConfigVersion": 0xFFFFFFFF}
    normalized_status = {
        "mac": TARGET_MAC,
        "version": 0xFFFFFFFF,
        "minimumAcceptedConfigVersion": None,
        "trialState": "idle",
        "lastConfigUpdate": None,
    }
    normalized_boundary_status = {
        **normalized_status,
        "minimumAcceptedConfigVersion": 0xFFFFFFFF,
    }

    assert decode_ota_device_status(json.dumps(exhausted_status)) == normalized_status
    assert (
        decode_ota_device_status(json.dumps(boundary_status))
        == normalized_boundary_status
    )


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("configVersion", True),
        ("configVersion", 1.0),
        ("configVersion", -1),
        ("configVersion", 0x100000000),
        ("minimumAcceptedConfigVersion", True),
        ("minimumAcceptedConfigVersion", 1.0),
        ("minimumAcceptedConfigVersion", 0),
        ("minimumAcceptedConfigVersion", -1),
        ("minimumAcceptedConfigVersion", 0x100000000),
    ],
)
def test_ota_device_status_rejects_non_uint32_versions(
    field: str, value: object
) -> None:
    status = {
        "mac": TARGET_MAC,
        "configVersion": 1,
        "minimumAcceptedConfigVersion": 2,
        "trialState": "idle",
        "lastConfigUpdate": None,
    }
    status[field] = value

    assert decode_ota_device_status(json.dumps(status)) is None


def test_ota_device_status_rejects_duplicate_and_incomplete_fields() -> None:
    duplicate = (
        '{"mac":"AA:BB:CC:DD:EE:FF","configVersion":1,'
        '"minimumAcceptedConfigVersion":2,"minimumAcceptedConfigVersion":null,'
        '"trialState":"idle","lastConfigUpdate":null}'
    )
    assert decode_ota_device_status(duplicate) is None
    complete_status = {
        "mac": TARGET_MAC,
        "configVersion": 1,
        "minimumAcceptedConfigVersion": None,
        "trialState": "idle",
        "lastConfigUpdate": None,
    }
    for field in complete_status:
        incomplete_status = {
            key: value for key, value in complete_status.items() if key != field
        }
        assert decode_ota_device_status(json.dumps(incomplete_status)) is None


@pytest.mark.parametrize(
    "payload",
    [
        '["OK","1","' + "a" * 64 + '"]',
        '{"status":"STAGED","transferred":"1","hash":"' + "a" * 64
        + '","version":1,"reason":""}',
        '{"status":"STAGED","transferred":true,"hash":"' + "a" * 64
        + '","version":1,"reason":""}',
        '{"status":"STAGED","transferred":1.0,"hash":"' + "a" * 64
        + '","version":1,"reason":""}',
        '{"status":"STAGED","transferred":4294967296,"hash":"' + "a" * 64
        + '","version":1,"reason":""}',
        '{"status":"STAGED","transferred":1,"hash":"' + "A" * 64
        + '","version":1,"reason":""}',
        '{"status":"STAGED","transferred":1,"hash":"' + "a" * 64
        + '","version":true,"reason":""}',
        '{"status":"STAGED","transferred":1,"hash":"' + "a" * 64
        + '","version":1,"reason":false}',
        '{"status":"STAGED","transferred":1,"hash":"' + "a" * 64
        + '","version":1,"reason":"","extra":0}',
        '{"status":"OK","transferred":1,"hash":"' + "a" * 64
        + '","version":1,"reason":""}',
    ],
)
def test_ota_result_decoder_rejects_legacy_and_malformed_reports(
    payload: str,
) -> None:
    assert decode_ota_stream_report(payload) is None


def _run_stream(
    client: FakeMqttClient,
    target: StreamTarget,
    data: bytes,
    *,
    ota_reports: bool,
    final_status: str,
) -> StreamOutcome:
    payload_hash = hashlib.sha256(data).hexdigest()

    def on_publish(_topic: str, payload: str) -> None:
        command = _command(payload)
        kind = command["type"]
        if kind == "begin":
            transferred = 0
            status = "PROGRESS"
        elif kind == "chunk":
            transferred = int(command["offset"]) + len(
                base64.b64decode(command["data"])
            )
            status = "PROGRESS"
        else:
            transferred = len(data)
            status = final_status
        if ota_reports:
            response = _ota_report(
                status,
                transferred,
                payload_hash,
                version=12 if status == "STAGED" else None,
            )
        else:
            response = json.dumps([status, str(transferred), payload_hash])
        client.report(target.result_topic, response)

    client.on_publish = on_publish
    return send_stream(
        client,
        "commands",
        [target],
        data,
        b"s" * 64,
        timeout=1.1,
        retry_interval=0.1,
    )


def test_send_stream_default_keeps_legacy_ok_behavior() -> None:
    client = FakeMqttClient()
    target = StreamTarget("device", "result/device")

    outcome = _run_stream(
        client,
        target,
        b"payload",
        ota_reports=False,
        final_status="OK",
    )

    assert outcome == StreamOutcome({"device"}, {})
    assert [
        _command(payload)["type"] for _, payload, _, _ in client.publishes
    ] == ["begin", "chunk", "finish"]


def test_send_stream_default_does_not_accept_staged(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    clock = FakeClock()
    monkeypatch.setattr(signature_stream.time, "monotonic", clock.monotonic)
    monkeypatch.setattr(signature_stream.time, "sleep", clock.sleep)
    client = FakeMqttClient()
    target = StreamTarget("device", "result/device")
    data = b"payload"
    payload_hash = hashlib.sha256(data).hexdigest()

    def on_publish(_topic: str, payload: str) -> None:
        command = _command(payload)
        kind = command["type"]
        if kind == "begin":
            transferred, status = 0, "PROGRESS"
        elif kind == "chunk":
            transferred = int(command["offset"]) + len(
                base64.b64decode(command["data"])
            )
            status = "PROGRESS"
        else:
            transferred, status = len(data), "STAGED"
        client.report(
            target.result_topic,
            json.dumps([status, str(transferred), payload_hash]),
        )

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [target],
        data,
        b"s" * 64,
        timeout=1.1,
    )

    assert outcome == StreamOutcome(set(), {"device": "timeout"})


def test_send_stream_accepts_injected_ota_decoder_and_final_status() -> None:
    client = FakeMqttClient()
    target = StreamTarget("device", "result/device")
    data = b"payload"
    payload_hash = hashlib.sha256(data).hexdigest()
    decoder_calls: list[str] = []

    def decode(payload: str) -> StreamReport | None:
        decoder_calls.append(payload)
        return decode_ota_stream_report(payload)

    def on_publish(_topic: str, payload: str) -> None:
        command = _command(payload)
        kind = command["type"]
        if kind == "begin":
            transferred, status = 0, "PROGRESS"
        elif kind == "chunk":
            transferred = int(command["offset"]) + len(
                base64.b64decode(command["data"])
            )
            status = "PROGRESS"
        else:
            transferred, status = len(data), "STAGED"
        client.report(
            target.result_topic,
            _ota_report(
                status,
                transferred,
                payload_hash,
                version=12 if status == "STAGED" else None,
            ),
        )

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [target],
        data,
        b"s" * 64,
        timeout=1.1,
        retry_interval=0.1,
        final_status="STAGED",
        report_decoder=decode,
    )

    assert outcome == StreamOutcome({"device"}, {})
    assert len(decoder_calls) >= 3


@pytest.mark.parametrize(
    ("result_status", "verified"), [("STAGED", True), ("OK", False)]
)
def test_device_config_sender_requires_staged_result(
    monkeypatch: pytest.MonkeyPatch,
    result_status: str,
    verified: bool,
) -> None:
    clock = FakeClock()
    monkeypatch.setattr(signature_stream.time, "monotonic", clock.monotonic)
    monkeypatch.setattr(signature_stream.time, "sleep", clock.sleep)
    client = FakeMqttClient()
    command_topic = "home/testDevice/update/device-config"
    result_topic = "home/testDevice/update/device-config/result"
    sent: list[dict[str, object]] = []

    def on_publish(topic: str, payload: str) -> None:
        assert topic == command_topic
        command = _command(payload)
        sent.append(command)
        kind = command["type"]
        if kind == "begin":
            transferred, status = 0, "PROGRESS"
        elif kind == "chunk":
            transferred = int(command["offset"]) + len(
                base64.b64decode(command["data"])
            )
            status = "PROGRESS"
        else:
            transferred = len(build_device_config_payload(TARGET_MAC, CONFIG))
            status = result_status
        payload_hash = command["hash"]
        client.report(
            result_topic,
            _ota_report(
                status,
                transferred,
                str(payload_hash),
                version=12 if status == "STAGED" else None,
            ),
        )

    client.on_publish = on_publish
    outcome = send_device_config_update(
        client,
        service_prefix="home/testDevice",
        target_mac=TARGET_MAC,
        config=CONFIG,
        signature=b"s" * 64,
        timeout=1.1,
    )

    assert bool(outcome.verified) is verified
    assert list(outcome.dropped.values()) == ([] if verified else ["timeout"])
    assert client.subscriptions == [result_topic]
    assert sent[0]["type"] == "begin"
    assert sent[-1]["type"] == "finish"
    assert len(sent) >= 3


def test_device_config_sender_accepts_482_byte_service_prefix(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    prefix = "p" * 482
    command_topics: list[str] = []

    def fake_send_stream(
        _client: FakeMqttClient,
        command_topic: str,
        targets: list[StreamTarget],
        _data: bytes,
        _signature: bytes,
        **_kwargs: object,
    ) -> StreamOutcome:
        command_topics.append(command_topic)
        return StreamOutcome({targets[0].name}, {})

    monkeypatch.setattr(device_config_ota, "send_stream", fake_send_stream)

    outcome = send_device_config_update(
        FakeMqttClient(),
        service_prefix=prefix,
        target_mac=TARGET_MAC,
        config=CONFIG,
        signature=b"s" * 64,
    )

    assert outcome == StreamOutcome({TARGET_MAC}, {})
    assert command_topics == [f"{prefix}/update/device-config"]


def test_device_config_sender_rejects_483_byte_service_prefix() -> None:
    with pytest.raises(ValueError):
        send_device_config_update(
            FakeMqttClient(),
            service_prefix="p" * 483,
            target_mac=TARGET_MAC,
            config=CONFIG,
            signature=b"s" * 64,
        )


@pytest.mark.parametrize("prefix", ["", "/", "a\0b", "a+b", "a#b", "\ud800"])
def test_device_config_sender_rejects_malformed_service_prefixes(
    prefix: str,
) -> None:
    with pytest.raises(ValueError):
        send_device_config_update(
            FakeMqttClient(),
            service_prefix=prefix,
            target_mac=TARGET_MAC,
            config=CONFIG,
            signature=b"s" * 64,
        )


def test_ota_status_matching_requires_fresh_exact_mac_version_hash_and_status() -> None:
    from lib.device_config_ota import matches_ota_status

    payload_hash = "a" * 64
    observed = {
        "mac": TARGET_MAC,
        "version": 12,
        "hash": payload_hash,
        "status": "CONFIRMED",
    }
    expected = {
        "expected_mac": TARGET_MAC,
        "expected_version": 12,
        "expected_hash": payload_hash,
        "expected_status": "CONFIRMED",
    }

    assert matches_ota_status(
        **expected, observed_status=observed, event_is_new=True
    )
    for mismatch in (
        {**observed, "mac": "11:22:33:44:55:66"},
        {**observed, "version": 11},
        {**observed, "hash": "b" * 64},
        {**observed, "status": "ROLLED_BACK"},
    ):
        assert not matches_ota_status(
            **expected, observed_status=mismatch, event_is_new=True
        )
    assert not matches_ota_status(
        **expected, observed_status=observed, event_is_new=False
    )
    assert not matches_ota_status(
        **expected, observed_status=observed, event_is_new=1
    )


def test_ota_result_topic_matcher_accepts_old_or_new_service_prefix() -> None:
    from lib.device_config_ota import is_ota_result_topic

    prefixes = ("home/old", "home/current")

    assert is_ota_result_topic(
        "home/old/update/device-config/result", prefixes
    )
    assert is_ota_result_topic(
        "home/current/update/device-config/result", prefixes
    )
    assert not is_ota_result_topic(
        "home/other/update/device-config/result", prefixes
    )
    assert not is_ota_result_topic(
        "home/old/+/update/device-config/result", prefixes
    )


def test_serial_log_marker_requires_bytes_after_captured_offset(
    tmp_path: Path,
) -> None:
    path = tmp_path / "device.log"

    assert serial_log_offset(path) == 0
    assert not serial_log_contains_marker_after(path, "trial", 0)

    path.write_bytes(b"trial from earlier boot\n")
    offset = serial_log_offset(path)
    assert not serial_log_contains_marker_after(path, "trial", offset)

    with path.open("ab") as log:
        log.write(b"Configuration boot selection: trial\n")
    assert serial_log_contains_marker_after(
        path, "Configuration boot selection: trial", offset
    )


def test_device_enable_drives_enable_pin_high(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    gpio_module = types.ModuleType("RPi.GPIO")
    rpi_package = types.ModuleType("RPi")
    setattr(rpi_package, "__path__", [])
    setattr(rpi_package, "GPIO", gpio_module)
    monkeypatch.setitem(sys.modules, "RPi", rpi_package)
    monkeypatch.setitem(sys.modules, "RPi.GPIO", gpio_module)

    from lib.device import Device

    class RecordingRpi:
        def __init__(self) -> None:
            self.events: list[tuple[str, int, int]] = []

        def setup_output(self, pin: int, value: int) -> None:
            self.events.append(("setup", pin, value))

        def write(self, pin: int, value: int) -> None:
            self.events.append(("write", pin, value))

    rpi = RecordingRpi()
    device = Device("unused", 4, 5, cast(Any, object()), cast(Any, rpi))
    device.disable()
    device.enable()

    assert rpi.events == [
        ("setup", 5, 1),
        ("write", 5, 0),
        ("write", 5, 1),
    ]


def test_retained_cleanup_timeout_does_not_clear_before_lwt() -> None:
    client = FakeMqttClient()

    class FakeDevice:
        disabled = False

        def disable(self) -> None:
            self.disabled = True

    device = FakeDevice()
    with pytest.raises(AssertionError, match="No fresh offline LWT"):
        wait_for_device_lwt_then_clear(
            client, device, ("home/owned", "home/peer"), timeout=0.001
        )

    assert device.disabled
    assert client.subscriptions == ["home/owned/available", "home/peer/available"]
    assert client.publishes == []


def test_retained_cleanup_default_timeout_allows_delayed_lwt(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    clock = FakeClock()
    monkeypatch.setattr(
        device_config_ota,
        "time",
        SimpleNamespace(monotonic=clock.monotonic, sleep=clock.sleep),
    )

    class DelayedLwtClient(FakeMqttClient):
        lwt_reported = False

        def get_event_count(self, topic: str) -> int:
            if clock.now >= 22.6 and not self.lwt_reported:
                self.report(topic, "0")
                self.lwt_reported = True
            return super().get_event_count(topic)

    class FakeDevice:
        disabled = False

        def disable(self) -> None:
            self.disabled = True

    client = DelayedLwtClient()
    device = FakeDevice()

    wait_for_device_lwt_then_clear(client, device, ("home/owned",))

    assert client.wait_ready_timeouts == [30.0]
    assert clock.now > 22.5
    assert device.disabled
    assert len(client.publishes) == 3


def test_ota_provisioning_quiesces_before_retained_cleanup_and_reenables(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    conftest_path = ROOT / "tests/device_config_ota/conftest.py"
    spec = importlib.util.spec_from_file_location(
        "isolated_device_config_ota_conftest", conftest_path
    )
    assert spec is not None and spec.loader is not None
    conftest = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(conftest)

    base_config = {"servicePrefix": "home/ota-base", "configVersion": 1}
    base_config_path = tmp_path / "device_config.json"
    base_config_path.write_text(json.dumps(base_config), encoding="utf-8")
    fixtures_dir = tmp_path / "signing_fixtures"
    (fixtures_dir / "device").mkdir(parents=True)
    (fixtures_dir / "host").mkdir()
    (fixtures_dir / "device/signing_test.pem").write_text("certificate")
    (fixtures_dir / "host/signing_test_private_key.pem").write_text("private")
    monkeypatch.setattr(conftest, "BASE_CONFIG_PATH", base_config_path)
    monkeypatch.setattr(conftest, "FIXTURES_DIR", fixtures_dir)

    events: list[tuple[object, ...]] = []

    class FakeRequest:
        node = SimpleNamespace(name="isolated-test")

        def __init__(self) -> None:
            self.finalizers: list[Callable[[], None]] = []

        def addfinalizer(self, finalizer: Callable[[], None]) -> None:
            self.finalizers.append(finalizer)

    class FakeClient:
        version = 1
        raised_publish_error = False
        waiting_for_lwt = False
        lwt_checks = 0

        def subscribe(self, topic: str) -> None:
            events.append(("subscribe", topic))

        def wait_ready(self, timeout: float) -> bool:
            events.append(("ready",))
            return True

        def clear_all(self) -> None:
            events.append(("clear-state",))

        def get_event_count(self, topic: str) -> int:
            if self.waiting_for_lwt and topic.endswith("/available"):
                self.lwt_checks += 1
                if self.lwt_checks == 3:
                    self.waiting_for_lwt = False
                    self.states[topic] = "0"
                    self.event_counts[topic] = self.event_counts.get(topic, 1) + 1
                    events.append(("lwt", topic, "0"))
            return self.event_counts.get(topic, 1)

        def get_state(self, topic: str) -> str | None:
            return self.states.get(topic)

        def wait_for_next_state(
            self, topic: str, previous_event: int, timeout: float
        ) -> str:
            return json.dumps(
                {
                    "mac": TARGET_MAC,
                    "configVersion": self.version,
                    "minimumAcceptedConfigVersion": 1,
                    "trialState": "idle",
                    "lastConfigUpdate": None,
                }
            )

        def publish(
            self, topic: str, payload: str, *, retain: bool, qos: int
        ) -> None:
            events.append(("publish", topic, payload, retain, qos))
            if not self.raised_publish_error:
                self.raised_publish_error = True
                raise ConnectionError("client disconnected")

        states: dict[str, str] = {}
        event_counts: dict[str, int] = {}

    client = FakeClient()

    class FakeDevice:
        def enable(self) -> None:
            events.append(("enable",))

        def upload_config(self, paths: list[Path]) -> None:
            upload_dir = Path(paths[0])
            config = json.loads(
                (upload_dir / "device_config.json").read_text(encoding="utf-8")
            )
            client.version = cast(int, config["configVersion"])
            events.append(("upload", upload_dir))

        def reset(self) -> None:
            events.append(("reset",))

        def wait_for_boot(self, timeout: float) -> bool:
            return True

        def disable(self) -> None:
            events.append(("disable",))
            client.waiting_for_lwt = True
            client.lwt_checks = 0

    request = FakeRequest()
    device = FakeDevice()
    global_config = tmp_path / "global_config.json"
    global_config.write_text("{}", encoding="utf-8")

    def start_serial() -> object:
        events.append(("serial",))
        return object()

    fixture = conftest.provision_ota_device.__wrapped__(
        request, start_serial, device, global_config, client
    )
    provision = next(fixture)
    first = provision(None)
    request.finalizers[0]()

    second_config = {
        **base_config,
        "servicePrefix": "home/ota-next",
        "configVersion": 2,
    }
    second = provision(second_config)
    second["owned_prefixes"].add("home/ota-peer")
    request.finalizers[0]()

    uploads = [index for index, event in enumerate(events) if event[0] == "upload"]
    enables = [index for index, event in enumerate(events) if event[0] == "enable"]
    assert len(uploads) == len(enables) == 2
    assert all(enable < upload for enable, upload in zip(enables, uploads))

    expected_prefixes = {first["prefix"], second["prefix"], "home/ota-peer"}
    expected_topics = {
        topic
        for prefix in expected_prefixes
        for topic in (
            f"{prefix}/status",
            f"{prefix}/available",
            f"{prefix}/update/device-config/result",
        )
    }
    publish_events = [
        (index, event) for index, event in enumerate(events) if event[0] == "publish"
    ]
    assert {event[1] for _, event in publish_events} == expected_topics
    assert all(event[2:] == ("", True, 1) for _, event in publish_events)
    waiting_for_lwt = False
    for event in events:
        if event[0] == "disable":
            waiting_for_lwt = True
        elif event[0] == "lwt":
            assert waiting_for_lwt
            waiting_for_lwt = False
        elif event[0] == "publish":
            assert not waiting_for_lwt
    disable_indices = [
        index for index, event in enumerate(events) if event[0] == "disable"
    ]
    assert len(disable_indices) == 2
    powered = False
    for event in events:
        if event[0] == "enable":
            powered = True
        elif event[0] == "disable":
            powered = False
        elif event[0] == "publish":
            assert not powered
    uploaded_dirs = [event[1] for event in events if event[0] == "upload"]
    assert all(not cast(Path, uploaded_dir).exists() for uploaded_dir in uploaded_dirs)
