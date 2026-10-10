# Regression checks

The host checks compile the current firmware method bodies with fault-injecting
I2C, queue, timer, and file stubs. Sensor task loops are limited to three queued
requests in the harness. These checks do not access hardware or replace a device
smoke test.

On Windows, use Zig 0.13.0 (the tests need its host C++ compiler):

```powershell
python -m pip install --target build/host_regressions/tools ziglang==0.13.0
python tests/run_host_regressions.py --zig build/host_regressions/tools/ziglang/zig.exe
python -B -m unittest discover -s tests -p test_runtime_verifier.py
```

The 28 native checks cover I2C errors, sensor recovery, display BUSY timeout,
CSV retention, GPIO registration lifecycle, and startup notification gating.
The eight Python checks verify runtime failure detection and reboot boundaries.

For an authorized device test, use the ESP-IDF Python environment:

```powershell
python scripts/verify_runtime.py --port COM5 --seconds 145 --output build/verification/device-run.log
```

`--reset` restarts the device before capturing. Only use it when a reset is
intended. The script writes a JSON summary beside the log. Physically enter
normal boot first if the device is still in download mode. Compare the
`existing_log_sizes` values before and after a reboot to verify log retention.
