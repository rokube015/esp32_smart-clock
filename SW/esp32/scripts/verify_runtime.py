"""Capture ESP32 runtime evidence without changing or erasing device storage.

Requires pyserial from the ESP-IDF Python environment. --reset explicitly
restarts the connected device; use only when a device reset is authorized.
"""
import argparse
import json
from pathlib import Path
import re
import time

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM5')
parser.add_argument('--seconds', type=float, default=145)
parser.add_argument('--reset', action='store_true')
parser.add_argument('--analyze-only', action='store_true', help='Analyze the existing output log without accessing a device')
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)
if not args.analyze_only:
    import serial
    port = serial.Serial(port=None, baudrate=115200, timeout=1)
    port.dtr = False
    port.rts = False
    port.port = args.port

    def connect():
        deadline = time.monotonic() + 30
        while True:
            try:
                port.open()
                return
            except (serial.SerialException, OSError):
                port.close()
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.5)

    connect()
    if args.reset:
        from esptool.reset import HardReset
        HardReset(port, uses_usb=True).reset()
        port.close()
        connect()

    try:
        with args.output.open('wb') as log:
            deadline = time.monotonic() + args.seconds
            print(f'Capturing {args.port} for {args.seconds:g}s into {args.output}', flush=True)
            while time.monotonic() < deadline:
                try:
                    if not port.is_open:
                        connect()
                    data = port.read(8192)
                    log.write(data)
                    log.flush()
                except (serial.SerialException, OSError):
                    port.close()
                    time.sleep(0.5)
    finally:
        port.close()


text = args.output.read_text(encoding='utf-8', errors='replace')
boot_samples = [
    [int(t) for t in re.findall(r'I \((\d+)\) smart_clock: Sensor snapshot:', boot)]
    for boot in text.split('main: Start main')
]
samples = [t for boot in boot_samples for t in boot]
intervals = [b-a for boot in boot_samples for a,b in zip(boot, boot[1:])]
report = {
    'sensor_samples': len(samples),
    'sensor_intervals_ms': intervals,
    'csv_appends': text.count('sd_card: Appended sensor log data'),
    'existing_log_sizes': [int(n) for n in re.findall(r'Sensor log: (\d+) existing bytes', text)],
    'normal_boots': text.count('main: Start main'),
    'ntp_synced': 'NTP synchronized' in text or 'NTP updated, current time is:' in text,
    'display_sleep_completions': text.count('set sleep mode.'),
    'panic': bool(re.search(r'Guru Meditation|panic\(|assert failed|Stack canary|CORRUPT HEAP', text)),
    'error_lines': [line for line in text.splitlines() if re.match(r'E \(', line)],
}
args.output.with_suffix('.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2), flush=True)
failures = []
if not samples: failures.append('No sensor measurement received')
if report['panic']: failures.append('Device panic detected')
if report['error_lines']: failures.append('Application error log detected')
if not report['ntp_synced']: failures.append('NTP sync not observed')
if not report['csv_appends']: failures.append('CSV append not observed')
if not report['display_sleep_completions']: failures.append('Display refresh completion not observed')
if args.seconds >= 90 and not intervals: failures.append('Not enough samples to verify cadence')
if any(abs(interval - 60000) > 1000 for interval in intervals):
    failures.append('Sensor measurement cadence is outside 60s +/- 1s')
if failures:
    print('FAIL: ' + '; '.join(failures), flush=True)
    raise SystemExit(1)
