"""Command-line entry point."""

import argparse
import json
import math
import time
from pathlib import Path

from src.server.diagnostics import Collector
from src.server.display import format_summary


def positive_seconds(value: str) -> float:
    seconds = float(value)
    if not math.isfinite(seconds) or seconds <= 0:
        raise argparse.ArgumentTypeError("Interval must be a finite positive number")
    return seconds


def main() -> None:
    parser = argparse.ArgumentParser(description="Collect PC metrics and sensor diagnostics")
    parser.add_argument("command", choices=["diagnose", "serve"])
    parser.add_argument("--output", type=Path, help="Save the latest full diagnostic JSON to this file")
    parser.add_argument("--watch", action="store_true", help="Keep sampling until Ctrl+C")
    parser.add_argument("--interval", type=positive_seconds, default=1.0,
                        help="Sampling interval in seconds for watch mode (default: 1)")
    parser.add_argument("--host", default="127.0.0.1", help="HTTP bind address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8080, help="HTTP port (default: 8080)")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("Port must be between 1 and 65535")
    if args.command == "serve":
        from src.server.web import serve
        serve(args.host, args.port, args.interval)
        return
    collector = Collector()
    print("Initializing sensors...", flush=True)
    try:
        started = time.perf_counter()
        collector.open()
        print(f"Initialization: {time.perf_counter() - started:.2f} s", flush=True)
        # CPU/GPU load and power counters need an initial sampling window.
        time.sleep(args.interval if args.watch else 1.0)
        while True:
            started = time.perf_counter()
            report = collector.collect()
            duration = time.perf_counter() - started
            print(f"\n{report['measured_at']} | Read time: {duration * 1000:.0f} ms")
            print(format_summary(report))
            if report["hardware_sensors"]["error"]:
                print(report["hardware_sensors"]["error"])
            if args.output:
                text = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
                args.output.write_text(text, encoding="utf-8")
            if not args.watch:
                break
            elapsed = time.perf_counter() - started
            if elapsed > args.interval:
                print(f"Update took {elapsed:.2f} s; exceeds the {args.interval:g} s interval.")
            # Subtract collection/rendering time; do not add another full interval.
            print(flush=True)
            time.sleep(max(0.0, args.interval - elapsed))
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        collector.close()


if __name__ == "__main__":
    main()
