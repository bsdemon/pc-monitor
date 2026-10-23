"""Generate a private build header from the device's local .env file."""

import argparse
import hashlib
import ipaddress
import json
import re
from pathlib import Path
from urllib.parse import urlsplit

KEYS = {
    "WIFI_SSID", "WIFI_PASSWORD", "PC_HOST", "PC_PORT", "METRICS_URL",
    "UPDATE_INTERVAL_MS", "DISPLAY_BRIGHTNESS", "START_VIEW",
}


def parse_env(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    values = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f"Invalid .env assignment on line {line_number}")
        key, value = (part.strip() for part in line.split("=", 1))
        if key not in KEYS or key in values:
            raise ValueError(f"Unknown or duplicate .env key on line {line_number}")
        if value.startswith(("'", '"')):
            if len(value) < 2 or value[-1] != value[0]:
                raise ValueError(f"Unclosed quote on .env line {line_number}")
            value = value[1:-1]
        if any(ord(c) < 32 or ord(c) == 127 for c in value):
            raise ValueError(f"Control characters are not allowed in {key}")
        values[key] = value
    return values


def integer(values, key, default, minimum, maximum):
    try:
        number = int(values.get(key, str(default)))
    except ValueError:
        raise ValueError(f"{key} must be an integer") from None
    if not minimum <= number <= maximum:
        raise ValueError(f"{key} must be between {minimum} and {maximum}")
    return number


def configuration(values):
    ssid, password = values.get("WIFI_SSID", ""), values.get("WIFI_PASSWORD", "")
    if not ssid:
        return None  # An unfilled local file must not replace working NVS settings.
    if len(ssid.encode("utf-8")) > 32:
        raise ValueError("WIFI_SSID must be at most 32 UTF-8 bytes")
    if password and not 8 <= len(password.encode("utf-8")) <= 63:
        raise ValueError("WIFI_PASSWORD must be empty for an open network, or 8-63 UTF-8 bytes")
    host = values.get("PC_HOST", "192.168.1.50")
    if not re.fullmatch(r"[A-Za-z0-9.-]+", host):
        raise ValueError("PC_HOST must be a LAN IPv4 address or hostname")
    try:
        ipaddress.IPv4Address(host)
    except ValueError:
        if not all(re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?", label)
                   for label in host.split(".")) or re.fullmatch(r"[0-9.]+", host):
            raise ValueError("PC_HOST must be a valid IPv4 address or hostname") from None
    port = integer(values, "PC_PORT", 8080, 1, 65535)
    url = values.get("METRICS_URL") or f"http://{host}:{port}/api/metrics"
    parsed = urlsplit(url)
    if (parsed.scheme != "http" or not parsed.hostname or parsed.username or parsed.password
            or any(c in url for c in " \t\r\n@#") or len(url.encode("utf-8")) >= 256):
        raise ValueError("METRICS_URL must be an HTTP URL shorter than 256 UTF-8 bytes")
    try:
        parsed.port
    except ValueError:
        raise ValueError("METRICS_URL has an invalid port") from None
    view = values.get("START_VIEW", "overview").lower()
    if view not in {"overview", "charts", "network"}:
        raise ValueError("START_VIEW must be overview, charts or network")
    return {
        "ssid": ssid, "password": password, "url": url,
        "interval_ms": integer(values, "UPDATE_INTERVAL_MS", 1000, 500, 10000),
        "brightness": integer(values, "DISPLAY_BRIGHTNESS", 80, 10, 100),
        "home_view": {"overview": 0, "charts": 1, "network": 2}[view],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        config = configuration(parse_env(args.env))
    except (ValueError, UnicodeError) as exc:
        parser.exit(2, f"Local configuration error: {exc}\n")
    lines = ["/* Generated private configuration. Do not commit or share. */", "#pragma once",
             f"#define LOCAL_SETTINGS_AVAILABLE {int(config is not None)}"]
    available = config is not None
    if not available:
        config = {"ssid": "", "password": "", "url": "http://192.168.1.50:8080/api/metrics",
                  "interval_ms": 1000, "brightness": 80, "home_view": 0}
    if config:
        digest = hashlib.sha256(json.dumps(config, sort_keys=True, ensure_ascii=False).encode()).hexdigest()
        lines.append(f'#define LOCAL_SETTINGS_HASH "{digest}"')
        for key, value in config.items():
            literal = json.dumps(value, ensure_ascii=False) if isinstance(value, str) else str(value)
            lines.append(f"#define LOCAL_SETTINGS_{key.upper()} {literal}")
    header = "\n".join(lines) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != header:
        args.output.write_text(header, encoding="utf-8")


if __name__ == "__main__":
    main()
