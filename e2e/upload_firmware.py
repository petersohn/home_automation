"""CI helper: upload pre-built firmware to ESP via Device (handles flash mode)."""

import sys
from pathlib import Path

from lib.device import Device
from lib.gpio import Gpio
from lib.pin_map import SERIAL_PORT, RESET_PIN


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <firmware.bin>", file=sys.stderr)
        return 1
    firmware = Path(sys.argv[1])
    gpio = Gpio()
    device = Device(serial_port=SERIAL_PORT, reset_pin=RESET_PIN, gpio=gpio)
    code = device.upload_firmware(firmware)
    gpio.cleanup()
    return code


if __name__ == "__main__":
    sys.exit(main())