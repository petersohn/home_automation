import base64
import hashlib
import json
from collections.abc import Callable

import pytest

from lib import signature_stream
from lib.signature_stream import (
    StreamOutcome,
    StreamReport,
    StreamTarget,
    decode_stream_report,
    send_stream,
)


UINT32_MAX = 0xFFFFFFFF


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0
        self._scheduled: list[tuple[float, Callable[[], None]]] = []

    def monotonic(self) -> float:
        return self.now

    def call_later(self, delay: float, callback: Callable[[], None]) -> None:
        if delay <= 0:
            callback()
            return
        self._scheduled.append((self.now + delay, callback))
        self._scheduled.sort(key=lambda entry: entry[0])

    def sleep(self, duration: float) -> None:
        target = self.now + duration
        while self._scheduled and self._scheduled[0][0] <= target:
            self.now, callback = self._scheduled.pop(0)
            callback()
        self.now = target


class FakeMqttClient:
    def __init__(self, clock: FakeClock) -> None:
        self.clock = clock
        self.subscriptions: list[str] = []
        self.publishes: list[tuple[str, str, bool, int]] = []
        self.states: dict[str, str] = {}
        self.event_counts: dict[str, int] = {}
        self.ready = False
        self.ready_generation = 0
        self.on_publish = None
        self.on_get_event_count = None

    def set_ready(self, ready: bool) -> None:
        if ready and not self.ready:
            self.ready_generation += 1
        self.ready = ready

    def get_readiness_snapshot(self) -> tuple[bool, int]:
        return self.ready, self.ready_generation

    def subscribe(self, topic: str) -> None:
        self.subscriptions.append(topic)

    def wait_ready(self, timeout: float) -> bool:
        assert timeout > 0
        self.set_ready(True)
        return True

    def get_event_count(self, topic: str) -> int:
        if self.on_get_event_count is not None:
            self.on_get_event_count(self, topic)
        return self.event_counts.get(topic, 0)

    def get_state(self, topic: str) -> str | None:
        return self.states.get(topic)

    def publish(
        self, topic: str, payload: str, *, retain: bool, qos: int
    ) -> None:
        if not self.ready:
            raise ConnectionError("MQTT client is disconnected")
        self.publishes.append((topic, payload, retain, qos))
        if self.on_publish is not None:
            self.on_publish(self, topic, payload)

    def report(
        self,
        target: StreamTarget,
        status: str,
        transferred: str | int,
        payload_hash: str,
        *,
        delay: float = 0.0,
    ) -> None:
        self.report_payload(
            target,
            json.dumps([status, str(transferred), payload_hash]),
            delay=delay,
        )

    def report_payload(
        self, target: StreamTarget, payload: str, *, delay: float = 0.0
    ) -> None:
        def deliver() -> None:
            self.states[target.result_topic] = payload
            self.event_counts[target.result_topic] = (
                self.event_counts.get(target.result_topic, 0) + 1
            )

        self.clock.call_later(delay, deliver)


@pytest.fixture
def fake_clock(monkeypatch: pytest.MonkeyPatch) -> FakeClock:
    clock = FakeClock()
    monkeypatch.setattr(signature_stream.time, "monotonic", clock.monotonic)
    monkeypatch.setattr(signature_stream.time, "sleep", clock.sleep)
    return clock


def _targets(*names: str) -> list[StreamTarget]:
    return [StreamTarget(name, f"result/{name}") for name in names]


def _command(payload: str) -> dict[str, object]:
    return json.loads(payload)


def test_progress_report_has_decoded_byte_count_and_hash() -> None:
    report = decode_stream_report(json.dumps(["PROGRESS", "256", "a" * 64]))
    assert report == StreamReport("PROGRESS", 256, "a" * 64)
    assert decode_stream_report(
        json.dumps(["PROGRESS", "4294967295", "a" * 64])
    ) == StreamReport("PROGRESS", UINT32_MAX, "a" * 64)
    assert decode_stream_report(
        json.dumps(["PROGRESS", "4294967296", "a" * 64])
    ) is None


@pytest.mark.parametrize(
    "payload",
    [
        "",
        "not json",
        '"startup"',
        '["PROGRESS","01","' + "a" * 64 + '"]',
        '["PROGRESS","-1","' + "a" * 64 + '"]',
        '["PROGRESS",true,"' + "a" * 64 + '"]',
        '["PROGRESS",1,"' + "a" * 64 + '"]',
        '["PROGRESS","1","' + "A" * 64 + '"]',
        '["PROGRESS","1",""]',
        '["ERROR","0","' + "a" * 64 + '","extra"]',
        '["PROGRESS","0",null]',
    ],
)
def test_decode_stream_report_rejects_unusable_payloads(payload: str) -> None:
    assert decode_stream_report(payload) is None


def test_decode_stream_report_allows_empty_hash_only_for_error() -> None:
    assert decode_stream_report('["ERROR busy","0",""]') == StreamReport(
        "ERROR busy", 0, ""
    )
    assert decode_stream_report('["OK","0",""]') is None


def test_legacy_decoder_does_not_accept_ota_result_objects() -> None:
    payload = json.dumps(
        {
            "status": "STAGED",
            "transferred": 0,
            "hash": "a" * 64,
            "version": 1,
            "reason": "",
        }
    )

    assert decode_stream_report(payload) is None


def test_broadcast_waits_for_every_ack_before_next_chunk(fake_clock: FakeClock) -> None:
    targets = _targets("fast", "middle", "slow")
    client = FakeMqttClient(fake_clock)
    data = bytes(range(256)) * 2
    payload_hash = hashlib.sha256(data).hexdigest()

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        command = _command(payload)
        kind = command["type"]
        if kind == "begin":
            responses = (("fast", 0.0), ("middle", 0.02), ("slow", 0.04))
            for name, delay in responses:
                mqtt_client.report(
                    next(target for target in targets if target.name == name),
                    "PROGRESS",
                    0,
                    payload_hash,
                    delay=delay,
                )
        elif kind == "chunk":
            offset = int(command["offset"])
            expected = offset + len(base64.b64decode(command["data"]))
            previous = offset
            for target in targets:
                if offset:
                    assert mqtt_client.get_state(target.result_topic) == json.dumps(
                        ["PROGRESS", str(previous), payload_hash]
                    )
                delay = {"fast": 0.0, "middle": 0.02, "slow": 0.05}[target.name]
                mqtt_client.report(
                    target, "PROGRESS", expected, payload_hash, delay=delay
                )
            if offset == 0:
                mqtt_client.report(
                    targets[0], "PROGRESS", expected, payload_hash, delay=0.01
                )
        elif kind == "finish":
            assert all(
                mqtt_client.get_state(target.result_topic)
                == json.dumps(["PROGRESS", str(len(data)), payload_hash])
                for target in targets
            )
            for target in targets:
                mqtt_client.report(target, "OK", len(data), payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        targets,
        data,
        b"s" * 64,
        timeout=0.2,
        retry_interval=0.1,
    )

    assert outcome == StreamOutcome({"fast", "middle", "slow"}, {})
    assert client.subscriptions == [target.result_topic for target in targets]
    assert [
        _command(payload)["type"] for _, payload, _, _ in client.publishes
    ] == ["begin", "chunk", "chunk", "finish"]
    assert all(not retain and qos == 1 for _, _, retain, qos in client.publishes)
    assert all(topic == "commands" for topic, _, _, _ in client.publishes)
    assert all(len(payload.encode()) <= 512 for _, payload, _, _ in client.publishes)
    assert [_command(payload).get("offset") for _, payload, _, _ in client.publishes] == [
        None,
        0,
        256,
        None,
    ]


def test_slow_target_drops_and_remaining_devices_finish(fake_clock: FakeClock) -> None:
    targets = _targets("first", "slow", "last")
    client = FakeMqttClient(fake_clock)
    data = b"x" * 300
    payload_hash = hashlib.sha256(data).hexdigest()

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        command = _command(payload)
        for target in targets:
            if target.name == "slow":
                continue
            if command["type"] == "finish":
                mqtt_client.report(target, "OK", len(data), payload_hash)
            elif command["type"] == "begin":
                mqtt_client.report(target, "PROGRESS", 0, payload_hash)
            else:
                end = int(command["offset"]) + len(
                    base64.b64decode(command["data"])
                )
                mqtt_client.report(target, "PROGRESS", end, payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        targets,
        data,
        b"s" * 64,
        timeout=0.06,
        retry_interval=0.02,
        chunk_size=256,
    )

    assert outcome.verified == {"first", "last"}
    assert outcome.dropped == {"slow": "timeout"}
    assert any(_command(payload)["type"] == "finish" for _, payload, _, _ in client.publishes)


def test_all_targets_timing_out_aborts_without_finish(fake_clock: FakeClock) -> None:
    targets = _targets("one", "two", "three")
    client = FakeMqttClient(fake_clock)

    outcome = send_stream(
        client,
        "commands",
        targets,
        b"data",
        b"s" * 64,
        timeout=0.05,
        retry_interval=0.02,
    )

    assert outcome == StreamOutcome(
        set(), {"one": "timeout", "two": "timeout", "three": "timeout"}
    )
    assert all(_command(payload)["type"] == "begin" for _, payload, _, _ in client.publishes)


def test_lost_ack_retries_identical_command_and_duplicate_ack_recovers(
    fake_clock: FakeClock,
) -> None:
    target = _targets("device")[0]
    client = FakeMqttClient(fake_clock)
    payload_hash = hashlib.sha256(b"payload").hexdigest()
    begin_attempts = 0

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        nonlocal begin_attempts
        command = _command(payload)
        if command["type"] == "begin":
            begin_attempts += 1
            if begin_attempts == 2:
                mqtt_client.report(target, "PROGRESS", 0, payload_hash)
        elif command["type"] == "chunk":
            mqtt_client.report(target, "PROGRESS", 7, payload_hash)
        else:
            mqtt_client.report(target, "OK", 7, payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [target],
        b"payload",
        b"s" * 64,
        timeout=0.1,
        retry_interval=0.02,
    )

    begin_payloads = [
        payload
        for _, payload, _, _ in client.publishes
        if _command(payload)["type"] == "begin"
    ]
    assert len(begin_payloads) == 2
    assert begin_payloads[0] == begin_payloads[1]
    assert outcome == StreamOutcome({"device"}, {})


def test_old_duplicate_progress_does_not_extend_target_deadline(
    fake_clock: FakeClock,
) -> None:
    target = _targets("device")[0]
    client = FakeMqttClient(fake_clock)
    payload_hash = hashlib.sha256(b"payload").hexdigest()
    chunk_start: list[float] = []

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        command = _command(payload)
        if command["type"] == "begin":
            mqtt_client.report(target, "PROGRESS", 0, payload_hash)
        elif command["type"] == "chunk":
            if not chunk_start:
                chunk_start.append(fake_clock.now)
            mqtt_client.report(target, "PROGRESS", 0, payload_hash, delay=0.01)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [target],
        b"payload",
        b"s" * 64,
        timeout=0.08,
        retry_interval=0.03,
    )

    assert outcome == StreamOutcome(set(), {"device": "timeout"})
    assert fake_clock.now - chunk_start[0] == pytest.approx(0.08)
    assert all(
        _command(payload)["type"] != "finish" for _, payload, _, _ in client.publishes
    )


def test_pre_send_and_mismatched_reports_are_ignored(fake_clock: FakeClock) -> None:
    target, other = _targets("target", "other")
    client = FakeMqttClient(fake_clock)
    data = b"z"
    payload_hash = hashlib.sha256(data).hexdigest()
    other_hash = "f" * 64
    client.states[target.result_topic] = json.dumps(["PROGRESS", "0", payload_hash])
    client.event_counts[target.result_topic] = 1
    client.states[other.result_topic] = json.dumps(["PROGRESS", "0", payload_hash])
    client.event_counts[other.result_topic] = 1
    attempts = 0

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        nonlocal attempts
        command = _command(payload)
        if command["type"] == "begin":
            attempts += 1
            if attempts == 1:
                mqtt_client.report(target, "PROGRESS", 0, other_hash)
                mqtt_client.report_payload(
                    target, '["PROGRESS",false,"' + payload_hash + '"]'
                )
                mqtt_client.report_payload(target, '["PROGRESS","0"]')
                mqtt_client.report_payload(target, "not-json")
                mqtt_client.report(target, "PROGRESS", 1, payload_hash)
                mqtt_client.report(target, "ERROR busy", 0, "")
            else:
                mqtt_client.report(target, "PROGRESS", 0, payload_hash)
                mqtt_client.report(other, "PROGRESS", 0, payload_hash)
        elif command["type"] == "chunk":
            mqtt_client.report(target, "PROGRESS", 1, payload_hash)
            mqtt_client.report(other, "PROGRESS", 1, payload_hash)
        elif command["type"] == "finish":
            mqtt_client.report(target, "OK", 1, payload_hash)
            mqtt_client.report(other, "OK", 1, payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [target, other],
        data,
        b"s" * 64,
        timeout=0.1,
        retry_interval=0.02,
    )

    assert outcome == StreamOutcome({"target", "other"}, {})
    assert attempts == 2


@pytest.mark.parametrize("failure_status", ["FAIL invalid signature", "ERROR busy"])
def test_matching_terminal_failure_drops_target_without_rejoining(
    fake_clock: FakeClock, failure_status: str
) -> None:
    failed, healthy = _targets("failed", "healthy")
    client = FakeMqttClient(fake_clock)
    payload_hash = hashlib.sha256(b"data").hexdigest()

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        command = _command(payload)
        if command["type"] == "begin":
            mqtt_client.report(failed, failure_status, 0, payload_hash)
            mqtt_client.report(healthy, "PROGRESS", 0, payload_hash)
        elif command["type"] == "chunk":
            mqtt_client.report(failed, "PROGRESS", 4, payload_hash)
            mqtt_client.report(healthy, "PROGRESS", 4, payload_hash)
        else:
            mqtt_client.report(failed, "OK", 4, payload_hash)
            mqtt_client.report(healthy, "OK", 4, payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [failed, healthy],
        b"data",
        b"s" * 64,
        timeout=0.1,
        retry_interval=0.05,
    )

    assert outcome == StreamOutcome(
        {"healthy"}, {"failed": failure_status}
    )
    assert fake_clock.now == 0
    assert [_command(payload)["type"] for _, payload, _, _ in client.publishes] == [
        "begin",
        "chunk",
        "finish",
    ]


def test_all_targets_timing_out_midstream_returns_no_success(
    fake_clock: FakeClock,
) -> None:
    targets = _targets("one", "two")
    client = FakeMqttClient(fake_clock)
    payload_hash = hashlib.sha256(b"data").hexdigest()

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        if _command(payload)["type"] == "begin":
            for target in targets:
                mqtt_client.report(target, "PROGRESS", 0, payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        targets,
        b"data",
        b"s" * 64,
        timeout=0.05,
        retry_interval=0.02,
    )

    assert outcome == StreamOutcome(set(), {"one": "timeout", "two": "timeout"})
    assert all(_command(payload)["type"] != "finish" for _, payload, _, _ in client.publishes)


def test_connection_error_propagates_without_resume() -> None:
    client = FakeMqttClient(FakeClock())

    def fail_publish(*_args: object, **_kwargs: object) -> None:
        raise ConnectionError("offline")

    client.publish = fail_publish
    with pytest.raises(ConnectionError, match="offline"):
        send_stream(client, "commands", _targets("device"), b"data", b"s" * 64)


def test_silent_reconnect_between_polls_aborts_stream(fake_clock: FakeClock) -> None:
    target = _targets("device")[0]
    client = FakeMqttClient(fake_clock)
    payload_hash = hashlib.sha256(b"data").hexdigest()

    def reconnect() -> None:
        client.set_ready(False)
        client.set_ready(True)

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        assert _command(payload)["type"] == "begin"
        fake_clock.call_later(0.005, reconnect)
        mqtt_client.report(target, "PROGRESS", 0, payload_hash, delay=0.01)

    client.on_publish = on_publish

    with pytest.raises(ConnectionError, match="readiness|connection"):
        send_stream(
            client,
            "commands",
            [target],
            b"data",
            b"s" * 64,
            timeout=0.1,
            retry_interval=0.02,
        )

    assert [_command(payload)["type"] for _, payload, _, _ in client.publishes] == [
        "begin"
    ]


def test_disconnect_during_finish_aborts_instead_of_timing_out(
    fake_clock: FakeClock,
) -> None:
    target = _targets("device")[0]
    client = FakeMqttClient(fake_clock)
    payload_hash = hashlib.sha256(b"").hexdigest()

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        if _command(payload)["type"] == "begin":
            mqtt_client.report(target, "PROGRESS", 0, payload_hash)
        else:
            mqtt_client.set_ready(False)

    client.on_publish = on_publish

    with pytest.raises(ConnectionError, match="readiness|connection"):
        send_stream(
            client,
            "commands",
            [target],
            b"",
            b"s" * 64,
            timeout=0.05,
            retry_interval=0.02,
        )

    assert [_command(payload)["type"] for _, payload, _, _ in client.publishes] == [
        "begin",
        "finish",
    ]


@pytest.mark.parametrize(
    ("slow_ack_delay", "timeout", "verified", "dropped", "finish_sent"),
    [
        (0.06, 0.12, {"slow"}, {"fast": "ERROR interrupted"}, True),
        (None, 0.07, set(), {"fast": "ERROR interrupted", "slow": "timeout"}, False),
    ],
)
def test_acknowledged_target_failure_is_monitored_while_straggler_pending(
    fake_clock: FakeClock,
    slow_ack_delay: float | None,
    timeout: float,
    verified: set[str],
    dropped: dict[str, str],
    finish_sent: bool,
) -> None:
    fast, slow = _targets("fast", "slow")
    targets = [fast, slow]
    client = FakeMqttClient(fake_clock)
    data = b"data"
    payload_hash = hashlib.sha256(data).hexdigest()
    chunk_started = False

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        nonlocal chunk_started
        command = _command(payload)
        if command["type"] == "begin":
            for target in targets:
                mqtt_client.report(target, "PROGRESS", 0, payload_hash)
        elif command["type"] == "chunk" and not chunk_started:
            chunk_started = True
            mqtt_client.report(fast, "PROGRESS", len(data), payload_hash)
            mqtt_client.report(
                fast, "ERROR interrupted", len(data), payload_hash, delay=0.02
            )
            if slow_ack_delay is not None:
                mqtt_client.report(
                    slow,
                    "PROGRESS",
                    len(data),
                    payload_hash,
                    delay=slow_ack_delay,
                )
        elif command["type"] == "finish":
            mqtt_client.report(slow, "OK", len(data), payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        targets,
        data,
        b"s" * 64,
        timeout=timeout,
        retry_interval=0.02,
    )

    assert outcome == StreamOutcome(verified, dropped)
    assert any(
        _command(payload)["type"] == "finish"
        for _, payload, _, _ in client.publishes
    ) is finish_sent


def test_terminal_failure_before_next_command_baseline_is_not_hidden(
    fake_clock: FakeClock,
) -> None:
    target = _targets("device")[0]
    client = FakeMqttClient(fake_clock)
    payload_hash = hashlib.sha256(b"data").hexdigest()
    event_count_reads = 0

    def inject_failure_before_baseline(
        mqtt_client: FakeMqttClient, topic: str
    ) -> None:
        nonlocal event_count_reads
        event_count_reads += 1
        if topic == target.result_topic and event_count_reads == 4:
            mqtt_client.report(target, "ERROR interrupted", 0, payload_hash)

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        kind = _command(payload)["type"]
        if kind == "begin":
            mqtt_client.report(target, "PROGRESS", 0, payload_hash)
        elif kind == "chunk":
            mqtt_client.report(target, "PROGRESS", 4, payload_hash)
        else:
            mqtt_client.report(target, "OK", 4, payload_hash)

    client.on_get_event_count = inject_failure_before_baseline
    client.on_publish = on_publish

    outcome = send_stream(
        client,
        "commands",
        [target],
        b"data",
        b"s" * 64,
        timeout=0.1,
        retry_interval=0.05,
    )

    assert outcome == StreamOutcome(set(), {"device": "ERROR interrupted"})
    assert [_command(payload)["type"] for _, payload, _, _ in client.publishes] == [
        "begin"
    ]


def test_finish_requires_ok_not_matching_total_progress(fake_clock: FakeClock) -> None:
    target = _targets("device")[0]
    client = FakeMqttClient(fake_clock)
    data = b"payload"
    payload_hash = hashlib.sha256(data).hexdigest()

    def on_publish(
        mqtt_client: FakeMqttClient, _topic: str, payload: str
    ) -> None:
        command = _command(payload)
        if command["type"] == "begin":
            mqtt_client.report(target, "PROGRESS", 0, payload_hash)
        elif command["type"] == "chunk":
            mqtt_client.report(target, "PROGRESS", len(data), payload_hash)
        else:
            mqtt_client.report(target, "PROGRESS", len(data), payload_hash)

    client.on_publish = on_publish
    outcome = send_stream(
        client,
        "commands",
        [target],
        data,
        b"s" * 64,
        timeout=0.05,
        retry_interval=0.02,
    )

    assert outcome == StreamOutcome(set(), {"device": "timeout"})
    assert sum(_command(payload)["type"] == "finish" for _, payload, _, _ in client.publishes) == 1


def test_input_validation() -> None:
    client = FakeMqttClient(FakeClock())
    valid_targets = _targets("device")
    valid_args = (client, "commands", valid_targets, b"", b"s" * 64)

    with pytest.raises(ValueError, match="target"):
        send_stream(client, "commands", [], b"", b"s" * 64)
    with pytest.raises(ValueError, match="name"):
        send_stream(client, "commands", [StreamTarget("", "result")], b"", b"s" * 64)
    with pytest.raises(ValueError, match="topic"):
        send_stream(client, "commands", [StreamTarget("device", "")], b"", b"s" * 64)
    with pytest.raises(ValueError, match="unique"):
        send_stream(client, "commands", _targets("same", "same"), b"", b"s" * 64)
    with pytest.raises(ValueError, match="unique"):
        send_stream(
            client,
            "commands",
            [StreamTarget("one", "same"), StreamTarget("two", "same")],
            b"",
            b"s" * 64,
        )
    with pytest.raises(ValueError, match="signature"):
        send_stream(client, "commands", valid_targets, b"", b"short")

    class OversizedBytes(bytes):
        def __len__(self) -> int:
            return UINT32_MAX + 1

    with pytest.raises(ValueError, match="data"):
        send_stream(client, "commands", valid_targets, OversizedBytes(), b"s" * 64)

    for chunk_size in (0, 257, True):
        with pytest.raises(ValueError, match="chunk"):
            send_stream(*valid_args, chunk_size=chunk_size)
    for timeout, retry_interval in (
        (0, 0.1),
        (30, 1),
        (1, 0),
        (1, 1),
        (0.5, 0.6),
    ):
        with pytest.raises(ValueError, match="timeout|retry"):
            send_stream(
                *valid_args, timeout=timeout, retry_interval=retry_interval
            )
