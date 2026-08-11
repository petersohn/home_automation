"""RPi WiFi AP control via systemctl (hostapd + dnsmasq)."""

import subprocess


class WifiAp:
    def up(self) -> None:
        subprocess.run(
            ["sudo", "systemctl", "start", "hostapd", "dnsmasq"],
            check=True,
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