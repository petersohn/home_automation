#!/usr/bin/env python3
"""Generate test-only signing fixtures.

Private keys and vectors stay under host/; only public certificates belong in
device/ or SPIFFS. Requires cryptography==45.0.7. Run from the repo root with
python3 e2e/signing_fixtures/generate_fixtures.py; use --output-dir to avoid
overwriting checked-in fixtures.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path
from typing import Any

from cryptography import __version__ as cryptography_version
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.hazmat.primitives.asymmetric.utils import (
    decode_dss_signature,
    encode_dss_signature,
)
from cryptography.exceptions import InvalidSignature
from cryptography.x509.oid import NameOID

HERE = Path(__file__).resolve().parent
MAX_CERTIFICATE_BYTES = 4096

PAYLOAD_EMPTY = b""
PAYLOAD_BINARY = b"\x00\x42\xfe\xff\x80\x7f\x01\x10\x00"
PAYLOAD_MAX256 = bytes((index * 37 + 11) % 256 for index in range(256))
PAYLOAD_STREAM65536 = bytes((index * 37 + 11) % 256 for index in range(65536))
PAYLOAD_ALTERED = (
    PAYLOAD_BINARY[:1]
    + bytes([PAYLOAD_BINARY[1] ^ 0x01])
    + PAYLOAD_BINARY[2:]
)


def certificate_for(private_key: Any, common_name: str) -> x509.Certificate:
    name = x509.Name(
        [
            x509.NameAttribute(NameOID.ORGANIZATION_NAME, "home-automation-e2e"),
            x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        ]
    )
    now = datetime.datetime.now(datetime.timezone.utc).replace(tzinfo=None)
    return (
        x509.CertificateBuilder()
        .subject_name(name)
        .issuer_name(name)
        .public_key(private_key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1))
        .not_valid_after(now + datetime.timedelta(days=36500))
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        .sign(private_key, hashes.SHA256())
    )


def write_private_key(path: Path, private_key: Any) -> None:
    path.write_bytes(
        private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption(),
        )
    )


def sign_raw64(private_key: ec.EllipticCurvePrivateKey, data: bytes) -> bytes:
    der_signature = private_key.sign(data, ec.ECDSA(hashes.SHA256()))
    r, s = decode_dss_signature(der_signature)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def verify_raw64(
    public_key: ec.EllipticCurvePublicKey,
    data: bytes,
    raw_signature: bytes,
    should_verify: bool,
) -> None:
    if len(raw_signature) != 64:
        raise ValueError("raw signature must be exactly 64 bytes")
    signature = encode_dss_signature(
        int.from_bytes(raw_signature[:32], "big"),
        int.from_bytes(raw_signature[32:], "big"),
    )
    try:
        public_key.verify(signature, data, ec.ECDSA(hashes.SHA256()))
        verified = True
    except InvalidSignature:
        verified = False
    if verified != should_verify:
        raise ValueError(
            f"unexpected vector verification: expected {should_verify}, got {verified}"
        )


def create_fixture_tree(output_dir: Path) -> None:
    host_dir = output_dir / "host"
    device_dir = output_dir / "device"
    host_dir.mkdir(parents=True, exist_ok=True)
    device_dir.mkdir(parents=True, exist_ok=True)

    main_key = ec.generate_private_key(ec.SECP256R1())
    wrong_key = ec.generate_private_key(ec.SECP256R1())
    p384_key = ec.generate_private_key(ec.SECP384R1())
    rsa_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    write_private_key(host_dir / "signing_test_private_key.pem", main_key)
    write_private_key(host_dir / "signing_test_wrong_private_key.pem", wrong_key)

    main_certificate = certificate_for(main_key, "TEST-ONLY signing fixture key")
    wrong_certificate = certificate_for(wrong_key, "TEST-ONLY wrong signing fixture key")
    unsupported_curve_certificate = certificate_for(
        p384_key, "TEST-ONLY unsupported P-384 fixture"
    )
    unsupported_algorithm_certificate = certificate_for(
        rsa_key, "TEST-ONLY unsupported RSA fixture"
    )

    valid_pem = main_certificate.public_bytes(serialization.Encoding.PEM)
    wrong_pem = wrong_certificate.public_bytes(serialization.Encoding.PEM)
    (device_dir / "signing_test.pem").write_bytes(valid_pem)
    (device_dir / "signing_test_wrong_key.pem").write_bytes(wrong_pem)
    (device_dir / "signing_test_unsupported_curve.pem").write_bytes(
        unsupported_curve_certificate.public_bytes(serialization.Encoding.PEM)
    )
    (device_dir / "signing_test_unsupported_algorithm.pem").write_bytes(
        unsupported_algorithm_certificate.public_bytes(serialization.Encoding.PEM)
    )

    # Regenerate derived fixtures to keep their certificate key material aligned.
    (device_dir / "signing_test_whitespace.pem").write_bytes(
        b" \t\r\n \t" + valid_pem + b"\n\t "
    )
    (device_dir / "signing_test_der.der").write_bytes(
        main_certificate.public_bytes(serialization.Encoding.DER)
    )
    (device_dir / "signing_test_trailing_junk.pem").write_bytes(
        valid_pem + b"trailing garbage bytes\n"
    )
    public_key_pem = main_key.public_key().public_bytes(
        serialization.Encoding.PEM,
        serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    (device_dir / "signing_test_other_pem_block.pem").write_bytes(
        valid_pem + public_key_pem
    )
    (device_dir / "signing_test_concatenated.pem").write_bytes(valid_pem + wrong_pem)
    (device_dir / "signing_test_malformed.pem").write_bytes(
        b"-----BEGIN CERTIFICATE-----\n"
        b"VGhpcyBmaWxlIGlzIG5vdCBhIGNlcnRpZmljYXRlIQ==\n"
        b"-----END CERTIFICATE-----\n"
        b"trailing garbage bytes\n"
    )
    if len(valid_pem) >= MAX_CERTIFICATE_BYTES:
        raise ValueError("generated certificate unexpectedly exceeds size limit")
    (device_dir / "signing_test_oversized.pem").write_bytes(
        valid_pem + b" " * (MAX_CERTIFICATE_BYTES + 1 - len(valid_pem))
    )

    vectors = {
        "empty": (PAYLOAD_EMPTY, sign_raw64(main_key, PAYLOAD_EMPTY)),
        "binary": (PAYLOAD_BINARY, sign_raw64(main_key, PAYLOAD_BINARY)),
        "binary_max256": (PAYLOAD_MAX256, sign_raw64(main_key, PAYLOAD_MAX256)),
        "binary_stream65536": (
            PAYLOAD_STREAM65536,
            sign_raw64(main_key, PAYLOAD_STREAM65536),
        ),
        "binary_altered_data": (
            PAYLOAD_ALTERED,
            sign_raw64(main_key, PAYLOAD_BINARY),
        ),
        "binary_wrong_key": (PAYLOAD_BINARY, sign_raw64(wrong_key, PAYLOAD_BINARY)),
    }
    main_public_key = main_key.public_key()
    verify_raw64(main_public_key, PAYLOAD_EMPTY, vectors["empty"][1], True)
    verify_raw64(main_public_key, PAYLOAD_BINARY, vectors["binary"][1], True)
    verify_raw64(main_public_key, PAYLOAD_MAX256, vectors["binary_max256"][1], True)
    verify_raw64(
        main_public_key,
        PAYLOAD_STREAM65536,
        vectors["binary_stream65536"][1],
        True,
    )
    verify_raw64(
        main_public_key, PAYLOAD_ALTERED, vectors["binary_altered_data"][1], False
    )
    verify_raw64(main_public_key, PAYLOAD_BINARY, vectors["binary_wrong_key"][1], False)

    document = {
        "meta": {
            "purpose": "Test-only signature vectors; never trust fixture keys in production.",
            "generated": datetime.datetime.now(datetime.timezone.utc).strftime(
                "%Y-%m-%d"
            ),
            "generator": "e2e/signing_fixtures/generate_fixtures.py",
            "host_tooling": f"cryptography {cryptography_version}",
            "curve": "NIST P-256 (secp256r1)",
            "hash": "SHA-256",
            "signature_format": "raw64: big-endian r and s, each zero-padded to 32 bytes",
            "valid_key_cert": "device/signing_test.pem",
            "valid_key": "host/signing_test_private_key.pem",
            "wrong_key": "host/signing_test_wrong_private_key.pem",
        },
        "expected_results": {
            "empty": "OK",
            "binary": "OK",
            "binary_max256": "OK",
            "binary_stream65536": "OK",
            "binary_altered_data": "FAIL",
            "binary_wrong_key": "FAIL",
        },
        "vectors": {
            name: {
                "data_hex": data.hex(),
                "sha256": hashlib.sha256(data).hexdigest(),
                "signature_hex": signature.hex(),
                "signature_len": len(signature),
            }
            for name, (data, signature) in vectors.items()
        },
    }
    (host_dir / "vectors.json").write_text(json.dumps(document, indent=2) + "\n")
    print(f"fixtures regenerated under {output_dir}")
    print("host/ contains private keys; only device/ certificates belong in SPIFFS")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=HERE,
        help="fixture root containing host/ and device/ (default: this directory)",
    )
    args = parser.parse_args()
    create_fixture_tree(args.output_dir.expanduser().resolve())


if __name__ == "__main__":
    main()
