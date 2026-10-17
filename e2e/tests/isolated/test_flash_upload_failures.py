"""Subprocess regressions for ESP8266 upload error propagation."""

import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[3]
FLASH_PY = ROOT / "flash.py"
FLASH_TOML = ROOT / "flash.toml"


@pytest.mark.parametrize("upload_kind", ["firmware-image", "filesystem-directory"])
def test_esptool_failure_is_nonzero_for_outer_check(
    upload_kind: str, tmp_path: Path
) -> None:
    missing_port = tmp_path / "no-such-serial-port"
    command = [
        sys.executable,
        str(FLASH_PY),
        "-c",
        str(FLASH_TOML),
        "-p",
        str(missing_port),
    ]

    if upload_kind == "firmware-image":
        image = tmp_path / "firmware.bin"
        image.write_bytes(b"firmware image")
        command += ["upload", "--image", str(image)]
    else:
        inputs = tmp_path / "config"
        inputs.mkdir()
        (inputs / "device_config.json").write_text("{}\n")
        command += ["upload-fs", str(inputs)]

    with pytest.raises(subprocess.CalledProcessError) as error:
        subprocess.run(
            command,
            check=True,
            capture_output=True,
            text=True,
            cwd=ROOT,
        )

    assert error.value.returncode != 0
    output = (error.value.stdout or "") + (error.value.stderr or "")
    assert "A fatal esptool.py error occurred:" in output
