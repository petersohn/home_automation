"""CI helper: upload pre-built firmware to ESP via Device (handles flash mode)."""

import subprocess
import sys
from pathlib import Path

from lib.device import Device
from lib.gpio import Gpio
from lib.rpi_gpio import RpiGpio
from lib.pin_map import SERIAL_PORT, RESET_PIN, ENABLE_PIN


def _diagnose_gpio() -> None:
    """Print RPi GPIO/SPI/I2C/serial status for debugging flash mode failures."""
    print("=== GPIO diagnostics ===", flush=True)
    # Check if SPI/I2C are loaded (would claim BCM4/BCM3 etc)
    lsmod = subprocess.run(["lsmod"], capture_output=True, text=True).stdout
    for mod in ("spi_bcm2835", "i2c_bcm2835", "spidev", "i2c_dev"):
        print(f"  kernel module {mod}: {'LOADED' if mod in lsmod else 'not loaded'}")
    # Check serial port
    import os
    serial_port = SERIAL_PORT
    print(f"  serial port {serial_port}: exists={os.path.exists(serial_port)}")
    # Try opening serial port briefly
    try:
        import serial
        s = serial.Serial(serial_port, 115200, timeout=0.1)
        print(f"  serial port {serial_port}: opened OK, is_open={s.is_open}")
        s.close()
    except Exception as e:
        print(f"  serial port {serial_port}: ERROR {e}")
    # Read back GPIO0 (BCM4) and RST (BCM23) state after setup
    rpi = RpiGpio()
    for name, bcm in [("GPIO0/BCM4", 4), ("RST/BCM23", 23)]:
        rpi.setup_input(bcm)
        val = rpi.read(bcm)
        print(f"  {name} read={val} (input/pull)")
    # Test: set GPIO0 low, RST high, read back
    rpi.setup_output(4, 0)
    rpi.setup_output(23, 1)
    print(f"  BCM4 set low, read={rpi.read(4)}")
    print(f"  BCM23 set high, read={rpi.read(23)}")
    # Pulse RST and check
    rpi.write(23, 0)
    import time
    time.sleep(0.05)
    rpi.write(23, 1)
    time.sleep(0.2)
    print(f"  after RST pulse, BCM23 read={rpi.read(23)}")
    rpi.setup_input(4)
    rpi.setup_input(23)
    print("=== end diagnostics ===", flush=True)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <firmware.bin>", file=sys.stderr)
        return 1
    firmware = Path(sys.argv[1])
    rpi = RpiGpio()
    gpio = Gpio(rpi)
    _diagnose_gpio()
    device = Device(
        serial_port=SERIAL_PORT,
        reset_pin=RESET_PIN,
        enable_pin=ENABLE_PIN,
        gpio=gpio,
        rpi=rpi,
    )
    code = device.upload_firmware(firmware)
    gpio.cleanup()
    return code


if __name__ == "__main__":
    sys.exit(main())