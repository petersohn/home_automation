import json
import subprocess
import sys
import threading
from collections.abc import Callable
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest

from lib import signature_stream
from lib.mqtt_client import MqttClient
from lib.signature_stream import verify_broker_restart_invalidation


ROOT = Path(__file__).resolve().parents[3]
SIGNATURE_TEST_PATH = ROOT / "e2e/tests/signature_verify/test_signature_verify.py"
PAYLOAD_HASH = "a" * 64


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0
        self.events: list[tuple[float, Any]] = []

    def monotonic(self) -> float:
        return self.now

    def call_later(self, delay: float, callback: Any) -> None:
        self.events.append((self.now + delay, callback))
        self.events.sort(key=lambda event: event[0])

    def call_at(self, when: float, callback: Any) -> None:
        self.events.append((when, callback))
        self.events.sort(key=lambda event: event[0])

    def sleep(self, duration: float) -> None:
        target = self.now + duration
        while self.events and self.events[0][0] <= target:
            self.now, callback = self.events.pop(0)
            callback()
        self.now = target


class FakeTransport:
    def __init__(self, owner: "FakeClient") -> None:
        self.owner = owner

    def publish(
        self, topic: str, payload: str, *, retain: bool, qos: int
    ) -> SimpleNamespace:
        self.owner.handle_publish(topic, payload, retain=retain, qos=qos)
        return SimpleNamespace(rc=0)


class FakeClient(MqttClient):
    def __init__(
        self,
        clock: FakeClock,
        *,
        partial_delay: float = 0.0,
        probe_reports: list[tuple[str, int, float]] | None = None,
        probe_hash: str = PAYLOAD_HASH,
        malformed_probe: bool = False,
        absolute_probe_response_at: float | None = None,
        after_probe_baseline: Callable[[MqttClient], None] | None = None,
    ) -> None:
        self._state = {}
        self._event_count = {}
        self._lock = threading.Lock()
        self._subscribed_topics = []
        self._connected = True
        self._ready = True
        self._ready_generation = 1
        self._pending_subscriptions = set()
        self._subscription_failed = False
        self._client = FakeTransport(self)
        self.clock = clock
        self.partial_delay = partial_delay
        self.probe_reports = probe_reports or []
        self.probe_hash = probe_hash
        self.malformed_probe = malformed_probe
        self.absolute_probe_response_at = absolute_probe_response_at
        self.absolute_probe_response_scheduled = False
        self.after_probe_baseline = after_probe_baseline
        self.probe_available_at = 0.0
        self.publishes: list[tuple[str, str, bool, int]] = []

    def get_event_count(self, topic: str) -> int:
        event_count = super().get_event_count(topic)
        if (
            topic == "result"
            and self._ready_generation > 1
            and [publish[1] for publish in self.publishes] == ["partial"]
            and self.after_probe_baseline is not None
        ):
            callback = self.after_probe_baseline
            self.after_probe_baseline = None
            callback(self)
        return event_count

    def handle_publish(
        self, topic: str, payload: str, *, retain: bool, qos: int
    ) -> None:
        self.publishes.append((topic, payload, retain, qos))
        if payload == "partial":
            self.clock.call_later(
                self.partial_delay,
                lambda: self.report("result", "PROGRESS", 128, PAYLOAD_HASH),
            )
        elif payload == "probe":
            if self.clock.monotonic() < self.probe_available_at:
                return
            if (
                self.absolute_probe_response_at is not None
                and not self.absolute_probe_response_scheduled
            ):
                self.absolute_probe_response_scheduled = True
                self.clock.call_at(
                    self.absolute_probe_response_at,
                    lambda: self.report(
                        "result", "ERROR no active transfer", 0, self.probe_hash
                    ),
                )
                return
            if self.malformed_probe:
                self.clock.call_later(
                    0.1, lambda: self.report_payload("result", "not json")
                )
                return
            for status, transferred, delay in self.probe_reports:
                self.clock.call_later(
                    delay,
                    lambda status=status, transferred=transferred: self.report(
                        "result", status, transferred
                    ),
                )

    def report(
        self,
        topic: str,
        status: str,
        transferred: int,
        payload_hash: str | None = None,
    ) -> None:
        self.report_payload(
            topic,
            json.dumps([status, str(transferred), payload_hash or self.probe_hash]),
        )

    def report_payload(self, topic: str, payload: str) -> None:
        self._on_message(
            None,
            None,
            SimpleNamespace(topic=topic, payload=payload.encode(), retain=False),
        )


class FakeBroker:
    def __init__(
        self,
        clock: FakeClock,
        client: FakeClient,
        *,
        stop_delay: float = 0.0,
        start_delay: float = 0.0,
        ready_delay: float = 0.0,
        device_subscription_delay: float = 0.0,
        retained_message: Any | None = None,
    ) -> None:
        self.clock = clock
        self.client = client
        self.stop_delay = stop_delay
        self.start_delay = start_delay
        self.ready_delay = ready_delay
        self.device_subscription_delay = device_subscription_delay
        self.retained_message = retained_message
        self.start_calls = 0

    def stop(self) -> None:
        self.client._ready = False
        self.clock.sleep(self.stop_delay)

    def start(self) -> None:
        self.start_calls += 1
        self.clock.sleep(self.start_delay)
        self.clock.call_later(self.ready_delay, self._set_ready)
        self.client.probe_available_at = (
            self.clock.monotonic() + self.ready_delay + self.device_subscription_delay
        )
        if self.retained_message is not None:
            self.clock.call_later(self.ready_delay, self.retained_message)

    def _set_ready(self) -> None:
        if not self.client._ready:
            self.client._ready_generation += 1
        self.client._ready = True


def _run_proof(
    clock: FakeClock,
    client: FakeClient,
    broker: FakeBroker,
    *,
    timeout: float = 25.0,
    restart_delay: float = 3.0,
) -> None:
    verify_broker_restart_invalidation(
        client,
        stop_broker=broker.stop,
        start_broker=broker.start,
        command_topic="command",
        result_topic="result",
        partial_command="partial",
        probe_command="probe",
        payload_hash=PAYLOAD_HASH,
        partial_count=128,
        timeout=timeout,
        restart_delay=restart_delay,
        poll_interval=0.05,
        monotonic=clock.monotonic,
        sleep=clock.sleep,
    )


def _harness(
    *,
    partial_delay: float = 0.0,
    stop_delay: float = 0.0,
    start_delay: float = 0.0,
    ready_delay: float = 0.0,
    device_subscription_delay: float = 0.0,
    retained_message: Callable[[MqttClient], None] | None = None,
    probe_reports: list[tuple[str, int, float]] | None = None,
    probe_hash: str = PAYLOAD_HASH,
    malformed_probe: bool = False,
    absolute_probe_response_at: float | None = None,
    after_probe_baseline: Callable[[MqttClient], None] | None = None,
) -> tuple[FakeClock, FakeClient, FakeBroker]:
    clock = FakeClock()
    client = FakeClient(
        clock,
        partial_delay=partial_delay,
        probe_reports=probe_reports,
        probe_hash=probe_hash,
        malformed_probe=malformed_probe,
        absolute_probe_response_at=absolute_probe_response_at,
        after_probe_baseline=after_probe_baseline,
    )
    broker = FakeBroker(
        clock,
        client,
        stop_delay=stop_delay,
        start_delay=start_delay,
        ready_delay=ready_delay,
        device_subscription_delay=device_subscription_delay,
        retained_message=(
            lambda: retained_message(client) if retained_message is not None else None
        )
        if retained_message is not None
        else None,
    )
    return clock, client, broker


def test_restart_invalidation_uses_one_monotonic_deadline_for_every_stage() -> None:
    clock, client, broker = _harness(
        partial_delay=4,
        stop_delay=5,
        start_delay=4,
        ready_delay=4,
        device_subscription_delay=3,
        probe_reports=[
            ("ERROR disconnected", 128, 0.2),
            ("ERROR no active transfer", 0, 1.0),
        ],
    )

    _run_proof(clock, client, broker)

    assert 24.0 <= clock.now < 24.2
    assert clock.now < signature_stream.RESTART_INVALIDATION_TIMEOUT < 30
    assert [publish[1] for publish in client.publishes] == [
        "partial",
        "probe",
        "probe",
        "probe",
        "probe",
    ]
    assert all(
        not retained and qos == 1
        for _, _, retained, qos in client.publishes
    )
    assert broker.start_calls == 1


@pytest.mark.parametrize(
    "partial_delay,stop_delay",
    [(15.0, 8.0), (5.0, 18.0)],
    ids=["slow-ack", "slow-broker-stop"],
)
def test_partial_ack_and_broker_operations_share_budget(
    partial_delay: float, stop_delay: float
) -> None:
    clock, client, broker = _harness(
        partial_delay=partial_delay,
        stop_delay=stop_delay,
        probe_reports=[("ERROR no active transfer", 0, 0)],
    )

    with pytest.raises(TimeoutError, match="deadline expired"):
        _run_proof(clock, client, broker)

    assert clock.now == pytest.approx(25.0)
    assert broker.start_calls == 1
    assert [publish[1] for publish in client.publishes] == ["partial"]


@pytest.mark.parametrize(
    "probe_reports",
    [
        [],
        [("ERROR disconnected", 128, 0.1)],
        [("PROGRESS", 128, 0.1)],
        [("ERROR no active transfer", 1, 0.1)],
    ],
    ids=[
        "no-response",
        "disconnect-only",
        "progress-is-not-invalidation",
        "wrong-count",
    ],
)
def test_probe_requires_fresh_no_active_response(
    probe_reports: list[tuple[str, int, float]],
) -> None:
    clock, client, broker = _harness(probe_reports=probe_reports)

    with pytest.raises(TimeoutError, match="no-active-transfer probe response"):
        _run_proof(clock, client, broker)

    assert clock.now == pytest.approx(25.0)
    assert client.publishes[0][1] == "partial"
    assert all(publish[1] == "probe" for publish in client.publishes[1:])
    assert len(client.publishes) > 2


@pytest.mark.parametrize(
    "late_response", [25.0, 25.1], ids=["at-deadline", "after-deadline"]
)
def test_no_active_response_at_or_after_deadline_fails(late_response: float) -> None:
    clock, client, broker = _harness(
        partial_delay=3.0,
        absolute_probe_response_at=late_response,
    )

    with pytest.raises(TimeoutError, match="deadline expired"):
        _run_proof(clock, client, broker, restart_delay=0)

    assert clock.now == pytest.approx(25.0)


def test_inactivity_timeout_cannot_supply_a_30_second_restart_proof() -> None:
    clock, client, broker = _harness(
        probe_reports=[("ERROR no active transfer", 0, 30.0)]
    )

    with pytest.raises(TimeoutError, match="deadline expired"):
        _run_proof(clock, client, broker)

    assert clock.now == pytest.approx(25.0)


def test_restart_proof_rejects_deadlines_longer_than_25_seconds() -> None:
    clock, client, broker = _harness()

    with pytest.raises(ValueError, match="at most 25 seconds"):
        _run_proof(clock, client, broker, timeout=30)

    assert client.publishes == []
    assert broker.start_calls == 0


def test_restart_proof_ignores_wall_clock_jumps(monkeypatch: pytest.MonkeyPatch) -> None:
    clock, client, broker = _harness(
        probe_reports=[("ERROR no active transfer", 0, 0.1)]
    )

    def wall_clock_must_not_be_used() -> float:
        raise AssertionError("wall clock used during restart proof")

    monkeypatch.setattr(
        signature_stream,
        "time",
        SimpleNamespace(time=wall_clock_must_not_be_used),
    )
    _run_proof(clock, client, broker, restart_delay=0)


def test_retained_reconnect_messages_do_not_block_fresh_probe() -> None:
    def deliver_retained_messages(client: MqttClient) -> None:
        client._on_message(
            None,
            None,
            SimpleNamespace(
                topic="status", payload=b'{"restarted":false}', retain=True
            ),
        )
        client._on_message(
            None,
            None,
            SimpleNamespace(
                topic="result",
                payload=json.dumps(
                    ["ERROR no active transfer", "0", PAYLOAD_HASH]
                ).encode(),
                retain=True,
            ),
        )

    clock, client, broker = _harness(
        ready_delay=0.5,
        retained_message=deliver_retained_messages,
        probe_reports=[("ERROR no active transfer", 0, 0.1)],
    )

    _run_proof(clock, client, broker, restart_delay=0)

    assert clock.now < 25
    assert client.get_event_count("status") == 0
    assert client.get_event_count("result") == 2
    assert [publish[1] for publish in client.publishes] == ["partial", "probe"]
    assert all(not retained and qos == 1 for _, _, retained, qos in client.publishes)


def test_retained_probe_response_cannot_prove_invalidation() -> None:
    def deliver_retained_response(client: MqttClient) -> None:
        client._on_message(
            None,
            None,
            SimpleNamespace(
                topic="result",
                payload=json.dumps(
                    ["ERROR no active transfer", "0", PAYLOAD_HASH]
                ).encode(),
                retain=True,
            ),
        )

    clock, client, broker = _harness(retained_message=deliver_retained_response)

    with pytest.raises(TimeoutError, match="no-active-transfer probe response"):
        _run_proof(clock, broker.client, broker, restart_delay=0)

    assert clock.now == pytest.approx(25.0)
    assert client.get_event_count("result") == 1


def test_first_probe_can_be_lost_before_device_resubscribes() -> None:
    clock, client, broker = _harness(
        ready_delay=0.5,
        device_subscription_delay=0.5,
        probe_reports=[("ERROR no active transfer", 0, 0.1)],
    )

    _run_proof(clock, client, broker, restart_delay=0)

    assert [publish[1] for publish in client.publishes] == [
        "partial",
        "probe",
        "probe",
    ]
    assert all(not retained and qos == 1 for _, _, retained, qos in client.publishes)
    assert 1.6 <= clock.now < 1.75


def test_preprobe_result_cannot_prove_invalidation() -> None:
    clock, client, broker = _harness(
        retained_message=lambda actual_client: actual_client.report_payload(
            "result", json.dumps(["ERROR no active transfer", "0", PAYLOAD_HASH])
        )
    )

    with pytest.raises(TimeoutError, match="no-active-transfer probe response"):
        _run_proof(clock, client, broker, restart_delay=0)

    assert clock.now == pytest.approx(25.0)


def test_result_injected_after_probe_baseline_still_requires_published_probe() -> None:
    clock, client, broker = _harness(
        after_probe_baseline=lambda actual_client: actual_client.report(
            "result", "ERROR no active transfer", 0
        )
    )

    _run_proof(clock, client, broker, restart_delay=0)

    assert [publish[1] for publish in client.publishes] == ["partial", "probe"]
    assert client.get_event_count("result") == 2
    assert clock.now < 25


def test_malformed_probe_response_does_not_prove_invalidation() -> None:
    clock, client, broker = _harness(malformed_probe=True)

    with pytest.raises(TimeoutError, match="no-active-transfer probe response"):
        _run_proof(clock, client, broker, restart_delay=0)

    assert clock.now == pytest.approx(25.0)


def test_wrong_hash_probe_response_does_not_prove_invalidation() -> None:
    clock, client, broker = _harness(
        probe_reports=[("ERROR no active transfer", 0, 0.1)],
        probe_hash="b" * 64,
    )

    with pytest.raises(TimeoutError, match="no-active-transfer probe response"):
        _run_proof(clock, client, broker, restart_delay=0)

    assert clock.now == pytest.approx(25.0)


def test_hardware_parser_regressions_are_collected() -> None:
    script = f"""
import sys
import types
import pytest

sys.path.insert(0, {str(ROOT / 'e2e')!r})
conftest = types.ModuleType('conftest')
conftest.assert_booted = lambda *args, **kwargs: None
sys.modules['conftest'] = conftest
device = types.ModuleType('lib.device')
device.Device = object
sys.modules['lib.device'] = device
raise SystemExit(pytest.main([
    '--collect-only', '-q', '--noconftest',
    {str(SIGNATURE_TEST_PATH)!r},
]))
"""
    result = subprocess.run(
        [sys.executable, "-c", script],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    assert "test_invalid_base64_error[noncanonical-pad-bits]" in result.stdout
    assert "test_duplicate_json_fields_use_last_value" in result.stdout
