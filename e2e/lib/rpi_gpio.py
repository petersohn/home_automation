"""Raw RPi BCM GPIO access. Sole owner of the RPi.GPIO library."""

from typing import cast

import RPi.GPIO as GPIO_lib

from .pin_map import PinValue


class RpiGpio:
    """Hardware access layer: methods take raw RPi BCM pin numbers.

    The only place in the codebase that may import RPi.GPIO.
    """

    def __init__(self) -> None:
        GPIO_lib.setmode(GPIO_lib.BCM)
        GPIO_lib.setwarnings(False)

    def setup_input(self, bcm: int) -> None:
        GPIO_lib.setup(bcm, GPIO_lib.IN)

    def setup_output(self, bcm: int, value: PinValue = 0) -> None:
        GPIO_lib.setup(bcm, GPIO_lib.OUT, initial=value)
        # rpi-lgpio ignores `initial` when the pin is already an output,
        # so always drive the value explicitly.
        GPIO_lib.output(bcm, value)

    def write(self, bcm: int, value: PinValue) -> None:
        GPIO_lib.output(bcm, value)

    def read(self, bcm: int) -> PinValue:
        return cast(PinValue, int(GPIO_lib.input(bcm)))

    def cleanup(self) -> None:
        GPIO_lib.cleanup()