"""ESP GPIO pin -> RPi BCM pin mapping. Optimized for physical alignment."""

from typing import Literal

PinValue = Literal[0, 1]

ESP_TO_RPI = {
    0: 4,    # ESP GPIO0  -> RPi BCM4  (pin 7)   boot strapping, direct
    2: 3,    # ESP GPIO2  -> RPi BCM3  (pin 5)   boot strapping, direct
    4: 17,   # ESP GPIO4  -> RPi BCM17 (pin 11)  input i2, same side, dist 1
    5: 27,   # ESP GPIO5  -> RPi BCM27 (pin 13)  output o1, same side, dist 1
    12: 24,  # ESP GPIO12 -> RPi BCM24 (pin 18)  pwm p1, same side, dist 6
    13: 25,  # ESP GPIO13 -> RPi BCM25 (pin 22)  pwm p2 (inverted), same side, dist 8
    14: 22,  # ESP GPIO14 -> RPi BCM22 (pin 15)  input i1, cross, dist 4 (insulated)
    15: 2,   # ESP GPIO15 -> RPi BCM2  (pin 3)   boot strapping, direct
    16: 18,  # ESP GPIO16 -> RPi BCM18 (pin 12)  output o2 (inverted), same side, dist 1
}

RESET_PIN = 23  # RPi BCM23 (pin 16) -> ESP RST, direct

SERIAL_PORT = "/dev/serial0"
SERIAL_BAUD = 115200

# RPi BCM pins for ESP boot strapping (set to input at boot)
BOOT_PINS: list[tuple[int, PinValue]] = [
    (0, 1),   # BCM4  - GPIO0, must be HIGH
    (2, 1),  # BCM3  - GPIO2, must be HIGH
    (15, 0), # BCM2  - GPIO15, must be LOW
]
