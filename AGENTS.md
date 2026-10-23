# PC Monitor contributor instructions

## Communication

- Communicate with the user in Bulgarian.
- Write all application text in English, including console labels, status messages, errors, and help output. Use a decimal point for numeric output.
- Write all documentation and Markdown files in English. Use English for code identifiers and API field names.

## Project and scope

- Read `PLAN.md` before working. It is the source of truth for the architecture, milestones, and known hardware constraints.
- The project monitors a Windows 11 PC and displays its metrics on an ESP32 with a display over the local network.
- The first version uses a Python Windows agent managed with uv with an HTTP server and an ESP32 client that requests JSON once per second.
- Do not add MQTT, a database, or a separate web interface unless requested.

## Metrics and protocol

- The main endpoint is `/api/metrics`; the planned port is `8080`.
- The payload includes a schema version, computer identifier, measurement timestamp, and GPU name.
- Core metrics are CPU and GPU utilization and temperature, used and total RAM, and used and total VRAM.
- Define units unambiguously. Represent unavailable values as `null`, not zero.
- Detect the GPU automatically and preserve a common payload format when it is replaced.
- Keep metric collection separate from HTTP presentation and display rendering.

## Hardware and unknowns

- The PC has a Ryzen 7 5700X. The exact current GPU model remains unconfirmed; a Radeon RX 9070 XT is planned.
- Confirm the ESP32 board model, display controller, resolution, and wiring before implementing display code.
- Do not invent pin assignments, drivers, or available sensors. Verify them through documentation and diagnostics on the actual hardware.

## Development and validation

- The next step is a Windows diagnostic tool that lists available sensors and their readings.
- Use psutil for baseline metrics and the HardwareMonitor Python package for direct sensor discovery through its bundled LibreHardwareMonitor library. Do not require a separate monitoring application or HTTP sensor server. Validate available sensors on the actual Windows PC before defining mappings.
- When adding a project, document exact build, run, and relevant validation commands.
- Run available checks appropriate to each change. Distinguish successful compilation from validation on actual Windows hardware.
- Do not claim that sensors or the display have been tested when the environment does not support such validation.
- Do not commit Wi-Fi passwords, tokens, or other secrets.
- Network communication is intended for the local network; do not expose ports to the internet.
