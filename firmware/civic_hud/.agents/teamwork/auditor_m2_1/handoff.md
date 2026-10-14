# Forensic Audit Report: Milestone 2 (Storage & Web Settings)

**Work Product**: Milestone 2 deliverables (`StorageManager.h`, `StorageManager.cpp`, `WebSettings.h`, `civic_hud.ino`)  
**Profile**: General Project (Integrity Mode: `development` per ORIGINAL_REQUEST.md)  
**Verdict**: **CLEAN**

---

## 1. Observation

### Target Files Audited
- `/Users/arm/Documents/Arduino/civic_hud/StorageManager.h` (Lines 1–53)
- `/Users/arm/Documents/Arduino/civic_hud/StorageManager.cpp` (Lines 1–309)
- `/Users/arm/Documents/Arduino/civic_hud/WebSettings.h` (Lines 1–332)
- `/Users/arm/Documents/Arduino/civic_hud/civic_hud.ino` (Lines 1–482)

### Tool Invocations and Direct Outputs

1. **Pre-populated Artifact Scan**:
   Command: `find . -name '*.log' -o -name '*result*' -o -name '*output*'`
   Result: Output was empty. No pre-populated logs or fabricated attestation files exist in the project directory.

2. **Absence of Forbidden `"TODAY"` String**:
   Command: Ripgrep for literal string `"TODAY"` across all source files.
   Result: Matched only in `tests/test_storage_web.cpp:609` (verifying its absence), `PROJECT.md:43` (feature description), and `tests/test_thai_engine.cpp:631`.
   Production code in `civic_hud.ino`, `StorageManager.cpp`, `WebSettings.h`, and `MaintenanceEngine.cpp` contains zero occurrences of the literal string `"TODAY"`, replacing it with formatted Thai date strings (e.g. `"13 ต.ค. 26"`).

3. **Master E2E Test Suite Execution**:
   Command: `./tests/run_all_tests.sh`
   Result: **336 / 336 Tests Passed (100% Pass Rate)** across all 5 test suites.
   - Tier 1 (Feature Coverage): 137 / 137 Passed
   - Tier 2 (Boundary & Corner Cases): 139 / 139 Passed
   - Tier 3 (Cross-Feature Combos): 29 / 29 Passed
   - Tier 4 (Real-World Scenarios): 31 / 31 Passed
   Exit code: 0.

4. **Clean Arduino-CLI Firmware Compilation**:
   Command:
   ```bash
   /opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud
   ```
   Result: Exit code 0, 0 errors, 0 warnings.
   Verbatim output:
   ```
   Sketch uses 2026432 bytes (64%) of program storage space. Maximum is 3145728 bytes.
   Global variables use 70292 bytes (21%) of dynamic memory, leaving 257388 bytes for local variables. Maximum is 327680 bytes.
   ```

5. **Independent Forensic Integrity Verification Test**:
   Source: `/tmp/auditor_m2_forensic_test.cpp` compiled directly against project files (`StorageManager.cpp`, `MaintenanceEngine.cpp`, `OdometerEngine.cpp`, `ObdTask.cpp`).
   Command:
   ```bash
   /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17 /tmp/auditor_m2_forensic_test.cpp /Users/arm/Documents/Arduino/civic_hud/StorageManager.cpp /Users/arm/Documents/Arduino/civic_hud/MaintenanceEngine.cpp /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp /Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp -I/Users/arm/Documents/Arduino/civic_hud -o /tmp/auditor_m2_exec && /tmp/auditor_m2_exec
   ```
   Result: Exit code 0.
   Verbatim output:
   ```
   ============================================================
      INDEPENDENT FORENSIC AUDIT SUITE: MILESTONE 2 (M2)
   ============================================================
   [CHECK 1] Testing PackedItemRecord struct layout and size...
     -> PASS: sizeof(PackedItemRecord) == 100 bytes, packing verified.
   [CHECK 2] Testing NVS Scalar Key Persistence...
     -> PASS: All scalar keys (odo, last_pid31, bt_name, bt_pin) persist correctly.
   [CHECK 3] Testing Maintenance Items Blob Serialization & Limits...
     -> PASS: Blob roundtrip OK, max 24 items cap strictly enforced.
   [CHECK 4] Testing LittleFS History CSV Append and Parsing...
     -> PASS: LittleFS CSV logging and parsing genuine and robust.
   [CHECK 5] Testing WebSettings HTML Output against design rubric...
     -> PASS: WebSettings HTML layout completely satisfies ui_layout_c.html design.
   [CHECK 6] Testing POST /save Atomic Form Processing...
     -> PASS: POST /save updates live memory, persists to NVS, and flags screen dirty.
   [CHECK 7] Testing Serial Command Bridge Execution...
     -> PASS: Serial Command Bridge handles commands identically to HTTP POST.
   [CHECK 8] Simulating Hardware Cold Reboot...
     -> PASS: All settings, items, history, and ODO survive cold reboot!

   ============================================================
   🎉 ALL FORENSIC INTEGRITY AUDIT CHECKS PASSED WITH 100% SUCCESS!
   ============================================================
   ```

6. **Adversarial Edge-Case & Stress Test**:
   Source: `/tmp/auditor_m2_adversarial_test.cpp`
   Command:
   ```bash
   /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17 /tmp/auditor_m2_adversarial_test.cpp /Users/arm/Documents/Arduino/civic_hud/StorageManager.cpp /Users/arm/Documents/Arduino/civic_hud/MaintenanceEngine.cpp /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp /Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp -I/Users/arm/Documents/Arduino/civic_hud -o /tmp/auditor_m2_adv_exec && /tmp/auditor_m2_adv_exec
   ```
   Result: Exit code 0.
   Verbatim output:
   ```
   ============================================================
     ADVERSARIAL STRESS TEST: MILESTONE 2 EDGE CASES & ATTACKS
   ============================================================
   [STRESS 1] Testing Odometer parser resilience under garbage input...
     -> PASS: Negative and non-numeric ODO safely handled without throw.
   [STRESS 2] Testing URL Decoder with malformed percent sequences...
     -> PASS: Malformed percent strings do not crash urlDecode.
   [STRESS 3] Testing buffer truncation with 500-char name...
     -> PASS: Ultra-long names safely bounded by sizeof(name)-1.
   [STRESS 4] Testing CSV history parser with malformed rows...
     -> PASS: History CSV parser extracts valid rows safely.
   [STRESS 5] Testing handleSavePayload with completely empty body...
     -> PASS: Empty payload handled cleanly without crash.

   ============================================================
   🎉 ALL ADVERSARIAL STRESS TESTS PASSED SUCCESSFULLY!
   ============================================================
   ```

---

## 2. Logic Chain

1. **Genuine NVS Preferences API Usage (No Hollow Facades)**:
   - In `StorageManager.cpp:28`, `prefs.begin("civichud", false)` opens the NVS namespace in read/write mode.
   - Scalars use genuine type-specific NVS APIs: `prefs.putUInt` / `prefs.getUInt` for `"odo"` and `"last_pid31"`, `prefs.putString` / `prefs.getString` for `"bt_name"` and `"bt_pin"`.
   - All key names (`"odo"`, `"last_pid31"`, `"bt_name"`, `"bt_pin"`, `"item_count"`, `"items_blob"`) are $\le 10$ characters, satisfying the ESP32 NVS 15-character key limit.
   - Item records use `#pragma pack(push, 1)` to enforce an exact 100-byte footprint (`sizeof(PackedItemRecord) == 100`).
   - `StorageManager::saveItems` persists items as a binary blob via `prefs.putBytes("items_blob", ...)`, and `StorageManager::loadItems` validates `blobLen == count * sizeof(PackedItemRecord)` before unpacking, protecting against partial/corrupt NVS data.
   - Rejection of oversized collections ($> 24$ items) is strictly enforced.

2. **Genuine LittleFS File Operations**:
   - `StorageManager.cpp:29` initializes LittleFS via `LittleFS.begin(true)` with `formatOnFail = true`.
   - `appendHistoryLog` opens `/service_history.csv` in append mode (`"a"`), formats CSV records with 6 standard columns, and explicitly flushes/closes the file.
   - `exportHistoryCsv` and `loadHistoryLogs` safely read and parse CSV records, with parsing bounds checks and `std::stoul` error handling that safely skips corrupt lines.
   - External SD hardware dependency has been completely replaced with genuine on-flash LittleFS.

3. **Web Settings HTML Rendering & Design Rubric Compliance**:
   - `WebSettings.h:renderHtmlPage` generates complete, semantic HTML/CSS matching the approved `ui_layout_c.html` specification:
     - Card 1: Odometer Sync (`<input type='number' name='odo' value='...'>`).
     - Card 2: Bluetooth OBD-II Settings (`name='bt_name'` and `name='bt_pin'`).
     - Card 3: Add Custom Item form (`name='new_name'` and `name='new_km'`).
     - Card 4: Items Table with visibility checkboxes (`vis_<id>`) and interval inputs (`km_<id>`).
     - Single "Save All" button: `💾 บันทึกการตั้งค่าทั้งหมด (Save All)`.
     - Card 5: Service History Table (5 columns: วันที่, รายการ, เลขไมล์, ระยะรอบนั้น, กำหนดรอบถัดไป) + CSV download link (`/history.csv`).

4. **Genuine POST Payload Parsing & Synchronization**:
   - `WebSettings::handleSavePayload` implements standard form decoding (`urlDecode` correctly converting `+` to spaces and `%XX` hex bytes including multi-byte Thai UTF-8 characters).
   - Updates odometer via `odoEngine.calibrate()` and NVS save.
   - Propagates new Bluetooth credentials to NVS and re-initializes `obdTask.begin(curBtName, curBtPin, &odoEngine)`.
   - Applies interval changes and visibility toggles to live `maintEngine`.
   - Appends custom items (up to the 24-item ceiling).
   - Re-evaluates item urgency immediately (`maintEngine.evaluate()`).
   - Sets `screenDirty = true`, causing `civic_hud.ino:loop()` to redraw the UI without delay.
   - Returns HTTP 303 Redirect to re-anchor the browser to `/`.

5. **Zero-Risk Serial Verification Bridge**:
   - In `civic_hud.ino:110-116`, incoming serial commands starting with `CMD:WEB_SAVE ` execute `WebSettings::handleSavePayload()`, enabling automated verification without requiring the host Mac to switch Wi-Fi off the internet.
   - `CMD:DUMP_STORAGE` emits current NVS state and LittleFS CSV history over serial for verification.

---

## 3. Caveats

- In-car vehicle tests with a physical ELM327 dongle were deferred to the owner per `ORIGINAL_REQUEST.md` line 5 ("The owner will do the final in-car test themselves").
- Physical LittleFS flash wear endurance over tens of thousands of writes is mitigated by wear-leveling in `OdometerEngine.checkAndClearNvsDirty(NVS_SAVE_INTERVAL_KM)` saving every 5 km rather than every meter.
- No other caveats.

---

## 4. Conclusion

**Verdict: CLEAN**

Milestone 2 implementation (`StorageManager.h`, `StorageManager.cpp`, `WebSettings.h`, `civic_hud.ino`) contains **ZERO integrity violations**:
1. No hardcoded test outputs or return constants.
2. No dummy or facade implementations (NVS Preferences, LittleFS file operations, HTML rendering, and POST parsing are genuine, robust, and empirically verified).
3. No circumvention of requirements. The literal `"TODAY"` string has been eliminated and replaced with structured Thai date formatting.
4. Firmware compiles cleanly (0 errors, 0 warnings).
5. All 336 project tests, 8 dedicated forensic audit tests, and 5 adversarial stress tests pass with a 100% success rate.

The work product is approved.

---

## 5. Verification Method

To independently reproduce this forensic audit:

1. **Verify Master E2E Test Suite**:
   ```bash
   cd /Users/arm/Documents/Arduino/civic_hud
   ./tests/run_all_tests.sh
   ```
   *Expected outcome*: Exit code 0, 336 / 336 tests passed.

2. **Verify Firmware Compilation**:
   ```bash
   /opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud
   ```
   *Expected outcome*: Exit code 0, 0 errors, 0 warnings (Flash ~64%, RAM ~21%).

3. **Verify Independent Forensic Audit Suite**:
   ```bash
   /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17 /tmp/auditor_m2_forensic_test.cpp /Users/arm/Documents/Arduino/civic_hud/StorageManager.cpp /Users/arm/Documents/Arduino/civic_hud/MaintenanceEngine.cpp /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp /Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp -I/Users/arm/Documents/Arduino/civic_hud -o /tmp/auditor_m2_exec && /tmp/auditor_m2_exec
   ```
   *Expected outcome*: Exit code 0, all 8 checks PASS.

4. **Verify Adversarial Stress Suite**:
   ```bash
   /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17 /tmp/auditor_m2_adversarial_test.cpp /Users/arm/Documents/Arduino/civic_hud/StorageManager.cpp /Users/arm/Documents/Arduino/civic_hud/MaintenanceEngine.cpp /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp /Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp -I/Users/arm/Documents/Arduino/civic_hud -o /tmp/auditor_m2_adv_exec && /tmp/auditor_m2_adv_exec
   ```
   *Expected outcome*: Exit code 0, all 5 stress scenarios PASS.
