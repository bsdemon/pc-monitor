import unittest
from unittest.mock import patch
from urllib.error import URLError

from src.server.diagnostics import collect, sensor_inventory


class DiagnosticsTests(unittest.TestCase):
    def test_nested_sensors_preserve_units_nulls_and_duplicate_ids(self):
        tree = {"Text": "PC", "Children": [{"Text": "GPU", "Children": [
            {"Text": "Core", "SensorId": "/gpu/0/temperature/0",
             "Type": "Temperature", "Value": "45,0 °C", "RawValue": 45},
            {"Text": "Unavailable", "SensorId": "/gpu/0/temperature/0", "Value": None},
        ]}]}
        sensors = sensor_inventory(tree)
        self.assertEqual(len(sensors), 2)
        self.assertEqual(sensors[0]["path"], ["PC", "GPU", "Core"])
        self.assertEqual(sensors[0]["value"], "45,0 °C")
        self.assertIsNone(sensors[1]["value"])

    @patch("src.server.diagnostics.psutil.cpu_percent", return_value=12.5)
    @patch("src.server.diagnostics.urlopen", side_effect=URLError("connection refused"))
    def test_missing_provider_preserves_baseline_metrics(self, _url, _cpu):
        report = collect("http://127.0.0.1:8085")
        self.assertEqual(report["cpu"]["utilization_percent"], 12.5)
        self.assertGreater(report["ram"]["total_bytes"], 0)
        self.assertEqual(report["hardware_sensors"]["status"], "unavailable")
        self.assertEqual(report["hardware_sensors"]["sensors"], [])
        self.assertIn("connection refused", report["hardware_sensors"]["error"])

    def test_malformed_tree_is_rejected(self):
        with self.assertRaises(ValueError):
            sensor_inventory({"Children": "invalid"})


if __name__ == "__main__":
    unittest.main()
