"""Host helpers for signing and delivering device-configuration OTA payloads."""

import json
import re
import time
from collections.abc import Iterable, Mapping
from pathlib import Path
from typing import Any

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

from .mqtt_client import MqttClient
from .signature_stream import StreamOutcome, StreamReport, StreamTarget, send_stream


UINT32_MAX = 0xFFFFFFFF
_HASH_RE = re.compile(r"[0-9a-f]{64}\Z")
_MAC_RE = re.compile(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\Z", re.IGNORECASE)
_RESULT_STATUSES = {
    "PROGRESS",
    "FAIL",
    "ERROR",
    "STAGED",
    "CONFIRMED",
    "ROLLED_BACK",
}
_LIFECYCLE_STATUSES = {"STAGED", "CONFIRMED", "ROLLED_BACK"}
_TRIAL_STATES = {"idle", "pending", "started"}


class _DuplicateJsonKey(ValueError):
    pass


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise _DuplicateJsonKey
        result[key] = value
    return result


def decode_ota_stream_report(payload: str) -> StreamReport | None:
    """Decode strict OTA result JSON into a stream ACK/failure report."""
    if not isinstance(payload, str):
        return None
    try:
        result = json.loads(payload, object_pairs_hook=_unique_object)
    except (TypeError, ValueError, RecursionError):
        return None

    fields = {"status", "transferred", "hash", "version", "reason"}
    if not isinstance(result, dict) or result.keys() != fields:
        return None

    status = result["status"]
    transferred = result["transferred"]
    payload_hash = result["hash"]
    version = result["version"]
    reason = result["reason"]
    if (
        not isinstance(status, str)
        or status not in _RESULT_STATUSES
        or type(transferred) is not int
        or not 0 <= transferred <= UINT32_MAX
        or not isinstance(payload_hash, str)
        or (payload_hash and not _HASH_RE.fullmatch(payload_hash))
        or (
            version is not None
            and (type(version) is not int or not 0 <= version <= UINT32_MAX)
        )
        or not isinstance(reason, str)
    ):
        return None
    try:
        reason_bytes = reason.encode("utf-8")
    except UnicodeEncodeError:
        return None
    if len(reason_bytes) > 64 or any(ord(character) < 0x20 for character in reason):
        return None
    if status in _LIFECYCLE_STATUSES and (
        version is None or not _HASH_RE.fullmatch(payload_hash)
    ):
        return None

    if status in {"FAIL", "ERROR"} and reason:
        status = f"{status} {reason}"
    return StreamReport(status, transferred, payload_hash)


def decode_ota_device_status(payload: str) -> dict[str, object] | None:
    """Decode status fields used to correlate OTA lifecycle outcomes."""
    if not isinstance(payload, str):
        return None
    try:
        result = json.loads(payload, object_pairs_hook=_unique_object)
    except (TypeError, ValueError, RecursionError):
        return None

    if not isinstance(result, dict):
        return None
    required_fields = {
        "mac",
        "configVersion",
        "minimumAcceptedConfigVersion",
        "trialState",
        "lastConfigUpdate",
    }
    if not required_fields.issubset(result):
        return None

    mac = result["mac"]
    version = result["configVersion"]
    minimum = result["minimumAcceptedConfigVersion"]
    trial_state = result["trialState"]
    last_update = result["lastConfigUpdate"]
    if (
        type(mac) is not str
        or not _MAC_RE.fullmatch(mac)
        or type(version) is not int
        or not 0 <= version <= UINT32_MAX
        or (
            minimum is not None
            and (type(minimum) is not int or not 1 <= minimum <= UINT32_MAX)
        )
        or type(trial_state) is not str
        or trial_state not in _TRIAL_STATES
    ):
        return None

    if last_update is not None:
        fields = {"status", "transferred", "hash", "version", "reason"}
        if not isinstance(last_update, dict) or last_update.keys() != fields:
            return None
        reason = last_update["reason"]
        if not isinstance(reason, str):
            return None
        try:
            reason_bytes = reason.encode("utf-8")
        except UnicodeEncodeError:
            return None
        if (
            type(last_update["status"]) is not str
            or last_update["status"] not in _LIFECYCLE_STATUSES
            or type(last_update["transferred"]) is not int
            or not 0 <= last_update["transferred"] <= UINT32_MAX
            or type(last_update["hash"]) is not str
            or not _HASH_RE.fullmatch(last_update["hash"])
            or type(last_update["version"]) is not int
            or not 0 <= last_update["version"] <= UINT32_MAX
            or len(reason_bytes) > 64
            or any(ord(character) < 0x20 for character in reason)
        ):
            return None

    return {
        "mac": mac,
        "version": version,
        "minimumAcceptedConfigVersion": minimum,
        "trialState": trial_state,
        "lastConfigUpdate": last_update,
    }


def build_device_config_payload(target_mac: str, config: dict) -> bytes:
    """Create compact UTF-8 bytes for a MAC-targeted configuration envelope."""
    if not isinstance(target_mac, str) or not _MAC_RE.fullmatch(target_mac):
        raise ValueError("target MAC must contain six hexadecimal octets")
    if not isinstance(config, dict):
        raise ValueError("configuration must be an object")
    config_version = config.get("configVersion")
    if type(config_version) is not int or not 0 <= config_version <= UINT32_MAX:
        raise ValueError("configVersion must be an unsigned 32-bit integer")

    envelope = {"type": "device-config", "target": target_mac, "config": config}
    try:
        serialized = json.dumps(
            envelope,
            separators=(",", ":"),
            ensure_ascii=False,
            allow_nan=False,
        )
        return serialized.encode("utf-8")
    except (TypeError, ValueError, UnicodeEncodeError, RecursionError) as error:
        raise ValueError("configuration must contain valid JSON values") from error


def sign_device_config_payload(payload: bytes, private_key_path: Path) -> bytes:
    """Sign exact payload bytes using a host P-256 PEM key; return raw64 ECDSA."""
    if not isinstance(payload, bytes):
        raise ValueError("payload must be bytes")
    private_key = serialization.load_pem_private_key(
        Path(private_key_path).read_bytes(), password=None
    )
    if not isinstance(private_key, ec.EllipticCurvePrivateKey) or not isinstance(
        private_key.curve, ec.SECP256R1
    ):
        raise ValueError("signing key must use the P-256 curve")
    der_signature = private_key.sign(payload, ec.ECDSA(hashes.SHA256()))
    r, s = utils.decode_dss_signature(der_signature)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def send_device_config_update(
    client: MqttClient,
    *,
    service_prefix: str,
    target_mac: str,
    config: dict[str, object],
    signature: bytes,
    timeout: float = 5.0,
) -> StreamOutcome:
    """Send one config update; verified means STAGED ACK, not confirmation."""
    if not isinstance(service_prefix, str):
        raise ValueError("service prefix must be a string")
    prefix = service_prefix.rstrip("/")
    if not prefix or any(character in prefix for character in "\0#+"):
        raise ValueError("service prefix is not usable")
    try:
        prefix_bytes = prefix.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ValueError("service prefix must be valid UTF-8") from error
    if len(prefix_bytes) > 482:
        raise ValueError("service prefix exceeds 482 bytes")

    payload = build_device_config_payload(target_mac, config)
    command_topic = f"{prefix}/update/device-config"
    result_topic = f"{prefix}/update/device-config/result"
    return send_stream(
        client,
        command_topic,
        [StreamTarget(target_mac, result_topic)],
        payload,
        signature,
        timeout=timeout,
        final_status="STAGED",
        report_decoder=decode_ota_stream_report,
    )


def wait_for_device_lwt_then_clear(
    client: MqttClient,
    device: Any,
    service_prefixes: Iterable[str],
    timeout: float = 30.0,
) -> None:
    """Power off device, observe fresh LWT, then clear owned retained topics."""
    prefixes = tuple(dict.fromkeys(service_prefixes))
    availability_topics = tuple(f"{prefix}/available" for prefix in prefixes)
    if not availability_topics:
        device.disable()
        return

    for topic in availability_topics:
        client.subscribe(topic)
    try:
        if not client.wait_ready(timeout=timeout):
            raise AssertionError("MQTT availability subscriptions not ready")
        cursors = {
            topic: client.get_event_count(topic) for topic in availability_topics
        }
    finally:
        device.disable()

    deadline = time.monotonic() + timeout
    while True:
        for topic in availability_topics:
            if (
                client.get_event_count(topic) > cursors[topic]
                and client.get_state(topic) == "0"
            ):
                for prefix in prefixes:
                    for retained_topic in (
                        f"{prefix}/status",
                        f"{prefix}/available",
                        f"{prefix}/update/device-config/result",
                    ):
                        try:
                            client.publish(retained_topic, "", retain=True, qos=1)
                        except Exception:
                            pass
                return
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        time.sleep(min(0.01, remaining))
    raise AssertionError(
        f"No fresh offline LWT on owned availability topics {availability_topics}"
    )


def matches_ota_status(
    *,
    expected_mac: str,
    expected_version: int,
    expected_hash: str,
    expected_status: str,
    observed_status: object,
    event_is_new: bool,
) -> bool:
    """Match a fresh OTA lifecycle status against expected identity and outcome."""
    if (
        event_is_new is not True
        or type(expected_mac) is not str
        or not _MAC_RE.fullmatch(expected_mac)
        or type(expected_version) is not int
        or not 0 <= expected_version <= UINT32_MAX
        or type(expected_hash) is not str
        or not _HASH_RE.fullmatch(expected_hash)
        or type(expected_status) is not str
        or expected_status not in _RESULT_STATUSES
        or not isinstance(observed_status, Mapping)
    ):
        return False

    try:
        mac = observed_status["mac"]
        version = observed_status["version"]
        payload_hash = observed_status["hash"]
        status = observed_status["status"]
    except Exception:
        return False

    return (
        type(mac) is str
        and _MAC_RE.fullmatch(mac) is not None
        and mac.lower() == expected_mac.lower()
        and type(version) is int
        and 0 <= version <= UINT32_MAX
        and version == expected_version
        and type(payload_hash) is str
        and _HASH_RE.fullmatch(payload_hash) is not None
        and payload_hash == expected_hash
        and type(status) is str
        and status in _RESULT_STATUSES
        and status == expected_status
    )


def is_ota_result_topic(topic: str, service_prefixes: Iterable[str]) -> bool:
    """Match an OTA result topic using only the explicitly supplied prefixes."""
    if (
        type(topic) is not str
        or "\0" in topic
        or "+" in topic
        or "#" in topic
        or isinstance(service_prefixes, (str, bytes))
    ):
        return False

    try:
        for prefix in service_prefixes:
            if (
                type(prefix) is str
                and prefix
                and not any(character in prefix for character in "\0+#")
                and topic == f"{prefix}/update/device-config/result"
            ):
                return True
    except Exception:
        return False
    return False
