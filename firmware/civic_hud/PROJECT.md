# Project: Honda Civic FC 1.8 Maintenance HUD Firmware

## Architecture
- **Hardware Target**: CYD (ESP32-2432S028R) 2.8" ILI9341 240x320 LCD + XPT2046 Resistive Touch.
- **Microcontroller Core Allocation**:
  - **Core 0 (Background Telemetry & Connectivity)**: FreeRTOS OBD worker task managing Bluetooth Classic SPP connection state machine, ELM327 initialization, PID 01 31 distance polling, and PID 01 0D speed fallback.
  - **Core 1 (Application, UI & Web Settings)**: Main Arduino execution loop, touch input polling (< 100 ms latency), TFT_eSPI partial-sprite rendering, Thai typography compositor, and Web Server (SoftAP 192.168.1.1).
- **Subsystem Breakdown**:
  1. `ObdTask` / `ElmDriver`: Bluetooth Classic SPP, connection state machine, auto-reconnect, PID polling, ELM327 clone quirk mitigation.
  2. `OdometerEngine`: Vehicle odometer tracking, PID 01 31 delta processing, 16-bit wrap / ECU counter reset safety, speed integration fallback.
  3. `MaintenanceEngine`: 14+ maintenance item evaluations, dual-constraint ratio calculations $\min(r_{\text{km}}, r_{\text{days}})$, urgency sorting hierarchy (Overdue > Due Soon > OK), pagination model.
  4. `StorageManager`: Preferences (NVS) for scalar config (ODO, last PID 31, BT credentials), packed POD blob for item configurations, and LittleFS for `/service_history.csv` log.
  5. `DisplayEngine` / `ThaiFont`: 16x16 PROGMEM Thai bitmap font table, 4-tier combining glyph compositor, 21.8 KB flicker-free partial card sprite, Layout C UI rendering, modal dialog, slide-to-confirm touch slider.
  6. `WebSettings`: SoftAP HTTP server at 192.168.1.1 rendering full configuration portal matching `ui_layout_c.html`, handling `POST /save`, triggering live screen sync.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Boot Panic Elimination | Fix `SerialBT.begin()` and `setPin()` order, prevent DRAM collision between Wi-Fi and BT | M1 | Survey |
| 2 | Non-Blocking BT State Machine | State machine on Core 0 (`DISCONNECTED`, `CONNECTING`, `INITIALIZING_ELM`, `CONNECTED`, `RECONNECT_WAIT`); never blocks UI | M1 | Survey |
| 3 | Auto-Reconnect on Drop | Automatically detect link loss via SPP events and reconnect within $\le 10$ s upon ignition cycle | M1 | Survey |
| 4 | SSP Pairing & PIN 1234 | Auto-accept SSP pairing (`onConfirmRequest`) and configure PIN `1234` | M1 | Survey |
| 5 | ELM327 Clone Quirk Handling | Tolerate echo, `SEARCHING...`, `NO DATA`, `?`, split responses, and `>` prompt | M1 | Survey |
| 6 | PID 01 31 Distance Tracking | Read PID `01 31` every 2s, compute delta km, feed `OdometerEngine` | M1 | Survey |
| 7 | Speed Integration Fallback | Fallback to PID `01 0D` (speed integration at 2 Hz) when PID 01 31 is unsupported / returns `NO DATA` | M1 | Survey |
| 8 | ECU Reset / Wrap Handling | Distinguish 16-bit counter rollover (65535 -> 0) from ECU code clear without losing or double-counting km | M1 | Survey |
| 9 | Automated ELM327 Simulator | Host-runnable test harness feeding simulated ELM streams to verify parser, odometer, and state machine | M1 | Survey |
| 10 | NVS Scalar Persistence | Persist odometer, last_pid31, BT name, and BT PIN across reboots | M2 | Survey |
| 11 | NVS Maintenance Items Blob | Packed POD struct array persisting 14+ items (intervals, lastDoneKm, lastDoneDays, hidden/visible, custom items) | M2 | Survey |
| 12 | LittleFS Service History Log | Mount LittleFS partition, persist `/service_history.csv` without requiring physical SD card | M2 | Survey |
| 13 | Web Settings UI & Form | Render full HTML/CSS at 192.168.1.1 with ODO sync, BT name/PIN, interval editing, visibility toggles, add-item | M2 | Survey |
| 14 | Web Settings "Save All" & Sync | Process `POST /save`, persist to NVS, update live memory, and trigger immediate screen refresh | M2 | Survey |
| 15 | Web Settings Service History View | Render scrollable/tabular history log table on the web page from `/service_history.csv` | M2 | Survey |
| 16 | Zero-Risk Web Verification Bridge | Verify web form serialization and HTTP handlers via host test and on-device serial bridge without switching Mac Wi-Fi | M2 | Survey |
| 17 | Thai Bitmap Font Engine | 16x16 PROGMEM bitmap font covering Thai consonants, vowels, and tone marks with zero heap allocation | M3 | Survey |
| 18 | Thai Combining Character Compositor | 4-tier combining compositor (base, below-vowel, above-vowel, tone mark) preventing floating vowels/collisions | M3 | Survey |
| 19 | Touchscreen Axis Swap | Swap X/Y axes (`tx = map(p.y)`, `ty = map(p.x)`) for CYD portrait orientation | M3 | Survey |
| 20 | Layout C Status Bar | Top status bar showing OBD connection state badge, battery voltage (V), and current odometer (km) | M3 | Survey |
| 21 | Layout C Service Cards | Render urgency-sorted cards (Overdue > Due Soon > OK) with usage progress bars (green/yellow/red) | M3 | Survey |
| 22 | Pagination & Reachability | 3-page pagination (`ITEMS_PER_PAGE=5`, bottom bar `◀` / `▶` touch zones) making all 14+ items reachable | M3 | Survey |
| 23 | Driving Touch Lockout | Disable touch navigation and modal interactions when vehicle speed > 5 km/h for driver safety | M3 | Survey |
| 24 | Previous Service Modal | Tap-a-card popup showing previous service date, previous ODO, delta km traveled, and new record preview | M3 | Survey |
| 25 | Formatted Thai Date Strings | Render structured date strings (e.g. "13 ต.ค. 26") instead of literal `"TODAY"` | M3 | Survey |
| 26 | Slide-to-Confirm Reset Slider | Slide-right touch slider with $\ge 85\%$ threshold, visual feedback, resetting item and sinking to bottom | M3 | Survey |
| 27 | Card-Level Partial Sprite Buffering | 21.8 KB partial card sprite eliminating screen redraw flicker without exceeding SRAM limits | M3 | Survey |
| 28 | On-Device Screen Capture (CMD:SNAPSHOT) | Stream physical ILI9341 GRAM buffer over serial, converted to true PNG on Mac host | M4 | Survey |
| 29 | Clean Arduino-CLI Compilation | Compile with zero errors/warnings on `esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800` | M4 | Survey |
| 30 | Device Flashing & Stability Run | Upload to `/dev/cu.usbserial-11220`, verify boot reaches ready state, run $\ge 60$ s without panic | M4 | Survey |
| 31 | On-Device Persistence Verification | Slide reset, interval edit, hide item, add item; hardware reset board; verify data survives via serial dump | M4 | Survey |
| 32 | Rubric-Compliant UI Image Evidence | Capture PNGs of main list, page 2, modal, and slider confirmed; verified by reviewer against rubric | M4 | Survey |
| 33 | Thai Deliverable Report | Deliver `REPORT.md` in Thai with verification results and owner in-car test checklist | M4 | Survey |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | OBD-II & BT State Machine & Simulator | Features 1, 2, 3, 4, 5, 6, 7, 8, 9 | none | DONE |
| M2 | Storage & Web Settings Persistence | Features 10, 11, 12, 13, 14, 15, 16 | none | IN_PROGRESS |
| M3 | Thai Typography & Layout C UI Engine | Features 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 | none | PLANNED |
| M4 | Final Integration, Flashing & Deliverables | Features 28, 29, 30, 31, 32, 33 | M1, M2, M3, E2E Test Suite | PLANNED |

## Interface Contracts

### Telemetry / OBD ↔ Odometer & Core Engine
```cpp
// ObdTask provides telemetry state:
struct ObdTelemetry {
    bool obdConnected;
    bool btConnected;
    float batteryVoltage;
    uint32_t currentPid31Km;
    uint8_t currentSpeedKph;
    bool pid31Supported;
    uint32_t lastPacketTimestampMs;
};

// ObdTask feeds OdometerEngine:
void OdometerEngine::updateFromPid31(uint32_t rawPid31DistanceKm);
void OdometerEngine::updateFromSpeed(uint8_t speedKph, uint32_t deltaTimeMs);
uint32_t OdometerEngine::getOdometerKm() const;
```

### StorageManager ↔ Core Engines & Web
```cpp
struct PackedItemRecord {
    uint8_t id;
    char name[48];        // UTF-8 Thai name
    uint32_t intervalKm;
    uint16_t intervalDays;
    uint32_t lastDoneKm;
    uint32_t lastDoneDays;
    uint8_t warnPercent;
    bool isVisible;
    bool isCustom;
};

class StorageManager {
public:
    bool begin();
    bool loadConfig(uint32_t& odo, uint32_t& lastPid31, String& btName, String& btPin);
    bool saveConfig(uint32_t odo, uint32_t lastPid31, const String& btName, const String& btPin);
    bool loadItems(std::vector<MaintenanceItem>& items);
    bool saveItems(const std::vector<MaintenanceItem>& items);
    bool appendHistoryLog(const String& itemName, uint32_t currentOdo, uint32_t deltaKm, const String& dateStr);
    String readHistoryLogsCsv();
};
```

### UI Engine ↔ MaintenanceEngine
```cpp
// MaintenanceEngine evaluates items and sorts by urgency:
enum class UrgencyStatus { OK = 0, DUE_SOON = 1, OVERDUE = 2 };

struct EvaluatedItem {
    MaintenanceItem item;
    UrgencyStatus status;
    float usageRatio;     // 0.0 to 1.0+ (pinned to 1.0 for overdue)
    int32_t kmRemaining;
    int32_t daysRemaining;
};

// UI Engine queries:
std::vector<EvaluatedItem> MaintenanceEngine::getEvaluatedItemsSorted(uint32_t currentOdoKm, uint32_t currentDays);
```

## Code Layout
- `civic_hud.ino`: Sketch entry point, FreeRTOS task coordination, setup, loop, web server endpoints.
- `pins.h`: Hardware pin assignments for CYD ESP32-2432S028.
- `config.h`: System configuration constants, thresholds, default values.
- `ObdTask.h` / `ObdTask.cpp`: Bluetooth Classic SPP connection state machine, FreeRTOS background task on Core 0, PID polling.
- `OdometerEngine.h` / `OdometerEngine.cpp`: Odometer advancement, PID 01 31 deltas, wrap/reset handling, speed fallback.
- `MaintenanceEngine.h` / `MaintenanceEngine.cpp`: Item evaluation, urgency sorting, wear ratio calculation.
- `MaintenanceItem.h`: Item data structure definitions.
- `StorageManager.h` / `StorageManager.cpp`: NVS Preferences & LittleFS persistence.
- `ThaiFont.h` / `ThaiFont.cpp`: 16x16 PROGMEM Thai font bitmap and 4-tier combining glyph compositor.
- `DisplayLayout.h` / `DisplayLayout.cpp`: Layout C rendering routines, partial sprite buffer, modal dialog, slide-to-confirm touch handler.
- `SnapshotHelper.h` / `SnapshotHelper.cpp`: Physical GRAM serial dump handler (`CMD:SNAPSHOT`).
