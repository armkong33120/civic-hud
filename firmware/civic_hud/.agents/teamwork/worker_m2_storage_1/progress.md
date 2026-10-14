# Progress Log - Milestone 2: Storage & Web Settings

Last visited: 2026-10-13T16:10:00Z

## Status Summary
- **StorageManager Complete**:
  - Implemented genuine Preferences (NVS) for scalar config (`odo`, `last_pid31`, `bt_name`, `bt_pin`).
  - Implemented packed 100-byte POD struct (`PackedItemRecord` with `#pragma pack(push, 1)`) for maintenance items persistence in NVS blob (`items_blob`, `item_count`), supporting slide resets, interval changes, visibility flags, and custom items up to 24 items.
  - Implemented LittleFS data partition mounting (`LittleFS.begin(true)`), eliminating SD card dependency, and logging confirmed service history to `/service_history.csv` with robust CSV reading and parsing.
- **WebSettings Complete**:
  - Implemented SoftAP HTTP Web Settings server (`http://192.168.1.1`, SSID `CivicHUD`) matching approved design `ui_layout_c.html`.
  - Implemented full HTML rendering with ODO sync, BT name & PIN, Add Custom Item form, all maintenance items with editable intervals (km) and visibility checkboxes, single "Save All" button (`💾 บันทึกการตั้งค่าทั้งหมด (Save All)`), and formatted service history table.
  - Implemented `POST /save` handler with URL-decoding, atomic NVS persistence, live memory update, BT credentials propagation to `ObdTask`, and immediate screen refresh trigger.
  - Implemented on-device Serial Command Bridge (`CMD:WEB_SAVE <payload>` and `CMD:DUMP_STORAGE`) for zero-risk verification without disconnecting the Mac from Wi-Fi.
  - Replaced literal `"TODAY"` date placeholder with formatted Thai date string (`"13 ต.ค. 26"`).
- **Verification**:
  - All 336 test cases in master test runner `./tests/run_all_tests.sh` pass 100%.
  - Dedicated Milestone 2 host test `/tmp/test_m2_storage_verify.cpp` compiled and passed 100%.
  - Full firmware compiled with `arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800` with 0 errors and 0 warnings (Sketch size: 2,026,432 bytes / 64%, RAM: 70,292 bytes / 21%).
