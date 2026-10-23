"""Serve the dashboard and a cached metrics snapshot on the local network."""

import json
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

from src.server.diagnostics import Collector
from src.server.metrics import metrics_from_report


class MetricsStore:
    def __init__(self):
        self.lock = threading.Lock()
        self.snapshot = None
        self.error = None

    def publish(self, snapshot):
        with self.lock:
            self.snapshot = snapshot
            self.error = None

    def fail(self, error):
        with self.lock:
            self.error = str(error)

    def read(self):
        with self.lock:
            if self.snapshot is None:
                return None
            result = dict(self.snapshot)
            age = (datetime.now(timezone.utc) - datetime.fromisoformat(result["measured_at"])).total_seconds()
            result["stale"] = age > 5 or self.error is not None
            result["collection_error"] = self.error
            return result


def sample(store, stop, interval):
    # Open, update, and close .NET objects on the same thread.
    collector = Collector()
    try:
        collector.open()
        if stop.wait(interval):
            return
        while not stop.is_set():
            started = time.perf_counter()
            try:
                report = collector.collect()
                snapshot = metrics_from_report(report)
                snapshot["read_time_ms"] = round((time.perf_counter() - started) * 1000, 1)
                store.publish(snapshot)
            except Exception as exc:
                store.fail(exc)
            stop.wait(max(0.0, interval - (time.perf_counter() - started)))
    except Exception as exc:
        store.fail(exc)
    finally:
        collector.close()


def serve(host="127.0.0.1", port=8080, interval=1.0):
    store = MetricsStore()
    page = Path(__file__).with_name("templates").joinpath("dashboard.html").read_bytes()

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            path = urlsplit(self.path).path
            if path == "/":
                self.respond(200, page, "text/html; charset=utf-8")
            elif path == "/api/metrics":
                snapshot = store.read()
                if snapshot is None:
                    with store.lock:
                        error = store.error
                    self.respond_json(503, {"status": "starting", "error": error})
                else:
                    self.respond_json(200, snapshot)
            else:
                self.respond_json(404, {"error": "Not found"})

        def respond_json(self, status, data):
            self.respond(status, json.dumps(data, allow_nan=False).encode("utf-8"), "application/json")

        def respond(self, status, body, content_type):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format, *args):
            # Polling once a second should not flood the console.
            pass

    server = ThreadingHTTPServer((host, port), Handler)
    server.daemon_threads = True
    stop = threading.Event()
    worker = threading.Thread(target=sample, args=(store, stop, interval), daemon=True)
    worker.start()
    print(f"Dashboard: http://{'localhost' if host == '0.0.0.0' else host}:{port}", flush=True)
    if host == "0.0.0.0":
        print(f"LAN clients: http://<PC-LAN-IP>:{port}", flush=True)
    print("Initializing sensors. Press Ctrl+C to stop.", flush=True)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        server.server_close()
        stop.set()
        worker.join()
