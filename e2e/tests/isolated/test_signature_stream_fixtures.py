import ast
import hashlib
import json
import math
import shutil
import subprocess
import tempfile
from pathlib import Path

from lib.signature_stream import decode_stream_report


E2E_ROOT = Path(__file__).resolve().parents[2]
FIXTURES_ROOT = E2E_ROOT / "signing_fixtures"
VECTOR_PATH = FIXTURES_ROOT / "host" / "vectors.json"


def _der_integer(value: bytes) -> bytes:
    value = value.lstrip(b"\0") or b"\0"
    if value[0] & 0x80:
        value = b"\0" + value
    return b"\x02" + bytes([len(value)]) + value


def _der_raw_signature(signature: bytes) -> bytes:
    body = _der_integer(signature[:32]) + _der_integer(signature[32:])
    return b"\x30" + bytes([len(body)]) + body


def _verify_raw_signature(
    public_key_path: Path, directory: Path, name: str, data: bytes, signature: bytes
) -> bool:
    data_path = directory / f"{name}.bin"
    signature_path = directory / f"{name}.der"
    data_path.write_bytes(data)
    signature_path.write_bytes(_der_raw_signature(signature))
    result = subprocess.run(
        [
            "openssl",
            "dgst",
            "-sha256",
            "-verify",
            str(public_key_path),
            "-signature",
            str(signature_path),
            str(data_path),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.returncode == 0


def test_signature_action_template_publishes_three_progress_fields() -> None:
    config = json.loads(
        (E2E_ROOT / "configs" / "signature_verify" / "device_config.json").read_text()
    )
    action = next(
        action
        for action in config["actions"]
        if action.get("interface") == "sv1"
    )

    assert action["topic"] == "home/testDevice/sv1/state"
    assert json.loads(action["template"]) == ["%1", "%2", "%3"]
    assert action["retain"] is False
    assert action["minimumSendInterval"] == 0


def test_fixture_vectors_have_expected_hashes_and_signatures() -> None:
    document = json.loads(VECTOR_PATH.read_text())
    with tempfile.TemporaryDirectory(prefix="signature_vectors_") as temp_dir:
        directory = Path(temp_dir)
        main_public_key_path = directory / "main_public.pem"
        subprocess.run(
            [
                "openssl",
                "x509",
                "-in",
                str(FIXTURES_ROOT / "device" / "signing_test.pem"),
                "-pubkey",
                "-noout",
                "-out",
                str(main_public_key_path),
            ],
            check=True,
            capture_output=True,
        )
        for name, vector in document["vectors"].items():
            data = bytes.fromhex(vector["data_hex"])
            signature = bytes.fromhex(vector["signature_hex"])
            assert vector["sha256"] == hashlib.sha256(data).hexdigest(), name
            assert len(signature) == vector["signature_len"] == 64, name
            should_verify = document["expected_results"][name] == "OK"
            assert (
                _verify_raw_signature(
                    main_public_key_path, directory, name, data, signature
                )
                is should_verify
            ), name

        wrong_public_key_path = directory / "wrong_public.pem"
        subprocess.run(
            [
                "openssl",
                "pkey",
                "-in",
                str(FIXTURES_ROOT / "host" / "signing_test_wrong_private_key.pem"),
                "-pubout",
                "-out",
                str(wrong_public_key_path),
            ],
            check=True,
            capture_output=True,
        )
        wrong_vector = document["vectors"]["binary_wrong_key"]
        assert _verify_raw_signature(
            wrong_public_key_path,
            directory,
            "binary_wrong_key_actual",
            bytes.fromhex(wrong_vector["data_hex"]),
            bytes.fromhex(wrong_vector["signature_hex"]),
        )


def test_large_fixture_has_expected_payload_hash() -> None:
    vectors = json.loads(VECTOR_PATH.read_text())
    vector = vectors["vectors"]["binary_stream65536"]
    data = bytes.fromhex(vector["data_hex"])

    assert data == bytes((index * 37 + 11) % 256 for index in range(65536))
    assert vector["sha256"] == hashlib.sha256(data).hexdigest()
    assert vector["signature_len"] == len(bytes.fromhex(vector["signature_hex"])) == 64
    assert math.ceil(len(data) / 256) == 256


def test_stream_decoder_ignores_startup_report_without_parameters() -> None:
    assert decode_stream_report("") is None
    assert decode_stream_report('"startup"') is None
    assert decode_stream_report('[""]') is None
    assert decode_stream_report('["", "", ""]') is None


def test_private_keys_never_enter_provisioning_directories() -> None:
    source_path = E2E_ROOT / "tests" / "signature_verify" / "test_signature_verify.py"
    source = ast.parse(source_path.read_text())
    function = next(
        node
        for node in source.body
        if isinstance(node, ast.FunctionDef) and node.name == "_make_temp_config"
    )
    namespace = {
        "shutil": shutil,
        "tempfile": tempfile,
        "Path": Path,
        "DEVICE_CONFIG_PATH": E2E_ROOT
        / "configs"
        / "signature_verify"
        / "device_config.json",
        "DEVICE_CERTS_DIR": FIXTURES_ROOT / "device",
        "DEVICE_CERT_ALIAS": "signing_test.pem",
    }
    exec(
        compile(ast.Module(body=[function], type_ignores=[]), str(source_path), "exec"),
        namespace,
    )

    for certificate_name in ("signing_test.pem", None):
        directory = namespace["_make_temp_config"](certificate_name)
        try:
            uploaded = {path.name for path in directory.iterdir()}
            assert "device_config.json" in uploaded
            assert "signing_test_private_key.pem" not in uploaded
            assert "signing_test_wrong_private_key.pem" not in uploaded
            assert uploaded == (
                {"device_config.json", "signing_test.pem"}
                if certificate_name is not None
                else {"device_config.json"}
            )
        finally:
            shutil.rmtree(directory, ignore_errors=True)
