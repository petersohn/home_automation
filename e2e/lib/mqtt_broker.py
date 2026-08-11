"""Mosquitto broker start/stop control via systemctl."""

import subprocess


class MqttBroker:
    def start(self) -> None:
        subprocess.run(
            ["sudo", "systemctl", "start", "mosquitto"],
            check=True,
        )

    def stop(self) -> None:
        subprocess.run(
            ["sudo", "systemctl", "stop", "mosquitto"],
            check=True,
        )

    def is_running(self) -> bool:
        result = subprocess.run(
            ["systemctl", "is-active", "mosquitto"],
            capture_output=True,
            text=True,
        )
        return result.stdout.strip() == "active"