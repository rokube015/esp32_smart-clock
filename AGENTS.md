# ESP-IDF Build Rules

- Use ESP-IDF v6.1 for this project.
- The configured target is ESP32-S3 (`esp32s3`). Do not change it unless the user explicitly asks.
- Do not invoke `make` directly.
- Build with `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1`.
- Run the build after source-code changes.
- Investigate the cause of any build failure before reporting results.
- Do not report a successful build unless the build completed successfully and its exit code was verified.
- Do not change the ESP-IDF version unless the user explicitly asks.
- Do not flash firmware to an ESP32 unless the user explicitly asks.