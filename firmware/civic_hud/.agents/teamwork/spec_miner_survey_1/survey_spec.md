# UI & System Architecture Specification: Civic FC Maintenance HUD
**Target Hardware:** CYD ESP32-2432S028 (2.8" 240x320 ILI9341/ST7789 + XPT2046 Resistive Touch)  
**Authoritative Reference Specifications:**  
1. `ui_layout_c.html` (Approved UX/UI Design & Interaction Prototype)  
2. `civic_fc_maintenance_display_plan.md` (System Architecture & Hardware Plan v2)  
3. `ORIGINAL_REQUEST.md` (Engineering Requirements, Known Gaps & Acceptance Rubric)

---

## 1. Executive Summary & Architecture Overview

The Honda Civic FC 1.8 Maintenance HUD is an automotive auxiliary display mounted in the cabin of a Civic FC (R18Z1 engine, CVT gearbox). It monitors vehicle mileage and maintenance intervals in real time by connecting to an ELM327 OBD-II Bluetooth Classic (SPP) dongle plugged into the vehicle's OBD-II port.

The display presents maintenance countdowns sorted strictly by urgency (**Layout C: Service List**). When parked, the driver can tap any item to inspect its previous service record and perform a **Slide-Right-to-Confirm** gesture to log a new service event. All configuration (odometer calibration, per-item intervals, show/hide visibility, adding custom items, and Bluetooth pairing credentials) is managed via an on-board Wi-Fi Access Point (`CivicHUD`) at `http://192.168.1.1`.

---

## 2. Screen Dimensions, Orientation & Geometry

### 2.1 Physical Display Characteristics
- **Module:** CYD (Cheap Yellow Display) ESP32-2432S028 (HW-458).
- **LCD Driver:** ILI9341 (or ST7789 depending on board revision; configured via TFT_eSPI `setRotation(0)`).
- **Touch Controller:** XPT2046 Resistive Touch Controller (SPI bus via `XPT2046_Touchscreen`).
- **Physical Resolution:** **240 × 320 pixels** (Portrait orientation: Width = 240 px, Height = 320 px).

### 2.2 Scale Factor: HTML Prototype vs Physical Screen
In `ui_layout_c.html`, the mockup screen `.scr` is styled as:
```css
.scr { width: 360px; height: 480px; ... }
```
As explicitly documented in `civic_fc_maintenance_display_plan.md` (§5) and `ui_mockups.html` (line 19):
> *"CYD 2.8" · 240×320 แสดงขยาย 1.5×"* (Displayed scaled at 1.5× for desktop browser legibility).

Therefore:
$$\text{Scale Ratio} = \frac{360}{240} = \frac{480}{320} = 1.50$$
All CSS pixel coordinates in `ui_layout_c.html` scale down to native physical pixels by multiplying by $\frac{2}{3}$ (0.667).

### 2.3 Layout C Spatial Geometry (Native 240 × 320 Portrait)

| Zone | Y Range (px) | Height (px) | Background | Key Visual Elements |
|---|---|---|---|---|
| **Status Bar (Header)** | `y = 0 .. 27` | 28 px | `#161b22` (0x18E4) | Title "CIVIC FC", OBD Status Pill, Voltage (14.2V), Odometer |
| **Header Divider Line** | `y = 27 .. 28` | 1 px | `#21262d` (0x2125) | Horizontal separator line |
| **Service Cards Viewport** | `y = 28 .. 291` | 264 px | `#0b0f14` (0x0841) | Up to 5 visible cards per page, sorted by urgency |
| **Bottom Navigation Bar** | `y = 292 .. 319` | 28 px | `#0b0f14` (0x0841) | Top separator line (`#21262d`), Page Indicator (`หน้า 1/3`), navigation hints |

### 2.4 Service Card Geometry (per item)
- **Card Width:** 224 px (Left: 8 px, Right: 8 px; `x = 8 .. 231`).
- **Card Height:** 48 px.
- **Card Vertical Step:** 52 px (48 px card + 4 px inter-card gap).
- **Cards per Viewport:** $\lfloor 264 / 52 \rfloor = 5$ cards fit without overlapping.
- **Card Border Radius:** 4 px.
- **Accent Border (Left Edge):** 4 px solid color on left edge indicating status (Green / Amber / Red).
- **Progress Bar Track:**
  - Position: `x = 18`, `y = cardY + 28`, Width = 204 px, Height = 6 px (rounded 3 px).
  - Track Background: `#21262d` (RGB565: `0x2125`).
  - Progress Fill: Width proportional to usage percentage (`0% .. 100%`), color matched to status.

---

## 3. Color Palette & RGB565 Conversions

The UI uses a dark, high-contrast palette optimized for automotive nighttime and daytime visibility. All RGB values are converted to 16-bit RGB565 format for embedded TFT rendering:
$$\text{RGB565} = ((R_8 \ \& \ 0xF8) \ll 8) \ | \ ((G_8 \ \& \ 0xFC) \ll 3) \ | \ (B_8 \gg 3)$$

### 3.1 Comprehensive Color Map Table

| UI Element / State | Hex Color | 8-bit RGB (R, G, B) | RGB565 (Hex) | Usage in UI |
|---|---|---|---|---|
| **Screen Background** | `#0b0f14` | (11, 15, 20) | `0x0862` / `0x0841` | Global screen background |
| **Header Bar BG** | `#161b22` | (22, 27, 34) | `0x18E4` | Status bar & modal container background |
| **Divider / Track BG** | `#21262d` | (33, 38, 45) | `0x2125` | Horizontal borders, un-filled progress track |
| **Border Secondary** | `#30363d` | (48, 54, 61) | `0x31A7` | Card stroke outlines, modal border |
| **Text Primary (White)** | `#ffffff` | (255, 255, 255) | `0xFFFF` | Titles, item names, active odometer |
| **Text Off-White** | `#e6edf3` | (230, 237, 243) | `0xE77E` | Card body text |
| **Text Muted Gray** | `#8b949e` | (139, 148, 158) | `0x8C71` | Secondary hints, units |
| **Text Dark Gray** | `#6b7280` | (107, 114, 128) | `0x6B6D` | Footer text ("แตะเพื่อยืนยัน") |
| **Accent Sky Blue** | `#38bdf8` | (56, 189, 248) | `0x3DFE` | Voltage readout ("14.2V"), IP badges |
| **Status OK (Green)** | `#3fb950` | (63, 185, 80) | `0x3DCA` | Normal state card border, text, progress bar |
| **Card OK BG** (`.s0`) | `#161b22` | (22, 27, 34) | `0x10C4` / `0x16E4` | Background for OK cards |
| **Status DUE_SOON (Amber)**| `#e3b341` | (227, 179, 65) | `0xE588` | Warning state card border, text, progress bar |
| **Card DUE_SOON BG** (`.s1`)| `#2a2210` | (42, 34, 16) | `0x2902` | Dark amber background for warning cards |
| **Status OVERDUE (Red)** | `#f85149` | (248, 81, 73) | `0xF289` | Overdue state card border, text, progress bar |
| **Card OVERDUE BG** (`.s2`)| `#2a1215` | (42, 18, 21) | `0x2882` | Dark red background for overdue cards |
| **Slider Fill Emerald** | `#059669` | (5, 150, 105) | `0x04B3` | Slide-to-confirm progress fill |
| **Slider Active Thumb** | `#10b981` | (16, 185, 129) | `0x15D0` | Slide thumb handle (idle / dragging) |
| **Slider Full Highlight** | `#34d399` | (52, 211, 153) | `0x36B3` | Thumb handle when reached $\ge 85\%$ threshold |

---

## 4. Thai Text Typography & Inventory

### 4.1 Thai Font Rendering Challenge & Technical Solution
- **The Problem:** Built-in TFT_eSPI Latin fonts only parse ASCII (0x00..0x7F) or Latin-1 (0x80..0xFF). Thai characters are multi-byte UTF-8 sequences (`0xE0 0xB8 0x80` to `0xE0 0xB9 0xBF`). Passing Thai strings directly to `drawString()` results in **tofu boxes, illegal character glyphs, or unreadable mojibake**.
- **The Requirement:** The physical screen must render crisp, legible Thai characters for all 14 default items and custom items.
- **Font Rendering Options for CYD:**
  1. **TFT_eSPI Smooth Font (VLW Font / Flash Array):** Pre-compiled font including ASCII + Thai Unicode block (U+0E00..U+0E7F). TFT_eSPI has built-in UTF-8 decoding (`tft.setAttribute(UTF8_DECODER, true)`).
  2. **Arduino_GFX / U8g2 UTF-8 Font:** The project environment already has `GFX Library for Arduino` with `u8g2_font` Thai/UTF-8 support installed.
  3. **Custom Thai Bitmap Font (12pt / 14pt):** Compact, fast 14px or 16px bitmap font table covering the Thai glyph set, optimized for the 240px wide screen.

### 4.2 Standard Item Inventory (14 Default Maintenance Items)

| Item ID | Thai Name & Emoji (`name`) | Default Interval Km (`km`) | Default Interval Days (`d`) | Warning % (`warnPct`) | Baseline Last Km (`lastKm`) | Sample Last Date (`lastDate`) | Default Visibility |
|---|---|---|---|---|---|---|---|
| `oil` | 🛢️ น้ำมันเครื่อง | 10,000 km | 180 วัน (6 เดือน) | 15% | 90,000 | 12 ส.ค. 26 | `true` |
| `cvt` | ⚙️ น้ำมันเกียร์ CVT | 25,000 km | 540 วัน (18 เดือน) | 15% | 80,000 | 10 ม.ค. 26 | `true` |
| `tire` | 🔄 สลับยาง | 10,000 km | 0 (ไม่ใช้เวลา) | 10% | 88,800 | 15 พ.ค. 26 | `true` |
| `air` | 🌬️ กรองอากาศ | 20,000 km | 365 วัน (12 เดือน) | 10% | 86,740 | 15 พ.ค. 26 | `true` |
| `cabin` | ❄️ กรองแอร์ | 15,000 km | 365 วัน (12 เดือน) | 10% | 86,640 | 25 มี.ค. 26 | `true` |
| `brake` | 🛑 น้ำมันเบรก | 40,000 km | 730 วัน (24 เดือน) | 10% | 72,000 | 10 ก.ย. 24 | `true` |
| `bpads` | 🛑 ผ้าเบรก | 40,000 km | 0 (ไม่ใช้เวลา) | 10% | 75,000 | 20 ก.ค. 25 | `true` |
| `plugs` | ⚡ หัวเทียน (Iridium) | 60,000 km | 0 (ไม่ใช้เวลา) | 10% | 55,000 | 10 พ.ค. 25 | `true` |
| `batt` | 🔋 แบตเตอรี่ | 0 (ไม่ใช้ระยะ) | 730 วัน (24 เดือน) | 8% (~2 เดือน) | 0 | 5 ธ.ค. 25 | `true` |
| `cool` | 💧 น้ำยาหม้อน้ำ | 100,000 km | 3,650 วัน (10 ปี) | 5% | 0 | 10 พ.ย. 18 | `true` |
| `prb` | 📄 ภาษี & พ.ร.บ. | 0 (ไม่ใช้ระยะ) | 365 วัน (12 เดือน) | 10% (~1 เดือน) | 0 | 15 พ.ย. 25 | `true` |
| `lpg_f` | ⛽ กรองแก๊ส LPG | 20,000 km | 365 วัน (12 เดือน) | 15% | 85,000 | 20 ก.พ. 26 | `true` |
| `lpg_t` | 🔧 เช็ครั่ว/จูนแก๊ส | 10,000 km | 365 วัน (12 เดือน) | 10% | 92,000 | 15 มิ.ย. 26 | `true` |
| `lpg_v` | ⚙️ เช็คระยะวาล์ว LPG | 40,000 km | 0 (ไม่ใช้เวลา) | 10% | 65,000 | 15 ก.ย. 25 | `true` |

### 4.3 Thai UI Strings & Labels Inventory

| UI Location | Thai String | English Equivalent / Context |
|---|---|---|
| **Status Bar** | `OBD OK` / `OBD...` | ELM327 Connection State |
| **Card (Right Label - Distance Overdue)** | `เกิน 8,540 km` | Overdue distance format: `เกิน [km] km` |
| **Card (Right Label - Distance Remaining)** | `เหลือ 1,460 km` | Remaining distance format: `เหลือ [km] km` |
| **Card (Right Label - Time Overdue)** | `เกิน 1 เดือน` | Overdue time format: `เกิน [เดือน] เดือน` |
| **Card (Right Label - Time Remaining)** | `เหลือ 2 เดือน` | Remaining time format: `เหลือ [เดือน] เดือน` |
| **Footer / Bottom Bar** | `▲ ▼ แตะที่รายการเพื่อยืนยันเปลี่ยนแล้ว` | Navigation hint in HTML spec |
| **Modal Header Cancel Button** | `✕ ยกเลิก` | Dismiss modal button |
| **Modal Section 1 Title** | `⏮️ ประวัติการเปลี่ยนครั้งก่อนหน้า:` | Previous Service History Section |
| **Modal Section 1 Line 1** | `เปลี่ยนเมื่อ:` | Service date label |
| **Modal Section 1 Line 2** | `เลขไมล์รอบก่อน:` | Previous odometer reading label |
| **Modal Section 1 Line 3** | `ใช้งานรอบนี้ไป:` | Distance traveled during this cycle |
| **Modal Section 2 Title** | `🆕 รอบใหม่ที่จะบันทึก ณ วันนี้:` | New Service Record Section |
| **Modal Section 2 Line 1** | `วันที่บันทึก:` | Current logging date label |
| **Modal Section 2 Line 2** | `เลขไมล์เริ่มต้นใหม่:` | New starting odometer label |
| **Modal Section 2 Line 3** | `กำหนดเปลี่ยนรอบถัดไป:` | Next target due odometer label |
| **Slider Track Prompt Text** | `รูดไปขวาสุด บันทึกรอบใหม่ ➔` | Slide right confirmation prompt |
| **Web Config Title** | `หน้าเว็บตั้งค่าผ่าน Wi-Fi (Web Settings)` | Web settings header |
| **Web Config AP Hint** | `เชื่อมต่อ Wi-Fi "CivicHUD" จากมือถือ...` | Wi-Fi instructions |
| **Web Config Save Button** | `💾 บันทึกการตั้งค่าทั้งหมด` | Primary Save All submit button |
| **Web Config History Table Title** | `📜 สมุดประวัติการเปลี่ยนถ่ายที่บันทึกไว้` | History log table header |

---

## 5. Urgency Sorting Rules & Mathematical Model

### 5.1 Dual-Constraint Remaining Ratio Calculation
For every item $i$, maintenance urgency depends on **distance** and/or **time**, whichever expires first:

1. **Distance Condition (if $I_{\text{km}} > 0$):**
   $$\text{usedKm} = \max(0, \text{currentOdo} - \text{lastKm})$$
   $$\text{kmLeft} = I_{\text{km}} - \text{usedKm}$$
   $$r_{\text{km}} = \frac{\text{kmLeft}}{I_{\text{km}}}$$
   *(If $I_{\text{km}} == 0$, $r_{\text{km}} = \infty$)*

2. **Time Condition (if $I_{\text{days}} > 0$):**
   $$\text{usedDays} = \max(0, \text{currentDays} - \text{lastDays})$$
   $$\text{daysLeft} = I_{\text{days}} - \text{usedDays}$$
   $$r_{\text{days}} = \frac{\text{daysLeft}}{I_{\text{days}}}$$
   *(If $I_{\text{days}} == 0$, $r_{\text{days}} = \infty$)*

3. **Governing Remaining Ratio:**
   $$\text{ratio} = \min(r_{\text{km}}, \ r_{\text{days}})$$
   $$\text{isLimitedByTime} = (r_{\text{days}} < r_{\text{km}})$$

### 5.2 Status Categorization
The item is classified into one of three distinct status tiers based on `ratio` and its item-specific warning threshold $W = \frac{\text{warnPct}}{100}$:

$$\text{Status} = \begin{cases}
\text{OVERDUE (2)}, & \text{if } \text{ratio} < 0.0 \\
\text{DUE\_SOON (1)}, & \text{if } 0.0 \le \text{ratio} \le W \\
\text{OK (0)}, & \text{if } \text{ratio} > W
\end{cases}$$

### 5.3 Strict Sorting Algorithm
Items are ordered using a strict weak ordering comparator:
1. **Primary Sort Key:** `Status` in descending order:
   $$\text{OVERDUE (2)} \ > \ \text{DUE\_SOON (1)} \ > \ \text{OK (0)}$$
2. **Secondary Sort Key:** `ratio` in ascending order:
   - For `OVERDUE` items: smaller (more negative) ratio comes first (most severely overdue at the very top).
   - For `DUE_SOON` items: smaller ratio comes first (fewest % remaining at top of warning tier).
   - For `OK` items: smaller ratio comes first (higher wear comes before fresh items).

```cpp
bool moreUrgent(const MaintenanceItem& a, const MaintenanceItem& b) {
    if (a.status != b.status) {
        return static_cast<uint8_t>(a.status) > static_cast<uint8_t>(b.status);
    }
    return a.remainingRatio < b.remainingRatio;
}
```

---

## 6. Progress Bar Semantics: "Usage" Semantics

### 6.1 Contrast: Usage vs Remaining Life
A conventional battery gauge displays remaining capacity (100% full when fresh, 0% when empty).  
In contrast, this HUD implements an **intuitive automotive wear/usage bar**:
- **0% (Empty/Short Bar):** Fresh fluid or brand-new part. Safe, long remaining lifespan.
- **85% - 90% (Near-Full Yellow Bar):** Approaching scheduled service window. Visual trigger that service is imminent.
- **100% (Full Red Bar):** Pinned at 100% width, indicating service has lapsed (Overdue).

### 6.2 Mathematical Definition of Progress Width
$$\text{usedPct} = \begin{cases}
100.0\%, & \text{if } \text{Status} == \text{OVERDUE} \\
\operatorname{clamp}\big((1.0 - \text{ratio}) \times 100.0\%, \ 0.0\%, \ 100.0\%\big), & \text{otherwise}
\end{cases}$$

```
Fresh Service (0%):
[|                                                  ] 🟢 OK (0% used)

Mid-cycle (50%):
[=========================                         ] 🟢 OK (50% used)

Due Soon (88%):
[============================================      ] 🟡 DUE SOON (88% used)

Overdue (100%):
[==================================================] 🔴 OVERDUE (100% pinned)
```

---

## 7. Card Navigation & Pagination Mechanism

### 7.1 The 14-Item Reachability Problem
- Physical display height: 320 px.
- Vertical space allows exactly **5 cards** to be rendered simultaneously without visual clutter.
- The system manages **14 standard items** + up to 10 user-defined items (total $\le 24$).
- In the initial unpatched code, items with index $> 4$ were completely unreachable.

### 7.2 Approved Pagination Specification
To make all items reachable without requiring complex, jittery smooth resistive scrolling while driving:
- **Pagination Structure:**
  - `ITEMS_PER_PAGE = 5`
  - Total Pages: $N_{\text{pages}} = \lceil (\text{visibleItems.size()}) / 5 \rceil$ (e.g. 14 items = 3 pages: 5 + 5 + 4).
  - Page 1: Items 0..4 (Most urgent items).
  - Page 2: Items 5..9.
  - Page 3: Items 10..13.
- **Navigation Controls & Touch Zones:**
  - Bottom Bar (`y = 292 .. 320`):
    - Left zone (`x = 0 .. 80`): `◀ ก่อนหน้า` (Previous Page).
    - Center zone (`x = 80 .. 160`): `หน้า 1/3` (Page indicator).
    - Right zone (`x = 160 .. 240`): `ถัดไป ▶` (Next Page).
  - Optional vertical swipe gesture: Swipe up/down navigates pages.
- **Safety Lockout During Driving:**
  - When vehicle speed $> 5 \text{ km/h}$, touch inputs on cards and page changes are **locked** to prevent driver distraction.
  - Screen defaults to **Page 1** (the 5 most critical/overdue items) whenever speed exceeds $5 \text{ km/h}$.

---

## 8. Service Detail Modal Dialog

### 8.1 Modal Activation & Geometry
- **Trigger:** Tapping any visible maintenance card when vehicle speed $\le 5 \text{ km/h}$.
- **Container Placement (Native 240x320):**
  - Overlay: Full-screen modal overlay (`y = 20 .. 310`, `x = 8 .. 232`, Width = 224 px, Height = 290 px).
  - Background: Dark slate `#161b22`, border `#30363d` (or accent color of the item).
  - Border radius: 8 px.

### 8.2 Content Hierarchy
1. **Modal Header Bar (`h = 28 px`):**
   - Title: Item Icon + Thai Name (e.g. `🛢️ น้ำมันเครื่อง`).
   - Close Button: `✕ ยกเลิก` on top-right (`x = 175 .. 225`, `y = 24 .. 48`).
2. **Section 1: Previous Service History (`h = 68 px`):**
   - Background: `#0b0f14`, border `#21262d`.
   - Header: `⏮️ ประวัติการเปลี่ยนครั้งก่อนหน้า:`
   - Field 1: `เปลี่ยนเมื่อ: [lastDate] (~[daysAgo] วันก่อน)` (e.g. `12 ส.ค. 26 (~62 วันก่อน)`).
   - Field 2: `เลขไมล์รอบก่อน: [lastKm] km` (e.g. `90,000 km`).
   - Field 3: `ใช้งานรอบนี้ไป: [deltaKm] km` (e.g. `8,540 km`).
3. **Section 2: New Service Record to Log (`h = 68 px`):**
   - Background: `#0b0f14`, border `#064e3b` (emerald).
   - Header: `🆕 รอบใหม่ที่จะบันทึก ณ วันนี้:`
   - Field 1: `วันที่บันทึก: [currentDateStr]` (Must NOT be `"TODAY"`, formatted Thai date e.g. `13 ต.ค. 26`).
   - Field 2: `เลขไมล์เริ่มต้นใหม่: [currentOdo] km` (e.g. `98,540 km`).
   - Field 3: `กำหนดเปลี่ยนรอบถัดไป: [nextDueKm] km (+[intervalKm] km)`.
4. **Section 3: Slide-to-Confirm Slider (`h = 36 px`):**
   - Track positioned at bottom of modal (`y = 250 .. 286`).

---

## 9. Slide-Right-to-Confirm Reset Mechanics

### 9.1 Control Architecture
- **Track Dimensions:** `x = 16 .. 224` (Width = 208 px), `y = 248 .. 284` (Height = 36 px), Radius = 18 px.
- **Track Styling:** Background `#21262d`, stroke outline `#30363d`.
- **Center Prompt Text:** `รูดไปขวาสุด บันทึกรอบใหม่ ➔` (Font size 11-12px, centered in track).
- **Progress Fill:** Dynamic rectangle `#059669` extending from `trackX` to current thumb X.
- **Thumb Handle:** Circular button (diameter 32 px), starting at `x = 18 px`. Color: `#10b981` (emerald-500) with arrow `➔`.

### 9.2 Touch Drag Mechanics & Threshold
- **Touch Tracking:**
  As the driver's finger slides along the resistive touch sensor:
  $$\text{Progress \%} = \operatorname{clamp}\left(\frac{T_x - X_{\text{start}}}{X_{\text{end}} - X_{\text{start}}} \times 100\%, \ 0\%, \ 100\%\right)$$
- **Confirmation Threshold:** **$\ge 85\%$** (or $90\%$).
  - When progress reaches $\ge 85\%$, thumb turns bright emerald (`#34d399`) and expands slightly.
- **Release Behaviors:**
  - **Released before threshold ($< 85\%$):** Thumb instantly snaps back to 0%. No state change.
  - **Released at or past threshold ($\ge 85\%$):**
    1. Audio confirmation tone: Multi-frequency success chime (1200 Hz $\rightarrow$ 1500 Hz).
    2. RGB LED flashes green for 300 ms.
    3. Generate new `ServiceHistoryRecord`:
       - `itemName`: current item name
       - `dateStr`: current formatted date
       - `odoKm`: current vehicle odometer
       - `prevOdoKm`: previous service mileage
       - `deltaKm`: distance driven during this cycle
       - `nextDueKm`: new next due mileage
    4. Append record to in-memory history and append line to `/service_history.csv` on Flash/SD.
    5. Update item state:
       - `lastKm = currentOdo`
       - `lastEpoch = currentEpoch`
       - `lastDate = currentDateStr`
    6. Save items to NVS via `StorageManager::saveItems()`.
    7. Re-evaluate maintenance engine:
       - Item `usedPct` becomes `0%`, status becomes `OK`.
       - Urgency sorting immediately drops this freshly serviced item to the bottom of the list.
    8. Dismiss modal and redraw main list.

---

## 10. Web Settings Portal at `http://192.168.1.1`

### 10.1 Network Configuration
- **Wi-Fi Mode:** SoftAP (`WIFI_AP`).
- **SSID:** `CivicHUD` (no password by default for instant onboarding).
- **Static IP:** `192.168.1.1`, Subnet: `255.255.255.0`, Gateway: `192.168.1.1`.
- **HTTP Server:** Port 80.

### 10.2 Page Structure & Component Breakdown
The Web Settings portal is styled identically to `ui_layout_c.html` with dark automotive styling:

```
+--------------------------------------------------------------+
|  [http://192.168.1.1]  Civic FC 1.8 HUD - Web Settings       |
|  เชื่อมต่อ Wi-Fi "CivicHUD" จัดการการตั้งค่าและประวัติบำรุงรักษา |
+--------------------------------------------------------------+
|  CARD 1: Odometer Sync (เลขไมล์หน้าปัดรถจริง)                   |
|  [ 98540 ] km   [ บันทึกเลขไมล์ ]                              |
+--------------------------------------------------------------+
|  CARD 2: Bluetooth OBD-II Settings (หัวอ่าน ELM327)          |
|  Device Name: [ OBDII      ]   PIN: [ 1234 ]                 |
|  Status: Master Auto-Connect (PIN: 1234)                     |
+--------------------------------------------------------------+
|  CARD 3: เพิ่มรายการบำรุงรักษาใหม่ (Add Custom Item)           |
|  ชื่อ: [ น้ำยาฉีดกระจก    ]  รอบ: [ 5000 ] km  [ + เพิ่ม ]      |
+--------------------------------------------------------------+
|  CARD 4: ปรับแต่งรอบระยะทาง (กม.) / ซ่อนรายการ (Items List)     |
|  [x] 🛢️ น้ำมันเครื่อง           รอบ: [ 10000 ] กม.           |
|  [x] ⚙️ น้ำมันเกียร์ CVT          รอบ: [ 25000 ] กม.           |
|  [x] 🔄 สลับยาง                 รอบ: [ 10000 ] กม.           |
|  ... (All 14 items + custom items listed with checkboxes)    |
+--------------------------------------------------------------+
|  [ 💾 บันทึกการตั้งค่าทั้งหมด (Save All) ]                      |
+--------------------------------------------------------------+
|  CARD 5: สมุดประวัติการเปลี่ยนถ่าย (Service History Log Table) |
|  วันที่บันทึก | รายการ | ไมล์ที่เปลี่ยน | วิ่งรอบนั้น | กำหนดรอบถัดไป |
|  12 ส.ค. 26 | น้ำมันเครื่อง | 90,000 km | 10,000 km | 100,000 km |
|  ...                                                         |
+--------------------------------------------------------------+
```

### 10.3 API Endpoints & Form Contracts

1. **`GET /`**:
   - Returns the complete HTML document containing:
     - Pre-populated Odometer input (`g_odoEngine.getCurrentOdo()`).
     - Pre-populated BT Name & PIN loaded from NVS.
     - Complete list of items with their current interval and `checked` status if visible.
     - Pre-populated HTML table rows of service history records.

2. **`POST /save` (or `POST /api/save`)**:
   - Single multi-parameter submission triggered by **"💾 บันทึกการตั้งค่าทั้งหมด"**:
     - `odo`: integer (updates odometer calibration).
     - `bt_name`: string (saved to NVS).
     - `bt_pin`: string (saved to NVS).
     - `km_<id>`: integer per item (e.g. `km_oil=15000`).
     - `vis_<id>`: `"1"` / `"on"` if checkbox checked, absent or `"0"` if unchecked.
   - Processing logic on ESP32:
     1. Calibrate odometer if `odo` provided.
     2. Save BT config to NVS.
     3. Iterate all items in `MaintenanceEngine`, update intervals and `visible` flags.
     4. Persist updated items to NVS via `StorageManager::saveItems()`.
     5. Re-run `evaluate()`.
     6. Mark screen dirty (`g_screenDirty = true`) to refresh TFT immediately.
     7. Return HTTP 303 Redirect to `/` or HTTP 200 with success JSON.

3. **`POST /add_item`**:
   - `name`: string (e.g. `น้ำยาปัดน้ำฝน`).
   - `km`: integer interval.
   - Creates a new custom item, adds to engine, persists to NVS, refreshes UI.

4. **`GET /history.csv`**:
   - Exports the raw CSV history file for user backup.

---

## 11. Acceptance Rubric & Verification Mapping

| Rubric ID | Requirement Statement | Authoritative Source | Verification Method |
|---|---|---|---|
| **R1.1** | Screen resolution & orientation is 240x320 portrait | `ui_layout_c.html`, plan §5 | Direct inspection of `TFT_eSPI` driver init (`setRotation(0)`) |
| **R1.2** | Status bar displays OBD state, voltage, ODO | `ui_layout_c.html` lines 25-36 | Framebuffer / screen capture inspection |
| **R1.3** | Cards sorted Overdue > Due Soon > OK | `civic_fc_maintenance_display_plan.md` §5 | Unit test + on-screen capture comparison |
| **R1.4** | Usage progress bar semantics (short=fresh, full=overdue)| `ORIGINAL_REQUEST.md` R1 | Visual check: fresh reset shows 0% green bar |
| **R1.5** | All visible items reachable (pagination/scrolling) | `ORIGINAL_REQUEST.md` R1 | Navigation interaction test reaches items 5..13 |
| **R1.6** | Modal shows previous date, previous km, delta km, and new record | `ui_layout_c.html` lines 49-85 | Tap card, inspect modal dialog fields |
| **R1.7** | Slide-right confirms reset, logs history, sinks item to bottom | `ui_layout_c.html` lines 424-455 | Slide gesture past 85%, verify item resets and re-sorts |
| **R1.8** | Thai strings legible, correctly shaped, no tofu boxes | `ORIGINAL_REQUEST.md` R1, known gap 1 | Screen captures of Thai font rendering |
| **R2.1** | Web Settings at 192.168.1.1 has ODO sync, interval editing, hide/show, add item, BT name/PIN, Save All, history table | `ui_layout_c.html` lines 137-268 | Automated HTTP client request asserts all form elements |
| **R2.2** | Data persistence across reboot | `ORIGINAL_REQUEST.md` R2, known gap 4 | Modify items, restart ESP32, verify NVS restore |
| **R3.1** | OBD-II PID 0x31 delta advances odometer | `civic_fc_maintenance_display_plan.md` §4.1 | Automated ELM327 simulator test |
| **R3.2** | Auto-reconnect without blocking UI loop | `ORIGINAL_REQUEST.md` R3 | Simulator disconnect/reconnect timing benchmark |

---

## 12. Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|---|---|---|---|---|---|---|
| 1 | Layout | 240x320 Portrait Geometry | Physical display orientation and spatial mapping | `setRotation(0)` | 240x320 screen buffer | Misaligned touch if rotation mismatch | `ui_layout_c.html`, CYD hardware specs |
| 2 | Status Bar | Multi-status header bar | Displays vehicle identity, OBD connection pill, battery voltage, and current odometer | OBD link state, voltage ADC/PID, OdometerEngine | Formatted header string & status pill | Displays "OBD..." in amber when disconnected | `ui_layout_c.html` lines 25-37 |
| 3 | Typography | Thai UTF-8 Font Rendering | Renders 14 default items and custom items in legible Thai script without mojibake | UTF-8 Thai strings | Rendered glyphs on ILI9341 | Tofu / garbage characters if Latin-only font used | `ORIGINAL_REQUEST.md` Gap 1 |
| 4 | Core Logic | Dual-Constraint Urgency Evaluation | Calculates remaining life by $\min(r_{\text{km}}, r_{\text{days}})$ | Current ODO, elapsed days, intervals | AlertStatus (`OK`, `DUE_SOON`, `OVERDUE`) | Fallback to single constraint if other is 0 | `civic_fc_maintenance_display_plan.md` §4.2 |
| 5 | Core Logic | Urgency Sorting Engine | Orders items: `OVERDUE` $\rightarrow$ `DUE_SOON` $\rightarrow$ `OK`, then by lowest ratio | List of maintenance items | Vector of sorted items | Non-destructive view; raw storage order preserved | `ui_layout_c.html` line 361 |
| 6 | Visuals | "Usage" Progress Bar Semantics | Visualizes consumed wear (short = fresh, full = overdue) | Item `usedPct` | Colored progress bar (green/amber/red) | Clamped to 0..100% | `ORIGINAL_REQUEST.md` R1 |
| 7 | Navigation | Multi-page / Scroll Navigation | Enables driver to browse all 14+ items across pages of 5 items | Bottom bar touch (`◀`, `▶`) | Page index change & view re-render | Locked when vehicle speed > 5 km/h | `ui_layout_c.html` line 39 |
| 8 | Modal | Detailed Previous Service View | Shows last service date, odometer, and mileage accumulated in cycle | Tapped item data | Pop-up modal dialog | Disabled while driving (> 5 km/h) | `ui_layout_c.html` lines 49-66 |
| 9 | Modal | New Service Record Preview | Displays current logging date, new baseline odometer, and next due odometer | Current vehicle state | Pop-up modal dialog | Literal `"TODAY"` forbidden | `ui_layout_c.html` lines 68-85 |
| 10 | Interaction | Slide-Right-to-Confirm Reset | Smooth slider control requiring horizontal swipe $\ge 85\%$ to reset item | Touch X/Y coordinates | Reset trigger, audio chime, LED flash | Snaps back to 0% if released early | `ui_layout_c.html` lines 88-103 |
| 11 | Storage | Service History Logging | Records confirmed service events into persistent log | Service history struct | CSV appended to Flash/SD & in-memory vector | Survives power cycle | `ui_layout_c.html` lines 272-277 |
| 12 | Web Portal | Odometer Calibration Sync | Synchronizes HUD odometer with car instrument cluster | Numeric ODO input | Calibrates `OdometerEngine` and saves to NVS | Ignored if $\le 0$ | `ui_layout_c.html` lines 152-156 |
| 13 | Web Portal | Per-item Interval & Visibility Edit | Form inputs allowing adjustment of intervals (e.g. 10k $\rightarrow$ 15k) and hide/show | Form checkboxes & interval numbers | Updated intervals & visibility in engine | Non-negative constraint | `ui_layout_c.html` lines 216-222 |
| 14 | Web Portal | Add Custom Maintenance Item | Dynamic addition of custom items (up to 24 items total) | Item name & interval | New item appended to active item list | Limited to max 24 items | `ui_layout_c.html` lines 159-165 |
| 15 | Web Portal | Bluetooth OBD Configuration | Configures ELM327 Bluetooth target device name and pairing PIN | Device Name, PIN string | NVS persistence, re-initializes BT client | Falls back to defaults ("OBDII", "1234") | `ui_layout_c.html` lines 184-214 |
| 16 | Web Portal | Single "Save All" Submission | Synchronizes all web changes in a single operation | Form POST to `/save` | Instant update of screen & flash storage | Visual green flash & toast notification | `ui_layout_c.html` lines 225-233 |
| 17 | Web Portal | Service History Table View | Displays historical service log in structured HTML table | `GET /` | HTML table rendering of history logs | Displays empty state if no records | `ui_layout_c.html` lines 245-266 |
| 18 | Audio/Light | Multi-Chime & RGB Alert Engine | Audible and visual notifications for warnings, touch clicks, and resets | Alert status & touch events | Buzzer tones (IO26), RGB LED (IO17,4,16) | Active-low LED polarity handled | `civic_fc_maintenance_display_plan.md` §4.4 |
| 19 | Hardware | Auto-dimming via LDR Sensor | Adjusts backlight PWM based on ambient light sensor | LDR analog read (GPIO34) | Backlight PWM on IO21 (60..255) | Clamped to safe brightness limits | `civic_fc_maintenance_display_plan.md` §4.4 |
| 20 | Safety | In-Motion Touch Lockout | Disables interactive modal opening when car is in motion | Vehicle speed (PID 0x0D) | Disables modal touch events if speed > 5 km/h | Protects driver safety | `civic_fc_maintenance_display_plan.md` §0 |

---

## 13. Edge Cases & Failure Modes

| # | Feature | Input / Condition | Observed / Required Behavior |
|---|---|---|---|
| 1 | Odometer | ECU DTC clear or 12V battery disconnect (PID 0x31 drops from 1200 km to 5 km) | Must treat drop as counter rollover/reset: add delta $5 \text{ km}$ to existing odometer without losing accumulated mileage or jumping backward. |
| 2 | Odometer | ELM327 returns `NO DATA` on PID 0x31 | Fall back to numerical integration of vehicle speed (PID 0x0D: $\Delta d = v \times \Delta t$) until PID 0x31 resumes. |
| 3 | Touch Interaction | Driver touches card while vehicle is traveling at 60 km/h | Touch event ignored; modal does not pop up. Driving safety lockout remains active until speed $\le 5\text{ km/h}$. |
| 4 | Slider Reset | Driver drags slider to 65% and lifts finger | Thumb snaps back to 0%. No service record logged; maintenance countdown remains unchanged. |
| 5 | Slider Reset | Driver drags slider to 92% and lifts finger | Service reset executes: audio chime plays, item resets to 0% used, sinks to bottom of sorted list, and history entry is logged. |
| 6 | Navigation | User reaches page 3 of 3 and taps "ถัดไป ▶" | Page index wraps to Page 1 or disables next button. |
| 7 | Web Settings | User unchecks visibility for "กรองอากาศ" and clicks "Save All" | Item disappears from on-screen display list immediately, but remains saved in NVS for re-enabling later. |
| 8 | Web Settings | User enters interval = 0 for an item with intervalDays = 0 | Validation rejects or item disabled; at least one interval dimension (km or days) must be positive. |
| 9 | Date Recording | Hardware reset performs service reset with no RTC/NTP | Formatted date string generated using relative calendar offset or NVS stored date hook; must NEVER store literal `"TODAY"`. |
| 10 | Power Loss | 12V ignition switched off abruptly during driving | Odometer wear-leveling saves every 5 km or when stopped; lost distance upon sudden cut is strictly $< 5\text{ km}$ (or $< 1\text{ km}$). |
| 11 | Bluetooth Dongle | Dongle powered off (ignition off) | Disconnect detected within $\le 10\text{ s}$; background retry task runs every 15 s without blocking UI/touch responsiveness. |
| 12 | Memory Limits | User adds custom items up to max capacity (24 items) | Item addition disabled beyond 24 items to safeguard SRAM on non-PSRAM ESP32-WROOM. |
