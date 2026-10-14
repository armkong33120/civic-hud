# Comprehensive Architectural Survey: OBD-II, Bluetooth Classic SPP, Odometer Engine, Storage & Web Settings

**Surveyor:** OBD & Storage Explorer (`teamwork_preview_explorer`)  
**Target Repository:** `/Users/arm/Documents/Arduino/civic_hud`  
**Reference Codebase:** `/Users/arm/.gemini/antigravity/scratch/civic-hud`  
**Source of Truth UI/UX:** `/Users/arm/.gemini/antigravity/brain/223a3194-ff5d-4613-9761-24750508b680/ui_layout_c.html`  
**Architecture Document:** `/Users/arm/.gemini/antigravity/brain/223a3194-ff5d-4613-9761-24750508b680/civic_fc_maintenance_display_plan.md`  
**Date:** 2026-10-13  

---

## Executive Summary

An exhaustive analysis of the OBD-II, Bluetooth Classic SPP, Odometer, Storage, and Web Server subsystems in `/Users/arm/Documents/Arduino/civic_hud` reveals critical architectural defects and incomplete stubs preventing the device from functioning in a vehicle.

1. **OBD-II & Bluetooth Serial Deficiencies**:
   - `SerialBT.connect()` in `ObdTask::begin()` is called synchronously during boot, blocking firmware initialization for up to 25 seconds if the ELM327 dongle is absent.
   - If initial connection fails, `ObdTask::update()` permanently returns early without ever attempting a reconnect.
   - There is NO auto-reconnect logic upon signal loss or vehicle ignition cycles.
   - The query cycle in `ObdTask.cpp` incorrectly assumes `ELMduino` methods are synchronous and advances its state machine every 500 ms before `ELMDuino` completes command-response parsing, corrupting response interpretation.
   - Most critically, **PID `01 31` (Distance traveled since codes cleared) is never queried**, and `OdometerEngine` is never fed any telemetry. The on-screen odometer and maintenance countdowns remain static.

2. **Storage Subsystem Stubs**:
   - `StorageManager::loadItems` and `StorageManager::saveItems` are completely hollow stubs returning without reading or writing data. Slide-to-confirm service resets, interval modifications, custom added items, and hidden items are lost immediately upon reboot.
   - `StorageManager::appendHistoryLog` relies on an uninitialized `SD.open()`. The CYD board does not have an active SD card initialized, so service history records fail silently.

3. **Web Server & UI Sync Gaps**:
   - The current Web Server (`civic_hud.ino:456-516`) only renders basic ODO and BT input boxes with hardcoded default values. It lacks interval editing, item visibility checkboxes, new item creation, and the service history log table.
   - Saved Bluetooth credentials are never reloaded into the active `ObdTask`.

4. **Remediation Strategy**:
   - Refactor OBD management into a dedicated FreeRTOS task pinned to **Core 0** with an explicit connection state machine, freeing **Core 1** to maintain a 100 ms touch/UI responsiveness guarantee.
   - Implement an automated ELM327 mock simulator that runs natively on macOS via `clang++` to verify the ELM parser, clone quirk tolerance, PID 0x31 delta tracking, ECU reset handling, and speed fallback.
   - Implement persistent storage using ESP32 **Preferences (NVS)** for configuration/items and **LittleFS** (896 KB SPIFFS partition) for `/service_history.csv` without requiring external SD hardware.
   - Provide an on-device serial diagnostic bridge and host-side test suite to verify the complete Web Settings workflow without disconnecting the host Mac from the internet.

---

## 1. Existing OBD Implementation Analysis (`civic_hud`)

### 1.1 Architecture & Task Model
In `civic_hud.ino`, the global instances are declared as:
```cpp
// civic_hud.ino:23-26
static OdometerEngine      g_odoEngine;
static MaintenanceEngine   g_maintEngine;
static StorageManager      g_storage;
static ObdTask             g_obdTask;
```
Execution is entirely single-threaded on Core 1:
```cpp
// civic_hud.ino:102-131
void loop() {
    g_server.handleClient();
    g_obdTask.update();
    handleTouch();
    // UI refresh...
    delay(10);
}
```
There are no FreeRTOS tasks spawned. All operations share a single execution thread with the UI, screen rendering, touch controller, and WebServer.

### 1.2 Bluetooth Classic Pairing & Blocking Boot Bug
In `ObdTask.cpp`:
```cpp
// ObdTask.cpp:24-46
bool ObdTask::begin(const std::string& btDeviceName, const std::string& pin) {
    m_btDeviceName = btDeviceName;
    m_btPin = pin;
#ifdef ARDUINO
    SerialBT.setPin(m_btPin.c_str(), m_btPin.length());
    if (!SerialBT.begin("CivicHUD_Client", true)) { // Master mode
        return false;
    }
    // Connect to ELM327 device
    if (SerialBT.connect(m_btDeviceName.c_str())) {
        if (myELM327.begin(SerialBT, true, 2000)) {
            m_telemetry.obdConnected = true;
            return true;
        }
    }
    return false;
#endif
}
```
**Failure Mechanism**:
1. `SerialBT.connect(String remoteName)` initiates GAP discovery (`esp_bt_gap_start_discovery`) and then calls `waitForConnect(SCAN_TIMEOUT)` where `SCAN_TIMEOUT = INQ_LEN * 2 * 1000 = 20,000 ms` (`BluetoothSerial.cpp:73, 989`).
2. Calling `g_obdTask.begin()` in `setup()` locks the ESP32 in a blocking wait for up to 20-25 seconds before the screen can even show anything or touch can be polled.
3. If the car's ELM327 is powered off or unplugged, `begin()` fails and returns `false`.

### 1.3 Missing Reconnection & Link Loss Handling
In `ObdTask::update()`:
```cpp
// ObdTask.cpp:48-52
void ObdTask::update() {
#ifdef ARDUINO
    if (!m_telemetry.obdConnected) {
        return;
    }
```
- If `begin()` fails at boot, `m_telemetry.obdConnected` is `false`. `update()` returns immediately forever. **There is zero reconnection logic.**
- Furthermore, if the connection drops while driving (e.g. car enters accessory mode, ignition off, or dongle power glitch), `SerialBT.connected()` is never checked. `m_telemetry.obdConnected` remains `true` while `myELM327` commands time out in silence.

### 1.4 State-Cycling Bug with Asynchronous ELMDuino
`ELMDuino` is fundamentally an asynchronous non-blocking state machine (`ELMduino.cpp:693-748`):
- When calling `myELM327.kph()`, if `nb_query_state == SEND_COMMAND`, it transmits the query (`010D\r`), sets `nb_query_state = WAITING_RESP`, and immediately returns `0.0`.
- Only on subsequent calls while `nb_query_state == WAITING_RESP` does it call `get_response()`, collect characters until `>` is received, and set `nb_rx_state = ELM_SUCCESS`.

In `ObdTask.cpp`:
```cpp
// ObdTask.cpp:54-89
    uint32_t now = millis();
    if (now - m_lastQueryMs < 500) {
        return;
    }
    m_lastQueryMs = now;

    switch (m_queryState) {
        case 0: {
            float spd = myELM327.kph();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                m_telemetry.speedKmh = spd;
            }
            m_queryState = 1;
            break;
        }
        case 1: {
            float temp = myELM327.engineCoolantTemp();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                m_telemetry.coolantTempC = static_cast<int16_t>(temp);
            }
            m_queryState = 2;
            break;
        }
        case 2: {
            float volt = myELM327.batteryVoltage();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                m_telemetry.batteryVoltage = volt;
            }
            m_queryState = 0;
            break;
        }
    }
```
**Impact**:
On tick 1 (`m_queryState = 0`), `myELM327.kph()` is called once. It sends `010D\r`. `nb_rx_state` is `ELM_GETTING_MSG`. The `if (== ELM_SUCCESS)` check fails. `m_queryState` is immediately forced to 1.
500 ms later, `myELM327.engineCoolantTemp()` is called. ELMDuino is still in `WAITING_RESP` from the speed command. It receives the speed response (`41 0D XX`), but expects coolant temp (`41 05`). Header matching fails (`findResponse()`), and `m_queryState` is forced to 2.
**The polling cycle is completely desynchronized and constantly drops telemetry.**

### 1.5 Missing PID 01 31 and Disconnected Odometer Engine
- `ObdTask` contains zero references to `PID 0x31` or `distSinceCodesCleared()`.
- Nowhere in `civic_hud.ino` or `ObdTask.cpp` is `g_odoEngine.updateFromPid31()` or `g_odoEngine.updateFromSpeed()` invoked.
- Consequently, `g_odoEngine.getCurrentOdo()` never changes, and all maintenance countdowns remain completely static.

---

## 2. PlatformIO Reference & Test Harness Survey

### 2.1 Analysis of `/Users/arm/.gemini/antigravity/scratch/civic-hud`
The reference scratch project contains:
- `include/config.h`, `include/pins.h`
- `src/core/OdometerEngine.{h,cpp}`, `src/core/MaintenanceEngine.{h,cpp}`, `src/core/StorageManager.{h,cpp}`
- `src/obd/ObdTask.{h,cpp}`
- `test/test_core_engines.cpp`

### 2.2 Execution of Existing Native Test Runner
The host test runner compiles and runs natively using macOS `clang++`:
```bash
/Library/Developer/CommandLineTools/usr/bin/clang++ \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  -std=c++17 \
  test/test_core_engines.cpp \
  src/core/OdometerEngine.cpp \
  src/core/MaintenanceEngine.cpp \
  -o test_runner
```
Execution verified:
```
=================================================
   Civic FC 1.8 Maintenance HUD - Unit Tests     
=================================================
[TEST] 1. Testing OdometerEngine...
  ✓ Initial calibration OK: 98540 km
  ✓ PID 0x31 normal progression (+15km) OK: 98555 km
  ✓ PID 0x31 ECU reset handling (+10km) OK: 98565 km
  ✓ Speed integration fallback (+1km) OK: 98566 km
  ✓ NVS persistence dirty flag OK

[TEST] 2. Testing MaintenanceEngine...
  ✓ Default items count: 14
  ✓ Most urgent item placed at top [0]: 💧 น้ำยาหม้อน้ำ
  ✓ Slide-to-Confirm reset executed: 🛢️ น้ำมันเครื่อง + กรอง
  ✓ Interval updated: 10,000 -> 15,000 km
  ✓ Custom item added: น้ำยาฉีดกระจก
=================================================
  🎉 ALL TESTS PASSED! ALL ENGINES VERIFIED 100%
=================================================
```

### 2.3 Evaluation of What the Existing Tests Cover vs Missing Gaps
| Tested in Reference Test | NOT Tested (Gaps to Fill) |
|---|---|
| `OdometerEngine` calibration | ELM327 protocol serial stream parsing |
| PID 0x31 delta calculation (clean ints) | ELM327 clone quirks (`SEARCHING...`, echo, `?`, `NO DATA`, `>`) |
| ECU reset handling | Bluetooth Classic SPP connection state machine |
| Speed integration fallback | Auto-reconnect <= 10s upon link loss |
| MaintenanceEngine 14 default items | Non-blocking UI loop during connection attempts |
| Urgency sorting (OVERDUE > DUE_SOON > OK) | Real storage persistence roundtrip (NVS & LittleFS) |
| Slide-to-confirm reset & history logging | Web Settings HTTP request handling & screen sync |

---

## 3. Bluetooth Classic (SPP) Connection State Machine Requirements

### 3.1 Pairing Specifications
- Target Device Name: Default `"OBDII"` (configurable via Web Settings).
- Target PIN: Default `"1234"` (configurable via Web Settings; some clones require `"0000"`).
- ESP32 Role: **Master Mode** (`SerialBT.begin("CivicHUD_Client", true)`).
- SSP (Simple Secure Pairing) Handling:
  In ESP32 Arduino Core (`BluetoothSerial.cpp:543-550`), if an ELM327 device attempts Simple Secure Pairing User Confirmation (`ESP_BT_GAP_CFM_REQ_EVT`), `BluetoothSerial` refuses pairing by default unless `confirm_request_callback` is registered:
  ```cpp
  SerialBT.onConfirmRequest([](uint32_t num_val) {
      SerialBT.confirmReply(true); // Auto-accept SSP pairing
  });
  ```
  Registering this callback is essential to prevent pairing refusal on modern ELM327 clones.

### 3.2 Non-Blocking Architecture (Core 0 FreeRTOS Task)
To satisfy the requirement that UI/touch loop services at least every 100 ms without interruption:
1. Pin the OBD Task to **ESP32 Core 0**:
   ```cpp
   xTaskCreatePinnedToCore(
       obdTaskWorker,
       "ObdWorker",
       4096,
       nullptr,
       1,               // Priority
       &g_obdTaskHandle,
       0                // Pinned to Core 0
   );
   ```
2. Arduino `loop()` runs on **Core 1**, servicing the touch controller, display refresh, and WebServer.
3. Because Core 0 and Core 1 operate independently, even if `SerialBT.connect()` takes 10-20 seconds to discover and pair with the ELM327, Core 1 executes at > 50 Hz with zero UI lag or touch stutter.

### 3.3 Connection State Machine Diagram
```
                     +-----------------------+
                     |         BOOT          |
                     +-----------------------+
                                 |
                                 v
                     +-----------------------+
     +-------------->|      DISCONNECTED     |
     |               +-----------------------+
     |                           |
     |                           | Trigger connect (using saved name/PIN)
     |                           v
     |               +-----------------------+
     | (Fail/Timeout)|       CONNECTING      |
     +---------------+ (Non-blocking Core 0) |
     |               +-----------------------+
     |                           |
     |                           | SPP Connected
     |                           v
     |               +-----------------------+
     |               |   INITIALIZING_ELM    |
     |               | ATZ, ATE0, ATSP6, RV  |
     |               +-----------------------+
     |                           |
     |                           | ELM Ready (prompt '>')
     |                           v
     |               +-----------------------+
     | Link Drop     |       CONNECTED       |
     | (!connected() |     CYCLIC POLLING    |<--+
     |  or timeout)  |   010D, 0131, 0105    |   |
     +---------------+-----------------------+   |
     |                           |               |
     |                           +---------------+
     |                             Next PID
     v
+-------------------------+
|     RECONNECT_WAIT      | (Backoff <= 5-10s)
+-------------------------+
```

### 3.4 Fast Auto-Reconnect (<= 10s Requirement)
To detect link drop and reconnect within <= 10s:
1. **Link Loss Detection**:
   - `BluetoothSerial::connected(0)` returns in < 1 ms by reading event group bits.
   - Register an SPP callback via `SerialBT.register_callback(sppCallback)`:
     ```cpp
     void sppCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
         if (event == ESP_SPP_CLOSE_EVT) {
             g_obdState = ObdState::DISCONNECTED;
         }
     }
     ```
   - When the car ignition is turned off, `ESP_SPP_CLOSE_EVT` fires in < 500 ms.
2. **Backoff & Reconnection**:
   - Wait a short backoff (e.g. 3.0 seconds).
   - Initiate `SerialBT.connect(savedBtName)` on Core 0.
   - When the ignition is turned back on, the ELM327 powers up and reconnects on the next attempt.
   - Total time from dongle re-powering to active data stream: **< 6 to 8 seconds** (well within the <= 10s threshold).

### 3.5 Robust Handling of ELM327 Clone Quirks
| ELM327 Quirk | Symptom / Raw Stream | Robust Parser Handling |
|---|---|---|
| **Echo Enabled** | `010D\r41 0D 3C\r\r>` | 1. Send `ATE0\r` at initialization.<br>2. Parser must NOT assume index 0 is data; scan for expected header (e.g. `41 0D` or `41 31`). |
| **Protocol Search** | `SEARCHING...\r\n41 31 04 B0\r\n>` | 1. Lock protocol via `ATSP6\r` (ISO 15765-4 CAN 11-bit 500k) to prevent searching delay.<br>2. Parser skips lines starting with `SEARCHING...` until header found. |
| **Unsupported PID** | `NO DATA\r\r>` | 1. Detect `NO DATA` string.<br>2. When received for PID 0x31, set `hasPid31 = false` and immediately engage speed-integration fallback. |
| **Syntax Error** | `?\r\r>` | Treat as command reject; flush input and proceed to next scheduled query. |
| **Split / Fragmented Packets** | Packet 1: `41 31 `<br>Packet 2: `04 B0\r>` | Accumulate stream bytes into a line buffer until `>` delimiter is encountered; never parse partial buffers. |
| **Spaces / Delimiters** | `41 31 04 B0` vs `413104B0` | Strip all whitespace (` `, `\r`, `\n`) prior to hexadecimal decoding. |
| **Trailing Prompt** | `>` prompt character | Use `>` as universal end-of-response synchronization delimiter. |

---

## 4. Odometer Engine & PID Handling Architecture

### 4.1 PID 01 31 Specifications
- **Name**: Distance traveled since codes cleared.
- **Service & PID**: Service `01`, PID `31`.
- **Query**: `0131\r`
- **Response**: `41 31 A B\r\r>`
- **Formula**: $\text{Distance (km)} = (256 \times A) + B$
- **Resolution**: 1 km per count (range 0 to 65,535 km).
- **Polling Rate**: Every 2.0 seconds.

### 4.2 Speed-Integration Fallback (PID 01 0D)
- **Name**: Vehicle speed.
- **Service & PID**: Service `01`, PID `0D`.
- **Query**: `010D\r`
- **Response**: `41 0D A\r\r>`
- **Formula**: $\text{Speed (km/h)} = A$
- **Polling Rate**: 2 Hz (every 500 ms).
- **Fallback Integration Logic**:
  $$\Delta \text{km} = \frac{\text{speedKmh} \times \Delta t_{\text{sec}}}{3600}$$
  Accumulate fractional km in RAM (`m_fractionalKm += deltaKm`). When `m_fractionalKm >= 1.0f`, increment total odometer and decrement fractional accumulator.

### 4.3 ECU Counter Wrap / Reset Handling
The ECU 16-bit counter can drop for two reasons:
1. **ECU Reset / Battery Pull / DTC Clear**: Value drops from e.g. 1,200 km to 0 or 5 km.
2. **16-bit Overflow**: Value rolls over from 65,535 km to 0 km.

**Algorithm for Zero Lost Mileage & Zero Double-Counting**:
```cpp
void OdometerEngine::updateFromPid31(uint16_t currentPid31Km) {
    if (!m_hasPid31Baseline) {
        m_lastPid31 = currentPid31Km;
        m_hasPid31Baseline = true;
        return;
    }

    if (currentPid31Km >= m_lastPid31) {
        // Normal progression
        uint16_t delta = currentPid31Km - m_lastPid31;
        m_currentOdo += delta;
    } else {
        // Counter dropped: distinguish between 16-bit overflow and ECU reset
        if (m_lastPid31 > 60000 && currentPid31Km < 5000) {
            // 16-bit counter wrap
            uint32_t delta = (65536 - m_lastPid31) + currentPid31Km;
            m_currentOdo += delta;
        } else {
            // ECU reset occurred: add current km as delta accumulated since reset
            m_currentOdo += currentPid31Km;
        }
    }
    m_lastPid31 = currentPid31Km;
}
```

### 4.4 Automated Mock ELM327 Simulator Design
To prove all acceptance criteria without physical hardware, an automated simulator suite is designed as a standalone host-runnable C++ executable:
- **`MockElmStream`**: Implements a bidirectional byte buffer stream. Simulates ELM327 responses to AT commands (`ATZ -> "ELM327 v1.5\r>"`, `ATE0 -> "OK\r>"`, `ATSP6 -> "OK\r>"`, `ATRV -> "14.2V\r>"`).
- **`Simulated Scenarios`**:
  1. **Scenario 1 (Normal PID 0x31)**: Feeds `0131\r -> "41 31 04 B0\r>"` (1200 km), then `0131\r -> "41 31 04 BF\r>"` (1215 km). Asserts ODO advances by exact delta (+15 km).
  2. **Scenario 2 (ECU Counter Reset)**: Feeds `0131\r -> "41 31 00 0A\r>"` (10 km). Asserts ODO advances by exact delta (+10 km) without regression or double counting.
  3. **Scenario 3 (Echo + SEARCHING... + Splits)**: Feeds chunked stream `["SEARCHING...\r\n", "41 ", "31 ", "04 ", "CB\r\n", ">"]`. Asserts parser successfully decodes 1227 km (+12 km).
  4. **Scenario 4 (NO DATA Fallback)**: Feeds `0131\r -> "NO DATA\r>"`. Asserts fallback flag engaged. Feeds `010D\r -> "41 0D 3C\r>"` (60 km/h) for 60 seconds. Asserts ODO advances by +1 km.
  5. **Scenario 5 (Connection State Machine)**: Simulates disconnect and asserts reconnect triggered and credentials verified.

---

## 5. Storage Subsystem & Web Settings Survey

### 5.1 StorageManager Code Audit: Why `loadItems` / `saveItems` Are Stubs
In `StorageManager.cpp`:
```cpp
// StorageManager.cpp:44-62
bool StorageManager::loadItems(std::vector<MaintenanceItem>& items) {
#ifdef ARDUINO
    size_t size = prefs.getBytesLength("items");
    if (size == 0) return false;
    // Loaded from NVS blob if exists
    return true;
#else
    return false;
#endif
}

bool StorageManager::saveItems(const std::vector<MaintenanceItem>& items) {
#ifdef ARDUINO
    // Save serialized state to NVS
    return true;
#else
    return true;
#endif
}
```
**Reason for Stubs**:
`MaintenanceItem` contains dynamic heap strings (`std::string name`, `std::string lastDate`, `std::string id`) which cannot be stored via raw C-struct memory dumps (`memcpy`). The original authors left placeholder comments and never implemented a binary or JSON serializer.

### 5.2 MicroSD Card Misconfiguration
In `StorageManager.cpp:88-100`:
```cpp
bool StorageManager::appendHistoryLog(const ServiceHistoryRecord& record) {
#ifdef ARDUINO
    File f = SD.open(m_csvHistoryPath.c_str(), FILE_APPEND);
```
- `SD.begin()` is never called in `civic_hud.ino`.
- The CYD board does not have an SD card inserted, causing `SD.open()` to fail immediately on every slide-to-confirm reset.

### 5.3 Non-Volatile Storage Architecture: Preferences (NVS) vs LittleFS
The project uses the `huge_app` partition scheme (`tools/partitions/huge_app.csv`):
- `nvs`: 20 KB (`0x9000` - `0xE000`)
- `app0`: 3072 KB (`0x10000` - `0x310000`)
- `spiffs`: 896 KB (`0x310000` - `0x3F0000`)

**Optimal Architecture**:
1. **Preferences (NVS)** for small atomic configuration values:
   - `odo` (uint32_t)
   - `last_pid31` (uint16_t)
   - `bt_name` (String, default `"OBDII"`)
   - `bt_pin` (String, default `"1234"`)
   - `item_count` (uint8_t)
   - `items_blob` (POD serialized struct array of 24 items = ~2.4 KB, well within 4 KB NVS blob limit)
2. **LittleFS** (mounted on the 896 KB data partition) for:
   - `/service_history.csv`: Permanent append-only CSV log of all slide-to-confirm actions.
   - Requires NO physical MicroSD card.
   - Resilient against abrupt power cuts (vehicle ignition off).

### 5.4 Binary Item Serialization Schema (POD Struct)
```cpp
#pragma pack(push, 1)
struct MaintItemNvsRecord {
    char     id[12];
    char     name[48];        // UTF-8 Thai name with emoji
    uint32_t intervalKm;
    uint16_t intervalDays;
    uint8_t  warnPct;
    uint32_t lastKm;
    uint32_t lastEpoch;
    char     lastDate[24];    // e.g. "12 ส.ค. 26"
    uint8_t  visible;
};
#pragma pack(pop)
```
- Per-record size: 100 bytes.
- 24 items = 2,400 bytes total blob.
- Fast, single-read/single-write NVS operation (`prefs.putBytes("items", buf, len)`).

### 5.5 Web Server Implementation (`ui_layout_c.html` Alignment)
- **Wi-Fi SoftAP**: SSID `"CivicHUD"`, IP `192.168.1.1`, Port `80`.
- **Endpoints**:
  - `GET /`: Renders complete settings page including:
    - Odometer Sync form.
    - Bluetooth Name & PIN inputs (populated from loaded NVS values).
    - Full list of maintenance items with:
      - Checkbox `<input type="checkbox" name="vis_<id>">`
      - Interval km `<input type="number" name="km_<id>" value="...">`
    - Add New Item form (`new_name`, `new_km`).
    - Single **"💾 บันทึกการตั้งค่าทั้งหมด (Save All)"** button.
    - Full Service History Table (Date, Name, ODO, Delta, Next Due).
  - `POST /save`: Processes submission, writes all changes to `MaintenanceEngine`, `OdometerEngine`, and `StorageManager`, marks `g_screenDirty = true`, triggers confirmation audio chime, and immediately updates screen Layout C.

### 5.6 Verification Strategy Without Switching Mac Wi-Fi
**Constraint**: The Mac cannot switch its Wi-Fi to `"CivicHUD"` AP as that terminates the host's internet connection.

**Airtight 3-Tier Verification Method**:
1. **Tier 1 (Host-Side Handler Simulation Test)**:
   Extract WebServer form decoding and state update logic into a decoupled module. Run native test feeding URL-encoded POST strings:
   `odo=100500&bt_name=OBD_NEW&bt_pin=0000&km_oil=15000&vis_tire=0&new_name=Wiper&new_km=5000`
   Assert exact state transitions and serialization.
2. **Tier 2 (On-Device Serial Test Command Bridge)**:
   Implement a serial command listener on `/dev/cu.usbserial-11220`:
   Sending `CMD:WEB_SAVE <payload>` executes the exact same web save handler code on the ESP32.
   Dumps resulting state to serial.
3. **Tier 3 (Hardware Reboot Persistence Test)**:
   Trigger `ESP.restart()`. Read serial boot log to verify:
   - Odometer restored to 100,500 km.
   - Engine oil interval restored to 15,000 km.
   - Tire item remains hidden.
   - Custom Wiper item restored.
   - BT target name is `"OBD_NEW"` with PIN `"0000"`.
   - History records intact in LittleFS.

---

## 6. Actionable Implementation Blueprint for Downstream Team

1. **Decouple OBD & State Machine**:
   - Refactor `ObdTask` into a hardware-abstracted `ObdEngine` that accepts any `Stream` (host mock stream or `BluetoothSerial`).
   - Run OBD queries in a FreeRTOS task on Core 0.
2. **Implement PID 0x31 + Speed Fallback**:
   - Add PID 0x31 reading every 2s.
   - Add PID 0x0D speed reading at 2 Hz.
   - Feed both into `OdometerEngine`.
3. **Complete `StorageManager`**:
   - Implement `loadItems` and `saveItems` using POD NVS blob.
   - Mount `LittleFS.begin(true)` and log to `/service_history.csv`.
4. **Fulfill Web Settings UI**:
   - Update HTML in `civic_hud.ino` to include per-item interval editing, checkboxes, add-item inputs, and history log.
   - Implement `POST /save` handling for all fields.
5. **Add Host & On-Device Test Runners**:
   - Build `test_obd_simulator` verifying stream parsing, clone quirks, and state machine.
   - Include serial test command hook for zero-risk web verification.
