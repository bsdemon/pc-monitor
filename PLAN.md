# Plan: PC Monitor

## Goal and hardware

Monitor a Windows PC and display its metrics on an ESP32 with a display over the local network.

- Computer: Windows 11, Ryzen 7 5700X.
- Current GPU: reported as ATI RX6700X; the exact model remains to be confirmed.
- Planned GPU: Radeon RX 9070 XT.
- Board: reported by the user as ESP32-S31 Korvo-1 with a display.
- Display resolution, controller, and wiring remain to be confirmed.

## First-version architecture

Start without an MQTT broker. The Windows agent collects metrics and exposes them through an HTTP server. The ESP32 requests JSON data once per second.

Example URL: `http://<PC-IP>:8080/api/metrics`.

The agent and HTTP server run on the Windows PC itself. The agent implementation is Python, with uv managing the virtual environment and dependencies. Initial diagnostics use psutil for CPU utilization and RAM, and the HardwareMonitor Python package with its bundled LibreHardwareMonitor library for direct hardware sensor discovery. No separate monitoring application is required. Run Python with administrator privileges for sensor access; some sensors may require the PawnIO driver and must remain unavailable if it is absent.

## Milestones

1. **Check Windows sensors**
   - A diagnostic tool lists available CPU, GPU, and RAM metrics.
   - Check available temperatures, clock speeds, power consumption, and fan speeds.
   - The initial scope is CPU utilization and temperature, GPU utilization and temperature, used and total RAM, and used and total VRAM.
   - Add further metrics according to the available sensors.

2. **Windows agent with an HTTP server**
   - Collect metrics every second.
   - Expose the latest measurement as JSON through `/api/metrics`.
   - Include a schema version, computer identifier, measurement timestamp, and GPU name.
   - Use explicit units; unavailable values are `null`, not zero.
   - Detect the GPU automatically and preserve a common format when it is replaced. Check sensor availability again after replacement.

3. **Connect the ESP32**
   - Connect to the home Wi-Fi network.
   - Request data every second.
   - First verify reception using the serial monitor.
   - Automatically reconnect after connection loss.

4. **Render metrics on the display**
   - Confirm the controller, resolution, and wiring before implementation.
   - Main screen: CPU and GPU temperatures and utilization bars, RAM, and VRAM.
   - Show a disconnected or stale-data status when fresh data is unavailable.
   - UI refreshes may run independently of measurement retrieval.

5. **Daily use and validation**
   - Start the agent automatically with Windows.
   - Configure the PC address and the necessary Windows Firewall rule for the local network.
   - Do not expose ports to the internet.
   - Test Wi-Fi loss, agent shutdown, PC sleep, wake, and restart.

6. **Possible future milestones**
   - WebSocket if a persistent connection and updates on change become necessary.
   - An MQTT broker on the Windows PC if more displays or other consumers are needed.
   - Preserve the common data format when adding MQTT.
   - Charts, additional screens, and settings after a working prototype.
   - A database and separate web interface are outside the scope of the first version.

## Next step

Run the Python diagnostic tool on the Windows PC from an elevated PowerShell session, then inspect the report to establish available sensors and define metric mappings. The initial diagnostic tool is implemented; actual Windows hardware validation is pending.
