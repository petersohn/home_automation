# E2E Tests

End-to-end tests for the ESP8266 firmware, running on real hardware
(Raspberry Pi + ESP-12F).

See `docs/superpowers/specs/2026-08-04-e2e-tests-design.md` for full design.

## Running

    cd e2e
    python3 -m pytest tests/basic/test_gpio_input.py -v

## Hardware setup

See spec for RPi wiring, OS config, and software install instructions.