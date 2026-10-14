## 2026-10-13T15:58:44Z
You are Firmware Worker for Milestone 2: Storage & Web Settings (teamwork_preview_worker).
Your working directory is: /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/worker_m2_storage_1/

You MUST read:
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/ORIGINAL_REQUEST.md
/Users/arm/Documents/Arduino/civic_hud/PROJECT.md
/Users/arm/Documents/Arduino/civic_hud/TEST_READY.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_obd_storage_survey_3/survey_obd_storage.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/spec_miner_survey_1/survey_spec.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Exclusive file write ownership:
- /Users/arm/Documents/Arduino/civic_hud/StorageManager.h
- /Users/arm/Documents/Arduino/civic_hud/StorageManager.cpp
- /Users/arm/Documents/Arduino/civic_hud/WebSettings.h (or web handlers included by civic_hud.ino)

Your mission:
Implement Milestone 2 (Features 10-16 in PROJECT.md):
1. Complete StorageManager:
   - Genuine Preferences (NVS) for scalar config: odo, last_pid31, bt_name, bt_pin.
   - Genuine Preferences blob for items: serialize/deserialize 14+ maintenance items using packed 100-byte struct (#pragma pack(push, 1)) matching tests/test_storage_web.cpp. Ensure slide resets, interval changes, hidden states, and custom items persist across reboots.
   - LittleFS integration: mount LittleFS.begin(true) on data partition, remove SD card dependency, implement appendHistoryLog and readHistoryLogsCsv for /service_history.csv.
2. Web Settings server at 192.168.1.1 (SoftAP "CivicHUD"):
   - Render full HTML/CSS matching approved design ui_layout_c.html:
     - ODO sync field.
     - BT name & PIN fields reflecting saved values.
     - Add New Item form.
     - Full list of all items with editable interval (km) and visibility checkboxes.
     - Single "Save All" button.
     - Scrollable/tabular service history log table populated from /service_history.csv.
   - Handle POST /save: parse all fields, save to NVS immediately, reload live memory, propagate BT credentials to ObdTask, trigger immediate screen refresh.
3. Verification:
   - Run: ./tests/run_all_tests.sh (all suites must pass 100%, particularly test_storage_web.cpp).
   - Compile firmware:
     /opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud
   - Must build with 0 errors and 0 warnings.

Write handoff report to /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/worker_m2_storage_1/handoff.md.
Maintain progress.md in your directory.
Send message to orchestrator upon completion.
