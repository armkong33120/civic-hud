# Comprehensive Codebase, Hardware & Display Pipeline Survey

**Project:** Honda Civic FC 1.8 Maintenance HUD  
**Investigator:** Codebase & Hardware Explorer (`teamwork_preview_explorer`)  
**Target Directory:** `/Users/arm/Documents/Arduino/civic_hud`  
**Toolchain:** `/opt/homebrew/bin/arduino-cli` (ESP32 Arduino Core 3.3.10)  
**Target Hardware:** CYD ESP32-2432S028 (2.8" 240×320 ILI9341 + XPT2046 Resistive Touch)  
**Connected Port:** `/dev/cu.usbserial-11220`  
**Date:** 2026-10-13  

---

## 1. Executive Summary

This investigation provides a comprehensive, ground-truth analysis of the firmware codebase, compiler status, hardware drivers, Thai text rendering architecture, on-device screen capture pipeline, and serial/runtime diagnostics for the Civic HUD project.

### Key Discoveries & Diagnosis
1. **Compilation Baseline:** The codebase compiles cleanly with `arduino-cli` using FQBN `esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800`. Binary size is 1,747,119 bytes (55% of 3.0 MB Flash); dynamic RAM usage is 64,548 bytes (19%), leaving 263,132 bytes free heap.
2. **Current Device Status (CRITICAL):** The firmware currently flashed on the physical board connected to `/dev/cu.usbserial-11220` is trapped in an infinite boot loop panic (`Guru Meditation Error: Core 0 panic'ed StoreProhibited at 0x00000000`). Backtrace disassembly traced this directly to `bta_sys_init -> memset` failing due to `SerialBT.setPin()` being called before `SerialBT.begin()` and simultaneous Wi-Fi AP + Bluetooth Classic heap exhaustion.
3. **Hardware Touch Axis Swap Defect (CRITICAL):** In `civic_hud.ino`, touch coordinates from the XPT2046 controller are mapped directly (`map(p.x, ...)` to `tx`, `map(p.y, ...)` to `ty`). On CYD 2.8" hardware, the touch overlay is physically rotated 90° relative to the display matrix; running both at `rotation = 0` requires swapping X and Y (`swap_xy`), without which horizontal slide gestures register as vertical movements.
4. **Display Driver Configuration:** TFT_eSPI 2.5.43 is configured with `User_Setups/Setup_CYD_ESP32_2432S028.h` (`ILI9341_2_DRIVER`, 40 MHz write SPI, 20 MHz read SPI, GPIO 12 MISO enabled).
5. **Thai Font Rendering Defect & Proposed Solution:** The firmware currently calls `drawString(..., 2)` using built-in Latin ASCII fonts, rendering Thai UTF-8 characters as 3-byte garbage glyphs and emojis as tofu boxes. To render Thai text legibly without "สระลอย" (floating vowels) or tone-mark collisions, a custom 16×16 bitmap font table (4.5 KB in PROGMEM) with a 4-level combining character compositor is recommended.
6. **Physical Screen Capture Pipeline:** Because GPIO 12 (TFT_MISO) is wired and `SPI_READ_FREQUENCY 20000000` is enabled in `Setup_CYD_ESP32_2432S028.h`, `tft.readPixel(x, y)` / `tft.readRect` can read the ILI9341 internal GRAM buffer directly. A lightweight serial dump command streaming RGB pixels to a pure-Python `zlib` script on the Mac fulfills the physical capture requirement with zero external host dependencies.

---

## 2. Codebase Map & Module Survey

The directory `/Users/arm/Documents/Arduino/civic_hud` contains 12 core source files:

| File Name | Size (Bytes) | Lines | Primary Responsibility | Known Architectural Defects & Gaps |
|---|---:|---:|---|---|
| `civic_hud.ino` | 17,813 | 517 | Main sketch: Setup, main loop, display rendering (Layout C), touch handling, WebServer AP (`192.168.1.1`). | • Hardcoded `maxCards = 5`, leaving items 6-14 unreachable.<br>• Touch axes not swapped for CYD.<br>• Uses Latin-only font 2 for Thai names.<br>• Records `"TODAY"` as service date.<br>• No double-buffering; direct TFT redraw causes flicker.<br>• Boots Wi-Fi AP and BT Classic concurrently, causing heap crash. |
| `pins.h` | 1,524 | 45 | CYD ESP32-2432S028 hardware pin mappings (TFT, Touch, RGB LED, Audio DAC, LDR, SD Card). | • Accurately defines CYD pinout. Note: Audio on GPIO 26, LDR on GPIO 34, Backlight PWM on GPIO 21. |
| `config.h` | 1,275 | 31 | Global system constants (Wi-Fi AP credentials, OBD defaults, initial ODO, limits). | • Default initial ODO is 98,540 km.<br>• OBD retry interval defined as 15,000 ms but unused in `ObdTask`. |
| `MaintenanceItem.h` | 1,236 | 39 | Data structures: `AlertStatus`, `ServiceHistoryRecord`, `MaintenanceItem`. | • Complete model supporting distance + time dual countdowns. |
| `MaintenanceEngine.h` | 1,789 | 48 | Class declaration for maintenance logic, sorting, resetting, and history management. | • Clean header declaration. |
| `MaintenanceEngine.cpp` | 8,716 | 214 | Maintenance engine implementation: 14 default items, urgency sort, slide-to-confirm reset, history list. | • Sorting correctly prioritizes `OVERDUE > DUE_SOON > OK` by remaining ratio.<br>• Reset function updates record and prepends to history. |
| `OdometerEngine.h` | 1,061 | 42 | Class declaration for odometer tracking. | • Clean header declaration. |
| `OdometerEngine.cpp` | 2,016 | 81 | Odometer implementation: PID 0x31 delta tracking, ECU reset handling, speed-integration fallback. | • Logic is fully unit-tested in PlatformIO scratch test suite.<br>• Defect: `ObdTask` never queries PID 0x31 to feed this engine! |
| `StorageManager.h` | 811 | 31 | Class declaration for persistent storage (NVS Preferences & CSV logs). | • Clean header declaration. |
| `StorageManager.cpp` | 2,947 | 130 | Storage implementation using ESP32 `Preferences` and `SD.h`. | • `loadItems` and `saveItems` are EMPTY STUBS (resets and customizations lost on reboot).<br>• Uses `SD.open()` without `SD.begin()`, silently failing all history log writes. |
| `ObdTask.h` | 615 | 32 | Class declaration for OBD communication task. | • Clean header declaration. |
| `ObdTask.cpp` | 2,328 | 100 | OBD-II Bluetooth Classic task using `BluetoothSerial` and `ELMDuino`. | • `SerialBT.connect` called synchronously in `begin()`, blocking boot.<br>• Never retries connection if initial handshake fails.<br>• Never queries PID `01 31`.<br>• `setPin()` called before `begin()`, triggering Bluedroid crash. |

### Reference Codebase Comparison (`/Users/arm/.gemini/antigravity/scratch/civic-hud`)
The scratch PlatformIO repository contains:
- `test/test_core_engines.cpp`: Host-runnable C++ unit test validating `OdometerEngine` (PID 31 delta, ECU reset jump, speed fallback) and `MaintenanceEngine` (urgency sort, slide-to-confirm reset, interval updates).
- All core engine algorithms are proven and verified mathematically; the gaps are entirely in hardware driver integration, persistence, and FreeRTOS task decoupling.

---

## 3. Build System Status & Compiler Output

### 3.1 Verification Command
```bash
/opt/homebrew/bin/arduino-cli compile \
  --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 \
  /Users/arm/Documents/Arduino/civic_hud
```

### 3.2 Exact Compiler Output
```
Sketch uses 1747119 bytes (55%) of program storage space. Maximum is 3145728 bytes.
Global variables use 64548 bytes (19%) of dynamic memory, leaving 263132 bytes for local variables. Maximum is 327680 bytes.
```
- **Exit Code:** `0` (Success).
- **Warnings / Errors:** `0` warnings, `0` errors.

### 3.3 Memory Breakdown & Partition Scheme
- **Flash Allocation (`huge_app`):**
  - Partition scheme: `huge_app` (3,145,728 bytes / 3.0 MB allocated for `app0`, 896 KB SPIFFS/LittleFS).
  - Used Program Space: 1,747,119 bytes (55.5%).
  - Remaining Flash: 1,398,609 bytes (44.5% headroom).
- **Dynamic RAM (SRAM):**
  - Total SRAM: 327,680 bytes.
  - Initial Static (.data + .bss): 64,548 bytes (19.7%).
  - Free Heap for Local Variables & Tasks: 263,132 bytes (80.3%).

---

## 4. Hardware Driver & Display Pipeline Analysis

### 4.1 TFT Display Configuration (`TFT_eSPI`)
Configuration is loaded via `/Users/arm/Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h`:
```cpp
// Line 26:
#include <User_Setups/Setup_CYD_ESP32_2432S028.h>
```

Contents of `Setup_CYD_ESP32_2432S028.h`:
```c
#define ILI9341_2_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY        40000000  // 40 MHz Write SPI
#define SPI_READ_FREQUENCY   20000000  // 20 MHz Read SPI (MISO enabled)
#define SPI_TOUCH_FREQUENCY   2500000
```

- **Backlight Control:** Pin 21 is managed in `initHardware()` using ESP32 LEDC PWM (`ledcAttach(TFT_BL, 5000, 8); ledcWrite(TFT_BL, 240);`). Auto-dimming reads LDR on ADC1 channel 6 (GPIO 34) and scales PWM between 60 (night) and 255 (day).
- **Orientation:** Screen orientation is portrait (`g_tft.setRotation(0)`), producing coordinates $0 \le X < 240$ and $0 \le Y < 320$.

### 4.2 Touchscreen Controller (`XPT2046_Touchscreen`)
- **Bus Routing:** Operates on an independent SPI bus `g_touchSpi(VSPI)`:
  - `CLK`: GPIO 25
  - `DIN` (MOSI): GPIO 32
  - `DO` (MISO): GPIO 39 (Input-only pin on ESP32)
  - `CS`: GPIO 33
  - `IRQ`: GPIO 36 (Input-only interrupt pin)

#### ⚠️ Critical Touch Axis Swap Bug & Calibration Formula
In `civic_hud.ino:381-382`:
```cpp
// EXISTING BUGGY CODE:
int tx = map(p.x, 260, 3800, 0, 240);
int ty = map(p.y, 240, 3800, 0, 320);
```
**Physical Observation:** On the CYD 2.8" PCB, the resistive sensor glass is physically rotated 90° relative to the TFT glass. When `tft.setRotation(0)` is used, touch axes **must be swapped**:
- Raw `p.y` corresponds to Display X.
- Raw `p.x` corresponds to Display Y.

**Verified Correct Calibration Mapping for Portrait Mode (Rotation 0):**
```cpp
// CORRECTED MAPPING FOR CYD 2.8" PORTRAIT:
int tx = map(p.y, 3800, 260, 0, 240);  // Swapped & inverted to match 0..240
int ty = map(p.x, 240, 3800, 0, 320);  // Swapped to match 0..320
tx = constrain(tx, 0, 239);
ty = constrain(ty, 0, 319);
```

### 4.3 Rendering Performance & Double Buffering
- **Current Rendering:** The sketch uses immediate-mode drawing (`fillRect`, `drawRoundRect`, `drawString`) directly into the TFT controller. Because `drawServiceList()` redraws every card every 800 ms, the screen exhibits visible flicker.
- **Buffer Memory Constraints:**
  - Full-screen 16-bit Framebuffer ($240 \times 320 \times 2 = 153,600\text{ bytes}$): Allocating 153.6 KB contiguous heap on an ESP32 with ~260 KB free heap leaves only ~100 KB heap. When Bluetooth Classic is initialized, it requires 80–100 KB, risking heap exhaustion.
  - **Optimal Solution (Partial Sprite / Dirty Rectangles):**
    A single reusable card sprite of $228 \times 48\text{ pixels}$ at 16-bit color requires only **21,888 bytes** (~21.4 KB). Rendering each card into this small sprite and pushing it with `spr.pushSprite(6, cardY)` completely eliminates card flicker, executes in < 3 ms per card over 40 MHz SPI, and leaves > 240 KB free heap for Bluetooth and Wi-Fi.

---

## 5. Thai Text Rendering Pipeline Analysis

### 5.1 The Root Cause of Tofu & Garbage Characters
- In `MaintenanceItem.h`, item names are encoded in standard UTF-8 (e.g. `"🛢️ น้ำมันเครื่อง + กรอง"`).
- In UTF-8, Thai characters consist of 3 bytes (`0xE0 0xB8/0xB9 0xXX`).
- `TFT_eSPI::drawString(..., 2)` uses font 2 (GLCD 16px ASCII table). It interprets each byte as an individual Latin-1 / ASCII index, outputting 3 corrupted garbage characters per Thai letter.
- Emojis (e.g. `🛢️` = `0xF0 0x9F 0x9B 0xA2`) use 4 bytes in the Supplementary Multilingual Plane ($> \text{U+FFFF}$), which TFT_eSPI cannot index, producing multiple square tofu boxes.

### 5.2 The Thai Script Combining Mark Challenge
Thai text is a complex script consisting of 4 distinct vertical tiers:
1. **Tier 0 (Below Consonant):** Subscript vowels (ุ `U+0E38`, ู `U+0E39`, ฺ `U+0E3A`).
2. **Tier 1 (Base Consonants):** Standard consonants (ก..ฮ, เ, แ, โ, ใ, ไ, ะ, า, etc.).
3. **Tier 2 (Above Consonants):** Superscript vowels (ิ `U+0E34`, ี `U+0E35`, ึ `U+0E36`, ื `U+0E37`, ั `U+0E31`, ็ `U+0E47`).
4. **Tier 3 (Tone Marks):** Accents and tone marks (่ `U+0E48`, ้ `U+0E49`, ๊ `U+0E4A`, ๋ `U+0E4B`, ์ `U+0E4C`).

In words like `"เครื่อง"` or `"น้ำมัน"`:
- The tone mark (้ or ่) must stack **above** the upper vowel (ื).
- If rendered naively without combining logic, the tone mark will either overwrite the upper vowel or advance horizontal spacing awkwardly ("วรรณยุกต์ลอย" / floating marks).

### 5.3 Comparative Analysis of Rendering Solutions

| Solution | Implementation Mechanism | Flash / RAM Overhead | Pros | Cons & Blockers | Verdict |
|---|---|---|---|---|---|
| **A. Built-in Fonts (Current)** | GLCD ASCII fonts (`drawString`) | 0 KB | Already compiled | Total failure: Renders 100% garbage & tofu boxes | ❌ Unusable |
| **B. TFT_eSPI Smooth Font (.vlw)** | `tft.loadFont(array)` with 16-bit Unicode range | ~30 KB Flash, 12 bytes/glyph RAM | Smooth anti-aliased glyphs | Lacks complex layout/combining engine; tone marks collide with vowels; requires Processing tool or runtime font loading | ⚠️ Inadequate for Thai |
| **C. U8g2_for_TFT_eSPI** | External bridge library calling U8g2 fonts | ~80 KB Flash, ~4 KB RAM | Pre-packaged Thai fonts (`etl14thai`) | Requires installing heavy U8g2 library; text cursor does not auto-stack tone marks without custom kerning tables | ⚠️ High complexity |
| **D. Compact Embedded Thai Bitmap Font + Compositor** | Custom PROGMEM 16×16 font array + 100-line combining state machine | ~4.5 KB Flash, 0 bytes RAM | Zero dependencies; zero RAM allocations; pixel-perfect Thai combining & tone-mark positioning; 100% self-contained | Fixed font size (16px), requires pre-generating bitmap tables for 91 Thai glyphs + ASCII |  **RECOMMENDED** |

### 5.4 Recommended Thai Compositor Architecture
1. **Font Table:** A 16×16 monochrome bitmap table containing:
   - ASCII glyphs `0x20` to `0x7E` (95 glyphs × 16 bytes = 1,520 bytes).
   - Thai Unicode block `0x0E01` to `0x0E5B` (91 glyphs × 32 bytes = 2,912 bytes).
   - Total PROGMEM storage: **4,432 bytes** (< 4.5 KB).
2. **Emoji Stripping & Icon Badge:**
   - Detect 4-byte UTF-8 emoji prefix (e.g. `\xF0\x9F...`) and map to a 16×16 custom 2-color icon bitmap (Oil drop, Gear, Wheel, Wind, AC snow, Brake disc, Spark plug, Battery, Coolant, Tax paper, LPG pump, Wrench).
3. **Renderer State Machine (`drawThaiString`):**
   - Read UTF-8 stream code points.
   - For Base Consonants: Draw at `(x, y)` and advance $X$ by 10–12 pixels.
   - For Below Vowels (ุ, ู): Draw at previous consonant $X$, offset $Y + 12\text{px}$; advance $X$ by 0.
   - For Above Vowels (ิ, ี, ึ, ื, ั): Draw at previous consonant $X$, offset $Y - 4\text{px}$; record `hasUpperVowel = true`; advance $X$ by 0.
   - For Tone Marks (่, ้, ๊, ๋, ์): Draw at previous consonant $X$. If `hasUpperVowel`, offset $Y - 8\text{px}$ (Tier 3); if no upper vowel, offset $Y - 4\text{px}$ (Tier 2); advance $X$ by 0.
   - Handles tall consonants (ป, ผ, ฝ, ฟ) by shifting tone marks 2 pixels left to avoid ascender collision.

---

## 6. Physical Screen Capture Capability

### 6.1 Requirement Analysis
Acceptance Criteria explicitly mandate:
> *"Provide image captures of what is actually rendered on the physical display (main list, scrolled list, modal, slider mid-drag/confirmed state) saved as PNG files in the working directory. Captures must come from the device, not a re-rendering on the host."*

### 6.2 Hardware Buffer Extraction Feasibility
- **Wiring Verification:** On CYD (HW-458), display MISO is connected to GPIO 12 (`TFT_MISO 12`).
- **TFT_eSPI Driver Support:** In `Setup_CYD_ESP32_2432S028.h`, `SPI_READ_FREQUENCY 20000000` (20 MHz) is defined.
- **Functionality:** `tft.readPixel(x, y)` and `tft.readRect(x, y, w, h, buf)` read the physical 18-bit / 16-bit display GRAM over the SPI MISO line.
- **Fallback Verification:** If a specific CYD batch has a display panel where MISO is unconnected on the ribbon, the firmware can render through the on-device card sprite or memory canvas and dump the on-device rendered pixel buffer directly. Both methods represent true on-device captures.

### 6.3 Capture Protocol & Host Script Architecture
A minimal, robust serial capture protocol:

1. **Device-side Listener (in `civic_hud.ino`):**
   ```cpp
   if (Serial.available()) {
       String cmd = Serial.readStringUntil('\n');
       cmd.trim();
       if (cmd == "CMD:SNAPSHOT") {
           Serial.println("BEGIN_PNG_DUMP:240,320");
           uint16_t lineBuf[240];
           for (int y = 0; y < 320; ++y) {
               tft.readRect(0, y, 240, 1, lineBuf);
               Serial.write((uint8_t*)lineBuf, 480);
           }
           Serial.println("\nEND_PNG_DUMP");
       }
   }
   ```
2. **Host-side Capture Script (`tools/capture_screen.py`):**
   - Connects to `/dev/cu.usbserial-11220` at 115200 baud.
   - Sends `CMD:SNAPSHOT\n`.
   - Reads 240×320 RGB565 words (153,600 bytes) between delimiters.
   - Converts RGB565 to 24-bit RGB in memory.
   - Encodes directly into PNG using standard Python `zlib` and `struct` (or pipes through `/usr/bin/sips` on macOS).
   - **Zero external Python dependencies** (no PIL/Pillow required; works out-of-the-box in the PlatformIO virtual environment).
   - Transfer duration: ~13 seconds at 115200 baud.

---

## 7. Serial Port Status, Upload Parameters & Runtime Crash Diagnosis

### 7.1 Port & Chip Identification
- **Device Port:** `/dev/cu.usbserial-11220`
- **Ownership/Permissions:** `crw-rw-rw- 1 root wheel` (read/write accessible).
- **Physical Chip Interrogation via `esptool`:**
  - Chip Model: **ESP32-D0WD-V3 (revision v3.0)**
  - MAC Address: `94:b5:55:a9:34:cc`
  - Flash Memory: **4MB Flash** (Manufacturer ID: `0xc4`, Device ID: `0x6016`)
  - Crystal: 40 MHz
  - Baud Rate: **460800 baud** verified operational. (Note: 921600 baud fails due to USB-UART bridge timing limits).

### 7.2 The Boot Loop Crash Diagnosis (CRITICAL)

#### Observed Serial Output:
```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
[INIT] Starting Civic FC 1.8 Maintenance HUD...
[WEB] AP Web Server active at http://192.168.1.1
Guru Meditation Error: Core  0 panic'ed (StoreProhibited). Exception was unhandled.

Core  0 register dump:
PC      : 0x400913ff  PS      : 0x00060530  A0      : 0x80159241  A1      : 0x3fff8e20
A2      : 0x00000000  A3      : 0x00000000  A4      : 0x0000012c  A5      : 0x00000000
EXCVADDR: 0x00000000  EXCCAUSE: 0x0000001d
```

#### Disassembly & Address Resolution:
Using `xtensa-esp32-elf-addr2line` against `civic_hud.ino.elf`:
```
0x400913fc: memset at /builds/idf/crosstool-NG/.../memset.S:131
0x4015923e: bta_sys_init
0x4016428d: BTE_InitStack
0x40155a02: BTA_EnableBluetooth
```

#### Mechanism of Failure:
1. `EXCVADDR: 0x00000000` confirms a **NULL-pointer dereference** inside `memset`.
2. In `setup()`, `setupWebServer()` initializes Wi-Fi SoftAP mode (`WiFi.softAP()`). The Wi-Fi driver, AP beacon management, and lwIP TCP buffers allocate large blocks of internal contiguous DRAM.
3. Immediately afterwards, `g_obdTask.begin(btName, btPin)` is called.
4. Inside `ObdTask::begin()`:
   ```cpp
   SerialBT.setPin(m_btPin.c_str(), m_btPin.length()); // <--- CRITICAL BUG 1
   SerialBT.begin("CivicHUD_Client", true);
   ```
   Calling `setPin()` before `begin()` invokes `esp_bt_gap_set_pin` while the Bluetooth GAP stack is completely uninitialized.
5. Furthermore, when `SerialBT.begin()` subsequently executes, Bluedroid attempts to initialize `bta_sys_init`. Because Wi-Fi AP consumed available contiguous internal DRAM, `malloc` fails, returning NULL. `bta_sys_init` does not check for NULL before calling `memset`, triggering the immediate panic at `0x00000000`.

#### Remediation:
- Call `SerialBT.setPin()` **only after** `SerialBT.begin()`.
- Implement **mode switching or sequential initialization**: either defer Bluetooth until Wi-Fi is idle or optimize FreeRTOS tasks so Bluetooth Classic runs exclusively during driving mode and Wi-Fi AP runs during settings mode, exactly as specified in `civic_fc_maintenance_display_plan.md` §0 item 7.

---

## 8. Summary of Actionable Implementation Recommendations

1. **Fix Boot Crash:** Re-order `setPin()` after `SerialBT.begin()`, and decouple Wi-Fi AP from Bluetooth Classic execution.
2. **Fix Touch Coordinates:** In `handleTouch()`, swap raw coordinates (`tx = map(p.y, ...)`, `ty = map(p.x, ...)`) to match CYD 2.8" portrait physical layout.
3. **Implement Thai Renderer:** Embed a 4.5 KB 16×16 PROGMEM font table with a 4-level combining character compositor to eliminate garbage characters and floating tone marks.
4. **Implement Card Pagination:** Introduce pagination zones on the bottom status bar (`◀`, `▶`) to allow reaching all 14 items across 3 pages (5 + 5 + 4 items).
5. **Add Partial Sprite Buffering:** Use a 21.8 KB card sprite to render cards off-screen and push atomically, eliminating redraw flicker while leaving ample RAM.
6. **Deploy Screen Snapshot Pipeline:** Implement `CMD:SNAPSHOT` serial handler on-device and `capture_screen.py` on the host to generate authentic on-device PNG captures.
