"""RPi WiFi AP control via systemctl (hostapd + dnsmasq)."""

import subprocess
import time


class WifiAp:
    def up(self) -> None:
        last_result: subprocess.CompletedProcess[str] | None = None
        for _ in range(3):
            last_result = subprocess.run(
                ["sudo", "systemctl", "start", "hostapd", "dnsmasq"],
                capture_output=True,
                text=True,
            )
            if last_result.returncode == 0:
                return
            time.sleep(2)
        assert last_result is not None
        raise subprocess.CalledProcessError(
            last_result.returncode,
            last_result.args,
            last_result.stdout,
            last_result.stderr,
        )

    def down(self) -> None:
        subprocess.run(
            ["sudo", "systemctl", "stop", "hostapd", "dnsmasq"],
            check=True,
        )

    def is_up(self) -> bool:
        # Both units are required: hostapd alone gives no DHCP (dnsmasq),
        # and dnsmasq without hostapd gives no association.
        result = subprocess.run(
            ["systemctl", "is-active", "hostapd", "dnsmasq"],
            capture_output=True,
            text=True,
        )
        # is-active prints one status per unit and exits non-zero if any
        # is not active.
        return result.returncode == 0