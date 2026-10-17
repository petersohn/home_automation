"""paho-mqtt wrapper with background state tracking and wait_for_state."""

import json
import threading
import time
from typing import Any

import paho.mqtt.client as mqtt

from .wait import wait_for


class MqttClient:
    def __init__(self, host: str, port: int, username: str, password: str):
        self._host = host
        self._port = port
        self._username = username
        self._password = password
        self._state: dict[str, str] = {}
        self._event_count: dict[str, int] = {}
        self._lock = threading.Lock()
        self._subscribed_topics: list[str] = []
        self._connected = False
        self._ready = False
        self._ready_generation = 0
        self._pending_subscriptions: set[int] = set()
        self._subscription_failed = False
        self._client = mqtt.Client()
        self._client.username_pw_set(username, password)
        self._client.on_message = self._on_message
        # paho does not restore subscriptions after a reconnect (e.g. broker
        # restart); resubscribe on every (re)connect.
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_subscribe = self._on_subscribe
        self._client.connect(host, port)
        loop_rc = self._client.loop_start()
        if loop_rc != mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"Failed to start MQTT network loop: rc={loop_rc}")

    def _subscribe_locked(self, topic: str) -> None:
        rc, mid = self._client.subscribe(topic)
        if rc == mqtt.MQTT_ERR_SUCCESS:
            self._pending_subscriptions.add(mid)
        else:
            self._subscription_failed = True

    def _update_ready_locked(self) -> None:
        ready = (
            self._connected
            and not self._pending_subscriptions
            and not self._subscription_failed
        )
        if ready and not self._ready:
            self._ready_generation += 1
        self._ready = ready

    def _on_connect(
        self, client: mqtt.Client, userdata: object, flags: dict, rc: int, *args: object
    ) -> None:
        with self._lock:
            reason_code = getattr(rc, "value", rc)
            try:
                self._connected = int(reason_code) == 0
            except (TypeError, ValueError):
                self._connected = False
            self._ready = False
            self._pending_subscriptions.clear()
            self._subscription_failed = False
            if self._connected:
                for topic in self._subscribed_topics:
                    self._subscribe_locked(topic)
            self._update_ready_locked()

    def _on_disconnect(
        self, client: mqtt.Client, userdata: object, *args: object
    ) -> None:
        with self._lock:
            self._connected = False
            self._ready = False
            self._pending_subscriptions.clear()

    @staticmethod
    def _subscription_succeeded(reason_codes: tuple[object, ...]) -> bool:
        if not reason_codes:
            return True
        codes = reason_codes[0]
        if not isinstance(codes, (list, tuple)):
            codes = (codes,)
        for code in codes:
            is_failure = getattr(code, "is_failure", False)
            if callable(is_failure):
                is_failure = is_failure()
            if is_failure:
                return False
            value = getattr(code, "value", code)
            try:
                if int(value) >= 128:
                    return False
            except (TypeError, ValueError):
                pass
        return True

    def _on_subscribe(
        self, client: mqtt.Client, userdata: object, mid: int, *args: object
    ) -> None:
        with self._lock:
            if mid not in self._pending_subscriptions:
                return
            self._pending_subscriptions.remove(mid)
            if not self._subscription_succeeded(args):
                self._subscription_failed = True
            self._update_ready_locked()

    def _on_message(
        self, client: mqtt.Client, userdata: object, msg: mqtt.MQTTMessage
    ) -> None:
        # MQTT spec: a PUBLISH from the broker has the RETAIN flag set only
        # for stale copies delivered on (re)subscribe; live forwards have
        # retain=0 even for retained topics. Dropping retained deliveries
        # keeps the state map fresh (mosquitto persistence survives broker
        # restarts, so retained snapshots are otherwise stale).
        if msg.retain:
            return
        payload = msg.payload.decode("utf-8") if msg.payload else ""
        with self._lock:
            self._state[msg.topic] = payload
            self._event_count[msg.topic] = self._event_count.get(msg.topic, 0) + 1

    def subscribe(self, topic: str) -> None:
        with self._lock:
            if topic in self._subscribed_topics:
                return
            self._subscribed_topics.append(topic)
            if self._connected:
                self._ready = False
                self._subscribe_locked(topic)
                self._update_ready_locked()

    def publish(self, topic: str, payload: str, retain: bool = False) -> None:
        # paho returns MQTT_ERR_NO_CONN and drops the message silently when
        # not connected; fail fast instead.
        result = self._client.publish(topic, payload, retain=retain)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            raise ConnectionError(
                f"publish({topic!r}) failed with rc={result.rc}"
            )

    def _locked_state_matches(self, topic: str, expected: str) -> bool:
        with self._lock:
            return self._state.get(topic) == expected

    def _locked_state_exists(self, topic: str) -> bool:
        with self._lock:
            return topic in self._state

    def _locked_event_count(self, topic: str) -> int:
        with self._lock:
            return self._event_count.get(topic, 0)

    def _locked_state_matches_after(
        self, topic: str, expected: str, previous_event_count: int
    ) -> bool:
        with self._lock:
            return (
                self._state.get(topic) == expected
                and self._event_count.get(topic, 0) > previous_event_count
            )

    def wait_for_state(self, topic: str, expected: str, timeout: float) -> bool:
        """Wait until state[topic] matches expected. Returns True if matched."""
        return wait_for(
            lambda: self._locked_state_matches(topic, expected), timeout=timeout
        )

    def wait_for_any_state(self, topic: str, timeout: float) -> bool:
        """Wait until any message received on topic. Returns True if received."""
        return wait_for(
            lambda: self._locked_state_exists(topic), timeout=timeout
        )

    def get_event_count(self, topic: str) -> int:
        """Return count of non-retained MQTT messages observed for topic."""
        return self._locked_event_count(topic)

    def wait_for_next_state(
        self, topic: str, previous_event_count: int, timeout: float
    ) -> str | None:
        """Wait for a message after a baseline, including identical payloads."""
        received = wait_for(
            lambda: self._locked_event_count(topic) > previous_event_count,
            timeout=timeout,
        )
        if not received:
            return None
        return self.get_state(topic)

    def wait_for_state_after(
        self,
        topic: str,
        expected: str,
        previous_event_count: int,
        timeout: float,
    ) -> bool:
        """Wait for expected state published in a new MQTT event."""
        return wait_for(
            lambda: self._locked_state_matches_after(
                topic, expected, previous_event_count
            ),
            timeout=timeout,
        )

    def _locked_ready(self, after_generation: int | None) -> bool:
        with self._lock:
            return self._ready and (
                after_generation is None
                or self._ready_generation > after_generation
            )

    def wait_ready(
        self, timeout: float, after_generation: int | None = None
    ) -> bool:
        """Wait for CONNACK and successful SUBACKs for all tracked topics."""
        return wait_for(
            lambda: self._locked_ready(after_generation), timeout=timeout
        )

    def get_ready_generation(self) -> int:
        with self._lock:
            return self._ready_generation

    def get_state(self, topic: str) -> str | None:
        with self._lock:
            return self._state.get(topic)

    def get_state_json(self, topic: str) -> dict[str, Any] | None:
        with self._lock:
            raw = self._state.get(topic)
            if raw is None:
                return None
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return None

    def clear_state(self, topic: str) -> None:
        with self._lock:
            self._state.pop(topic, None)

    def clear_all(self) -> None:
        with self._lock:
            self._state.clear()

    def reconnect(self) -> None:
        """Clear state and resubscribe after broker restart."""
        self.clear_all()
        self._client.reconnect()

    def disconnect(self) -> None:
        self._client.disconnect()
        self._client.loop_stop()
