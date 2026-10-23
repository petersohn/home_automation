"""ACK-gated broadcaster for streamed signature verification tests."""

import base64
import hashlib
import json
import re
import time
from collections.abc import Callable
from dataclasses import dataclass

from .mqtt_client import MqttClient


UINT32_MAX = 0xFFFFFFFF
_HASH_RE = re.compile(r"[0-9a-f]{64}\Z")
_COUNT_RE = re.compile(r"(?:0|[1-9][0-9]*)\Z")
RESTART_INVALIDATION_TIMEOUT = 25.0


@dataclass
class StreamTarget:
    name: str
    result_topic: str


@dataclass
class StreamOutcome:
    verified: set[str]
    dropped: dict[str, str]


@dataclass(frozen=True)
class StreamReport:
    status: str
    transferred: int
    hash: str


def decode_stream_report(payload: str) -> StreamReport | None:
    try:
        values = json.loads(payload)
    except (TypeError, json.JSONDecodeError, RecursionError):
        return None
    if (
        not isinstance(values, list)
        or len(values) != 3
        or any(not isinstance(value, str) for value in values)
    ):
        return None

    status, count_text, payload_hash = values
    if len(count_text) > 10 or not _COUNT_RE.fullmatch(count_text):
        return None
    transferred = int(count_text)
    if transferred > UINT32_MAX:
        return None
    if payload_hash == "":
        if status != "ERROR" and not status.startswith("ERROR "):
            return None
    elif not _HASH_RE.fullmatch(payload_hash):
        return None
    return StreamReport(status, transferred, payload_hash)


def verify_broker_restart_invalidation(
    client: MqttClient,
    *,
    stop_broker: Callable[[], None],
    start_broker: Callable[[], None],
    command_topic: str,
    result_topic: str,
    partial_command: str,
    probe_command: str,
    payload_hash: str,
    partial_count: int,
    timeout: float = RESTART_INVALIDATION_TIMEOUT,
    restart_delay: float = 3.0,
    poll_interval: float = 0.05,
    monotonic: Callable[[], float] | None = None,
    sleep: Callable[[float], None] | None = None,
) -> None:
    if (
        isinstance(timeout, bool)
        or not isinstance(timeout, (int, float))
        or not 0 < timeout <= RESTART_INVALIDATION_TIMEOUT
    ):
        raise ValueError("restart invalidation timeout must be at most 25 seconds")
    if restart_delay < 0 or poll_interval <= 0:
        raise ValueError(
            "restart delay must be nonnegative and poll interval positive"
        )

    clock = monotonic or time.monotonic
    sleeper = sleep or time.sleep
    _, previous_generation = client.get_readiness_snapshot()
    partial_event_baseline = client.get_event_count(result_topic)
    deadline = clock() + timeout

    def remaining(stage: str) -> float:
        amount = deadline - clock()
        if amount <= 0:
            raise TimeoutError(
                f"broker restart invalidation deadline expired during {stage}"
            )
        return amount

    def wait_until(check: Callable[[], bool], stage: str) -> None:
        while True:
            remaining(stage)
            matched = check()
            if matched:
                remaining(stage)
                return
            sleeper(min(poll_interval, remaining(stage)))

    def wait_for_report(
        baseline: int,
        match: Callable[[StreamReport], bool],
        stage: str,
    ) -> None:
        seen_event = baseline

        def check() -> bool:
            nonlocal seen_event
            event_count = client.get_event_count(result_topic)
            if event_count <= seen_event:
                return False
            seen_event = event_count
            payload = client.get_state(result_topic)
            report = decode_stream_report(payload) if payload is not None else None
            return (
                report is not None
                and report.hash == payload_hash
                and match(report)
            )

        wait_until(check, stage)

    try:
        client.publish(command_topic, partial_command, retain=False, qos=1)
        remaining("partial-chunk publish")
        wait_for_report(
            partial_event_baseline,
            lambda report: report.status == "PROGRESS"
            and report.transferred == partial_count,
            "partial-chunk acknowledgement",
        )
        stop_broker()
        remaining("broker stop")
        sleeper(min(restart_delay, remaining("broker restart delay")))
        remaining("broker restart delay")
    finally:
        start_broker()

    remaining("broker start")

    def observer_is_reconnected() -> bool:
        ready_now, generation_now = client.get_readiness_snapshot()
        return ready_now and generation_now > previous_generation

    wait_until(observer_is_reconnected, "observer reconnection")

    remaining("probe baseline")
    probe_event_baseline = client.get_event_count(result_topic)
    remaining("probe publish")
    client.publish(command_topic, probe_command, retain=False, qos=1)
    remaining("probe publish")
    next_probe_at = clock() + 1.0
    seen_event = probe_event_baseline
    while True:
        remaining("no-active-transfer probe response")
        event_count = client.get_event_count(result_topic)
        if event_count > seen_event:
            seen_event = event_count
            payload = client.get_state(result_topic)
            report = decode_stream_report(payload) if payload is not None else None
            if (
                report is not None
                and report.hash == payload_hash
                and report.status == "ERROR no active transfer"
                and report.transferred == 0
            ):
                remaining("no-active-transfer probe response")
                return

        now = clock()
        if now >= next_probe_at:
            remaining("probe publish")
            client.publish(command_topic, probe_command, retain=False, qos=1)
            remaining("probe publish")
            next_probe_at = clock() + 1.0
            continue

        sleeper(
            min(
                poll_interval,
                remaining("no-active-transfer probe response"),
                max(0.0, next_probe_at - clock()),
            )
        )


def _compact_command(command: dict[str, object]) -> str:
    payload = json.dumps(command, separators=(",", ":"))
    if len(payload.encode("utf-8")) > 512:
        raise ValueError("stream command exceeds 512 bytes")
    return payload


def _is_failure(status: str) -> bool:
    return (
        status == "FAIL"
        or status.startswith("FAIL ")
        or status == "ERROR"
        or status.startswith("ERROR ")
    )


def send_stream(
    client: MqttClient,
    command_topic: str,
    targets: list[StreamTarget],
    data: bytes,
    signature: bytes,
    *,
    timeout: float = 5.0,
    retry_interval: float = 1.0,
    chunk_size: int = 256,
) -> StreamOutcome:
    if not targets:
        raise ValueError("at least one stream target is required")
    if not isinstance(command_topic, str) or not command_topic:
        raise ValueError("command topic must be nonempty")
    names: set[str] = set()
    result_topics: set[str] = set()
    for target in targets:
        if not isinstance(target.name, str) or not target.name:
            raise ValueError("target name must be nonempty")
        if not isinstance(target.result_topic, str) or not target.result_topic:
            raise ValueError("target result topic must be nonempty")
        if target.name in names or target.result_topic in result_topics:
            raise ValueError("target names and result topics must be unique")
        names.add(target.name)
        result_topics.add(target.result_topic)
    if not isinstance(data, bytes) or len(data) > UINT32_MAX:
        raise ValueError("stream data must fit in UINT32_MAX bytes")
    if not isinstance(signature, bytes) or len(signature) != 64:
        raise ValueError("signature must contain exactly 64 bytes")
    if type(chunk_size) is not int or not 1 <= chunk_size <= 256:
        raise ValueError("chunk size must be between 1 and 256")
    if (
        isinstance(timeout, bool)
        or isinstance(retry_interval, bool)
        or not isinstance(timeout, (int, float))
        or not isinstance(retry_interval, (int, float))
        or not 0 < retry_interval < timeout < 30
    ):
        raise ValueError("require 0 < retry_interval < timeout < 30")

    payload_hash = hashlib.sha256(data).hexdigest()
    begin = _compact_command(
        {"type": "begin", "hash": payload_hash, "size": len(data)}
    )
    finish = _compact_command(
        {
            "type": "finish",
            "hash": payload_hash,
            "signature": base64.b64encode(signature).decode("ascii"),
        }
    )

    for target in targets:
        client.subscribe(target.result_topic)
    if not client.wait_ready(timeout):
        raise ConnectionError("MQTT client did not become ready")
    ready, ready_generation = client.get_readiness_snapshot()
    if not ready:
        raise ConnectionError("MQTT readiness changed before stream start")

    def check_readiness() -> None:
        ready_now, generation_now = client.get_readiness_snapshot()
        if not ready_now or generation_now != ready_generation:
            raise ConnectionError(
                "MQTT readiness lost or connection generation changed"
            )

    active = {target.name: target for target in targets}
    dropped: dict[str, str] = {}
    event_cursors = {
        name: client.get_event_count(target.result_topic)
        for name, target in active.items()
    }

    def drop_target(
        name: str,
        reason: str,
        pending: set[str] | None = None,
        acknowledged: set[str] | None = None,
    ) -> None:
        active.pop(name, None)
        dropped[name] = reason
        if pending is not None:
            pending.discard(name)
        if acknowledged is not None:
            acknowledged.discard(name)

    def scan_terminal_failures(
        pending: set[str] | None = None,
        acknowledged: set[str] | None = None,
    ) -> None:
        for name, target in tuple(active.items()):
            event_count = client.get_event_count(target.result_topic)
            if event_count <= event_cursors[name]:
                continue
            state = client.get_state(target.result_topic)
            if state is None:
                continue
            event_cursors[name] = event_count
            report = decode_stream_report(state)
            if report is not None and report.hash == payload_hash and _is_failure(
                report.status
            ):
                drop_target(name, report.status, pending, acknowledged)

    def send_command(frame: str, expected_count: int, final: bool = False) -> set[str]:
        check_readiness()
        scan_terminal_failures()
        check_readiness()
        if not active:
            return set()

        baselines = {name: event_cursors[name] for name in active}
        started = time.monotonic()
        deadlines = {name: started + timeout for name in active}
        pending = set(active)
        acknowledged: set[str] = set()
        next_retry = started + retry_interval if not final else float("inf")
        check_readiness()
        client.publish(command_topic, frame, retain=False, qos=1)

        while pending:
            check_readiness()
            now = time.monotonic()
            for name in tuple(pending):
                if now >= deadlines[name]:
                    drop_target(name, "timeout", pending, acknowledged)

            for name, target in tuple(active.items()):
                check_readiness()
                event_count = client.get_event_count(target.result_topic)
                if (
                    event_count <= baselines[name]
                    or event_count <= event_cursors[name]
                ):
                    continue
                state = client.get_state(target.result_topic)
                if state is None:
                    continue
                event_cursors[name] = event_count
                report = decode_stream_report(state)
                if report is None or report.hash != payload_hash:
                    continue
                if _is_failure(report.status):
                    drop_target(name, report.status, pending, acknowledged)
                    continue
                if name not in pending:
                    continue
                if time.monotonic() >= deadlines[name]:
                    drop_target(name, "timeout", pending, acknowledged)
                    continue
                if report.transferred != expected_count:
                    continue
                if final:
                    matches = report.status == "OK"
                else:
                    matches = report.status == "PROGRESS"
                if matches:
                    acknowledged.add(name)
                    pending.remove(name)

            if not pending:
                break
            now = time.monotonic()
            if not final and now >= next_retry:
                check_readiness()
                client.publish(command_topic, frame, retain=False, qos=1)
                next_retry += retry_interval
                continue

            time.sleep(0.01)
        check_readiness()
        scan_terminal_failures(pending, acknowledged)
        check_readiness()
        return acknowledged

    if not send_command(begin, 0):
        check_readiness()
        return StreamOutcome(set(), dropped)
    if not active:
        check_readiness()
        return StreamOutcome(set(), dropped)

    transferred = 0
    while transferred < len(data):
        chunk = data[transferred : transferred + chunk_size]
        frame = _compact_command(
            {
                "type": "chunk",
                "hash": payload_hash,
                "offset": transferred,
                "data": base64.b64encode(chunk).decode("ascii"),
            }
        )
        transferred += len(chunk)
        if not send_command(frame, transferred):
            check_readiness()
            return StreamOutcome(set(), dropped)
        if not active:
            check_readiness()
            return StreamOutcome(set(), dropped)

    if not active:
        check_readiness()
        return StreamOutcome(set(), dropped)
    verified = send_command(finish, len(data), final=True)
    check_readiness()
    scan_terminal_failures(acknowledged=verified)
    check_readiness()
    return StreamOutcome(verified, dropped)
