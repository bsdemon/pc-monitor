# Windows sensor access

## Findings from LibreHardwareMonitor

The agent uses the HardwareMonitor Python package, which loads a bundled LibreHardwareMonitorLib DLL into the Python process through pythonnet. The standalone LibreHardwareMonitor application and its HTTP server are not required.

The inspected upstream implementation separates hardware access by device:

- [AMD CPU implementation](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/master/LibreHardwareMonitorLib/Hardware/Cpu/Amd17Cpu.cs): Ryzen sensor handling uses the PawnIO-backed AMD Family 17 module, including the paths used for newer Ryzen families. The Ryzen 7 5700X must be validated on the actual machine.
- [PawnIO AMD module](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/master/LibreHardwareMonitorLib/PawnIo/AmdFamily17.cs): SMN and MSR access is performed through driver IOCTL operations. Administrator access to a normal process does not replace this kernel interface.
- [AMD GPU implementation](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/master/LibreHardwareMonitorLib/Hardware/Gpu/AmdGpu.cs): Radeon metrics use AMD ADL calls, including Overdrive temperature queries and PMLog telemetry, with additional Windows graphics APIs for some metrics. Available readings depend on the installed AMD graphics driver and GPU model.
- [Python package loader](https://github.com/snip3rnick/PyHardwareMonitor/blob/master/HardwareMonitor/__init__.py): the package loads its bundled DLL and warns when administrator privileges or PawnIO are absent. It does not install PawnIO automatically.

## Implementation decision

Use the bundled library from Python rather than reimplementing the hardware access in C. Native C code alone would not remove the CPU kernel-driver requirement. A direct AMD ADL backend through Python ctypes or a C helper remains an option if the current GPU integration proves insufficient.

Do not install additional drivers automatically. If the required access is unavailable, report the missing readings or provider error and preserve baseline CPU utilization and RAM data. Hardware sensor availability is not yet validated on the target Windows PC.

The upstream links describe the inspected development branch, which can differ from the DLL bundled in a particular HardwareMonitor release. Record actual diagnostic output before promising specific sensors.
