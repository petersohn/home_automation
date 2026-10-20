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

## Stream protocol acceptance

Hardware tests exercise the test-only streamed verifier. Commands are compact
JSON objects on `home/testDevice/sv1/set`:

```json
{"type":"begin","hash":"<64 lowercase SHA-256 hex>","size":65536}
{"type":"chunk","hash":"<same hash>","offset":0,"data":"<base64>"}
{"type":"finish","hash":"<same hash>","signature":"<base64 raw64>"}
```

`size` and `offset` count decoded payload bytes, not Base64 or MQTT bytes. A
payload can exceed one chunk; each decoded chunk contains 1–256 bytes and each
command is at most 512 bytes. The hash covers the complete decoded payload.
Finish signature is raw64 ECDSA P-256/SHA-256 (`r || s`), encoded as canonical
padded Base64.

The signature action publishes a JSON string template as its payload. Keep all
three values so each progress event is observable:

```json
{
  "type": "publish",
  "interface": "sv1",
  "topic": "home/testDevice/sv1/state",
  "template": "[\"%1\",\"%2\",\"%3\"]",
  "retain": false,
  "minimumSendInterval": 0
}
```

The decoded report is `[status, decodedBytes, advertisedHash]`; all fields are
strings. `decodedBytes` is decimal. Status is `PROGRESS`, `OK`, `FAIL ...`, or
`ERROR ...`. Startup still emits the old one-value empty report; host decoder
ignores it.

The sender publishes non-retained commands at QoS 1, and the device subscribes
to commands at QoS 1. Device result actions remain QoS 0, so application ACKs
provide progress correlation, not transport-level delivery guarantees. Sender
waits for every active target to acknowledge each frame's matching hash and
byte count before sending next frame. A failed target is dropped; otherwise
missing ACKs retry every 1 second until the 5 second target deadline. If every
target drops, sender stops without finish. Finish is not retried. Device abandons
an inactive transfer after 30 seconds.

Repeated begin with same hash and size reports current progress. A chunk wholly
inside already-consumed byte range is acknowledged without rehashing; receiver
does not retain prior chunk bytes for comparison. Only next contiguous range is
consumed; gaps and overlapping ranges extending beyond progress fail the
transfer. Duplicates do not extend the inactivity deadline. Reconnect discards
active transfer; sender never resumes and must retransmit from a fresh begin.
Protocol tests verify broadcast policy with simulated targets; physical hardware
suite has one device and does not claim multi-device acceptance.

These keys, payloads, vector hashes, and metadata are test-only. Fixture
generation requires `cryptography`; hardware acceptance consumes checked-in
signatures and needs no Python crypto package or firmware install. `sha256` and
expected-result metadata are unsigned descriptive fields; device verification
authenticates payload bytes only, not vector metadata.

## Regeneration

The generator requires the pinned host-only dependency. It creates fresh keys
and certificates when generating a complete fixture tree; do not use its
default output when preserving the checked-in fixture identity. Generate a
disposable tree instead:

```sh
python3 e2e/signing_fixtures/generate_fixtures.py \
  --output-dir /tmp/signing-fixtures
```

Generator emits private keys only beneath `host/`; public certificates and
derived strict-PEM regression fixtures are regenerated together beneath
`device/`. Hardware tests and CI consume checked-in fixtures and do not require
`cryptography` installed. Never upload the `host/` directory.
