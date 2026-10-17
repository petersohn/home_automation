#!/usr/bin/env python3
"""Generate the test-only signing fixtures for the signatureVerify e2e tests.

Everything produced by this script is public, dedicated TEST MATERIAL:
    - The private keys are purpose-generated fixture keys. They sign nothing
      but e2e test vectors. They must NEVER be uploaded to a device or used
      in production; only files under device/ may be provisioned into a
      SPIFFS image, and those are certificates, not keys.
    - The certificates are self-signed; trust comes purely from provisioning
      in the e2e config, never from a CA.

Host tooling: openssl (EC/RSA key generation, X.509 certificates,
ECDSA-SHA256 signatures) plus a minimal DER-to-raw conversion implemented
with the Python standard library only (no cryptography module dependency).

Raw64 signature format (as consumed by the device firmware):
    ECDSA P-256 signatures are stored as exactly 64 bytes:
    big-endian r padded to 32 bytes, followed by big-endian s padded to 32
    bytes, with leading zeros. openssl outputs DER
    (SEQUENCE { INTEGER r, INTEGER s }); this script converts DER to the raw
    form, then reconstructs DER for round-trip verification to prove the
    conversion is correct.

Outputs (relative to this directory):
    host/signing_test_private_key.pem          P-256 signing key (NEVER upload)
    host/signing_test_wrong_private_key.pem    second P-256 key (NEVER upload)
    host/vectors.json                          data / raw64 signature vectors
    device/signing_test.pem                    self-signed cert for the main key
    device/signing_test_wrong_key.pem          cert for the second key
    device/signing_test_unsupported_curve.pem  P-384 cert (unsupported curve)
    device/signing_test_unsupported_algorithm.pem RSA cert (unsupported key)
    device/signing_test_concatenated.pem       two certs in one file (rejected)
    device/signing_test_malformed.pem          garbage (rejected)

Run from the repository root or this directory:
    python3 e2e/signing_fixtures/generate_fixtures.py
"""

from __future__ import annotations

import datetime
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
HOST_DIR = HERE / "host"
DEVICE_DIR = HERE / "device"

MAIN_KEY = HOST_DIR / "signing_test_private_key.pem"
WRONG_KEY = HOST_DIR / "signing_test_wrong_private_key.pem"

CERT_VALID = DEVICE_DIR / "signing_test.pem"
CERT_WRONG_KEY = DEVICE_DIR / "signing_test_wrong_key.pem"
CERT_UNSUPPORTED_CURVE = DEVICE_DIR / "signing_test_unsupported_curve.pem"
CERT_UNSUPPORTED_ALGORITHM = DEVICE_DIR / "signing_test_unsupported_algorithm.pem"
CERT_CONCATENATED = DEVICE_DIR / "signing_test_concatenated.pem"
CERT_MALFORMED = DEVICE_DIR / "signing_test_malformed.pem"

VECTORS_PATH = HOST_DIR / "vectors.json"

# Deterministic payload vectors.
PAYLOAD_EMPTY = b""
PAYLOAD_BINARY = b"\x00\x42\xfe\xff\x80\x7f\x01\x10\x00"
PAYLOAD_MAX256 = bytes((i * 37 + 11) % 256 for i in range(256))
# Changed-data case: one byte flipped -> same signature no longer verifies.
PAYLOAD_ALTERED = bytes(PAYLOAD_BINARY[:1]) + bytes([PAYLOAD_BINARY[1] ^ 0x01]) + bytes(
    PAYLOAD_BINARY[2:]
)


def run(cmd: list[str]) -> bytes:
    print("+", " ".join(str(c) for c in cmd))
    result = subprocess.run(
        [str(c) for c in cmd], capture_output=True, check=False
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode(errors="replace"))
        raise SystemExit(f"command failed (rc={result.returncode}): {cmd}")
    return result.stdout


def openssl(*args: str) -> bytes:
    return run(["openssl", *args])


# ---------------------------------------------------------------------------
# DER <-> raw signature conversion
# ---------------------------------------------------------------------------


def _read_len(data: bytes, pos: int) -> tuple[int, int]:
    first = data[pos]
    if first < 0x80:
        return first, pos + 1
    count = first & 0x7F
    if count == 0 or pos + 1 + count > len(data):
        raise ValueError("bad DER length")
    value = int.from_bytes(data[pos + 1 : pos + 1 + count], "big")
    return value, pos + 1 + count


def _read_integer(data: bytes, pos: int) -> tuple[bytes, int]:
    if data[pos] != 0x02:
        raise ValueError("expected INTEGER tag")
    length, pos = _read_len(data, pos + 1)
    content = data[pos : pos + length]
    if pos + length > len(data):
        raise ValueError("truncated INTEGER")
    # DER integers are positive: a leading 0x00 byte is padding.
    stripped = content.lstrip(b"\x00")
    return stripped, pos + length


def der_signature_to_raw(der: bytes) -> bytes:
    """DER ECDSA-Sig-Value -> raw r||s, each 32 bytes big-endian."""
    if len(der) < 2 or der[0] != 0x30:
        raise ValueError("not a DER SEQUENCE")
    seq_len, pos = _read_len(der, 1)
    if seq_len != len(der) - pos:
        raise ValueError("trailing bytes after SEQUENCE")
    r_bytes, pos = _read_integer(der, pos)
    s_bytes, pos = _read_integer(der, pos)
    if pos != len(der):
        raise ValueError("extra content after INTEGERs")
    if len(r_bytes) > 32 or len(s_bytes) > 32:
        raise ValueError("integer too large for P-256")
    return r_bytes.rjust(32, b"\x00") + s_bytes.rjust(32, b"\x00")


def _len_der(length: int) -> bytes:
    if length < 0x80:
        return bytes([length])
    encoded = length.to_bytes((length.bit_length() + 7) // 8, "big")
    return bytes([0x80 | len(encoded)]) + encoded


def _integer_der(value: int) -> bytes:
    content = value.to_bytes((max(value.bit_length(), 1) + 7) // 8, "big")
    if content[0] & 0x80 or value == 0:
        content = b"\x00" + content
    return b"\x02" + _len_der(len(content)) + content


def raw_signature_to_der(raw: bytes) -> bytes:
    """Raw r||s (64 bytes) -> DER ECDSA-Sig-Value, for round-trip checks."""
    if len(raw) != 64:
        raise ValueError("raw signature must be 64 bytes")
    r = int.from_bytes(raw[:32], "big")
    s = int.from_bytes(raw[32:], "big")
    content = _integer_der(r) + _integer_der(s)
    return b"\x30" + _len_der(len(content)) + content


# ---------------------------------------------------------------------------
# Fixture material
# ---------------------------------------------------------------------------


def generate_keys_and_certs() -> None:
    HOST_DIR.mkdir(parents=True, exist_ok=True)
    DEVICE_DIR.mkdir(parents=True, exist_ok=True)

    openssl(
        "ecparam", "-name", "prime256v1", "-genkey", "-noout",
        "-out", MAIN_KEY,
    )
    openssl(
        "ecparam", "-name", "prime256v1", "-genkey", "-noout",
        "-out", WRONG_KEY,
    )

    subject = "/O=home-automation-e2e/CN=TEST-ONLY signing fixture key"
    for key, out in [
        (MAIN_KEY, CERT_VALID),
        (WRONG_KEY, CERT_WRONG_KEY),
    ]:
        run([
            "openssl", "req", "-new", "-x509",
            "-key", key,
            "-sha256", "-days", "36500",
            "-subj", subject,
            "-addext", "basicConstraints=critical,CA:FALSE",
            "-out", out,
        ])

    # Unsupported-key fixtures: correct self-signed structure, wrong key type.
    p384_key = HERE / ".gen_p384_key.pem"
    rsa_key = HERE / ".gen_rsa_key.pem"
    openssl("ecparam", "-name", "secp384r1", "-genkey", "-noout", "-out", p384_key)
    openssl("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048",
            "-out", rsa_key)
    for key, out in [(p384_key, CERT_UNSUPPORTED_CURVE),
                     (rsa_key, CERT_UNSUPPORTED_ALGORITHM)]:
        openssl("req", "-new", "-x509", "-key", key, "-sha256", "-days", "1",
                "-subj", subject, "-out", out)
    p384_key.unlink()
    rsa_key.unlink()

    CERT_CONCATENATED.write_bytes(
        CERT_VALID.read_bytes() + CERT_WRONG_KEY.read_bytes()
    )
    CERT_MALFORMED.write_bytes(
        b"-----BEGIN CERTIFICATE-----\n"
        b"VGhpcyBmaWxlIGlzIG5vdCBhIGNlcnRpZmljYXRlIQ==\n"
        b"-----END CERTIFICATE-----\n"
        b"trailing garbage bytes\n"
    )


def extract_pubkey(cert: Path) -> bytes:
    return openssl("x509", "-in", cert, "-pubkey", "-noout")


def sign(key: Path, data: bytes) -> bytes:
    tmp = HERE / ".gen_sig.der"
    self_dir_data = HERE / ".gen_data.bin"
    self_dir_data.write_bytes(data)
    openssl(
        "dgst", "-sha256", "-sign", key,
        "-out", tmp, self_dir_data,
    )
    der = tmp.read_bytes()
    tmp.unlink()
    self_dir_data.unlink()
    return der_signature_to_raw(der)


def verify(cert: Path, data: bytes, raw: bytes, must_pass: bool) -> None:
    """Round-trip check: raw -> DER -> openssl verify against cert pubkey."""
    pub = CERT_VALID if must_pass else cert
    check_data = HERE / ".gen_verify_data.bin"
    check_sig = HERE / ".gen_verify_sig.der"
    check_pub = HERE / ".gen_verify_pub.pem"
    check_data.write_bytes(data)
    check_sig.write_bytes(raw_signature_to_der(raw))
    openssl("x509", "-in", pub, "-pubkey", "-noout", "-out", check_pub)
    result = subprocess.run(
        [
            "openssl", "dgst", "-sha256",
            "-verify", str(check_pub),
            "-signature", str(check_sig), str(check_data),
        ],
        capture_output=True,
        check=False,
    )
    check_data.unlink()
    check_sig.unlink()
    check_pub.unlink()
    verified = (
        result.returncode == 0 and b"Verified OK" in result.stdout
    )
    if must_pass != verified:
        raise SystemExit(
            f"unexpected verification result for cert={pub.name}: "
            f"rc={result.returncode}, expected pass={must_pass}"
        )


def openssl_version() -> str:
    return openssl("version", "-a").decode().splitlines()[0].strip()


def generate_vectors() -> None:
    main_pub = extract_pubkey(CERT_VALID)

    vectors = {
        "empty": (PAYLOAD_EMPTY, sign(MAIN_KEY, PAYLOAD_EMPTY)),
        "binary": (PAYLOAD_BINARY, sign(MAIN_KEY, PAYLOAD_BINARY)),
        "binary_max256": (PAYLOAD_MAX256, sign(MAIN_KEY, PAYLOAD_MAX256)),
        # Altered data paired with the ORIGINAL binary signature: the device
        # must answer FAIL, so this raw signature must NOT match on the host.
        "binary_altered_data": (
            PAYLOAD_ALTERED,
            sign(MAIN_KEY, PAYLOAD_BINARY),
        ),
        # Same curve, different key: must verify FAIL against the pinned cert.
        "binary_wrong_key": (
            PAYLOAD_BINARY,
            sign(WRONG_KEY, PAYLOAD_BINARY),
        ),
    }

    # All vectors that must verify are checked here against the main key's
    # own key (round trip through DER proves the raw conversion). The FAIL
    # vectors are checked against the pinned cert and must NOT verify.
    verify(CERT_VALID, PAYLOAD_EMPTY, vectors["empty"][1], must_pass=True)
    verify(CERT_VALID, PAYLOAD_BINARY, vectors["binary"][1], must_pass=True)
    verify(CERT_VALID, PAYLOAD_MAX256, vectors["binary_max256"][1], must_pass=True)
    verify(CERT_VALID, PAYLOAD_ALTERED, vectors["binary_altered_data"][1],
           must_pass=False)
    verify(CERT_VALID, PAYLOAD_BINARY, vectors["binary_wrong_key"][1],
           must_pass=False)

    document = {
        "meta": {
            "purpose": "Test-only signature vectors for signatureVerify e2e "
                       "tests. Keys are dedicated public fixtures, never "
                       "trusted in production; never upload the key files.",
            "generated": datetime.datetime.now(datetime.timezone.utc)
            .strftime("%Y-%m-%d"),
            "generator": "e2e/signing_fixtures/generate_fixtures.py",
            "host_tooling": openssl_version(),
            "curve": "NIST P-256 (secp256r1)",
            "hash": "SHA-256",
            "signature_format": "raw64: big-endian r padded to 32 bytes, "
                                "big-endian s padded to 32 bytes "
                                "(DER -> raw conversion documented in "
                                "generate_fixtures.py)",
            "valid_key_cert": "device/signing_test.pem",
            "valid_key": "host/signing_test_private_key.pem",
            "wrong_key": "host/signing_test_wrong_private_key.pem",
        },
        "expected_results": {
            "empty": "OK",
            "binary": "OK",
            "binary_max256": "OK",
            "binary_altered_data": "FAIL",
            "binary_wrong_key": "FAIL",
        },
        "vectors": {
            name: {
                "data_hex": data.hex(),
                "signature_hex": raw.hex(),
                "signature_len": len(raw),
            }
            for name, (data, raw) in vectors.items()
        },
    }
    VECTORS_PATH.write_text(json.dumps(document, indent=2) + "\n")
    print(f"wrote {VECTORS_PATH.relative_to(HERE)}")
    assert main_pub.startswith(b"-----BEGIN PUBLIC KEY-----")


def main() -> None:
    os.chdir(HERE)
    generate_keys_and_certs()
    generate_vectors()
    print("fixtures regenerated; remember: host/ files must never be uploaded")


if __name__ == "__main__":
    main()
