"""Map diagnostic readings to the shared dashboard and ESP32 payload."""


def metrics_from_report(report: dict) -> dict:
    sensors = report["hardware_sensors"]["sensors"]
    cpu = next((s["path"][0] for s in sensors if s["hardware_type"] == "Cpu"), None)
    gpu = next((s["path"][0] for s in sensors if s["hardware_type"].startswith("Gpu")), None)

    def reading(device, kind, name):
        return next((s["value"] for s in sensors if s["path"][0] == device
                     and s["type"] == kind and s["path"][-1] == name), None)

    def vram_bytes(name):
        value = reading(gpu, "SmallData", name)
        return round(value * 1024**2) if value is not None else None

    return {
        "schema_version": 1,
        "computer_id": report["computer_id"],
        "measured_at": report["measured_at"],
        "cpu": {
            "name": cpu,
            "utilization_percent": report["cpu"]["utilization_percent"],
            "temperature_celsius": reading(cpu, "Temperature", "Core (Tctl/Tdie)"),
            "power_watts": reading(cpu, "Power", "Package"),
            "average_clock_mhz": reading(cpu, "Clock", "Cores (Average)"),
        },
        "gpu": {
            "name": gpu,
            "utilization_percent": reading(gpu, "Load", "GPU Core"),
            "temperature_celsius": reading(gpu, "Temperature", "GPU Core"),
            "hot_spot_celsius": reading(gpu, "Temperature", "GPU Hot Spot"),
            "power_watts": reading(gpu, "Power", "GPU Package"),
            "vram_used_bytes": vram_bytes("GPU Memory Used"),
            "vram_total_bytes": vram_bytes("GPU Memory Total"),
        },
        "ram": {
            "utilization_percent": report["ram"]["utilization_percent"],
            "used_bytes": report["ram"]["used_bytes"],
            "total_bytes": report["ram"]["total_bytes"],
            "available_bytes": report["ram"]["available_bytes"],
            "temperatures_celsius": [
                {"name": s["path"][-1], "value": s["value"]}
                for s in sensors if s["hardware_type"] == "Memory"
                and s["type"] == "Temperature" and s["path"][-1].startswith("DIMM #")
            ],
        },
        "hardware_status": report["hardware_sensors"]["status"],
        "hardware_error": report["hardware_sensors"]["error"],
    }
