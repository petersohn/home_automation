"""CI helper: upload pre-built firmware to ESP via Device (handles flash mode)."""

import subprocess
import sys
from pathlib import Path

from lib.device import Device
from lib.gpio import Gpio
from lib.pin_map import SERIAL_PORT, RESET_PIN, ESP_TO_RPI

import RPi.GPIO as GPIO_lib


def _diagnose_gpio() -> None:
    """Print RPi GPIO/SPI/I2C status for debugging flash mode failures."""
    print("=== GPIO diagnostics ===")
    # Check if SPI/I2C are loaded (would claim BCM4/BCM3 etc)
    for mod in ("spi_bcm2835", "i2c_bcm2835", "spidev", "i2c_dev"):
        r = subprocess.run(
            ["lsmod"], capture_output=True, text=True
        )
        loaded = mod in r.stdout
        print(f"  kernel module {mod}: {'LOADED' if loaded else 'not loaded'}")
    # Read back GPIO0 (BCM4) and RST (BCM23) state after setup
    for name, bcm in [("GPIO0/BCM4", 4), ("RST/BCM23", 23)]:
        GPIO_lib.setup(bcm, GPIO_lib.IN)
        val = GPIO_lib.input(bcm)
        print(f"  {name} read={val} (input/pull)")
    print("=== end diagnostics ===", flush=True)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <firmware.bin>", file=sys.stderr)
        return 1
    firmware = Path(sys.argv[1])
    gpio = Gpio()
    _diagnose_gpio()
    device = Device(serial_port=SERIAL_PORT, reset_pin=RESET_PIN, gpio=gpio)
    code = device.upload_firmware(firmware)
    gpio.cleanup()
    return code


if __name__ == "__main__":
    sys.exit(main())