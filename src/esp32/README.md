# PC Monitor ESP32 firmware

ESP-IDF C firmware for **ESP32-S31-Korvo-1 V1.1** and **ESP32-S3-LCD-EV-Board-SUB3 V1.3** (800 × 480, RGB ST7262E43, GT1151 touch).

The official board support package supplies display timings, pins, and touch initialization. The application does not duplicate the board wiring. Dependencies are pinned to BSP 1.0.1, esp_lvgl_port 2.9.0, and LVGL 8.4.0; cJSON is resolved by ESP Component Manager. This project targets `esp32s31`, not `esp32s3`.

## Build on Windows

ESP-IDF v6.1 with ESP32-S31 toolchain support is required. The target currently requires the `--preview` flag. The installed environment on the development PC uses `C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1`.

From PowerShell:

```powershell
cd C:\Users\eon\Workspace\pc-monior\src\esp32
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

For another installation, pass `-IdfProfile` with the path to its activation script. From an already activated ESP-IDF shell, the equivalent build command is:

```powershell
idf.py --preview -DIDF_TARGET=esp32s31 build
```

The first build downloads managed components. Keep `dependencies.lock` in version control once resolved. Generated `build/`, `managed_components/`, and `sdkconfig` stay local. Local `.env` provisioning is optional. When filled in, credentials are included in the private generated build header and firmware; see the local configuration section below.

## Flash when ready

Connect the board's **UART USB-C** port, not its power-only port. In an activated ESP-IDF terminal, replace COM5 with the board's actual serial port:

```powershell
idf.py --preview -p COM5 flash monitor
```

Exit the serial monitor with Ctrl+]. This command is documented for manual use; the development workflow does not flash hardware automatically. The custom partition table provides a 6 MiB factory app and NVS for settings. OTA updates are not implemented.

## Local .env configuration

For setup without typing on the small display, edit **`src/esp32/.env`** in the Windows project. A local file with empty Wi-Fi fields is supplied; `.env.example` contains illustrative values:

```dotenv
WIFI_SSID="YourHomeWiFi"
WIFI_PASSWORD="YourWiFiPassword"
PC_HOST=192.168.1.50
PC_PORT=8080
UPDATE_INTERVAL_MS=1000
DISPLAY_BRIGHTNESS=80
START_VIEW=overview
```

Use the PC's LAN IP or hostname. The URL `http://PC_HOST:PC_PORT/api/metrics` is constructed automatically. An optional `METRICS_URL` overrides the complete URL. START_VIEW accepts overview, charts or network. Empty WIFI_PASSWORD selects an open network. Wi-Fi names are limited to 32 UTF-8 bytes; secured passwords to 8–63 bytes.

Rebuild with `build.ps1`, then flash from the activated ESP-IDF terminal. Direct `idf.py --preview flash monitor` also picks up changes to `.env` because it builds first. No local configuration file is read by the ESP32 itself: the build generates a private header inside `build/`, and the firmware imports it into NVS on boot.

Only changed configuration values trigger a new NVS import. Subsequent normal boots preserve edits made on the touch screen. Changing `.env` values and rebuilding/flashing replaces the saved settings once. Missing `.env` or an empty WIFI_SSID leaves existing NVS settings untouched. A normal build with no code/config changes does not reset settings.

The parser supports blank lines, full-line # comments, and optional matching single/double quotes. Backslashes and # inside values are literal; shell execution, interpolation, escape processing, and inline comments are not supported. Invalid values stop the build without printing passwords.

`.env`, generated headers, and build outputs are ignored by Git and excluded from Python packaging. `.env.example` is a tracked template. Firmware built with real credentials also contains them, so keep those generated artifacts private. No credentials are printed in build logs or checked into source files.

## Wi-Fi and settings on the touch screen

1. Boot the board. It opens the monitor without a setup wizard.
2. Tap the small gear icon in the top-right corner.
3. Choose a nearby network from the scanned list, or type the SSID manually for a hidden network. Rescan refreshes the list.
4. Tap the password field and type on the on-screen keyboard. The Keyboard button also opens it for the last selected field. Use its checkmark/Ready key to close the keyboard. For an open network, select Open network instead.
5. Set the PC metrics URL, for example `http://192.168.1.50:8080/api/metrics`. Scroll the form for update interval, software brightness, and initial view.
6. Tap Save and connect. The board saves the configuration in NVS and restarts. Cancel/Back discards edits.

The ESP32 works only as a station/client. It does not create a setup access point and does not run a settings HTTP server. No phone or browser is needed for configuring the display. The HTTP client still reads metrics from the Windows PC.

When editing the current saved SSID, leaving the password blank keeps its stored password. Choosing another secured network requires entering its password. Passwords are masked and never prefilled or printed to logs. Open network explicitly clears the saved password. NVS encryption is not enabled in this prototype.

Scanning runs on the network task and may briefly interrupt metric retrieval while the display stays responsive. The strongest discovered entries are deduplicated by SSID and capped at twelve. Saved credentials reconnect automatically after restart or Wi-Fi loss. If connection fails, tap the gear icon and correct the settings; no setup network starts automatically.

Start the Windows agent with `start.cmd` (LAN binding enabled), or `uv run pc-monitor serve --host 0.0.0.0`. Use the Windows PC's LAN IP, not localhost. The PC and display must share a reachable local network. Allow the server port on the Windows Firewall private profile if required. A stable PC LAN address or DHCP reservation avoids changing the URL later.

The form offers 0.5, 1, 2, 5, or 10-second updates. The start screen is Overview, Charts or Network. Brightness uses a black UI overlay because the board BSP does not support physical backlight adjustment; it does not reduce backlight power. Changes apply after Save and connect. The receiver supports plain HTTP on a trusted LAN and metrics schema version 1.

## Display and navigation

- **Overview:** CPU/GPU temperature, utilization, power, clock/hot spot, RAM use and upload/download speed, and available DIMM temperature.
- **Charts:** CPU and GPU utilization/temperature only. History retains 120 one-second slots in RAM and resets on reboot. No database or cloud is involved.
- **Network:** upload/download in decimal Mbps and ping from the PC to google.com in milliseconds, using two minutes of history. Rate chart ticks use tenths of Mbps and auto-scale; the native rate plot saturates above 3000 Mbps while numeric/API values retain the actual rate. Missing ping results are gaps.
- **Details:** tap CPU, GPU or RAM; tap the Network panel to open network charts. CPU includes per-thread utilization when provided; GPU includes clocks/fan/core voltage; RAM includes available memory and DIMM temperatures.
- Swipe left/right to switch home views. Swipe from details or tap Back to return. Three small overlaid page controls also switch home views. There are no reserved branding or status text rows.
- Tap the gear icon to open the on-screen Wi-Fi/settings form. A small corner dot indicates connection/sensor status: green for live, amber for stale/limited, red for disconnected. Old readings dim after five seconds without fresh data.

HTTP requests run in a separate FreeRTOS task. A one-element queue passes the latest snapshot into an LVGL timer. Only the LVGL task touches widgets; HTTP waits never block rendering. The receiver bounds response sizes and array counts, validates schema version, preserves unavailable numeric readings as NaN internally, and displays N/A. Repeated measurement timestamps do not count as fresh data. Missing history points remain gaps.

The browser design mockup remains in `design/preview.html`, served at `/display-preview` by the Python server. It is a design reference, not HTML executed on the board. The firmware uses native LVGL widgets, so exact fonts and spacing can differ and need review on the physical display.

## Build validation

The firmware has been compiled successfully on Windows using the installed ESP-IDF v6.1 toolchain, the `esp32s31` preview target, and the resolved dependencies in `dependencies.lock`. The generated application is about 1.49 MiB and fits the 6 MiB app partition. No board was flashed during this validation. The SDK reports Kconfig/deprecation notices in third-party components; application C compilation and linking complete successfully.

## Hardware validation

Display/touch alignment, frame-buffer performance, on-screen keyboard, network scanning, Wi-Fi setup/recovery, settings icon/swipes, and metrics reception must be checked on the actual board after flashing. A successful build alone does not validate these behaviors.

References: [official BSP](https://components.espressif.com/components/espressif/esp32_s31_korvo_1/versions/1.0.1/readme), [main board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html), [display board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html).

Restart the Windows agent after updating it to provide the new network fields. An older agent remains compatible but network readings appear as N/A. Use START_VIEW=network in the local .env to start on the third page.
