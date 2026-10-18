import ast
import json
import os
import subprocess
import sys
import textwrap
from pathlib import Path

E2E_DIR = Path(__file__).resolve().parents[2]
SIGNATURE_CONFTEST = E2E_DIR / "tests" / "signature_verify" / "conftest.py"
SIGNATURE_TEST = E2E_DIR / "tests" / "signature_verify" / "test_signature_verify.py"
FIXED_CONFIG = E2E_DIR / "configs" / "signature_verify" / "device_config.json"
DEVICE_CERTS = E2E_DIR / "signing_fixtures" / "device"


def _fixture_source(
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
    root.mkdir(parents=True)
    project = root / "nested" / "project"
    tests_dir = project / "tests"
    tests_dir.mkdir(parents=True)
    (tests_dir / "__init__.py").write_text("")
    global_config = root / "global_config.json"
    global_config.write_text("{}\n")

    conftest = textwrap.dedent(
        f'''\
        from __future__ import annotations
        import json
        import os
        import shutil
        import tempfile
        from collections.abc import Iterator
        from pathlib import Path
        import pytest

        DEVICE_CERTS_DIR = Path({str(DEVICE_CERTS)!r})
        DEVICE_CONFIG_PATH = Path({str(FIXED_CONFIG)!r})
        DEVICE_CERT_ALIAS = "signing_test.pem"
        RESULT_TOPIC = "home/testDevice/sv1/state"
        STATUS_ACTION_TOPIC = "home/testDevice/s1/state"
        VALID_CERT_NAME = "signing_test.pem"
        WHITESPACE_CERT_NAME = "signing_test_whitespace.pem"
        INVALID_CERT_CASES = [pytest.param(("missing", None, "ERROR certificate unavailable"), id="missing")]
        SERIAL_PORT = "fake-serial"
        SERIAL_BAUD = 115200
        _EVENT_PATH = Path(os.environ["LIFECYCLE_EVENTS"])
        _FIXED_CONFIG_BYTES = DEVICE_CONFIG_PATH.read_bytes()

        def record_event(*event):
            with _EVENT_PATH.open("a") as stream:
                stream.write(json.dumps(event) + "\\n")

        class RecordingDevice:
            def __init__(self):
                self.upload_count = 0
                self.capture_active = False

            def upload_config(self, paths):
                input_dir = Path(paths[0])
                config_bytes = (input_dir / "device_config.json").read_bytes()
                config = json.loads(config_bytes)
                signature = next(item for item in config["interfaces"] if item.get("type") == "signatureVerify")
                certificate = signature["certificate"]
                assert certificate == "/signing_test.pem"
                cert_path = input_dir / "signing_test.pem"
                if not cert_path.exists():
                    label = "missing"
                elif cert_path.read_bytes() == (DEVICE_CERTS_DIR / "signing_test_whitespace.pem").read_bytes():
                    label = "whitespace"
                else:
                    label = "valid"
                if self.capture_active:
                    record_event("upload_during_capture", label)
                    raise RuntimeError("config upload must finish before serial capture starts")
                self.upload_count += 1
                record_event("upload", label, self.upload_count, config_bytes == _FIXED_CONFIG_BYTES)
                if self.upload_count == 5:
                    raise RuntimeError("config upload failed")

            def reset(self):
                assert self.capture_active, "serial capture must start before reset"
                record_event("reset")

        DEVICE = RecordingDevice()

        class RecordingCapture:
            def __init__(self, **kwargs):
                pass

            def start(self, test_name):
                assert not DEVICE.capture_active
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
            return Path({str(global_config)!r})
        '''
    )
    conftest += "\n\n" + _fixture_source(
        SIGNATURE_CONFTEST,
        {"serial", "reset_device"},
        remove_serial_barrier=remove_serial_barrier,
    )
    conftest += "\n\n" + _fixture_source(
        SIGNATURE_TEST,
        {
            "_make_temp_config",
            "_provision_certificate",
            "provisioning_complete",
            "valid_signing_config",
            "whitespace_signing_config",
            "invalid_certificate_config",
        },
    )
    (project / "conftest.py").write_text(conftest + "\n")

    (tests_dir / "test_signature_lifecycle.py").write_text(
        textwrap.dedent(
            '''\
            def test_01_valid(valid_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("valid")

            def test_02_missing(invalid_certificate_config, reset_device, mqtt_client):
                mqtt_client.mark_body("missing")

            def test_03_valid_after_invalid(valid_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("valid_after_invalid")

            def test_04_whitespace(whitespace_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("whitespace")

            def test_05_upload_failure(valid_signing_config, reset_device, mqtt_client):
                mqtt_client.mark_body("must_not_run")
            '''
        )
    )
    return project


def _run_resolver_project(
    project: Path, events_path: Path
) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env["LIFECYCLE_EVENTS"] = str(events_path)
    return subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            "-q",
            "--confcutdir",
            str(project),
            str(project / "tests"),
        ],
        cwd=project,
        env=env,
        capture_output=True,
        text=True,
        check=False,
        timeout=60,
    )


def test_real_pytest_fixture_resolution_provisions_before_capture_and_reset(
    tmp_path: Path,
) -> None:
    project = _write_resolver_project(tmp_path / "real")
    events_path = tmp_path / "real-events.jsonl"
    result = _run_resolver_project(project, events_path)
    output = result.stdout + result.stderr
    assert result.returncode == 1, output
    assert "4 passed, 1 error" in output, output
    assert "config upload failed" in output, output

    events = [
        tuple(json.loads(line))
        for line in events_path.read_text().splitlines()
        if line
    ]
    uploads = [event for event in events if event[0] == "upload"]
    assert uploads == [
        ("upload", "valid", 1, True),
        ("upload", "missing", 2, True),
        ("upload", "valid", 3, True),
        ("upload", "whitespace", 4, True),
        ("upload", "valid", 5, True),
    ]
    assert [event[1] for event in events if event[0] == "body"] == [
        "valid",
        "missing",
        "valid_after_invalid",
        "whitespace",
    ]
    assert sum(event[0] == "capture.start" for event in events) == 4
    assert sum(event[0] == "capture.stop" for event in events) == 4
    assert sum(event[0] == "reset" for event in events) == 4
    assert sum(event[0] == "mqtt.clear" for event in events) == 4

    for upload in uploads[:4]:
        upload_index = events.index(upload)
        capture_index = next(
            index
            for index in range(upload_index + 1, len(events))
            if events[index][0] == "capture.start"
        )
        reset_index = next(
            index
            for index in range(capture_index + 1, len(events))
            if events[index][0] == "reset"
        )
        assert upload_index < capture_index < reset_index

    failed_upload_index = events.index(uploads[-1])
    assert not any(
        event[0] in {"capture.start", "reset", "mqtt.clear", "body"}
        for event in events[failed_upload_index + 1 :]
    )
    assert len(uploads) == 5  # No baseline upload or teardown restore.

    mutant = _write_resolver_project(
        tmp_path / "mutant", remove_serial_barrier=True
    )
    mutant_events = tmp_path / "mutant-events.jsonl"
    mutant_result = _run_resolver_project(mutant, mutant_events)
    mutant_output = mutant_result.stdout + mutant_result.stderr
    assert mutant_result.returncode != 0, mutant_output
    assert "config upload must finish before serial capture starts" in mutant_output
    assert any(
        json.loads(line)[0] == "upload_during_capture"
        for line in mutant_events.read_text().splitlines()
        if line
    )
