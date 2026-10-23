# PC Monitor

Python diagnostics for a Windows PC that will supply metrics to an ESP32 display.
The Windows agent provides console diagnostics, a browser dashboard, and a shared HTTP metrics API. ESP32 firmware and setup instructions are in `src/esp32/`.

## Setup on Windows

Install [uv](https://docs.astral.sh/uv/getting-started/installation/), then open PowerShell as administrator in the project directory:

```powershell
uv sync
uv run pc-monitor diagnose
```

`uv sync` creates `.venv` and installs the locked dependencies, including the Windows-only HardwareMonitor package. Python 3.12 is selected by `.python-version`; uv can download it if needed. Manual environment activation is unnecessary. Use a Windows environment: a WSL virtual environment cannot read Windows hardware directly.

No separate LibreHardwareMonitor application, web server, or open sensor port is needed. The Python process reads CPU, GPU, and memory sensors through the library bundled with [HardwareMonitor](https://github.com/snip3rnick/PyHardwareMonitor). This package uses pythonnet and requires .NET Framework 4.7 or newer.

Administrator privileges do not replace hardware drivers. The package documents a PawnIO driver requirement for some system sensors. This project does not install drivers or change Windows security settings. Without the required driver or hardware support, some temperatures may be absent even when CPU utilization, RAM, and GPU sensors are available.

The console shows CPU utilization, temperature, power and average clock; GPU core utilization, temperature, hot spot and power; upload/download speed and Google ping; RAM utilization and used/total RAM in GiB; and available DIMM temperatures. CPU and RAM utilization come from psutil; GPU utilization comes from the GPU Core load sensor, with unavailable readings shown as N/A. Missing readings display `N/A`. The first detected CPU and GPU are selected.

To print this summary and also save the full diagnostic JSON:

```powershell
uv run pc-monitor diagnose --output diagnostics.json
```

Reports contain the computer hostname and hardware names. Review them before sharing. Generated `diagnostics*.json` reports are ignored by Git.

## Output and limitations

The report includes a one-second CPU utilization sample, RAM in bytes, a UTC collection completion timestamp, and a hardware sensor inventory with identifiers, paths, hardware types, sensor types, units, and numeric values. Unavailable readings are `null`; sensor types without an explicit unit mapping have a `null` unit.

The hardware provider reports `ok`, `empty`, or `unavailable` and provides an error when initialization or collection fails. `ok` means some sensors were enumerated, not that every expected metric is available. Baseline metrics remain available when the hardware library fails. Measurements are sampled sequentially rather than atomically.

This diagnostic schema is not the future `/api/metrics` contract. Mappings are based on user-provided Windows readings for a Ryzen 7 5700X and Radeon RX 6700 XT, with PawnIO installed and an elevated terminal. The development environment cannot independently validate Windows hardware access. No sensor values or fan settings are modified.

## Source layout

The source tree separates the Windows agent from the display firmware:

- `src/server/`: Python sensor collection, console output, and future HTTP server.
- `src/esp32/`: ESP-IDF/LVGL display firmware, touch Wi-Fi settings, and browser design reference.

The `pc-monitor` command remains unchanged. Its entry point is `src.server.cli:main`.

See [PLAN.md](PLAN.md) for milestones and [AGENTS.md](AGENTS.md) for contributor instructions.

See [Windows sensor access](docs/SENSOR_ACCESS.md) for upstream implementation findings and driver limitations.

## Continuous sampling

```powershell
uv run pc-monitor diagnose --watch
```

The default interval is one second. Use `--interval 2` to change it and Ctrl+C to stop. With `--output diagnostics.json`, the file is overwritten with the latest full report on each update.

The hardware library is initialized and primed once, remains open throughout the session, and closes on exit. An initial sampling window precedes the first report. Subsequent reads do not deliberately sleep inside sensor collection. The console shows initialization time separately from each collection duration in milliseconds. Scheduling subtracts collection and display time from the requested interval; if an update exceeds the interval, a message reports the overrun. One-second operation must be confirmed on the target Windows hardware.

## Browser dashboard and HTTP API

Run from an elevated Windows PowerShell session:

```powershell
uv run pc-monitor serve
```

Open **http://localhost:8080**. Python serves the ready HTML page with its embedded styles and script; no frontend build, external assets, or separate frontend server is required. The browser polls the metrics endpoint every second without reloading the page.

For ESP32 or another device on your home network:

```powershell
uv run pc-monitor serve --host 0.0.0.0
```

Use `http://<PC-LAN-IP>:8080` for the dashboard and `http://<PC-LAN-IP>:8080/api/metrics` for JSON. Allow TCP port 8080 on the Windows Firewall private network profile if needed. The default bind address is loopback; LAN mode is intended for a trusted local network and has no authentication. Do not forward this port to the internet.

`--port 8081` changes the port and `--interval 2` changes the sensor sampling interval. The browser still polls every second. Stop with Ctrl+C. Run one agent instance rather than keeping a separate diagnostic watcher open.

The collector opens once on its own thread and publishes the latest snapshot. HTTP requests read this cached snapshot and never trigger hardware access. While initializing, `/api/metrics` returns HTTP 503. A snapshot includes schema version 1, computer ID, UTC timestamp, CPU/GPU names and metrics, RAM, hardware availability, read duration, and a stale flag. Units are explicit in field names; VRAM is represented in bytes. Unavailable numeric values are `null`. The first detected CPU/GPU are selected. The browser displays N/A for absent sensors and marks data stale after five seconds without a fresh measurement.

The web interface is in `src/server/templates/dashboard.html`; HTTP handling is in `src/server/web.py`, and metric mapping is in `src/server/metrics.py`. The HTML is included in the Python wheel.

## Windows launcher

Double-click `start.cmd` in File Explorer, or run it from PowerShell:

```powershell
.\start.cmd
```

This wrapper invokes `start.ps1` with an execution-policy override for that process only; it does not change the system policy. Accept the UAC prompt to allow hardware access. Both launcher files must stay in the project directory.

Alternatively, when script execution is already allowed, run the PowerShell launcher directly:

```powershell
.\start.ps1
```

The launcher requests administrator privileges through UAC when needed, runs from its own project directory, and starts the dashboard and metrics API on port 8080 with LAN access enabled. Open **http://localhost:8080**; the ESP32 can use `http://<PC-LAN-IP>:8080/api/metrics`. Keep the terminal open and press Ctrl+C to stop. Stop any previous server before launching another instance.

uv must already be installed and available on PATH. `uv run --locked` prepares or updates the Windows `.venv` using the committed lockfile; no manual activation is required. PawnIO is still required for the CPU sensor access verified on this machine. The launcher does not install drivers, add firewall rules, or register automatic startup.

If PowerShell blocks execution of the local script, run it with a policy override for this process only:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\start.ps1
```

For loopback-only access or a different port:

```powershell
.\start.ps1 -BindAddress 127.0.0.1 -Port 8081
```

The launcher targets Windows PowerShell 5.1 or newer. Its UAC and sensor behavior must be checked on Windows; they cannot be exercised in the Linux development environment.

## ESP32 screen preview

Open **http://localhost:8080/display-preview** to review the 800 × 480 screen design with live metrics, or explicitly simulated demo/stale states. Overview and Charts switch the home view; tapping CPU/GPU/RAM opens details with a Back button. Charts keep the last two minutes while the page is open. This is a browser mockup for the future LVGL firmware.

## ESP32 firmware

The native firmware is in [src/esp32](src/esp32/README.md). It targets ESP32-S31-Korvo-1 V1.1 with the SUB3 V1.3 display and includes Overview, Charts, device details, and touch navigation. Wi-Fi and PC URL are configured directly on the touch screen with a network list and keyboard; settings persist in NVS. The ESP32 does not create a setup network or serve a settings web page. See the firmware README for build, flash, setup, and hardware validation instructions.

## Network telemetry

The Windows agent measures upload/download from byte-counter differences and elapsed monotonic time. `/api/metrics` adds `network.upload_bytes_per_second`, `download_bytes_per_second`, `ping_ms`, `ping_target`, and the ping timestamp. Interfaces are active non-loopback adapters with IP addresses. By default their traffic is summed; virtual/VPN adapters can count overlapping traffic. To select one adapter, set `PC_MONITOR_INTERFACE` to its exact name before running the server (for example `$env:PC_MONITOR_INTERFACE = "Ethernet 2"` in an elevated PowerShell session).

Rates describe current PC traffic, including LAN traffic; they are not an internet bandwidth test. Displays convert bytes/second to decimal Mbps (8 bits per byte). New/disconnected interfaces and counter resets do not create rate spikes. Missing measurements are null, while a measured idle rate is zero.

A background worker pings `google.com` about once per second using the system ICMP ping command. It does not block sensor collection or HTTP requests. Timeouts, missing ping utility, or unrecognized output produce null latency. Results older than five seconds are unavailable. `ping_upper_bound` marks replies reported as less than the given milliseconds. Ping measures the Windows PC's route to Google, rather than the ESP32 Wi-Fi round trip. ICMP may be blocked even when other internet services work.

The ESP32 overview replaces VRAM with Upload and Download. The third Network page plots Upload, Download, and Google ping over two minutes. VRAM fields remain in the JSON for compatibility, but are no longer displayed. The browser dashboard and `/display-preview` show the same new network readings.
