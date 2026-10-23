"""Sample PC interface traffic and run nonblocking ICMP probes to Google."""

import os
import platform
import re
import shutil
import socket
import subprocess
import threading
import time
from datetime import datetime, timezone

import psutil


class NetworkMonitor:
    def __init__(self):
        self.previous = None
        self.previous_time = None
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.thread = None
        self.ping = None
        self.ping_time = 0.0
        self.ping_timestamp = None
        self.ping_upper_bound = False
        self.interface = os.environ.get("PC_MONITOR_INTERFACE", "")

    def open(self):
        self.sample()
        self.thread = threading.Thread(target=self._probe_loop, name="google-ping", daemon=True)
        self.thread.start()

    def close(self):
        self.stop.set()
        if self.thread:
            self.thread.join(timeout=4)

    def _probe_loop(self):
        executable = shutil.which("ping")
        while not self.stop.is_set():
            started = time.monotonic()
            latency, upper_bound = None, False
            if executable:
                command = ([executable, "-4", "-n", "1", "-w", "1000", "google.com"]
                           if platform.system() == "Windows"
                           else [executable, "-n", "-c", "1", "-W", "1", "google.com"])
                try:
                    result = subprocess.run(command, capture_output=True, timeout=3,
                                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                    # Parse the reply's RTT, not the process duration or summary average.
                    match = re.search(rb"([=<])\s*(\d+(?:[.,]\d+)?)\s*ms\b", result.stdout)
                    if result.returncode == 0 and match:
                        latency = float(match[2].replace(b",", b"."))
                        upper_bound = match[1] == b"<"
                except (OSError, subprocess.TimeoutExpired):
                    pass
            with self.lock:
                self.ping, self.ping_upper_bound = latency, upper_bound
                self.ping_time = time.monotonic()
                self.ping_timestamp = datetime.now(timezone.utc).isoformat()
            self.stop.wait(max(0.0, 1.0 - (time.monotonic() - started)))

    def sample(self):
        now = time.monotonic()
        selected = {}
        error = None
        try:
            stats, addresses = psutil.net_if_stats(), psutil.net_if_addrs()
            counters = psutil.net_io_counters(pernic=True, nowrap=False)
            for name, count in counters.items():
                if self.interface and name != self.interface:
                    continue
                state = stats.get(name)
                ips = [a.address.split("%", 1)[0] for a in addresses.get(name, [])
                       if a.family in {socket.AF_INET, socket.AF_INET6}]
                if (not state or not state.isup or "loopback" in state.flags
                        or not ips or all(ip == "::1" or ip.startswith("127.") for ip in ips)):
                    continue
                selected[name] = (count.bytes_sent, count.bytes_recv)
        except (OSError, psutil.Error) as exc:
            error = str(exc)
        upload = download = None
        if self.previous is not None and self.previous_time is not None and now > self.previous_time:
            pairs = [(current, self.previous[name]) for name, current in selected.items()
                     if name in self.previous and current[0] >= self.previous[name][0]
                     and current[1] >= self.previous[name][1]]
            if pairs:
                elapsed = now - self.previous_time
                upload = sum(cur[0] - prev[0] for cur, prev in pairs) / elapsed
                download = sum(cur[1] - prev[1] for cur, prev in pairs) / elapsed
        self.previous, self.previous_time = selected, now
        with self.lock:
            fresh = now - self.ping_time <= 5
            ping = self.ping if fresh else None
            stamp, upper = self.ping_timestamp, self.ping_upper_bound if fresh else False
        return {
            "upload_bytes_per_second": upload,
            "download_bytes_per_second": download,
            "ping_ms": ping,
            "ping_target": "google.com",
            "ping_measured_at": stamp,
            "ping_upper_bound": upper,
            "interfaces": list(selected),
            "error": error,
        }
