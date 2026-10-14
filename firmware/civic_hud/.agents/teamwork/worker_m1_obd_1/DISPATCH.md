## 2026-10-13T15:16:32Z
You are Firmware Worker for Milestone 1: OBD-II & BT State Machine (teamwork_preview_worker).
Your working directory is: /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/worker_m1_obd_1/

You MUST read:
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/ORIGINAL_REQUEST.md
/Users/arm/Documents/Arduino/civic_hud/PROJECT.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_obd_storage_survey_3/survey_obd_storage.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_codebase_survey_2/survey_codebase.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Exclusive file write ownership:
- /Users/arm/Documents/Arduino/civic_hud/ObdTask.h
- /Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp
- /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.h
- /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp

Your mission:
Implement Milestone 1 (Features 1-9 in PROJECT.md):
1. Fix boot panic: Call SerialBT.begin("CivicHUD_BT", true) BEFORE SerialBT.setPin("1234"). Auto-accept SSP pairing via SerialBT.onConfirmRequest([](uint32_t){ SerialBT.confirmReply(true); }).
2. Non-blocking FreeRTOS worker on Core 0:
   - Move BT connect and polling to FreeRTOS task on Core 0.
   - States: DISCONNECTED, CONNECTING, INITIALIZING_ELM, CONNECTED, RECONNECT_WAIT.
   - Connecting never blocks Core 1 (UI/touch responsiveness < 100 ms).
   - Auto-detect disconnect and auto-reconnect within <= 10s upon ignition off/on.
3. Robust ELM327 initialization and polling:
   - Handle clone quirks: echo (ATE0), SEARCHING..., NO DATA, ?, split bytes, > prompt.
4. PID 01 31 distance reading:
   - Poll PID 01 31 every 2s, calculate delta km, feed OdometerEngine::updateFromPid31.
   - Handle 16-bit wrap (65535 -> 0) and ECU DTC clear (counter drops) safely.
5. Speed integration fallback:
   - When PID 01 31 returns NO DATA or is unsupported, automatically engage speed integration via PID 01 0D (polled at 2 Hz) feeding OdometerEngine::updateFromSpeed.
6. Build verification:
   - Verify compilation with:
     /opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud
   - Must compile with 0 errors.

Write handoff report to /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/worker_m1_obd_1/handoff.md.
Maintain progress.md in your directory.
Send message to orchestrator upon completion.
