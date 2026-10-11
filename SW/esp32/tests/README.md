# Regression checks

The host checks compile the current firmware method bodies with fault-injecting
I2C, queue, timer, and file stubs. Sensor task loops are limited to three queued
requests in the harness. These checks do not access hardware or replace a device
smoke test.

On Windows, use Zig 0.13.0 (the tests need its host C++ compiler):

```powershell
python -m pip install --target build/host_regressions/tools ziglang==0.13.0
python tests/run_host_regressions.py --zig build/host_regressions/tools/ziglang/zig.exe
python tests/run_architecture_tests.py --zig build/host_regressions/tools/ziglang/zig.exe
python -B -m unittest discover -s tests -p test_runtime_verifier.py
```

The 56 native checks cover I2C errors, sensor recovery, display BUSY timeout,
CSV retention, GPIO registration lifecycle, startup notification gating,
button debounce/long presses, component event publication and coordinator reception,
tick wrap, idle timeout with active transfers,
CSV downloads with concurrent append, read failures, and send failures,
and measurement-completion display notifications on success, failure, and before startup readiness.
The nine Python checks verify runtime failure detection and reboot boundaries.

The 108 architecture checks compile the production shared data type, recorder,
display, HTTP handlers, coordinator initialization/run/cleanup, component task
creation/entries, and network policy with host stubs. Every task is immediately
scheduled during creation to verify the shared gate. All six task creation failure
positions and initialization failure positions are injected. Checks cover CSV
sequence consumption and captured timestamps, partial sensor failure/non-finite
values, placeholders, SD failure followed by display notification, unchanged JSON
and since/204/503, deadlines/retry, provisioning, button access, and active transfers.
SD, drawing and HTTP stubs reject calls while the measurement mutex is held.
The Windows harness supplies a localtime_r adapter; formatting uses the production
recorder and the host C runtime. These are host policy checks, not hardware or
FreeRTOS concurrency tests.

Build after changes using the required project-root command:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

See [software architecture](../SOFTWARE_ARCHITECTURE.md) for responsibilities,
startup rollback scope and remaining device checks.

For an authorized device test, use the ESP-IDF Python environment:

```powershell
python scripts/verify_runtime.py --port COM5 --seconds 145 --output build/verification/device-run.log
```

`--reset` restarts the device before capturing. Only use it when a reset is
intended. The script writes a JSON summary beside the log. Physically enter
normal boot first if the device is still in download mode. Compare the
`existing_log_sizes` values before and after a reboot to verify log retention.
