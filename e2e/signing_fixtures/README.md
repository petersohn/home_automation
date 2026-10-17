# Signing fixtures (TEST ONLY)

Dedicated test material for `signatureVerify` hardware acceptance tests. These
keys and certificates are not trusted for production. The private keys live
under `host/` and must never be uploaded to a device or included in SPIFFS.

## Layout

| Path | SPIFFS? | Purpose |
| --- | --- | --- |
| `host/` | **NEVER** | Private signing keys and signed test vectors |
| `device/` | Certificate fixtures only | Test trust anchors and rejected PEM variants |

`host/signing_test_private_key.pem` matches `device/signing_test.pem` and signs
valid vectors. `host/signing_test_wrong_private_key.pem` signs the wrong-key
failure vector. `host/vectors.json` stores payload bytes and raw64 signatures.
Only `device/` certificate files are copied into the temporary SPIFFS image by
the hardware tests.

Device fixtures include:

- `signing_test.pem`: self-signed P-256 certificate accepted by the tests.
- `signing_test_whitespace.pem`: same certificate surrounded by ASCII whitespace.
- `signing_test_wrong_key.pem`: unrelated P-256 certificate.
- `signing_test_unsupported_curve.pem`: P-384 key; rejected as unsupported.
- `signing_test_unsupported_algorithm.pem`: RSA key; rejected as unsupported.
- `signing_test_der.der`: valid certificate in DER form; rejected.
- `signing_test_trailing_junk.pem`: valid PEM followed by non-whitespace bytes.
- `signing_test_other_pem_block.pem`: valid certificate followed by a PUBLIC KEY block.
- `signing_test_concatenated.pem`: two certificates in one file.
- `signing_test_malformed.pem`: malformed certificate text.
- `signing_test_oversized.pem`: valid certificate padded beyond 4096 bytes.

## Raw64 signature format

The payload is signed with ECDSA P-256 and SHA-256. `cryptography` returns a
DER `ECDSA-Sig-Value`; the generator calls `decode_dss_signature`, then writes
`r.to_bytes(32, "big") + s.to_bytes(32, "big")`. It verifies vectors by
converting raw64 back to DER with `encode_dss_signature` and checking them with
the generated public key. ECDSA signatures are randomized, so regeneration may
produce different signature bytes while preserving the same expected results.

## Regeneration

Install the pinned host-only dependency and regenerate checked-in fixtures:

```sh
python3 -m pip install -r e2e/signing_fixtures/requirements.txt
python3 e2e/signing_fixtures/generate_fixtures.py
```

To check regeneration in a disposable directory without modifying fixtures:

```sh
python3 e2e/signing_fixtures/generate_fixtures.py \
  --output-dir /tmp/signing-fixtures
```

Generator emits private keys only beneath `host/`; public certificates and
derived strict-PEM regression fixtures are regenerated together beneath
`device/`. Hardware tests and CI consume checked-in fixtures and do not require
`cryptography` installed. Never upload the `host/` directory.
