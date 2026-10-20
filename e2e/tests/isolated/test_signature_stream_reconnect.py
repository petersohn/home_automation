import json
import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest

from lib import signature_stream
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

    def sleep(self, duration: float) -> None:
        target = self.now + duration
        while self.events and self.events[0][0] <= target:
            self.now, callback = self.events.pop(0)
            callback()
        self.now = target


class FakeClient:
    def __init__(
        self,
        clock: FakeClock,
        *,
        partial_delay: float = 0.0,
        probe_reports: list[tuple[str, int, float]] | None = None,
    ) -> None:
        self.clock = clock
        self.partial_delay = partial_delay
        self.probe_reports = probe_reports or []
        self.ready = True
        self.generation = 1
        self.states: dict[str, str] = {}
        self.events: dict[str, int] = {}
        self.publishes: list[tuple[str, str, bool, int]] = []

    def get_readiness_snapshot(self) -> tuple[bool, int]:
        return self.ready, self.generation

    def get_event_count(self, topic: str) -> int:
        return self.events.get(topic, 0)

    def get_state(self, topic: str) -> str | None:
        return self.states.get(topic)

    def get_state_json(self, topic: str) -> dict[str, Any] | None:
        payload = self.get_state(topic)
        if payload is None:
            return None
        value = json.loads(payload)
        return value if isinstance(value, dict) else None

    def publish(self, topic: str, payload: str, *, retain: bool, qos: int) -> None:
        self.publishes.append((topic, payload, retain, qos))
        if payload == "partial":
            self.clock.call_later(
                self.partial_delay,
                lambda: self.report("result", "PROGRESS", 128),
            )
        elif payload == "probe":
            for status, transferred, delay in self.probe_reports:
                self.clock.call_later(
                    delay,
                    lambda status=status, transferred=transferred: self.report(
                        "result", status, transferred
                    ),
                )

    def report(self, topic: str, status: str, transferred: int) -> None:
        self.states[topic] = json.dumps([status, str(transferred), PAYLOAD_HASH])
        self.events[topic] = self.events.get(topic, 0) + 1

    def report_status(self) -> None:
        self.states["status"] = json.dumps({"restarted": False})
        self.events["status"] = self.events.get("status", 0) + 1


class FakeBroker:
    def __init__(
        self,
        clock: FakeClock,
        client: FakeClient,
        *,
        stop_delay: float = 0.0,
        start_delay: float = 0.0,
        ready_delay: float = 0.0,
        status_delay: float = 0.0,
    ) -> None:
        self.clock = clock
        self.client = client
        self.stop_delay = stop_delay
        self.start_delay = start_delay
        self.ready_delay = ready_delay
        self.status_delay = status_delay
        self.start_calls = 0

    def stop(self) -> None:
        self.client.ready = False
        self.clock.sleep(self.stop_delay)

    def start(self) -> None:
        self.start_calls += 1
        self.clock.sleep(self.start_delay)
        self.clock.call_later(self.ready_delay, self._set_ready)
        self.clock.call_later(
            self.ready_delay + self.status_delay, self.client.report_status
        )

    def _set_ready(self) -> None:
        if not self.client.ready:
            self.client.generation += 1
        self.client.ready = True


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
        status_topic="status",
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
    status_delay: float = 0.0,
    probe_reports: list[tuple[str, int, float]] | None = None,
) -> tuple[FakeClock, FakeClient, FakeBroker]:
    clock = FakeClock()
    client = FakeClient(
        clock, partial_delay=partial_delay, probe_reports=probe_reports
    )
    broker = FakeBroker(
        clock,
        client,
        stop_delay=stop_delay,
        start_delay=start_delay,
        ready_delay=ready_delay,
        status_delay=status_delay,
    )
    return clock, client, broker


def test_restart_invalidation_uses_one_monotonic_deadline_for_every_stage() -> None:
    clock, client, broker = _harness(
        partial_delay=4,
        stop_delay=5,
        start_delay=4,
        ready_delay=4,
        status_delay=3,
        probe_reports=[
            ("ERROR disconnected", 128, 0.2),
            ("ERROR no active transfer", 0, 1.0),
        ],
    )

    _run_proof(clock, client, broker)

    assert 24.0 <= clock.now < 24.2
    assert clock.now < signature_stream.RESTART_INVALIDATION_TIMEOUT < 30
    assert [publish[1] for publish in client.publishes] == ["partial", "probe"]
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
        [("ERROR disconnected", 128, 0.1)],
        [("PROGRESS", 128, 0.1)],
    ],
    ids=["disconnect-only", "progress-is-not-invalidation"],
)
def test_probe_requires_fresh_no_active_response(
    probe_reports: list[tuple[str, int, float]],
) -> None:
    clock, client, broker = _harness(probe_reports=probe_reports)

    with pytest.raises(TimeoutError, match="no-active-transfer probe response"):
        _run_proof(clock, client, broker)

    assert clock.now == pytest.approx(25.0)
    assert [publish[1] for publish in client.publishes] == ["partial", "probe"]


@pytest.mark.parametrize(
    "late_response", [24.9, 25.0], ids=["at-deadline", "after-deadline"]
)
def test_no_active_response_at_or_after_deadline_fails(late_response: float) -> None:
    clock, client, broker = _harness(
        probe_reports=[("ERROR no active transfer", 0, late_response)]
    )

    with pytest.raises(TimeoutError, match="deadline expired"):
        _run_proof(clock, client, broker, restart_delay=0)


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
