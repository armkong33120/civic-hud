## 2026-10-13T14:59:44Z
You are OBD & Storage Explorer (teamwork_preview_explorer).
Your working directory is: /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_obd_storage_survey_3/
You MUST read:
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/ORIGINAL_REQUEST.md

Your mission:
Investigate the OBD-II, Bluetooth Classic SPP, and Storage subsystems in /Users/arm/Documents/Arduino/civic_hud and reference implementations:
1. Examine existing OBD implementation in /Users/arm/Documents/Arduino/civic_hud (BluetoothSerial, ELMDuino, task architecture, FreeRTOS tasks, polling loops).
2. Examine the older PlatformIO reference at /Users/arm/.gemini/antigravity/scratch/civic-hud/, specifically test/test_core_engines.cpp and how core engines, simulators, and tests were written.
3. Analyze Bluetooth Classic (SPP) connection state machine requirements:
   - Pairing with PIN 1234, target device name (default "OBDII").
   - Non-blocking connection attempts (must not block UI loop or boot).
   - Auto-reconnect within <= 10s upon disconnect / ignition cycle.
   - Robustness against ELM327 clone quirks (echo, SEARCHING..., NO DATA, ?, >, split packets, slow responses).
4. Analyze Odometer and PID handling:
   - PID 01 31 (Distance traveled since codes cleared, 2 bytes in km).
   - PID 01 0D (Vehicle speed, km/h) for speed integration fallback when 01 31 returns NO DATA or is unsupported.
   - ECU counter wrap/reset handling (when PID 01 31 value drops, must not lose or double-count mileage).
   - How to build an automated, host-runnable (and/or on-device) ELM327 simulator that can feed streams and verify parser + odometer + state machine.
5. Analyze Storage & Web Settings requirements:
   - Existing StorageManager: why are loadItems/saveItems stubs?
   - Storage schema for items, intervals, visibility, history records, odometer, BT credentials.
   - Non-volatile storage choices: Preferences (NVS) vs LittleFS vs SPIFFS.
   - Web Server implementation: Wi-Fi AP "CivicHUD", IP 192.168.1.1, Web server endpoints, form submission handling, live screen sync.
   - Verification strategy for Web Settings without switching Mac Wi-Fi to CivicHUD AP.

Scope boundaries:
- Read-only exploration and diagnosis. Do NOT modify source code files.
- Write your comprehensive findings to /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_obd_storage_survey_3/survey_obd_storage.md.
- Write your final handoff to /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_obd_storage_survey_3/handoff.md.
- Maintain progress.md in your directory.
- When finished, send a message to the orchestrator summarizing key findings and referencing your reports.
