"""Collect baseline metrics and read Windows hardware sensors in-process."""

import math
import platform
import socket
from datetime import datetime, timezone

import psutil

from src.server.network import NetworkMonitor


UNITS = {
    "Temperature": "celsius", "Load": "percent", "Control": "percent",
    "Clock": "MHz", "Fan": "RPM", "Power": "watts", "Voltage": "volts",
    "Current": "amperes", "Data": "GiB", "SmallData": "MiB",
    "Throughput": "bytes_per_second",
}


class Collector:
    """Keep the sensor library open across samples."""

    def __init__(self):
        self.computer = None
        self.error = None
        self.network = NetworkMonitor()

    def open(self):
        self.network.open()
        psutil.cpu_percent(interval=None)
        try:
            if platform.system() != "Windows":
                raise RuntimeError("Direct hardware sensor access is supported on Windows only")
            from HardwareMonitor.Hardware import Computer

            self.computer = Computer()
            self.computer.IsCpuEnabled = True
            self.computer.IsGpuEnabled = True
            self.computer.IsMemoryEnabled = True
            self.computer.Open()
            # Prime counters once; the caller provides the initial sampling window.
            read_hardware(self.computer)
        except Exception as exc:
            self.error = f"Cannot initialize hardware sensors: {exc}"
            computer, self.computer = self.computer, None
            if computer is not None:
                computer.Close()

    def close(self):
        self.network.close()
        computer, self.computer = self.computer, None
        if computer is not None:
            computer.Close()

    def collect(self) -> dict:
        report = collect(self.computer, self.error)
        report["network"] = self.network.sample()
        return report


def read_hardware(computer) -> list[dict]:
    sensors = []

    def visit(hardware, path=()):
        hardware.Update()
        path = (*path, str(hardware.Name))
        for sensor in hardware.Sensors:
            kind = str(sensor.SensorType)
            value = None if sensor.Value is None else float(sensor.Value)
            if value is not None and not math.isfinite(value):
                value = None
            sensors.append({
                "id": str(sensor.Identifier),
                "path": [*path, str(sensor.Name)],
                "hardware_type": str(hardware.HardwareType),
                "type": kind,
                "unit": UNITS.get(kind),
                "value": value,
            })
        for child in hardware.SubHardware:
            visit(child, path)

    for hardware in computer.Hardware:
        visit(hardware)
    return sensors


def collect(computer=None, initialization_error=None) -> dict:
    cpu_percent = psutil.cpu_percent(interval=None)
    memory = psutil.virtual_memory()
    result = {
        "schema_version": 1,
        "computer_id": socket.gethostname(),
        "platform": platform.platform(),
        "cpu": {"utilization_percent": cpu_percent, "logical_cores": psutil.cpu_count()},
        "ram": {"utilization_percent": memory.percent,
                "used_bytes": memory.used, "total_bytes": memory.total,
                "available_bytes": memory.available},
        "hardware_sensors": {"status": "unavailable", "error": None, "sensors": []},
    }
    hardware = result["hardware_sensors"]
    try:
        if computer is None:
            raise RuntimeError(initialization_error or "Hardware collector is not initialized")
        hardware["sensors"] = read_hardware(computer)
        hardware["status"] = "ok" if hardware["sensors"] else "empty"
    except Exception as exc:
        # Hardware/.NET failures should not prevent CPU and RAM diagnostics.
        hardware["error"] = f"Cannot read hardware sensors: {exc}"
    result["measured_at"] = datetime.now(timezone.utc).isoformat()
    return result
