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
        result = subprocess.run(
            ["systemctl", "is-active", "hostapd"],
            capture_output=True,
            text=True,
        )
        return result.stdout.strip() == "active"