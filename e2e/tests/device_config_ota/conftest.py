import json
import shutil
import tempfile
from collections.abc import Callable, Iterator
from pathlib import Path
from typing import Any

import pytest

from lib.pin_map import SERIAL_BAUD, SERIAL_PORT
from lib.device_config_ota import (
    decode_ota_device_status,
    wait_for_device_lwt_then_clear,
)
from lib.serial_capture import SerialCapture


E2E_DIR = Path(__file__).resolve().parents[2]
FIXTURES_DIR = E2E_DIR / "signing_fixtures"
BASE_CONFIG_PATH = E2E_DIR / "configs/device_config_ota/device_config.json"


@pytest.fixture
def serial(
    request: pytest.FixtureRequest,
) -> Iterator[Callable[[], SerialCapture]]:
    capture = SerialCapture(
        port=SERIAL_PORT,
        baud=SERIAL_BAUD,
        log_dir=E2E_DIR / "artifacts",
    )
    started = False

    def start() -> SerialCapture:
        nonlocal started
        if not started:
            capture.start(request.node.name)
            started = True
        return capture

    yield start
    if started:
        capture.stop()


@pytest.fixture
def provision_ota_device(
    request: pytest.FixtureRequest,
    serial: Callable[[], SerialCapture],
    device: Any,
    global_config_path: Path,
    mqtt_client: Any,
) -> Iterator[Callable[[dict[str, object] | None], dict[str, object]]]:
    temp_dirs: list[Path] = []
    owned_prefixes: set[str] = set()

    def cleanup() -> None:
        try:
            wait_for_device_lwt_then_clear(
                mqtt_client, device, tuple(owned_prefixes)
            )
        finally:
            for temp_dir in temp_dirs:
                shutil.rmtree(temp_dir, ignore_errors=True)

    request.addfinalizer(cleanup)

    def provision(config: dict[str, object] | None = None) -> dict[str, object]:
        if config is None:
            config = json.loads(BASE_CONFIG_PATH.read_text(encoding="utf-8"))
        prefix = config["servicePrefix"]
        owned_prefixes.add(prefix)
        temp_dir = Path(tempfile.mkdtemp(prefix="device_config_ota_"))
        temp_dirs.append(temp_dir)
        config_path = temp_dir / "device_config.json"
        if config == json.loads(BASE_CONFIG_PATH.read_text(encoding="utf-8")):
            shutil.copy2(BASE_CONFIG_PATH, config_path)
        else:
            config_path.write_text(
                json.dumps(config, separators=(",", ":")), encoding="utf-8"
            )
        shutil.copy2(
            FIXTURES_DIR / "device" / "signing_test.pem",
            temp_dir / "ota_certificate.pem",
        )
        upload_inputs = [temp_dir, global_config_path]
        host_key = (
            FIXTURES_DIR / "host" / "signing_test_private_key.pem"
        ).resolve()
        assert host_key not in [path.resolve() for path in upload_inputs]
        device.enable()
        device.upload_config(upload_inputs)
        serial()

        status_topic = f"{prefix}/status"
        for topic in (
            status_topic,
            f"{prefix}/available",
            f"{prefix}/update/device-config/result",
        ):
            mqtt_client.subscribe(topic)
        assert mqtt_client.wait_ready(timeout=10.0)

        mqtt_client.clear_all()
        previous_status_event = mqtt_client.get_event_count(status_topic)
        device.reset()
        assert device.wait_for_boot(timeout=30.0), "Device did not boot"
        raw_status = mqtt_client.wait_for_next_state(
            status_topic, previous_status_event, timeout=30.0
        )
        assert raw_status is not None, f"No fresh status on {status_topic}"
        status = decode_ota_device_status(raw_status)
        assert status is not None, f"Invalid OTA status: {raw_status}"
        assert status["version"] == config["configVersion"]
        return {
            "client": mqtt_client,
            "device": device,
            "mac": status["mac"],
            "prefix": prefix,
            "owned_prefixes": owned_prefixes,
            "status": status,
            "serial_log": E2E_DIR / "artifacts" / f"{request.node.name}.log",
        }

    yield provision


@pytest.fixture
def ota_baseline(
    provision_ota_device: Callable[[dict[str, object] | None], dict[str, object]],
) -> dict[str, object]:
    return provision_ota_device(None)


@pytest.fixture
def ota_max_version_baseline(
    provision_ota_device: Callable[[dict[str, object] | None], dict[str, object]],
) -> dict[str, object]:
    config = json.loads(BASE_CONFIG_PATH.read_text(encoding="utf-8"))
    config["configVersion"] = 0xFFFFFFFE
    return provision_ota_device(config)
