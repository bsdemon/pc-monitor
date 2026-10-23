# ESP32 display firmware

This directory is reserved for firmware that retrieves PC metrics over Wi-Fi/HTTP and displays them on the terminal. Firmware is not implemented yet.

## Confirmed hardware

- Main board: ESP32-S31-Korvo-1 V1.1.
- Display board: ESP32-S3-LCD-EV-Board-SUB3 V1.3.

The planned implementation uses ESP-IDF, the official board support package, and LVGL. All application text must be in English.

## Display design preview

Open `http://localhost:8080/display-preview` while the Python server is running. The mockup is an 800 × 480 landscape canvas, scaled down on smaller browser windows. It uses live `/api/metrics` readings by default. The Demo data and Stale state buttons show explicitly simulated states for reviewing the layout.

The source is `design/preview.html`. It is a browser mockup, not ESP32 firmware or a browser running on the terminal. The intended LVGL layout has two large CPU/GPU panels and compact RAM/VRAM panels, without a header or footer row. Small overlaid page indicators at the bottom switch views, and a corner dot shows connection state. The screen uses 18-pixel outer padding, 10–12-pixel gaps, and a dark background with green CPU, blue GPU, and amber memory accents. Temperatures and utilization have the strongest hierarchy. Missing readings are shown as N/A or a dash; stale/disconnected states dim the previous values.

The display specification is 4.3 inches, 800 × 480, ST7262E43 RGB controller and GT1151 touch, as listed for SUB3 V1.3 in the [official LCD board documentation](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html).

### Navigation and charts

Swipe horizontally to switch between Overview and Charts, or tap the small page indicators at the bottom. A Switch view button outside the simulated device provides another browser preview control. The Charts home view shows only CPU and GPU; RAM remains available in Overview and its detail view. Tap a CPU, GPU, RAM, or VRAM panel (or a chart row) to open its detail view. Back returns to the previous home view; swiping from details also returns to the previous home view. Keyboard Enter/Space also activates panels in the browser preview.

Charts retain the last two minutes in browser memory, independently of the selected view. Reloading starts a fresh live history; repeated or stale samples are not appended, missing values and gaps longer than five seconds break the curve. Utilization uses a fixed 0–100% axis. Temperature uses at least 0–100°C, expanding if needed. RAM charts show the hottest measured DIMM, not the sensor resolution or thermal limits. Demo mode explicitly simulates a full history and live movement; switching modes keeps live and simulated histories separate.

CPU details include thread utilization when available. GPU details include core/memory clock, fan speed, and VRAM. RAM details include used, total, available memory, and DIMM temperatures. Additional metrics are additive fields on `/api/metrics`; requests still read a single cached measurement rather than querying sensors again.

The overview and charts reserve no rows for branding, connection text, or navigation. A horizontal swipe is distinguished from vertical movement and suppresses the corresponding card click. Disconnected and stale data remain dimmed; details retain a Back button and device heading.
