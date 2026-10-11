"""Plot sensor history: python scripts/plot_sensor_log.py [csv] [--output png]."""
import argparse
import csv
import math
from datetime import datetime
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.dates as mdates
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('csv', nargs='?', type=Path, default=Path('build/verification/sensor_log-latest.csv'))
parser.add_argument('--output', type=Path)
args = parser.parse_args()
output = args.output or args.csv.with_suffix('.png')
records = []
skipped = []
with args.csv.open(encoding='utf-8-sig', newline='') as stream:
    for line, row in enumerate(csv.reader(stream, skipinitialspace=True), 1):
        if not row or row[0].strip() == 'YYYY/MM/DD':
            continue
        try:
            if len(row) != 7:
                raise ValueError('expected seven columns')
            timestamp = datetime.strptime(row[0].strip() + ' ' + row[2].strip(), '%Y/%m/%d %H:%M:%S')
            values = [float(value) for value in row[3:7]]
            if not all(math.isfinite(value) for value in values):
                raise ValueError('nonfinite value')
            records.append((timestamp, *values))
        except ValueError as error:
            skipped.append((line, str(error)))
if not records:
    raise SystemExit('No valid sensor records')
records.sort(key=lambda record: record[0])
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams['font.family'] = 'DejaVu Sans'
fig, axes = plt.subplots(4, 1, figsize=(12, 9), sharex=True, layout='constrained')
labels = ['CO₂ (ppm)', 'Temperature (°C)', 'Humidity (%)', 'Pressure (hPa)']
colors = ['#2563eb', '#dc2626', '#0891b2', '#7c3aed']
times = [record[0] for record in records]
for column, (axis, label, color) in enumerate(zip(axes, labels, colors), 1):
    # Break the line at recording gaps longer than five minutes.
    plot_times, values = [], []
    for i, record in enumerate(records):
        if i and (record[0] - records[i - 1][0]).total_seconds() > 300:
            plot_times.append(record[0])
            values.append(float('nan'))
        plot_times.append(record[0])
        values.append(record[column])
    axis.plot(plot_times, values, color=color, linewidth=1.2)
    axis.set_ylabel(label)
    axis.margins(x=0.01)
    axis.grid(alpha=0.3)
locator = mdates.AutoDateLocator()
axes[-1].xaxis.set_major_locator(locator)
axes[-1].xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
axes[-1].set_xlabel('Time (JST)')
fig.suptitle(f'Smart Clock sensor history | {len(records):,} records\n'
             f'{times[0]:%Y-%m-%d %H:%M} – {times[-1]:%Y-%m-%d %H:%M} JST', fontsize=14)
output.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(output, dpi=180)
plt.close(fig)
print(f'Saved {output.resolve()} ({len(records)} records; {len(skipped)} skipped)')
for line, error in skipped:
    print(f'Skipped CSV line {line}: {error}')
