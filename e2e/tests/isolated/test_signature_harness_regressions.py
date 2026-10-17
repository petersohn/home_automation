"""Host-only regressions for signature provisioning and MQTT recovery harness."""

import ast
import copy
import json
import shutil
import socket
import tempfile
import threading
import time
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest

E2E_DIR = Path(__file__).resolve().parents[2]
SIGNATURE_TEST_PATH = E2E_DIR / "tests" / "basic" / "test_signature_verify.py"
MQTT_CLIENT_PATH = E2E_DIR / "lib" / "mqtt_client.py"
FIXTURES_DIR = E2E_DIR / "signing_fixtures" / "device"
DEVICE_CONFIG_DIR = E2E_DIR / "configs" / "basic"
GLOBAL_CONFIG_PATH = E2E_DIR / "configs" / "global_config.json"


def _module_tree(path: Path) -> ast.Module:
    return ast.parse(path.read_text())


def _constant(tree: ast.Module, name: str) -> Any:
    assignment = next(
        node
        for node in tree.body
        if isinstance(node, ast.Assign)
        and any(
            isinstance(target, ast.Name) and target.id == name
            for target in node.targets
        )
    )
    return ast.literal_eval(assignment.value)


def _load_provisioning_functions() -> dict[str, Any]:
    tree = _module_tree(SIGNATURE_TEST_PATH)
    names = {"_make_temp_config", "_provision_certificate"}
    functions = [
        node
        for node in tree.body
        if isinstance(node, ast.FunctionDef) and node.name in names
    ]
    namespace: dict[str, Any] = {
        "copy": copy,
        "json": json,
        "shutil": shutil,
        "tempfile": tempfile,
        "Path": Path,
        "Device": object,
        "MqttClient": object,
        "DEVICE_CERTS_DIR": FIXTURES_DIR,
        "DEVICE_CERT_ALIAS": _constant(tree, "DEVICE_CERT_ALIAS"),
        "COMMAND_TOPIC": "home/testDevice/sv1/set",
        "RESULT_TOPIC": "home/testDevice/sv1/state",
        "STATUS_ACTION_TOPIC": "home/testDevice/s1/state",
    }
    module = ast.Module(body=functions, type_ignores=[])
    exec(compile(module, str(SIGNATURE_TEST_PATH), "exec"), namespace)
    return namespace


def _fixture_scope(tree: ast.Module, name: str) -> str | None:
    function = next(
        node
        for node in tree.body
        if isinstance(node, ast.FunctionDef) and node.name == name
    )
    decorator = next(
        decorator
        for decorator in function.decorator_list
        if (
            isinstance(decorator, ast.Attribute)
            and decorator.attr == "fixture"
        )
        or (
            isinstance(decorator, ast.Call)
            and isinstance(decorator.func, ast.Attribute)
            and decorator.func.attr == "fixture"
        )
    )
    scope = next(
        (
            keyword.value
            for keyword in decorator.keywords
            if keyword.arg == "scope"
        )
        if isinstance(decorator, ast.Call)
        else iter(()),
        None,
    )
    return ast.literal_eval(scope) if scope is not None else None


def test_fixture_scope_handles_bare_explicit_and_missing_decorators() -> None:
    tree = ast.parse(
        """
@pytest.fixture
def default_scope():
    pass

@pytest.fixture(scope=\"module\")
def module_scope():
    pass

def missing_fixture():
    pass
"""
    )

    assert _fixture_scope(tree, "default_scope") is None
    assert _fixture_scope(tree, "module_scope") == "module"
    with pytest.raises(StopIteration):
        _fixture_scope(tree, "missing_fixture")


def test_all_device_certificates_use_short_alias_and_preserve_bytes() -> None:
    tree = _module_tree(SIGNATURE_TEST_PATH)
    alias = _constant(tree, "DEVICE_CERT_ALIAS")
    assert len(alias.encode("utf-8")) <= 31
    namespace = _load_provisioning_functions()
    base_config = DEVICE_CONFIG_DIR / "device_config.json"

    for source in FIXTURES_DIR.iterdir():
        if not source.is_file():
            continue
        temp_dir = namespace["_make_temp_config"](source.name, base_config)
        try:
            config = json.loads((temp_dir / "device_config.json").read_text())
            interface = config["interfaces"][-1]
            assert interface["certificate"] == f"/{alias}"
            assert len(interface["certificate"].encode("utf-8")) <= 31
            assert (temp_dir / alias).read_bytes() == source.read_bytes()
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    missing_dir = namespace["_make_temp_config"](None, base_config)
    try:
        config = json.loads((missing_dir / "device_config.json").read_text())
        assert config["interfaces"][-1]["certificate"] == "/signing_test_missing.pem"
        assert not (missing_dir / alias).exists()
        assert not (missing_dir / "signing_test_missing.pem").exists()
    finally:
        shutil.rmtree(missing_dir, ignore_errors=True)


def test_provisioning_fixtures_upload_each_scenario_with_function_scope() -> None:
    tree = _module_tree(SIGNATURE_TEST_PATH)
    for fixture_name in (
        "valid_signing_config",
        "whitespace_signing_config",
        "invalid_certificate_config",
    ):
        assert _fixture_scope(tree, fixture_name) in (None, "function")

    namespace = _load_provisioning_functions()
    alias = namespace["DEVICE_CERT_ALIAS"]

    class RecordingDevice:
        def __init__(self) -> None:
            self.uploads: list[tuple[str, bytes | None]] = []

        def upload_config(self, paths: list[Path]) -> None:
            config = json.loads((paths[0] / "device_config.json").read_text())
            certificate = config["interfaces"][-1]["certificate"]
            certificate_path = paths[0] / Path(certificate).name
            self.uploads.append(
                (
                    certificate,
                    certificate_path.read_bytes()
                    if certificate_path.exists()
                    else None,
                )
            )

    class RecordingMqttClient:
        def subscribe(self, topic: str) -> None:
            return None

    device = RecordingDevice()
    client = RecordingMqttClient()
    scenarios = [
        "signing_test.pem",
        None,
        "signing_test.pem",
        "signing_test_whitespace.pem",
        "signing_test.pem",
    ]
    temp_dirs: list[Path] = []
    try:
        for certificate_name in scenarios:
            temp_dirs.append(
                namespace["_provision_certificate"](
                    certificate_name,
                    DEVICE_CONFIG_DIR,
                    device,
                    GLOBAL_CONFIG_PATH,
                    client,
                )
            )

        expected_names = scenarios
        assert len(device.uploads) == len(expected_names)
        for (certificate_path, contents), certificate_name in zip(
            device.uploads, expected_names, strict=True
        ):
            if certificate_name is None:
                assert certificate_path == "/signing_test_missing.pem"
                assert contents is None
            else:
                assert certificate_path == f"/{alias}"
                assert contents == (FIXTURES_DIR / certificate_name).read_bytes()
    finally:
        for temp_dir in temp_dirs:
            shutil.rmtree(temp_dir, ignore_errors=True)


def _load_mqtt_client_class(mqtt_module: Any | None = None) -> type:
    tree = _module_tree(MQTT_CLIENT_PATH)
    client_class = next(
        node
        for node in tree.body
        if isinstance(node, ast.ClassDef) and node.name == "MqttClient"
    )
    namespace = {
        "mqtt": mqtt_module
        or SimpleNamespace(Client=object, MQTTMessage=object, MQTT_ERR_SUCCESS=0),
        "threading": threading,
        "json": json,
        "Any": Any,
        "wait_for": lambda check, timeout: _wait_for(check, timeout),
    }
    module = ast.Module(body=[client_class], type_ignores=[])
    exec(compile(module, str(MQTT_CLIENT_PATH), "exec"), namespace)
    return namespace["MqttClient"]


def test_mqtt_client_manages_paho_network_loop() -> None:
    calls: list[str] = []

    class FakePahoClient:
        def username_pw_set(self, username: str, password: str) -> None:
            calls.append("credentials")

        def connect(self, host: str, port: int) -> None:
            calls.append("connect")

        def loop_start(self) -> int:
            calls.append("loop_start")
            return 0

        def disconnect(self) -> None:
            calls.append("disconnect")

        def loop_stop(self) -> None:
            calls.append("loop_stop")

    mqtt_module = SimpleNamespace(
        Client=FakePahoClient, MQTTMessage=object, MQTT_ERR_SUCCESS=0
    )
    mqtt_client_class = _load_mqtt_client_class(mqtt_module)

    client = mqtt_client_class("localhost", 1883, "user", "password")
    client.disconnect()

    assert calls == [
        "credentials",
        "connect",
        "loop_start",
        "disconnect",
        "loop_stop",
    ]

    class FailedLoopClient(FakePahoClient):
        def loop_start(self) -> int:
            calls.append("loop_start_failed")
            return 1

    mqtt_module.Client = FailedLoopClient
    mqtt_client_class = _load_mqtt_client_class(mqtt_module)
    with pytest.raises(RuntimeError, match="Failed to start MQTT network loop"):
        mqtt_client_class("localhost", 1883, "user", "password")


def test_real_paho_write_failure_does_not_block_subscribe_and_clears_pending() -> None:
    import paho.mqtt.client as real_mqtt

    mqtt_client_class = _load_mqtt_client_class(real_mqtt)
    paho_client = real_mqtt.Client(
        callback_api_version=real_mqtt.CallbackAPIVersion.VERSION2,
        reconnect_on_failure=False,
    )
    observer = mqtt_client_class.__new__(mqtt_client_class)
    events: list[tuple[str, Any]] = []

    class TrackingPendingSubscriptions(set[int]):
        def add(self, mid: int) -> None:
            events.append(("add", mid))
            super().add(mid)

        def clear(self) -> None:
            events.append(("clear", tuple(self)))
            super().clear()

    observer._lock = threading.Lock()
    observer._client = paho_client
    observer._subscribed_topics = []
    observer._connected = True
    observer._ready = True
    observer._ready_generation = 0
    observer._pending_subscriptions = TrackingPendingSubscriptions()
    observer._subscription_failed = False
    disconnected = threading.Event()

    def on_disconnect(*args: object) -> None:
        observer._on_disconnect(*args)
        disconnected.set()

    paho_client.on_disconnect = on_disconnect
    paho_client.on_subscribe = observer._on_subscribe

    class BrokenWriteSocket:
        def __init__(self, sock: socket.socket) -> None:
            self.sock = sock

        def fileno(self) -> int:
            return self.sock.fileno()

        def recv(self, size: int) -> bytes:
            return self.sock.recv(size)

        def send(self, data: bytes) -> int:
            raise BrokenPipeError("injected MQTT socket write failure")

        def setblocking(self, blocking: bool) -> None:
            self.sock.setblocking(blocking)

        def close(self) -> None:
            self.sock.close()

    network_sock, peer_sock = socket.socketpair()
    network_sock.setblocking(False)
    paho_client._sock = BrokenWriteSocket(network_sock)
    paho_client._state = real_mqtt._ConnectionState.MQTT_CS_CONNECTED

    subscribe_errors: list[BaseException] = []

    def subscribe() -> None:
        try:
            observer.subscribe("write-failure/topic")
        except BaseException as error:
            subscribe_errors.append(error)

    try:
        assert paho_client.loop_start() == real_mqtt.MQTT_ERR_SUCCESS
        subscribe_thread = threading.Thread(target=subscribe, daemon=True)
        subscribe_thread.start()
        subscribe_thread.join(timeout=1.0)
        if subscribe_thread.is_alive():
            if observer._lock.locked():
                observer._lock.release()
            subscribe_thread.join(timeout=1.0)
            pytest.fail("subscribe blocked on Paho socket write failure")
        assert subscribe_errors == []

        assert _wait_for(
            lambda: disconnected.is_set()
            and not observer._connected
            and not observer._pending_subscriptions,
            timeout=2.0,
        )
        assert events[0][0] == "add"
        assert events[1] == ("clear", (events[0][1],))
    finally:
        observer.disconnect()
        peer_sock.close()


def test_concurrent_early_suback_waits_for_mid_registration() -> None:
    mqtt_client_class = _load_mqtt_client_class()
    callback_attempted = threading.Event()
    callback_finished = threading.Event()
    mid_checks: list[tuple[int, bool]] = []

    class ObservedLock:
        def __init__(self) -> None:
            self.lock = threading.Lock()

        def __enter__(self) -> "ObservedLock":
            if threading.current_thread().name == "early-suback":
                callback_attempted.set()
            self.lock.acquire()
            return self

        def __exit__(self, *_: object) -> None:
            self.lock.release()

    class TrackingPendingSubscriptions(set[int]):
        def __contains__(self, mid: object) -> bool:
            found = super().__contains__(mid)
            if threading.current_thread().name == "early-suback":
                mid_checks.append((int(mid), found))
            return found

    class FakePahoClient:
        def subscribe(self, topic: str) -> tuple[int, int]:
            callback_thread = threading.Thread(
                target=self._send_early_suback,
                name="early-suback",
            )
            callback_thread.start()
            assert callback_attempted.wait(timeout=0.5)
            assert not callback_finished.wait(timeout=0.05)
            return 0, 17

        def _send_early_suback(self) -> None:
            observer._on_subscribe(self, None, 17, [0])
            callback_finished.set()

    observer = mqtt_client_class.__new__(mqtt_client_class)
    paho_client = FakePahoClient()
    observer._lock = ObservedLock()
    observer._client = paho_client
    observer._subscribed_topics = []
    observer._connected = True
    observer._ready = True
    observer._ready_generation = 0
    observer._pending_subscriptions = TrackingPendingSubscriptions()
    observer._subscription_failed = False

    observer.subscribe("early/topic")

    assert callback_finished.wait(timeout=0.5)
    assert mid_checks == [(17, True)]
    assert observer.wait_ready(timeout=0.5)


def _wait_for(check: Any, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if check():
            return True
        time.sleep(0.001)
    return check()


def test_observer_readiness_requires_fresh_connection_and_all_subacks() -> None:
    mqtt_client_class = _load_mqtt_client_class()

    class FakePahoClient:
        def __init__(self) -> None:
            self.next_mid = 1
            self.subscriptions: list[tuple[str, int]] = []

        def subscribe(self, topic: str) -> tuple[int, int]:
            mid = self.next_mid
            self.next_mid += 1
            self.subscriptions.append((topic, mid))
            return 0, mid

    client = mqtt_client_class.__new__(mqtt_client_class)
    fake_client = FakePahoClient()
    client._lock = threading.Lock()
    client._client = fake_client
    client._subscribed_topics = ["result", "status"]
    client._connected = False
    client._ready = False
    client._ready_generation = 0
    client._pending_subscriptions = set()
    client._subscription_failed = False

    client._on_connect(fake_client, None, {}, 0)
    assert not client.wait_ready(timeout=0.01)
    first_mids = [mid for _, mid in fake_client.subscriptions]
    client._on_subscribe(fake_client, None, first_mids[0], [0])
    assert not client.wait_ready(timeout=0.01)
    client._on_subscribe(fake_client, None, first_mids[1], [0])
    assert client.wait_ready(timeout=0.01)
    first_generation = client.get_ready_generation()

    client._on_disconnect(fake_client, None, 1, "broker stopped")
    assert not client.wait_ready(timeout=0.01)
    client._on_connect(fake_client, None, {}, 0)
    assert not client.wait_ready(timeout=0.01, after_generation=first_generation)
    new_mids = [mid for _, mid in fake_client.subscriptions[2:]]
    for mid in new_mids:
        client._on_subscribe(fake_client, None, mid, [0])
    assert client.wait_ready(timeout=0.01, after_generation=first_generation)
    assert client.get_ready_generation() > first_generation


def _load_reconnect_roundtrip_helper() -> dict[str, Any]:
    tree = _module_tree(SIGNATURE_TEST_PATH)
    function = next(
        node
        for node in tree.body
        if isinstance(node, ast.FunctionDef)
        and node.name == "send_signed_command_until_ok"
    )
    namespace = {
        "MqttClient": object,
        "RESULT_TOPIC": "home/testDevice/sv1/state",
        "COMMAND_TOPIC": "home/testDevice/sv1/set",
        "RECONNECT_RESULT_TIMEOUT": 0.01,
        "RECONNECT_ATTEMPTS": 3,
        "time": SimpleNamespace(sleep=lambda _: None),
    }
    module = ast.Module(body=[function], type_ignores=[])
    exec(compile(module, str(SIGNATURE_TEST_PATH), "exec"), namespace)
    return namespace


def test_reconnect_roundtrip_retries_only_no_response_and_transient_publish() -> None:
    namespace = _load_reconnect_roundtrip_helper()

    class FakeObserver:
        def __init__(self, results: list[str | None], errors: int = 0) -> None:
            self.results = iter(results)
            self.errors = errors
            self.publish_count = 0

        def get_event_count(self, topic: str) -> int:
            assert topic == namespace["RESULT_TOPIC"]
            return self.publish_count

        def publish(self, topic: str, command: str) -> None:
            assert topic == namespace["COMMAND_TOPIC"]
            assert command == "signed-envelope"
            self.publish_count += 1
            if self.errors:
                self.errors -= 1
                raise ConnectionError("transient disconnect")

        def wait_for_next_state(
            self, topic: str, previous_event: int, timeout: float
        ) -> str | None:
            assert topic == namespace["RESULT_TOPIC"]
            assert timeout == namespace["RECONNECT_RESULT_TIMEOUT"]
            return next(self.results)

    observer = FakeObserver([None, "OK"], errors=1)
    namespace["send_signed_command_until_ok"](observer, "signed-envelope")
    assert observer.publish_count == 3

    wrong_result = FakeObserver(["FAIL invalid signature"])
    with pytest.raises(AssertionError, match="FAIL invalid signature"):
        namespace["send_signed_command_until_ok"](wrong_result, "signed-envelope")
    assert wrong_result.publish_count == 1

    no_response = FakeObserver([None, None, None])
    with pytest.raises(AssertionError, match="no signed-command result"):
        namespace["send_signed_command_until_ok"](no_response, "signed-envelope")
    assert no_response.publish_count == namespace["RECONNECT_ATTEMPTS"]
