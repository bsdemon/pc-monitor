# PC Monitor

Python diagnostics for a Windows PC that will supply metrics to an ESP32 display.
The current milestone is sensor discovery; the agent HTTP endpoint and ESP32 firmware are not implemented yet.

## Setup on Windows

Install [uv](https://docs.astral.sh/uv/getting-started/installation/), then open PowerShell as administrator in the project directory:

```powershell
uv sync
uv run pc-monitor diagnose
```

`uv sync` creates `.venv` and installs the locked dependencies, including the Windows-only HardwareMonitor package. Python 3.12 is selected by `.python-version`; uv can download it if needed. Manual environment activation is unnecessary. Use a Windows environment: a WSL virtual environment cannot read Windows hardware directly.

No separate LibreHardwareMonitor application, web server, or open sensor port is needed. The Python process reads CPU, GPU, and memory sensors through the library bundled with [HardwareMonitor](https://github.com/snip3rnick/PyHardwareMonitor). This package uses pythonnet and requires .NET Framework 4.7 or newer.

Administrator privileges do not replace hardware drivers. The package documents a PawnIO driver requirement for some system sensors. This project does not install drivers or change Windows security settings. Without the required driver or hardware support, some temperatures may be absent even when CPU utilization, RAM, and GPU sensors are available.

The console shows CPU utilization, temperature, power and average clock; GPU core utilization, temperature, hot spot and power; used/total VRAM; RAM utilization and used/total RAM in GiB; and available DIMM temperatures. CPU and RAM utilization come from psutil; GPU utilization comes from the GPU Core load sensor, with unavailable readings shown as N/A. Missing readings display `N/A`. The first detected CPU and GPU are selected.

To print this summary and also save the full diagnostic JSON:

```powershell
uv run pc-monitor diagnose --output diagnostics.json
```

Reports contain the computer hostname and hardware names. Review them before sharing. Generated `diagnostics*.json` reports are ignored by Git.

## Output and limitations

The report includes a one-second CPU utilization sample, RAM in bytes, a UTC collection completion timestamp, and a hardware sensor inventory with identifiers, paths, hardware types, sensor types, units, and numeric values. Unavailable readings are `null`; sensor types without an explicit unit mapping have a `null` unit.

The hardware provider reports `ok`, `empty`, or `unavailable` and provides an error when initialization or collection fails. `ok` means some sensors were enumerated, not that every expected metric is available. Baseline metrics remain available when the hardware library fails. Measurements are sampled sequentially rather than atomically.

This diagnostic schema is not the future `/api/metrics` contract. An actual Windows report is needed before mapping CPU temperature, GPU temperature/utilization, and VRAM. Windows hardware access has not been validated from the development environment. No sensor values or fan settings are modified.

## Source layout

Python files live directly in `src/`: `cli.py` provides the command line and `diagnostics.py` collects metrics, and `display.py` formats the console summary.

See [PLAN.md](PLAN.md) for milestones and [AGENTS.md](AGENTS.md) for contributor instructions.

See [Windows sensor access](docs/SENSOR_ACCESS.md) for upstream implementation findings and driver limitations.

## Continuous sampling

```powershell
uv run pc-monitor diagnose --watch
```

The default interval is one second. Use `--interval 2` to change it and Ctrl+C to stop. With `--output diagnostics.json`, the file is overwritten with the latest full report on each update.

The hardware library is initialized and primed once, remains open throughout the session, and closes on exit. An initial sampling window precedes the first report. Subsequent reads do not deliberately sleep inside sensor collection. The console shows initialization time separately from each collection duration in milliseconds. Scheduling subtracts collection and display time from the requested interval; if an update exceeds the interval, a message reports the overrun. One-second operation must be confirmed on the target Windows hardware.
