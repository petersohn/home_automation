"""paho-mqtt wrapper with background state tracking and wait_for_state."""

import json
import threading
import time
from dataclasses import dataclass
from typing import Any

import paho.mqtt.client as mqtt


@dataclass
class Message:
    topic: str
    payload: str


class MqttClient:
    def __init__(self, host: str, port: int, username: str, password: str):
        self._host = host
        self._port = port
        self._username = username
        self._password = password
        self._state: dict[str, str] = {}
        self._lock = threading.Lock()
        self._subscribed_topics: list[str] = []
        self._client = mqtt.Client()
        self._client.username_pw_set(username, password)
        self._client.on_message = self._on_message
        # paho does not restore subscriptions after a reconnect (e.g. broker
        # restart); resubscribe on every (re)connect.
        self._client.on_connect = self._on_connect
        self._client.connect(host, port)
        self._thread = threading.Thread(target=self._client.loop_forever, daemon=True)
        self._thread.start()

    def _on_connect(self, client: mqtt.Client, userdata: object, flags: dict, rc: int) -> None:
        for topic in self._subscribed_topics:
            client.subscribe(topic)

    def _on_message(self, client: mqtt.Client, userdata: object, msg: mqtt.MQTTMessage) -> None:
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

    def subscribe(self, topic: str) -> None:
        if topic not in self._subscribed_topics:
            self._client.subscribe(topic)
            self._subscribed_topics.append(topic)

    def publish(self, topic: str, payload: str, retain: bool = False) -> None:
        self._client.publish(topic, payload, retain=retain)

    def wait_for_state(self, topic: str, expected: str, timeout: float) -> bool:
        """Wait until state[topic] matches expected. Returns True if matched."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                if self._state.get(topic) == expected:
                    return True
            time.sleep(0.1)
        return False

    def wait_for_any_state(self, topic: str, timeout: float) -> bool:
        """Wait until any message received on topic. Returns True if received."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                if topic in self._state:
                    return True
            time.sleep(0.1)
        return False

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
        for topic in self._subscribed_topics:
            self._client.subscribe(topic)

    def disconnect(self) -> None:
        self._client.disconnect()
        self._thread.join(timeout=5.0)