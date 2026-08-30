"""ESP GPIO test API: ESP-numbered pins translated to RPi BCM pins."""

import time
from typing import cast

from .rpi_gpio import RpiGpio
from .pin_map import ESP_TO_RPI, BOOT_PINS, PinValue


class Gpio:
    """Test-side API for driving/reading ESP GPIO pins by ESP number."""

    def __init__(self, rpi: RpiGpio) -> None:
        self._rpi = rpi

    def setup_input(self, pin: int) -> None:
        """Set RPi pin as input. pin is an ESP GPIO number."""
        self._rpi.setup_input(ESP_TO_RPI[pin])

    def setup_output(self, pin: int, value: PinValue = 0) -> None:
        """Set RPi pin as output. pin is an ESP GPIO number. value 0 or 1."""
        self._rpi.setup_output(ESP_TO_RPI[pin], value)

    def write(self, pin: int, value: PinValue) -> None:
        """Drive ESP input pin high/low via RPi output. value 0 or 1."""
        self._rpi.write(ESP_TO_RPI[pin], value)

    def read(self, pin: int) -> PinValue:
        """Read ESP output pin via RPi input. Returns 0 or 1."""
        return cast(PinValue, self._rpi.read(ESP_TO_RPI[pin]))

    def read_pwm(self, pin: int, sample_ms: int = 10) -> float:
        """Sample pin rapidly for sample_ms, return duty cycle ratio 0.0-1.0."""
        rpi_pin = ESP_TO_RPI[pin]
        high = 0
        total = 0
        end = time.time() + sample_ms / 1000.0
        while time.time() < end:
            if self._rpi.read(rpi_pin):
                high += 1
            total += 1
        return high / total if total else 0.0

    def set_boot_safe_state(self) -> None:
        """Set boot strapping pins to input so ESP internal pulls work."""
        for pin, value in BOOT_PINS:
            if value is None:
                self.setup_input(pin)
            else:
                self.setup_output(pin, value)

    def cleanup(self) -> None:
        self._rpi.cleanup()