# Signing fixtures (TEST ONLY)

Dedicated test material for the `signatureVerify` interface e2e tests. The
private keys here are purpose-generated public fixtures; they sign nothing
but the checked-in test vectors and must never be trusted in production or
uploaded to any device.

## Layout

| Path | Uploaded to SPIFFS? | Contents |
| --- | --- | --- |
| `host/` | **NEVER** | Private keys + signature vectors |
| `device/` | yes (certificates only) | PEM certificates for provisioning |

- `host/signing_test_private_key.pem` — P-256 key matching
  `device/signing_test.pem`. Signs the valid vectors.
- `host/signing_test_wrong_private_key.pem` — second, independent P-256 key
  matching `device/signing_test_wrong_key.pem`. Signs the wrong-key FAIL
  vector.
- `host/vectors.json` — deterministic payloads with independently generated
  raw64 (r\|\|s) signatures, plus expected results and provenance.
- `device/signing_test.pem` — self-signed P-256 cert; the "valid
  certificate" provisioned by the e2e config (`/signing_test.pem`).
- `device/signing_test_whitespace.pem` — the valid cert surrounded by ASCII
  whitespace; accepted as a regression case.
- `device/signing_test_wrong_key.pem` — cert of the second key.
- `device/signing_test_unsupported_curve.pem` — P-384 cert (unsupported
  curve → device must report ERROR).
- `device/signing_test_unsupported_algorithm.pem` — RSA-2048 cert
  (unsupported key type → ERROR).
- `device/signing_test_concatenated.pem` — two certificates in one file
  (ambiguous multiple-certificate input → ERROR).
- `device/signing_test_der.der` — DER-encoded copy of the valid cert (DER input
  → ERROR).
- `device/signing_test_trailing_junk.pem` — valid PEM followed by non-whitespace
  junk (→ ERROR).
- `device/signing_test_other_pem_block.pem` — valid cert followed by a PUBLIC
  KEY block (→ ERROR).
- `device/signing_test_malformed.pem` — junk bytes (invalid certificate →
  ERROR).

## Raw64 signature format

ECDSA P-256 signatures are 64 bytes: big-endian `r` zero-padded to 32 bytes,
then big-endian `s` zero-padded to 32 bytes. `openssl dgst -sha256 -sign`
emits DER (`ECDSA-Sig-Value`); `generate_fixtures.py` converts DER to raw64
with a minimal stdlib parser and proves the conversion by round-tripping
back to DER and re-verifying with openssl.

## Provenance / regeneration

Generated locally with OpenSSL 3.5.8 + the stdlib-only converter in
`generate_fixtures.py` (version recorded in `host/vectors.json` under
`meta.host_tooling`). Regenerate with:

    python3 e2e/signing_fixtures/generate_fixtures.py

No `cryptography` pip module is needed. Keys never leave this directory and
are never used outside these tests.
