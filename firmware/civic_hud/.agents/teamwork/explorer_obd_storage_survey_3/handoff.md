# Handoff Report: OBD-II, Bluetooth Classic SPP, Odometer Engine, Storage & Web Settings

**Agent:** OBD & Storage Explorer (`teamwork_preview_explorer`)  
**Working Directory:** `/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_obd_storage_survey_3/`  
**Date:** 2026-10-13  
**Status:** Complete (Hard Handoff)  
**Detailed Report:** [survey_obd_storage.md](survey_obd_storage.md)  

---

## 1. Observation

1. **Blocking Bluetooth Connect at Boot**:
   - In `ObdTask.cpp:35`, `SerialBT.connect(m_btDeviceName.c_str())` is called synchronously in `ObdTask::begin()`.
   - In ESP32 Arduino Core (`BluetoothSerial.cpp:989`), `connect()` calls `retval = waitForConnect(SCAN_TIMEOUT);` where `#define SCAN_TIMEOUT (INQ_LEN * 2 * 1000)` (`BluetoothSerial.cpp:73`), which blocks for up to 20–25 seconds if the ELM327 dongle is unpowered.
   - In `civic_hud.ino:91`, `g_obdTask.begin()` is invoked directly in `setup()`. If the ELM327 dongle is not reachable, boot halts for ~20 s.

2. **No Reconnect & Permanently Dead OBD Loop**:
   - In `ObdTask.cpp:50-52`:
     ```cpp
     void ObdTask::update() {
     #ifdef ARDUINO
         if (!m_telemetry.obdConnected) {
             return;
         }
     ```
   - When boot connection fails, `m_telemetry.obdConnected` remains `false`. `update()` returns immediately forever. There is zero retry mechanism or timer.
   - If Bluetooth connection drops while driving, `SerialBT.connected()` is never checked, and no auto-reconnect is attempted.

3. **ELMDuino State-Cycling Bug**:
   - In `ObdTask.cpp:60-89`, `m_queryState` cycles sequentially every 500 ms through `case 0 (kph)`, `case 1 (engineCoolantTemp)`, `case 2 (batteryVoltage)`.
   - In `ELMduino.cpp:693-748`, `processPID()` is asynchronous. On the first call, it sends the command and returns `0.0` while `nb_rx_state = ELM_GETTING_MSG`.
   - Because `ObdTask.cpp` switches `m_queryState` immediately without waiting for `nb_rx_state == ELM_SUCCESS`, on the next 500 ms tick it calls the wrong PID handler while `ELMDuino` is still receiving the previous response. Responses fail header matching in `findResponse()` (`ELMduino.cpp:2752`).

4. **Missing PID 01 31 & Disconnected Odometer**:
   - `ObdTask.cpp` does not poll PID `01 31` (`distSinceCodesCleared()`).
   - In `civic_hud.ino:102-131`, `g_odoEngine.updateFromPid31()` and `g_odoEngine.updateFromSpeed()` are never called anywhere in `loop()`. On-screen odometer and maintenance countdowns never advance.

5. **StorageManager Stubs**:
   - In `StorageManager.cpp:44-62`:
     ```cpp
     bool StorageManager::loadItems(std::vector<MaintenanceItem>& items) {
     #ifdef ARDUINO
         size_t size = prefs.getBytesLength("items");
         if (size == 0) return false;
         return true; // Empty stub
     #else
         return false;
     #endif
     }
     bool StorageManager::saveItems(const std::vector<MaintenanceItem>& items) {
     #ifdef ARDUINO
         return true; // Empty stub
     #else
         return true;
     #endif
     }
     ```
   - In `StorageManager.cpp:88-100`, `appendHistoryLog` calls `SD.open(m_csvHistoryPath.c_str(), FILE_APPEND)`. `SD.begin()` is never called in `civic_hud.ino`, and no SD card is present in the CYD board. History logs fail silently.

6. **Web Settings Incomplete**:
   - In `civic_hud.ino:456-516`, `setupWebServer()` only renders `<input name='odo'>`, `<input name='bt_name' value='OBDII'>`, and `<input name='bt_pin' value='1234'>`.
   - Missing: per-item interval editing, visibility toggles, add-item inputs, and the service history log table designed in `ui_layout_c.html`.
   - In `civic_hud.ino:501-505`, saving `bt_name` and `bt_pin` updates NVS but never notifies `g_obdTask` to apply the new credentials.

7. **Compilation & Resource Verification**:
   - Compilation command:
     `/opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud`
   - Output: `Sketch uses 1747119 bytes (55%) of program storage space. Maximum is 3145728 bytes. Global variables use 64548 bytes (19%) of dynamic memory, leaving 263132 bytes for local variables.`
   - Host unit test runner compiled cleanly with `clang++` in 1.4s, and passed 100% of native core engine tests.

---

## 2. Logic Chain

1. **From Observation 1 to Boot Hang**:
   `SerialBT.connect()` blocking for `SCAN_TIMEOUT` (20-25 s) in `setup()` means that whenever the vehicle is off or the dongle is not advertising, the CYD display remains blank and unbooted for ~20 s.
2. **From Observation 2 to Permanent Disconnect**:
   Because `ObdTask::update()` returns early when `!m_telemetry.obdConnected` and never initiates a retry, any failure at boot or any disconnection while driving permanently disables all OBD features until a manual hardware reset.
3. **From Observations 3 and 4 to Static Odometer**:
   Because `ObdTask` does not query PID `01 31`, and because telemetry is never fed to `g_odoEngine`, the odometer reading never advances. Maintenance items never decrement their countdowns, rendering the core purpose of the HUD inoperative.
4. **From Observation 5 to Data Loss**:
   Because `loadItems` and `saveItems` are empty stubs, all slide-to-confirm resets, interval edits, hidden items, and custom additions are held only in volatile RAM. They are completely lost on reboot.
5. **From Observation 5 to LittleFS Solution**:
   Because the CYD board lacks an SD card but has an 896 KB data partition (`spiffs` subtype in `huge_app.csv`), mounting `LittleFS` allows saving `/service_history.csv` reliably to on-board flash with zero external hardware dependencies.
6. **From Observation 6 & Constraint 45 to Verification Strategy**:
   Because Mac Wi-Fi cannot connect to `"CivicHUD"` AP (which terminates internet), we must test the Web Settings handlers via host-side unit tests and an on-device USB-serial test bridge.

---

## 3. Caveats

1. **Physical Car CAN Bus Differences**:
   While the Honda Civic FC 1.8 R18Z1 uses standard ISO 15765-4 CAN 11-bit 500k (`ATSP6`), individual ECUs may return `NO DATA` for PID `01 31` if non-standard firmware is installed. The speed-integration fallback (PID `01 0D`) is mandatory.
2. **Thai Font Engine**:
   Font rendering on TFT_eSPI is investigated by the UI explorer; the storage and Web server subsystems handle Thai names as UTF-8 byte sequences without alteration.
3. **Date Representation**:
   Per the owner's explicit deferral in `ORIGINAL_REQUEST.md`, real NTP/RTC date synchronization is out of scope. The placeholder date representation must avoid the literal `"TODAY"` string and provide a structured date format (e.g. `"13 ต.ค. 26"`).

---

## 4. Conclusion

The OBD, Bluetooth, Storage, and Web Server subsystems require targeted remediation across five concrete modules:

1. **Core 0 FreeRTOS OBD Worker & State Machine**:
   Move OBD management to a FreeRTOS task on Core 0. Implement states: `DISCONNECTED`, `CONNECTING`, `INITIALIZING_ELM`, `QUERYING`, `RECONNECT_WAIT`. Ensure `SerialBT.onConfirmRequest` auto-confirms SSP pairing, and connection status is reported to Core 1 via atomic variables or mutex.
2. **PID 0x31 Polling with Speed Fallback**:
   Poll PID `01 31` every 2s. If `NO DATA` is returned, engage speed-integration fallback using PID `01 0D` (polled at 2 Hz). Handle ECU 16-bit counter wrap and reset so mileage is neither lost nor double-counted.
3. **StorageManager Completion**:
   - Persist `odo`, `last_pid31`, `bt_name`, `bt_pin` in Preferences (NVS).
   - Persist 24 maintenance items as a serialized POD blob in Preferences (`items` key, ~2.4 KB).
   - Mount `LittleFS.begin(true)` and append service records to `/service_history.csv`.
4. **Full Web Settings Implementation**:
   Update `civic_hud.ino` WebServer to generate the complete form matching `ui_layout_c.html`: ODO sync, BT credentials, per-item intervals, visibility toggles, add-item fields, and service history log table. Handle `POST /save`, trigger immediate screen recalculation, and reload updated BT credentials into `ObdTask`.
5. **Standalone ELM327 Mock Simulator Test Runner**:
   Provide a host-runnable C++ test suite compiling via `clang++` that feeds realistic streams (echo, `SEARCHING...`, `NO DATA`, `?`, split packets) and asserts parser, odometer delta, reset handling, and connection state machine.

---

## 5. Verification Method

To independently verify all findings and validate future implementations:

### 5.1 Host Core Engine & Simulator Build
Run host compilation using the exact command without triggering Xcode prompts:
```bash
/Library/Developer/CommandLineTools/usr/bin/clang++ \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  -std=c++17 \
  /Users/arm/.gemini/antigravity/scratch/civic-hud/test/test_core_engines.cpp \
  /Users/arm/.gemini/antigravity/scratch/civic-hud/src/core/OdometerEngine.cpp \
  /Users/arm/.gemini/antigravity/scratch/civic-hud/src/core/MaintenanceEngine.cpp \
  -o /tmp/test_engine_verify && /tmp/test_engine_verify
```
*Expected Output*: Returns code 0 with `🎉 ALL TESTS PASSED! ALL ENGINES VERIFIED 100%`.

### 5.2 Device Firmware Compilation
Run Arduino CLI compile:
```bash
/opt/homebrew/bin/arduino-cli compile \
  --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 \
  /Users/arm/Documents/Arduino/civic_hud
```
*Expected Output*: Returns code 0, sketch size < 3,145,728 bytes.

### 5.3 Invalidation Conditions
- If `SerialBT.connect()` is called on the main thread during boot, the screen will freeze for > 15 seconds when no ELM327 is present (Invalidated if UI/touch loop stalls > 100 ms).
- If `StorageManager::loadItems` is called after reboot and returns empty or default items after a slide-to-confirm reset or interval change (Invalidated if changes are lost).
- If PID `01 31` returns `NO DATA` on a car and on-screen mileage stops advancing (Invalidated if speed integration does not automatically advance the odometer).
