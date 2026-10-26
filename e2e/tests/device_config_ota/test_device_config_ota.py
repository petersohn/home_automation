import hashlib
import json
import time
from pathlib import Path
from typing import Any
from uuid import uuid4

from lib.device_config_ota import (
    build_device_config_payload,
    decode_ota_device_status,
    decode_ota_stream_report,
    is_ota_result_topic,
    matches_ota_status,
    send_device_config_update,
    sign_device_config_payload,
)
from lib.serial_log import serial_log_contains_marker_after, serial_log_offset
from lib.signature_stream import StreamOutcome, StreamTarget, send_stream


E2E_DIR = Path(__file__).resolve().parents[2]
HOST_KEY = E2E_DIR / "signing_fixtures/host/signing_test_private_key.pem"
UINT32_MAX = 0xFFFFFFFF
RESULT_TIMEOUT = 30.0
STATUS_REFRESH_TIMEOUT = 60.0


def _value(baseline: dict[str, object], key: str) -> Any:
    return baseline[key]


def _result_topic(prefix: str) -> str:
    return f"{prefix}/update/device-config/result"


def _status_topic(prefix: str) -> str:
    return f"{prefix}/status"


def _watch_prefixes(
    baseline: dict[str, object], prefixes: tuple[str, ...]
) -> dict[str, int]:
    client = _value(baseline, "client")
    _value(baseline, "owned_prefixes").update(prefixes)
    topics = [
        topic
        for prefix in prefixes
        for topic in (_result_topic(prefix), _status_topic(prefix))
    ]
    for topic in topics:
        client.subscribe(topic)
    assert client.wait_ready(timeout=10.0), "MQTT subscriptions not ready"
    return {topic: client.get_event_count(topic) for topic in topics}


def _send_envelope(
    baseline: dict[str, object],
    service_prefix: str,
    payload: bytes,
    *,
    signature: bytes | None = None,
) -> StreamOutcome:
    client = _value(baseline, "client")
    mac = _value(baseline, "mac")
    if signature is None:
        signature = sign_device_config_payload(payload, HOST_KEY)
    return send_stream(
        client,
        f"{service_prefix}/update/device-config",
        [StreamTarget(mac, _result_topic(service_prefix))],
        payload,
        signature,
        final_status="STAGED",
        report_decoder=decode_ota_stream_report,
    )


def _send_update(
    baseline: dict[str, object], service_prefix: str, config: dict[str, object]
) -> tuple[StreamOutcome, bytes]:
    payload = build_device_config_payload(_value(baseline, "mac"), config)
    signature = sign_device_config_payload(payload, HOST_KEY)
    outcome = send_device_config_update(
        _value(baseline, "client"),
        service_prefix=service_prefix,
        target_mac=_value(baseline, "mac"),
        config=config,
        signature=signature,
    )
    return outcome, payload


def _wait_lifecycle(
    baseline: dict[str, object],
    prefixes: tuple[str, ...],
    cursors: dict[str, int],
    *,
    version: int,
    payload: bytes,
    expected_status: str,
    timeout: float = RESULT_TIMEOUT,
) -> tuple[str, dict[str, object]]:
    client = _value(baseline, "client")
    expected_hash = hashlib.sha256(payload).hexdigest()
    topics = [_result_topic(prefix) for prefix in prefixes]
    assert all(is_ota_result_topic(topic, prefixes) for topic in topics)
    deadline = time.monotonic() + timeout
    latest: dict[str, object] = {}
    while time.monotonic() < deadline:
        for topic in topics:
            count = client.get_event_count(topic)
            if count <= cursors.get(topic, 0):
                continue
            raw = client.get_state(topic)
            if raw is None:
                cursors[topic] = count
                continue
            report = decode_ota_stream_report(raw)
            try:
                result = json.loads(raw)
            except json.JSONDecodeError:
                cursors[topic] = count
                continue
            if report is None or not isinstance(result, dict):
                cursors[topic] = count
                continue
            if report.hash != expected_hash:
                cursors[topic] = count
                continue
            prefix = topic.removesuffix("/update/device-config/result")
            status_topic = _status_topic(prefix)
            status_count = client.get_event_count(status_topic)
            if status_count <= cursors.get(status_topic, 0):
                continue
            status_raw = client.get_state(status_topic)
            status = (
                decode_ota_device_status(status_raw)
                if status_raw is not None
                else None
            )
            if status is None or status["mac"] != _value(baseline, "mac"):
                continue
            last_update = status["lastConfigUpdate"]
            if not isinstance(last_update, dict):
                continue
            if expected_status == "CONFIRMED":
                status_matches = (
                    status["version"] == version
                    and last_update["status"] in {"STAGED", "CONFIRMED"}
                )
            else:
                status_matches = last_update["status"] == expected_status
            if (
                not status_matches
                or last_update["version"] != version
                or last_update["hash"] != expected_hash
            ):
                continue
            observed = {
                "mac": status["mac"],
                "version": result.get("version"),
                "hash": report.hash,
                "status": report.status,
            }
            latest = result
            if matches_ota_status(
                expected_mac=_value(baseline, "mac"),
                expected_version=version,
                expected_hash=expected_hash,
                expected_status=expected_status,
                observed_status=observed,
                event_is_new=count > cursors.get(topic, 0),
            ):
                cursors[topic] = count
                assert is_ota_result_topic(topic, prefixes)
                return topic, result
            cursors[topic] = count
        time.sleep(0.05)
    raise AssertionError(
        f"No fresh {expected_status} for version {version} on {topics}; "
        f"last result was {latest}"
    )


def _wait_for_status(
    client: Any,
    prefix: str,
    previous_event: int,
    *,
    version: int,
    minimum_accepted_version: int | None = None,
    timeout: float = RESULT_TIMEOUT,
) -> dict[str, object]:
    topic = _status_topic(prefix)
    deadline = time.monotonic() + timeout
    latest: str | None = None
    while time.monotonic() < deadline:
        if client.get_event_count(topic) > previous_event:
            latest = client.get_state(topic)
            status = decode_ota_device_status(latest) if latest is not None else None
            if (
                status is not None
                and status["version"] == version
                and status["minimumAcceptedConfigVersion"]
                == minimum_accepted_version
            ):
                return status
        time.sleep(0.05)
    raise AssertionError(
        f"No fresh matching status on {topic}; last status was {latest}"
    )


def _base_update(version: int, prefix: str, **changes: object) -> dict[str, object]:
    return {
        "debug": True,
        "name": f"testDeviceV{version}",
        "servicePrefix": prefix,
        "otaEnabled": True,
        "configVersion": version,
        "interfaces": [],
        "actions": [],
        **changes,
    }


def _assert_staged(outcome: StreamOutcome, baseline: dict[str, object]) -> None:
    mac = _value(baseline, "mac")
    assert outcome.verified == {mac}
    assert outcome.dropped == {}


def test_sequential_updates_confirm_versions_across_alternating_slots(
    ota_baseline: dict[str, object],
) -> None:
    client = _value(ota_baseline, "client")
    current_prefix = _value(ota_baseline, "prefix")
    for version in (2, 3):
        next_prefix = f"home/ota-version-{version}-{uuid4().hex[:8]}"
        cursors = _watch_prefixes(ota_baseline, (current_prefix, next_prefix))
        config = _base_update(version, next_prefix)
        outcome, payload = _send_update(ota_baseline, current_prefix, config)
        _assert_staged(outcome, ota_baseline)
        topic, result = _wait_lifecycle(
            ota_baseline,
            (current_prefix, next_prefix),
            cursors,
            version=version,
            payload=payload,
            expected_status="CONFIRMED",
        )
        assert topic == _result_topic(next_prefix)
        assert result["status"] == "CONFIRMED"
        current_prefix = next_prefix


def test_invalid_signature_is_rejected(
    ota_baseline: dict[str, object],
) -> None:
    config = _base_update(2, _value(ota_baseline, "prefix"))
    payload = build_device_config_payload(_value(ota_baseline, "mac"), config)

    outcome = _send_envelope(
        ota_baseline, _value(ota_baseline, "prefix"), payload, signature=bytes(64)
    )

    assert outcome.verified == set()
    assert outcome.dropped == {
        _value(ota_baseline, "mac"): "FAIL invalid signature"
    }


def test_valid_signature_for_wrong_target_is_rejected(
    ota_baseline: dict[str, object],
) -> None:
    wrong_mac = "11:22:33:44:55:66"
    config = _base_update(2, _value(ota_baseline, "prefix"))
    payload = build_device_config_payload(wrong_mac, config)

    outcome = _send_envelope(ota_baseline, _value(ota_baseline, "prefix"), payload)

    assert outcome.verified == set()
    assert outcome.dropped == {
        _value(ota_baseline, "mac"): "ERROR target-mismatch"
    }


def test_disabled_ota_candidate_is_rejected(
    ota_baseline: dict[str, object],
) -> None:
    prefix = _value(ota_baseline, "prefix")
    config = _base_update(2, prefix, otaEnabled=False)
    payload = build_device_config_payload(_value(ota_baseline, "mac"), config)

    outcome = _send_envelope(ota_baseline, prefix, payload)

    assert outcome.verified == set()
    assert outcome.dropped == {
        _value(ota_baseline, "mac"): "ERROR ota-disabled"
    }


def test_invalid_interfaces_are_tolerated_in_confirmed_candidate(
    ota_baseline: dict[str, object],
) -> None:
    client = _value(ota_baseline, "client")
    old_prefix = _value(ota_baseline, "prefix")
    new_prefix = f"home/ota-invalid-interface-{uuid4().hex[:8]}"
    cursors = _watch_prefixes(ota_baseline, (old_prefix, new_prefix))
    config = _base_update(
        2,
        new_prefix,
        interfaces=[{"type": "unsupported-hardware", "name": "ignored"}],
    )
    outcome, payload = _send_update(ota_baseline, old_prefix, config)
    _assert_staged(outcome, ota_baseline)

    topic, _ = _wait_lifecycle(
        ota_baseline,
        (old_prefix, new_prefix),
        cursors,
        version=2,
        payload=payload,
        expected_status="CONFIRMED",
    )

    assert topic == _result_topic(new_prefix)


def _publish_retained_peer(client: Any, prefix: str) -> None:
    availability = f"{prefix}/available"
    status = _status_topic(prefix)
    previous_status_event = client.get_event_count(status)
    peer_status = json.dumps(
        {"name": "differentPeer", "mac": "11:22:33:44:55:66"}
    )
    client.publish(availability, "1", retain=True, qos=1)
    client.publish(status, peer_status, retain=True, qos=1)
    assert client.wait_for_next_state(
        status, previous_status_event, timeout=10.0
    ) == peer_status


def test_differently_named_collision_rolls_back_and_replay_is_rejected(
    ota_baseline: dict[str, object],
) -> None:
    client = _value(ota_baseline, "client")
    old_prefix = _value(ota_baseline, "prefix")
    collision_prefix = f"home/ota-collision-{uuid4().hex[:8]}"
    cursors = _watch_prefixes(ota_baseline, (old_prefix, collision_prefix))
    config = _base_update(
        2, collision_prefix, name="candidateWithDifferentName"
    )
    _publish_retained_peer(client, collision_prefix)
    outcome, payload = _send_update(ota_baseline, old_prefix, config)
    _assert_staged(outcome, ota_baseline)
    _, rollback = _wait_lifecycle(
        ota_baseline,
        (old_prefix, collision_prefix),
        cursors,
        version=2,
        payload=payload,
        expected_status="ROLLED_BACK",
    )
    assert rollback["reason"] == "candidate-collision"

    replay = _send_update(ota_baseline, old_prefix, config)[0]
    assert replay.verified == set()
    assert replay.dropped == {
        _value(ota_baseline, "mac"): "ERROR version-too-old"
    }


def _wait_for_serial_marker(
    path: Path, marker: str, timeout: float, *, start_offset: int
) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if serial_log_contains_marker_after(path, marker, start_offset):
                return True
        except OSError:
            pass
        time.sleep(0.05)
    return False


def test_interrupted_trial_rolls_back_before_mqtt_readiness(
    ota_baseline: dict[str, object],
) -> None:
    client = _value(ota_baseline, "client")
    device = _value(ota_baseline, "device")
    old_prefix = _value(ota_baseline, "prefix")
    trial_prefix = f"home/ota-interrupted-{uuid4().hex[:8]}"
    cursors = _watch_prefixes(ota_baseline, (old_prefix, trial_prefix))
    retained_availability = f"{trial_prefix}/available"
    config = _base_update(2, trial_prefix)
    client.subscribe(retained_availability)
    assert client.wait_ready(timeout=10.0), "Availability subscription not ready"
    previous_availability_event = client.get_event_count(retained_availability)
    client.publish(retained_availability, "1", retain=True, qos=1)
    assert client.wait_for_next_state(
        retained_availability, previous_availability_event, timeout=10.0
    ) == "1"

    serial_log = _value(ota_baseline, "serial_log")
    marker_offset = serial_log_offset(serial_log)
    outcome, payload = _send_update(ota_baseline, old_prefix, config)
    _assert_staged(outcome, ota_baseline)

    assert _wait_for_serial_marker(
        serial_log,
        "Configuration boot selection: trial",
        timeout=15.0,
        start_offset=marker_offset,
    ), "No durable trial-start serial marker observed"
    previous_old_status_event = client.get_event_count(_status_topic(old_prefix))
    device.reset()

    _, rollback = _wait_lifecycle(
        ota_baseline,
        (old_prefix, trial_prefix),
        cursors,
        version=2,
        payload=payload,
        expected_status="ROLLED_BACK",
    )
    assert rollback["reason"] == "interrupted-trial"
    status = _wait_for_status(
        client,
        old_prefix,
        previous_old_status_event,
        version=1,
        minimum_accepted_version=3,
    )
    assert status["trialState"] == "idle"
    old_status_raw = client.get_state(_status_topic(old_prefix))
    assert old_status_raw is not None
    old_status = json.loads(old_status_raw)
    assert old_status["name"] == "testDevice"

    replay, replay_payload = _send_update(ota_baseline, old_prefix, config)
    assert replay_payload == payload
    assert json.loads(replay_payload) == {
        "type": "device-config",
        "target": _value(ota_baseline, "mac"),
        "config": config,
    }
    assert replay.verified == set()
    assert replay.dropped == {
        _value(ota_baseline, "mac"): "ERROR version-too-old"
    }
    old_status_raw = client.get_state(_status_topic(old_prefix))
    assert old_status_raw is not None
    old_status = json.loads(old_status_raw)
    assert old_status["configVersion"] == 1
    assert old_status["minimumAcceptedConfigVersion"] == 3
    assert old_status["trialState"] == "idle"
    assert old_status["lastConfigUpdate"]["status"] == "ROLLED_BACK"


def test_maximum_version_exhausts_floor_to_null(
    ota_max_version_baseline: dict[str, object],
) -> None:
    client = _value(ota_max_version_baseline, "client")
    old_prefix = _value(ota_max_version_baseline, "prefix")
    new_prefix = f"home/ota-max-version-{uuid4().hex[:8]}"
    cursors = _watch_prefixes(ota_max_version_baseline, (old_prefix, new_prefix))
    config = _base_update(UINT32_MAX, new_prefix)
    outcome, payload = _send_update(ota_max_version_baseline, old_prefix, config)
    _assert_staged(outcome, ota_max_version_baseline)

    topic, result = _wait_lifecycle(
        ota_max_version_baseline,
        (old_prefix, new_prefix),
        cursors,
        version=UINT32_MAX,
        payload=payload,
        expected_status="CONFIRMED",
    )
    assert topic == _result_topic(new_prefix)
    assert result["version"] == UINT32_MAX
    status_topic = _status_topic(new_prefix)
    status = _wait_for_status(
        client,
        new_prefix,
        cursors[status_topic],
        version=UINT32_MAX,
    )
    assert status["minimumAcceptedConfigVersion"] is None

    replay_config = _base_update(
        UINT32_MAX, new_prefix, name="testDeviceVMaxReplay"
    )
    status_event = client.get_event_count(status_topic)
    replay = _send_update(ota_max_version_baseline, new_prefix, replay_config)[0]
    assert replay.verified == set()
    assert replay.dropped == {
        _value(ota_max_version_baseline, "mac"): "ERROR version-range-exhausted"
    }

    status = _wait_for_status(
        client,
        new_prefix,
        status_event,
        version=UINT32_MAX,
        timeout=STATUS_REFRESH_TIMEOUT,
    )
    assert status["minimumAcceptedConfigVersion"] is None
    assert status["trialState"] == "idle"

    active_status_raw = client.get_state(status_topic)
    assert active_status_raw is not None
    active_status = json.loads(active_status_raw)
    assert active_status["name"] == config["name"]
    assert active_status["configVersion"] == UINT32_MAX
    assert active_status["minimumAcceptedConfigVersion"] is None
    assert active_status["trialState"] == "idle"
