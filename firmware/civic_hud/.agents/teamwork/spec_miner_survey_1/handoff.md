# Handoff Report: UI & System Specification Survey

**Agent:** UI Specification Miner (`teamwork_preview_spec_miner`)  
**Workspace Folder:** `/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/spec_miner_survey_1/`  
**Primary Deliverable:** `/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/spec_miner_survey_1/survey_spec.md`  
**Timestamp:** 2026-10-13T15:06:00Z  

---

## 1. Observation

1. **Approved Design Prototype (`ui_layout_c.html`):**
   - In `ui_layout_c.html` lines 7-14, the viewport `.scr` is styled with `width: 360px; height: 480px;`, background `#0b0f14`, font `system-ui`.
   - Cards are 56px high, with left accent borders: `.s0` (Green `#3fb950`, bg `#161b22`), `.s1` (Amber `#e3b341`, bg `#2a2210`), `.s2` (Red `#f85149`, bg `#2a1215`).
   - Modal dialog (lines 42-104) incorporates two detailed history sections:
     - Section 1 (Previous): Last service date, previous ODO, delta km traveled.
     - Section 2 (New): Current logging date, current baseline ODO, next due ODO.
     - Section 3: Slider input (`type="range" min="0" max="100"`) requiring slide to $\ge 90\%$ (`onSlideEnd`) to trigger confirmation and unshift record into `historyLogs`.
   - Web Settings Mockup (lines 137-268): Includes Odometer Sync input (`#cfg_odo`), Add New Item (`#new_name`, `#new_km`), Bluetooth settings (`#cfg_bt_name`, `#cfg_bt_pin`), full item interval list with visibility checkboxes (`#cfg_list`), single "Save All" button (`saveAllSettings()`), and Service History Log table (`#history_body`).

2. **Architecture & Hardware Plan (`civic_fc_maintenance_display_plan.md`):**
   - Line 212 explicitly states: *"ดูภาพ mockup ขนาดจริง (ขยาย 1.5 เท่า) ได้ที่ไฟล์ ui_mockups.html"*. Thus 360×480 in the HTML prototype is a 1.5× scale representation of the physical 240×320 portrait screen.
   - Section 2 defines the 14 standard Civic FC 1.8 + LPG maintenance items with their specific intervals (e.g., Engine Oil 10,000 km / 6 mo / 15%, CVT fluid 25,000 km / 18 mo / 15%, etc.).
   - Section 4.2 & 5 define the urgency sorting formula:
     ```cpp
     // ratio = min(kmLeft / intervalKm, daysLeft / intervalDays)
     bool moreUrgent(const Eval& a, const Eval& b) {
       if (a.status != b.status) return a.status > b.status;  // OVERDUE > DUE_SOON > OK
       return a.ratio < b.ratio;                              // smaller ratio higher up
     }
     ```
   - Section 0 item 9 and §5 specify safety locking: touch is locked when speed $> 5\text{ km/h}$.

3. **Current Firmware Implementation (`/Users/arm/Documents/Arduino/civic_hud/civic_hud.ino`):**
   - Line 158: `g_tft.setRotation(0);` establishes 240×320 portrait mode.
   - Line 240: Hardcoded `int maxCards = 5;` without pagination or scrolling, leaving items 6 to 14 completely unreachable.
   - Lines 264-276: Standard Latin fonts (`drawString(..., 2)`) used for Thai item names, causing illegible tofu boxes on physical screen.
   - Lines 413-415: Modal hardcodes date as literal `"TODAY"`, violating requirement R1 and Gap 6.
   - Lines 465-489: `setupWebServer()` only implements basic ODO and BT input fields, lacking interval adjustments, visibility checkboxes, add item, and service history logs.

4. **Clean Toolchain Compilation:**
   - Command: `/opt/homebrew/bin/arduino-cli compile --clean --fqbn "esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800" /Users/arm/Documents/Arduino/civic_hud` exited with code 0 (`Sketch uses 1747119 bytes (55%), Global variables use 64548 bytes (19%)`).

---

## 2. Logic Chain

1. **Resolution & Orientation:** The physical hardware is a CYD ESP32-2432S028 with 240×320 native resolution. The 360×480 HTML mockup in `ui_layout_c.html` represents a 1.5× scale magnification of 240×320 portrait display. Therefore, all layout coordinates scale by factor $2/3$.
2. **Color Palette Mapping:** Direct translation of Tailwind and hex color definitions in `ui_layout_c.html` to 16-bit RGB565 provides the exact high-contrast palette needed for the automotive display: `#0b0f14` (0x0841), OK Green `#3fb950` (0x3DCA), Amber `#e3b341` (0xE588), Red `#f85149` (0xF289).
3. **Usage Progress Bar Semantics:** In `ui_layout_c.html` lines 367-373, the progress bar fill width is calculated as $(1 - \text{ratio}) \times 100\%$, with 100% pinned for overdue items. This confirms the specification is strictly "Usage Consumed" (short green bar = fresh; near-full yellow = due soon; full red = overdue), rather than remaining life.
4. **Item Reachability:** Since vertical space (264 px) accommodates 5 cards (52 px each) and the vehicle has 14 items, a 3-page pagination model (5 + 5 + 4 items) operated via bottom bar touch zones (`◀`, `▶`) and locked when vehicle speed $> 5\text{ km/h}$ resolves the reachability defect while preserving driving safety.
5. **Modal & Slider Confirmation:** The modal requires rendering two distinct history boxes (Previous and New Record) and a slide-right-to-confirm touch slider with threshold $\ge 85-90\%$. The date must use a formatted Thai date representation rather than `"TODAY"`.
6. **Web Portal Completeness:** The web server at `192.168.1.1` must deliver the full interface from `ui_layout_c.html` (ODO sync, BT name/PIN, per-item interval editing, show/hide checkboxes, add item, Save All button, and history table), persisting all changes to NVS on submit.

---

## 3. Caveats

1. **Thai Font Engine Implementation Choice:** While this survey documents the exact Thai string requirements, font sizes, and layout constraints, the implementation team must select between a TFT_eSPI Smooth Font (.vlw in LittleFS/PROGMEM), Arduino_GFX U8g2 font, or a custom Thai bitmap font table based on Flash/RAM margins.
2. **Real-time Clock Absence:** As noted in the original request, hardware RTC / NTP sync is deferred; dates should use structured formatted relative dates or NVS-stored date placeholders rather than literal `"TODAY"`.

---

## 4. Conclusion

The authoritative UX/UI specifications from `ui_layout_c.html` and `civic_fc_maintenance_display_plan.md` have been fully surveyed, analyzed, and synthesized into `survey_spec.md`. The design represents a 240×320 portrait display with three distinct status tiers, intuitive wear-based progress bars, full pagination across 14+ items, a multi-stage reset modal with slide-right confirmation, and a comprehensive Web Settings portal at `192.168.1.1`.

---

## 5. Verification Method

To verify the specifications and findings:
1. Inspect the detailed specification deliverable:
   ```bash
   cat /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/spec_miner_survey_1/survey_spec.md
   ```
2. Inspect the reference design files:
   - `ui_layout_c.html`
   - `civic_fc_maintenance_display_plan.md`
   - `ORIGINAL_REQUEST.md`
3. Verify clean compilation baseline with `arduino-cli`:
   ```bash
   /opt/homebrew/bin/arduino-cli compile --clean --fqbn "esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800" /Users/arm/Documents/Arduino/civic_hud
   ```
