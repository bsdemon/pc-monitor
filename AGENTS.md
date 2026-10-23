# PC Monitor contributor instructions

## Communication

- Communicate with the user in Bulgarian.
- Write all application text in English, including console labels, status messages, errors, and help output. Use a decimal point for numeric output.
- Write all documentation and Markdown files in English. Use English for code identifiers and API field names.

## Project and scope

- Read `PLAN.md` before working. It is the source of truth for the architecture, milestones, and known hardware constraints.
- The project monitors a Windows 11 PC and displays its metrics on an ESP32 with a display over the local network.
- The first version uses a Python Windows agent managed with uv with an HTTP server and an ESP32 client that requests JSON once per second.
- The user has authorized a browser dashboard served by the Python agent alongside the metrics API. Do not add MQTT or a database unless requested.

## Metrics and protocol

- The main endpoint is `/api/metrics`; the planned port is `8080`.
- The payload includes a schema version, computer identifier, measurement timestamp, and GPU name.
- Network telemetry includes PC upload/download rates and ICMP ping to google.com; network probes must not block sensor sampling or HTTP requests.
- Core metrics are CPU and GPU utilization and temperature, used and total RAM, and used and total VRAM.
- Define units unambiguously. Represent unavailable values as `null`, not zero.
- Detect the GPU automatically and preserve a common payload format when it is replaced.
- Keep metric collection separate from HTTP presentation and display rendering.

## Hardware and unknowns

- The PC has a Ryzen 7 5700X. The current GPU is a Radeon RX 6700 XT, confirmed by Windows diagnostics; a Radeon RX 9070 XT is planned.
- Confirmed hardware: ESP32-S31-Korvo-1 V1.1 main board and ESP32-S3-LCD-EV-Board-SUB3 V1.3 display board. Verify the matching BSP display configuration before implementing firmware.
- Do not invent pin assignments, drivers, or available sensors. Verify them through documentation and diagnostics on the actual hardware.

## Development and validation

- Python code is in `src/server/`; native ESP-IDF/LVGL firmware and its browser design reference are in `src/esp32/`. The chip target is `esp32s31`, not `esp32s3`. The installed ESP-IDF v6.1 requires `--preview` for this target. Firmware settings can be provisioned from the ignored `src/esp32/.env` during build, or edited on the touch screen, then saved in NVS. Keep generated credential headers and firmware artifacts private and excluded from Git/Python packaging; do not add a setup AP or settings web server. Do not embed Wi-Fi secrets in source. The server provides diagnostics, a browser dashboard, and cached `/api/metrics` data. User-provided Windows diagnostics confirm core sensor availability.
- Use psutil for baseline metrics and the HardwareMonitor Python package for direct sensor discovery through its bundled LibreHardwareMonitor library. Do not require a separate monitoring application or HTTP sensor server. Validate available sensors on the actual Windows PC before defining mappings.
- When adding a project, document exact build, run, and relevant validation commands.
- Run available checks appropriate to each change. Distinguish successful compilation from validation on actual Windows hardware.
- Do not claim that sensors or the display have been tested when the environment does not support such validation.
- Do not commit Wi-Fi passwords, tokens, or other secrets.
- Network communication is intended for the local network; do not expose ports to the internet.
