"""Host-only regressions for basic-suite provisioning/capture ordering."""

import ast
import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
from collections.abc import Iterator
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest

E2E_DIR = Path(__file__).resolve().parents[2]
BASIC_CONFTEST = E2E_DIR / "tests" / "basic" / "conftest.py"
SIGNATURE_TEST = E2E_DIR / "tests" / "basic" / "test_signature_verify.py"


def _load_functions(path: Path, names: set[str], namespace: dict[str, Any]) -> None:
    tree = ast.parse(path.read_text())
    functions = [
        node
        for node in tree.body
        if isinstance(node, ast.FunctionDef) and node.name in names
    ]
    assert {function.name for function in functions} == names
    for function in functions:
        function.decorator_list = []
    module = ast.Module(
        body=[
            ast.ImportFrom(
                module="__future__",
                names=[ast.alias(name="annotations")],
                level=0,
            ),
            *functions,
        ],
        type_ignores=[],
    )
    ast.fix_missing_locations(module)
    exec(compile(module, str(path), "exec"), namespace)


def _load_fixture_functions(created_dirs: list[Path]) -> dict[str, Any]:
    class RecordingTempfile:
        @staticmethod
        def mkdtemp(*args: Any, **kwargs: Any) -> str:
            path = Path(tempfile.mkdtemp(*args, **kwargs))
            created_dirs.append(path)
            return str(path)

    namespace: dict[str, Any] = {
        "Any": Any,
        "Device": object,
        "Gpio": object,
        "Iterator": Iterator,
        "MqttClient": object,
        "Path": Path,
        "SerialCapture": object,
        "SERIAL_PORT": "fake-serial",
        "SERIAL_BAUD": 115200,
        "__file__": str(BASIC_CONFTEST),
        "config_name": "basic",
        "copy": __import__("copy"),
        "json": json,
        "pytest": pytest,
        "shutil": shutil,
        "tempfile": RecordingTempfile,
        "COMMAND_TOPIC": "home/testDevice/sv1/set",
        "RESULT_TOPIC": "home/testDevice/sv1/state",
        "STATUS_ACTION_TOPIC": "home/testDevice/s1/state",
        "DEVICE_CERTS_DIR": E2E_DIR / "signing_fixtures" / "device",
        "DEVICE_CERT_ALIAS": "signing_test.pem",
        "VALID_CERT_NAME": "signing_test.pem",
        "WHITESPACE_CERT_NAME": "signing_test_whitespace.pem",
    }
    _load_functions(
        BASIC_CONFTEST,
        {"device_config", "provisioning_complete", "serial", "reset_device"},
        namespace,
    )
    namespace["basic_provisioning_complete"] = namespace["provisioning_complete"]
    _load_functions(
        SIGNATURE_TEST,
        {
            "_make_temp_config",
            "_provision_certificate",
            "provisioning_complete",
            "valid_signing_config",
            "whitespace_signing_config",
            "invalid_certificate_config",
        },
        namespace,
    )
    return namespace


def _finish_fixture(generator: Iterator[Any]) -> None:
    with pytest.raises(StopIteration):
        next(generator)


def test_signature_provisioning_finishes_before_capture_and_reset() -> None:
    created_dirs: list[Path] = []
    namespace = _load_fixture_functions(created_dirs)
    events: list[tuple[str, str]] = []

    class RecordingCapture:
        def __init__(self, **_: Any) -> None:
            self.active = False

        def start(self, test_name: str) -> None:
            assert not device.uploading
            assert not self.active
            self.active = True
            events.append(("capture.start", test_name))

        def stop(self) -> None:
            assert self.active
            self.active = False
            events.append(("capture.stop", ""))

    capture = RecordingCapture()

    class RecordingDevice:
        uploading = False
        fail_next_upload = False

        def upload_config(self, paths: list[Path]) -> None:
            assert not capture.active, "capture must stay closed during upload"
            self.uploading = True
            try:
                config = json.loads((paths[0] / "device_config.json").read_text())
                signature = next(
                    (
                        interface
                        for interface in config["interfaces"]
                        if interface.get("type") == "signatureVerify"
                    ),
                    None,
                )
                if signature is None:
                    label = "baseline"
                elif signature["certificate"] == "/signing_test_missing.pem":
                    label = "missing"
                elif (
                    (paths[0] / "signing_test.pem").read_bytes()
                    == (
                        E2E_DIR
                        / "signing_fixtures"
                        / "device"
                        / "signing_test_whitespace.pem"
                    ).read_bytes()
                ):
                    label = "whitespace"
                else:
                    label = "valid"
                events.append(("upload", label))
                if self.fail_next_upload:
                    self.fail_next_upload = False
                    raise RuntimeError("config upload failed")
            finally:
                self.uploading = False

        def reset(self) -> None:
            assert capture.active, "serial capture must start before reset"
            events.append(("reset", ""))

    device = RecordingDevice()

    class RecordingMqttClient:
        def subscribe(self, topic: str) -> None:
            return None

        def clear_all(self) -> None:
            events.append(("mqtt.clear", ""))

    mqtt_client = RecordingMqttClient()
    namespace["SerialCapture"] = lambda **kwargs: capture
    global_config_path = E2E_DIR / "configs" / "global_config.json"
    package_request = SimpleNamespace()
    package_config = namespace["device_config"](
        package_request, device, global_config_path
    )

    try:
        device_config = next(package_config)
        namespace["basic_provisioning_complete"](device_config)
        assert events == [("upload", "baseline")]

        scenarios = (
            ("valid_signing_config", None, "valid"),
            (
                "invalid_certificate_config",
                ("missing", None, "ERROR certificate unavailable"),
                "missing",
            ),
            ("valid_signing_config", None, "valid"),
            ("whitespace_signing_config", None, "whitespace"),
        )
        for index, (fixture_name, param, expected_label) in enumerate(scenarios):
            fixture_cache: dict[str, tuple[Iterator[Any], Any]] = {}

            class ScenarioRequest:
                fixturenames = [fixture_name]
                node = SimpleNamespace(name=f"scenario-{index}")

                def getfixturevalue(self, name: str) -> Any:
                    if name in fixture_cache:
                        return fixture_cache[name][1]
                    if name in (
                        "valid_signing_config",
                        "whitespace_signing_config",
                    ):
                        generator = namespace[name](
                            device_config, device, global_config_path, mqtt_client
                        )
                    else:
                        fixture_request = SimpleNamespace(param=param)
                        generator = namespace[name](
                            fixture_request,
                            device_config,
                            device,
                            global_config_path,
                            mqtt_client,
                        )
                    value = next(generator)
                    fixture_cache[name] = (generator, value)
                    return value

            request = ScenarioRequest()
            namespace["provisioning_complete"](
                request, device_config, device, global_config_path, mqtt_client
            )
            # Explicit test dependency reuses barrier-provisioned fixture.
            request.getfixturevalue(fixture_name)
            assert events[-1] == ("upload", expected_label)

            serial_fixture = namespace["serial"](request, None)
            serial_capture = next(serial_fixture)
            namespace["reset_device"](
                serial_capture, device, mqtt_client, object()
            )
            _finish_fixture(serial_fixture)
            _finish_fixture(fixture_cache[fixture_name][0])

        assert [event for event in events if event[0] == "upload"] == [
            ("upload", "baseline"),
            ("upload", "valid"),
            ("upload", "missing"),
            ("upload", "valid"),
            ("upload", "whitespace"),
        ]
        assert [event[0] for event in events].count("capture.start") == 4
        assert [event[0] for event in events].count("capture.stop") == 4
        assert [event[0] for event in events].count("reset") == 4

        starts_before_failure = sum(event[0] == "capture.start" for event in events)
        device.fail_next_upload = True

        def fail_during_fixture_setup(name: str) -> Any:
            generator = namespace[name](
                device_config, device, global_config_path, mqtt_client
            )
            return next(generator)

        failed_request = SimpleNamespace(
            fixturenames=["valid_signing_config"],
            getfixturevalue=fail_during_fixture_setup,
        )
        with pytest.raises(RuntimeError, match="config upload failed"):
            namespace["provisioning_complete"](
                failed_request,
                device_config,
                device,
                global_config_path,
                mqtt_client,
            )
        assert sum(event[0] == "capture.start" for event in events) == starts_before_failure
        assert not capture.active
    finally:
        for path in created_dirs:
            shutil.rmtree(path, ignore_errors=True)
        _finish_fixture(package_config)


def _source_fixture_functions(
    path: Path, names: set[str], *, remove_serial_barrier: bool = False
) -> str:
    tree = ast.parse(path.read_text())
    functions = [
        node
        for node in tree.body
        if isinstance(node, ast.FunctionDef) and node.name in names
    ]
    assert {function.name for function in functions} == names
    if remove_serial_barrier:
        serial = next(function for function in functions if function.name == "serial")
        serial.args.args = [
            argument
            for argument in serial.args.args
            if argument.arg != "provisioning_complete"
        ]
    return "\n\n".join(ast.unparse(function) for function in functions)


def _write_resolver_project(root: Path, *, remove_serial_barrier: bool = False) -> Path:
    project = root / "nested" / "project"
    tests_dir = project / "tests"
    tests_dir.mkdir(parents=True)
    (tests_dir / "__init__.py").write_text("")

    config_dir = root / "configs" / "basic"
    config_dir.mkdir(parents=True)
    (config_dir / "device_config.json").write_text(
        json.dumps({"interfaces": [], "actions": []})
    )
    (root / "global_config.json").write_text("{}")

    basic_functions = _source_fixture_functions(
        BASIC_CONFTEST,
        {"device_config", "provisioning_complete", "serial", "reset_device"},
        remove_serial_barrier=remove_serial_barrier,
    )
    conftest = textwrap.dedent(
        '''\
        from __future__ import annotations
        import json
        import os
        from pathlib import Path
        import pytest

        SERIAL_PORT = "fake-serial"
        SERIAL_BAUD = 115200
        config_name = "basic"
        _EVENT_PATH = Path(os.environ["LIFECYCLE_EVENTS"])
        _DEVICE_CERTS_DIR = Path(os.environ["DEVICE_CERTS_DIR"])

        def record_event(*event):
            with _EVENT_PATH.open("a") as stream:
                stream.write(json.dumps(event) + "\\n")

        class RecordingDevice:
            def __init__(self):
                self.upload_count = 0
                self.capture_active = False

            def upload_config(self, paths):
                config = json.loads((Path(paths[0]) / "device_config.json").read_text())
                signature = next((item for item in config["interfaces"] if item.get("type") == "signatureVerify"), None)
                if signature is None:
                    label = "baseline"
                elif signature["certificate"] == "/signing_test_missing.pem":
                    label = "missing"
                elif (Path(paths[0]) / "signing_test.pem").read_bytes() == (_DEVICE_CERTS_DIR / "signing_test_whitespace.pem").read_bytes():
                    label = "whitespace"
                else:
                    label = "valid"
                if self.capture_active:
                    record_event("upload_during_capture", label)
                    raise RuntimeError("config upload must finish before serial capture starts")
                self.upload_count += 1
                record_event("upload", label, self.upload_count)
                if self.upload_count == 6:
                    raise RuntimeError("config upload failed")

            def reset(self):
                record_event("reset")

        DEVICE = RecordingDevice()

        class RecordingCapture:
            def __init__(self, **kwargs):
                pass

            def start(self, test_name):
                DEVICE.capture_active = True
                record_event("capture.start", test_name)

            def stop(self):
                record_event("capture.stop")
                DEVICE.capture_active = False

        SerialCapture = RecordingCapture

        class RecordingMqttClient:
            def subscribe(self, topic):
                record_event("mqtt.subscribe", topic)

            def clear_all(self):
                record_event("mqtt.clear")

            def mark_body(self, name):
                record_event("body", name)

        MQTT = RecordingMqttClient()

        @pytest.fixture(scope="package")
        def device():
            return DEVICE

        @pytest.fixture(scope="package")
        def mqtt_client():
            return MQTT

        @pytest.fixture
        def gpio():
            return object()

        @pytest.fixture(scope="package")
        def global_config_path():
            return Path(__file__).parent.parent.parent / "global_config.json"
        '''
    )
    (project / "conftest.py").write_text(conftest + "\n\n" + basic_functions)

    signature_functions = _source_fixture_functions(
        SIGNATURE_TEST,
        {
            "_make_temp_config",
            "_provision_certificate",
            "restore_basic_config",
            "provisioning_complete",
            "valid_signing_config",
            "whitespace_signing_config",
            "invalid_certificate_config",
        },
    )
    signature_module = textwrap.dedent(
        f'''\
        from __future__ import annotations
        import copy
        import json
        import shutil
        from pathlib import Path
        import pytest

        COMMAND_TOPIC = "home/testDevice/sv1/set"
        RESULT_TOPIC = "home/testDevice/sv1/state"
        STATUS_ACTION_TOPIC = "home/testDevice/s1/state"
        VALID_CERT_NAME = "signing_test.pem"
        DEVICE_CERT_ALIAS = "signing_test.pem"
        WHITESPACE_CERT_NAME = "signing_test_whitespace.pem"
        DEVICE_CERTS_DIR = Path({str(E2E_DIR / "signing_fixtures" / "device")!r})
        INVALID_CERT_CASES = [pytest.param(("missing", None, "ERROR certificate unavailable"), id="missing")]

        class _ProjectTempFiles:
            def __init__(self):
                self.count = 0

            def mkdtemp(self, prefix):
                self.count += 1
                path = Path(__file__).parent / f"config-{{self.count}}"
                path.mkdir()
                return str(path)

        tempfile = _ProjectTempFiles()
        '''
    )
    (tests_dir / "test_signature.py").write_text(
        signature_module
        + "\n\n"
        + signature_functions
        + textwrap.dedent(
            '''\

            def test_01_valid(valid_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("valid")

            def test_02_missing(invalid_certificate_config, reset_device, mqtt_client):
                mqtt_client.mark_body("missing")

            def test_03_valid(valid_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("valid_again")

            def test_04_whitespace(whitespace_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("whitespace")

            def test_05_upload_failure(valid_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("upload_failure")
            '''
        ),
    )
    (tests_dir / "test_baseline.py").write_text(
        textwrap.dedent(
            '''\
            def test_baseline(device_config, reset_device, mqtt_client):
                mqtt_client.mark_body("baseline")
            '''
        )
    )
    return project


def _run_resolver_project(project: Path, events_path: Path) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env["LIFECYCLE_EVENTS"] = str(events_path)
    env["DEVICE_CERTS_DIR"] = str(E2E_DIR / "signing_fixtures" / "device")
    return subprocess.run(
        [sys.executable, "-m", "pytest", "-q", "--confcutdir", str(project), str(project / "tests")],
        cwd=project,
        env=env,
        capture_output=True,
        text=True,
        check=False,
        timeout=60,
    )


def test_real_pytest_fixture_resolution_serial_barrier(tmp_path: Path) -> None:
    project = _write_resolver_project(tmp_path / "real")
    events_path = tmp_path / "real-events.jsonl"
    result = _run_resolver_project(project, events_path)
    output = result.stdout + result.stderr
    assert result.returncode == 1, output
    assert "5 passed, 1 error" in output, output
    assert "config upload failed" in output, output

    events = [
        tuple(json.loads(line))
        for line in events_path.read_text().splitlines()
        if line
    ]
    assert [event[1] for event in events if event[0] == "upload"] == [
        "baseline",
        "valid",
        "missing",
        "valid",
        "whitespace",
        "valid",
        "baseline",
    ]
    assert sum(event[0] == "capture.start" for event in events) == 5
    assert sum(event[0] == "capture.stop" for event in events) == 5
    assert sum(event[0] == "reset" for event in events) == 5
    assert sum(event[0] == "mqtt.clear" for event in events) == 5
    assert [event[1] for event in events if event[0] == "body"] == [
        "baseline",
        "valid",
        "missing",
        "valid_again",
        "whitespace",
    ]
    failed_upload = events.index(("upload", "valid", 6))
    restore_upload = events.index(("upload", "baseline", 7))
    assert not any(
        event[0] in {"capture.start", "reset", "mqtt.clear", "mqtt.subscribe", "body"}
        for event in events[failed_upload + 1 : restore_upload]
    )

    mutant_project = _write_resolver_project(
        tmp_path / "mutant", remove_serial_barrier=True
    )
    mutant_events = tmp_path / "mutant-events.jsonl"
    mutant = _run_resolver_project(mutant_project, mutant_events)
    mutant_output = mutant.stdout + mutant.stderr
    assert mutant.returncode != 0, mutant_output
    assert "config upload must finish before serial capture starts" in mutant_output
    assert any(
        json.loads(line)[0] == "upload_during_capture"
        for line in mutant_events.read_text().splitlines()
        if line
    )
