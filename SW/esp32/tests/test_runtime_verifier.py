"""Check runtime verification failures and reboot boundaries without hardware."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts' / 'verify_runtime.py'
VALID = '''I (1) main: Start main
I (1000) smart_clock: Sensor snapshot: test
I (61000) smart_clock: Sensor snapshot: test
I (61001) smart_clock: NTP synchronized
I (61002) sd_card: Appended sensor log data
I (61003) display: set sleep mode.
'''


class RuntimeVerifierTests(unittest.TestCase):
    def verify(self, log, expected):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'device.log'
            path.write_text(log, encoding='utf-8')
            result = subprocess.run(
                [sys.executable, str(SCRIPT), '--analyze-only', '--output', str(path)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
            return json.loads(path.with_suffix('.json').read_text(encoding='utf-8'))

    def test_valid(self):
        self.verify(VALID, 0)

    def test_ntp_callback_log(self):
        self.verify(VALID.replace('I (61001) smart_clock: NTP synchronized',
                                  'NTP updated, current time is: 2026-10-11 12:50:42'), 0)

    def test_application_error(self):
        self.verify(VALID + 'E (62000) sd_card: Write failed\n', 1)

    def test_missing_csv(self):
        self.verify(VALID.replace('sd_card: Appended sensor log data', 'no append'), 1)

    def test_bad_cadence(self):
        self.verify(VALID.replace('(61000)', '(71000)'), 1)

    def test_panic(self):
        self.verify(VALID + 'Guru Meditation Error\n', 1)

    def test_missing_samples(self):
        self.verify(VALID.replace('Sensor snapshot:', 'other:'), 1)

    def test_missing_ntp(self):
        self.verify(VALID.replace('NTP synchronized', 'NTP pending'), 1)

    def test_reboot_boundary(self):
        report = self.verify(VALID + VALID.replace('(1000)', '(100000)').replace('(61000)', '(160000)'), 0)
        self.assertEqual(report['sensor_intervals_ms'], [60000, 60000])


if __name__ == '__main__':
    unittest.main()
