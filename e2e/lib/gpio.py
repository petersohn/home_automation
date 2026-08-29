"""RPi.GPIO wrapper for driving ESP GPIO pins and reading PWM."""

import time
from typing import Literal, cast

import RPi.GPIO as GPIO_lib

from . import pin_map

PinValue = Literal[0, 1]


class Gpio:
    def __init__(self) -> None:
        GPIO_lib.setmode(GPIO_lib.BCM)
        GPIO_lib.setwarnings(False)

    def setup_input(self, pin: int) -> None:
        """Set RPi pin as input. pin is an ESP GPIO number."""
        GPIO_lib.setup(pin_map.ESP_TO_RPI[pin], GPIO_lib.IN)

    def setup_output(self, pin: int, value: PinValue = 0) -> None:
        """Set RPi pin as output. pin is ESP GPIO number. value 0 or 1."""
        GPIO_lib.setup(pin_map.ESP_TO_RPI[pin], GPIO_lib.OUT, initial=value)

    def write(self, pin: int, value: PinValue) -> None:
        """Drive ESP input pin high/low via RPi output. value 0 or 1."""
        GPIO_lib.output(pin_map.ESP_TO_RPI[pin], value)

    def read(self, pin: int) -> PinValue:
        """Read ESP output pin via RPi input. Returns 0 or 1."""
        return cast(PinValue, GPIO_lib.input(pin_map.ESP_TO_RPI[pin]))

    def read_pwm(self, pin: int, sample_ms: int = 10) -> float:
        """Sample pin rapidly for sample_ms, return duty cycle ratio 0.0-1.0."""
        rpi_pin = pin_map.ESP_TO_RPI[pin]
        high = 0
        total = 0
        end = time.time() + sample_ms / 1000.0
        while time.time() < end:
            if GPIO_lib.input(rpi_pin):
                high += 1
            total += 1
        return high / total if total else 0.0

    def set_boot_safe_state(self) -> None:
        """Set boot strapping pins to input so ESP internal pulls work."""
        for bcm in pin_map.BOOT_PINS:
            GPIO_lib.setup(bcm, GPIO_lib.IN)

    def cleanup(self) -> None:
        GPIO_lib.cleanup()