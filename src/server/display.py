"""Render the selected readings as a compact console table."""


def format_summary(report: dict) -> str:
    sensors = report["hardware_sensors"]["sensors"]
    cpu = next((s["path"][0] for s in sensors if s["hardware_type"] == "Cpu"), None)
    gpu = next((s["path"][0] for s in sensors if s["hardware_type"].startswith("Gpu")), None)

    def reading(device, kind, name):
        return next((s["value"] for s in sensors
                     if s["path"][0] == device and s["type"] == kind
                     and s["path"][-1] == name), None)

    def number(value, unit="", decimals=2):
        if value is None:
            return "N/A"
        text = f"{value:.{decimals}f}".rstrip("0").rstrip(".") if decimals else f"{value:.0f}"
        return text + unit

    used = reading(gpu, "SmallData", "GPU Memory Used")
    total = reading(gpu, "SmallData", "GPU Memory Total")
    ram_temperatures = [
        number(s["value"], "°C") for s in sensors
        if s["hardware_type"] == "Memory" and s["type"] == "Temperature"
        and s["path"][-1].startswith("DIMM #")
    ]
    rows = [
        ("CPU utilization", number(report["cpu"]["utilization_percent"], "%")),
        ("CPU temperature", number(reading(cpu, "Temperature", "Core (Tctl/Tdie)"), "°C")),
        ("CPU power", number(reading(cpu, "Power", "Package"), " W")),
        ("CPU average clock", number(reading(cpu, "Clock", "Cores (Average)"), " MHz", 0)),
        ("GPU utilization", number(reading(gpu, "Load", "GPU Core"), "%")),
        ("GPU temperature", number(reading(gpu, "Temperature", "GPU Core"), "°C")),
        ("GPU Hot Spot", number(reading(gpu, "Temperature", "GPU Hot Spot"), "°C")),
        ("GPU power", number(reading(gpu, "Power", "GPU Package"), " W")),
        ("VRAM used / total", f"{number(used, decimals=0)} / {number(total, decimals=0)} MiB"),
        ("RAM utilization", number(report["ram"]["utilization_percent"], "%")),
        ("RAM used / total", f"{number(report['ram']['used_bytes'] / 1024**3)} / "
         f"{number(report['ram']['total_bytes'] / 1024**3)} GiB"),
        ("RAM temperatures", " / ".join(ram_temperatures) or "N/A"),
    ]
    width = max(len(label) for label, _ in rows)
    return "\n".join(f"{label:<{width}} | {value}" for label, value in rows)
