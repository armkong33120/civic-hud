# Handoff Report: Milestone 2 Adversarial Challenge (Challenger 1)

## 1. Observation

### Empirical Test Execution Commands & Results
1. **Dedicated Adversarial Test Suite Execution**:
   Command:
   ```bash
   /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17 -O2 -I/Users/arm/Documents/Arduino/civic_hud -I/Users/arm/Documents/Arduino/civic_hud/tests /Users/arm/Documents/Arduino/civic_hud/tests/test_challenger_storage.cpp /Users/arm/Documents/Arduino/civic_hud/MaintenanceEngine.cpp /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp /Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp -o /tmp/civic_hud_tests/test_challenger_storage && /tmp/civic_hud_tests/test_challenger_storage
   ```
   Result:
   ```text
   ======================================================================
     CHALLENGER 1: EMPIRICAL & ADVERSARIAL STRESS SUITE (MILESTONE 2)
   ======================================================================
     ✓ [Tier 2: Boundary]    Adv_Interval_ZeroKmAndZeroDays_Handling (0.03 ms)
     ✓ [Tier 2: Boundary]    Adv_Interval_OneMillionKm_BoundaryHandling (0.01 ms)
     ✓ [Tier 2: Boundary]    Adv_Interval_MaxUint16Days_65535Days (0.04 ms)
     ✓ [Tier 2: Boundary]    Adv_Interval_Uint32Max_BoundaryInvestigation (0.01 ms)
     ✓ [Tier 3: Combination] Adv_Interval_All24Items_ExtremePermutations_Sorting (0.03 ms)
     ✓ [Tier 1: Feature]     Adv_ThaiUtf8_ToneMarksAndCombiningCharacters_FidelityWithin47Bytes (0.00 ms)
     ✓ [Tier 1: Feature]     Adv_ThaiUtf8_EmpiricalBug_DefaultItemsSilentDataLoss (0.02 ms)
     ✓ [Tier 2: Boundary]    Adv_ThaiUtf8_ExactBoundary_47Bytes_NoTruncation (0.03 ms)
     ✓ [Tier 2: Boundary]    Adv_ThaiUtf8_Truncation_Over47Bytes_NullTerminated (0.00 ms)
     ✓ [Tier 2: Boundary]    Adv_ThaiUtf8_MultiByteCutAcross47ByteBoundary_NoCrash (0.02 ms)
     ✓ [Tier 2: Boundary]    Adv_Boundary_IdAndDateLengths_TruncatedSafely (0.01 ms)
     ✓ [Tier 2: Boundary]    Adv_CorruptBlob_TruncatedLength_RejectsGracefully (0.01 ms)
     ✓ [Tier 2: Boundary]    Adv_CorruptBlob_InvalidItemCount_RejectsGracefully (0.03 ms)
     ✓ [Tier 2: Boundary]    Adv_CorruptBlob_ErasedFlash_All0xFF_GracefulFallback (0.02 ms)
     ✓ [Tier 2: Boundary]    Adv_CorruptBlob_UnterminatedStringsInBlob_SafetyInvestigation (0.00 ms)
     ✓ [Tier 3: Combination] Adv_CorruptBlob_FuzzedRandomStreams_Stability (0.81 ms)
     ✓ [Tier 1: Feature]     Adv_RapidSequentialSaves_100Cycles (0.10 ms)
     ✓ [Tier 1: Feature]     Adv_HistoryLog_150EntriesStress_AccuracyAndOrder (0.29 ms)
     ✓ [Tier 2: Boundary]    Adv_HistoryLog_MalformedRowsSkippedGracefully (0.17 ms)
     ✓ [Tier 2: Boundary]    Adv_HistoryLog_CommasInItemName_EdgeCaseInvestigation (0.02 ms)
     ✓ [Tier 1: Feature]     Adv_WebSave_RapidSequential_50Cycles (0.25 ms)
     ✓ [Tier 2: Boundary]    Adv_WebSave_MalformedUrlEncodingFuzz (0.20 ms)
     ✓ [Tier 3: Combination] Adv_WebHtml_LargeHistoryRenderingPerformance (0.17 ms)
   ======================================================================
   TOTAL: 23 passed, 0 failed (Total 23 tests)
   ```

2. **Master Test Runner Execution**:
   Command:
   ```bash
   ./tests/run_all_tests.sh
   ```
   Result: **366 / 366 tests passed (100% pass rate across 7 test suites)**.

3. **Arduino Compilation Check**:
   Command:
   ```bash
   /opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud
   ```
   Result: Exit code 0, 0 errors, 0 warnings (Flash 2,026,432 bytes / 64%, RAM 70,292 bytes / 21%).

---

### Defects Observed & Empirically Verified

#### 1. CRITICAL DEFECT: Silent Truncation & Mutilated Thai Glyphs in 6 of 14 Default Items (BUG-M2-01)
- **Code Locations**:
  - `StorageManager.h:9-20`:
    ```cpp
    #pragma pack(push, 1)
    struct PackedItemRecord {
        char     id[12];
        char     name[48];        // UTF-8 Thai name with emoji
        uint32_t intervalKm;
        uint16_t intervalDays;
        uint8_t  warnPct;
        uint32_t lastKm;
        uint32_t lastEpoch;
        char     lastDate[24];
        uint8_t  flags;           // bit 0 = visible, bit 1 = isCustom (exact 100 bytes)
    };
    #pragma pack(pop)
    ```
  - `StorageManager.cpp:198-202`:
    ```cpp
    std::strncpy(blob[i].name, items[i].name.c_str(), sizeof(blob[i].name) - 1);
    ```
  - `MaintenanceEngine.cpp:12-26`:
    ```cpp
    m_items = {
        {"oil",    "🛢️ น้ำมันเครื่อง + กรอง", 10000, ...},
        {"cvt",    "⚙️ น้ำมันเกียร์ CVT",    25000, ...},
        {"tire",   "🔄 สลับยาง & ถ่วงล้อ",   10000, ...},
        {"air",    "🌬️ กรองอากาศเครื่องยนต์", 20000, ...},
        {"cabin",  "❄️ กรองแอร์ห้องโดยสาร", 15000, ...},
        ...
        {"lpg_t",  "🔧 เช็ครั่ว/จูนแก๊ส",  10000, ...},
        {"lpg_v",  "⚙️ เช็คระยะวาล์ว LPG", 40000, ...}
    };
    ```
- **Observed Behavior**:
  - `PackedItemRecord::name` is hardcoded to 48 bytes (`sizeof(name) - 1 = 47` usable bytes).
  - Thai UTF-8 characters require 3 bytes each; emojis require 4–7 bytes.
  - 6 of the 14 default items exceed 47 bytes in UTF-8:
    1. `oil` (62 bytes): `"🛢️ น้ำมันเครื่อง + กรอง"` $\rightarrow$ truncated to `"🛢️ น้ำมันเครื่อง"` (loses `+ กรอง`).
    2. `tire` (50 bytes): `"🔄 สลับยาง & ถ่วงล้อ"` $\rightarrow$ byte 47 cuts in the middle of character `อ` $\rightarrow$ truncated to `"🔄 สลับยาง & ถ่วงล้"`.
    3. `air` (68 bytes): `"🌬️ กรองอากาศเครื่องยนต์"` $\rightarrow$ byte 47 cuts in the middle of character `อ` $\rightarrow$ truncated to `"🌬️ กรองอากาศเครื"`.
    4. `cabin` (61 bytes): `"❄️ กรองแอร์ห้องโดยสาร"` $\rightarrow$ byte 47 cuts in the middle of character `ด` $\rightarrow$ truncated to `"❄️ กรองแอร์ห้องโ"`.
    5. `lpg_t` (51 bytes): `"🔧 เช็ครั่ว/จูนแก๊ส"` $\rightarrow$ byte 47 cuts in the middle of character `ส` $\rightarrow$ truncated to `"🔧 เช็ครั่ว/จูนแก"`.
    6. `lpg_v` (50 bytes): `"⚙️ เช็คระยะวาล์ว LPG"` $\rightarrow$ byte 47 cuts after space $\rightarrow$ truncated to `"⚙️ เช็คระยะวาล์ว "` (loses `LPG`).
  - Upon cold boot or first web save, `StorageManager::saveItems()` saves these truncated records. On subsequent reloads, the screen displays permanently mutilated Thai names.
  - Previous tests in `test_storage_web.cpp` missed this because they only checked `.find("น้ำมันเครื่อง") != npos` instead of exact equality.

#### 2. MEDIUM DEFECT: Unbounded String Deserialization in Corrupted NVS Blobs (BUG-M2-02)
- **Code Location**: `StorageManager.cpp:138-146` and `170-178`:
  ```cpp
  it.id = blob[i].id;
  it.name = blob[i].name;
  ...
  it.lastDate = blob[i].lastDate;
  ```
- **Observed Behavior**:
  - In `PackedItemRecord`, `id` (12B), `name` (48B), and `lastDate` (24B) are raw fixed-size char arrays.
  - When loading, they are assigned to `std::string` via `operator=(const char*)`, which executes `strlen()`.
  - If an NVS blob is corrupted in flash (e.g. uninitialized flash, bit flip, or truncated without a trailing zero), `strlen()` reads past the field boundary into subsequent struct members or past the buffer end, causing buffer over-read.

#### 3. LOW DEFECT: Unquoted Comma in History Log Causes Parsing Row Drop (BUG-M2-03)
- **Code Location**: `StorageManager.cpp:229-235` and `280-305`:
  ```cpp
  f.printf("%s,%s,%u,%u,%u,%u\n", record.dateStr.c_str(), record.itemName.c_str(), ...);
  ```
- **Observed Behavior**:
  - If a user names a maintenance item with a comma (e.g. `"🛢️ น้ำมันเครื่อง, กรอง"`), `appendHistoryLog` does not escape or wrap the field in double quotes.
  - When `loadHistoryLogs()` parses lines via `std::getline(stream, col, ',')`, the row produces 7 columns instead of 6. Column 2 contains `" กรอง"`, which fails `std::stoul`, and the entire history record is silently discarded.

---

## 2. Logic Chain

1. **Premise 1**: Requirement R1 and R2 in `ORIGINAL_REQUEST.md` state:
   - "Thai item names must render correctly on the real screen."
   - "Thai item names are legible and correctly shaped in those captures (no tofu boxes/garbage)."
   - "(7) text not clipped or overlapping."
   - "All designed features actually work and persist."
2. **Premise 2**: The design requires 14 specific default items defined in `MaintenanceEngine.cpp` for the Honda Civic FC 1.8 (LPG), including `"🛢️ น้ำมันเครื่อง + กรอง"` (62 bytes), `"🔄 สลับยาง & ถ่วงล้อ"` (50 bytes), `"🌬️ กรองอากาศเครื่องยนต์"` (68 bytes), `"❄️ กรองแอร์ห้องโดยสาร"` (61 bytes), `"🔧 เช็ครั่ว/จูนแก๊ส"` (51 bytes), and `"⚙️ เช็คระยะวาล์ว LPG"` (50 bytes).
3. **Premise 3**: In `StorageManager.h:11`, `PackedItemRecord` declares `char name[48]`, leaving 47 bytes for UTF-8 characters and 1 byte for null terminator.
4. **Premise 4**: Whenever `StorageManager::saveItems()` is called (which occurs on cold boot initialization in `civic_hud.ino:84`, during Web Settings `POST /save`, or upon slide-to-confirm reset), `std::strncpy(blob[i].name, items[i].name.c_str(), 47)` truncates strings exceeding 47 bytes.
5. **Deduction 1**: 6 out of 14 default items (42.8% of all items) are truncated.
6. **Deduction 2**: Truncation at byte 47 splits 3-byte Thai UTF-8 character sequences, creating severed consonant/vowel marks (`ถ่วงล้`, `กรองอากาศเครื`, `กรองแอร์ห้องโ`, `เช็ครั่ว/จูนแก`) that render as broken text on screen and lose critical service descriptors (`+ กรอง`, `LPG`).
7. **Conclusion**: This constitutes persistent data loss and violates R1 and R2.

---

## 3. Caveats

- **No Caveats on Bug Reproduction**: The truncation is 100% reproducible and proven empirically by test `Adv_ThaiUtf8_EmpiricalBug_DefaultItemsSilentDataLoss` in `tests/test_challenger_storage.cpp`.
- **Struct Size Constraint**: The 48-byte limit was introduced to fit `PackedItemRecord` into an exact 100-byte footprint ($14 \times 100 = 1,400$ bytes, $24 \times 100 = 2,400$ bytes). To support Thai item names up to 24 characters without truncation, `name` must be expanded (e.g. to `char name[72]` or `char name[80]`, resulting in a ~128-byte or ~132-byte struct), or item names must be abbreviated. Expanding the struct requires updating the blob size assertion from 100 to the new size across all tests.

---

## 4. Conclusion

**Verdict: REJECT**

Milestone 2 cannot be approved in its current state due to **BUG-M2-01 (Critical Data Loss & Thai Glyph Mutilation)**. 

### Required Remediations for Worker M2:
1. **Fix BUG-M2-01 (PackedItemRecord buffer size)**:
   - Expand `char name[48]` in `PackedItemRecord` to `char name[72]` (or `char name[80]`) so all 14 default Thai item names (max 68 bytes) fit with zero truncation.
   - Update `sizeof(PackedItemRecord)` expectation accordingly (e.g., if expanded to 72 bytes, struct size becomes 124 bytes; blob size for 14 items becomes 1,736 bytes, well within ESP32 NVS limits).
2. **Fix BUG-M2-02 (Safe String Deserialization)**:
   - In `StorageManager::loadItems()`, ensure bounded string construction:
     ```cpp
     it.id = std::string(blob[i].id, strnlen(blob[i].id, sizeof(blob[i].id)));
     it.name = std::string(blob[i].name, strnlen(blob[i].name, sizeof(blob[i].name)));
     it.lastDate = std::string(blob[i].lastDate, strnlen(blob[i].lastDate, sizeof(blob[i].lastDate)));
     ```
3. **Fix BUG-M2-03 (CSV Quoting)**:
   - In `StorageManager::appendHistoryLog()`, quote the item name (`"\"%s\""`), or sanitize commas in `record.itemName`.

---

## 5. Verification Method

### How to Independently Verify:
1. **Run the Challenger Storage Test Suite**:
   ```bash
   /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17 -O2 -I/Users/arm/Documents/Arduino/civic_hud -I/Users/arm/Documents/Arduino/civic_hud/tests /Users/arm/Documents/Arduino/civic_hud/tests/test_challenger_storage.cpp /Users/arm/Documents/Arduino/civic_hud/MaintenanceEngine.cpp /Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp /Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp -o /tmp/civic_hud_tests/test_challenger_storage && /tmp/civic_hud_tests/test_challenger_storage
   ```
   *Expected outcome*: `Adv_ThaiUtf8_EmpiricalBug_DefaultItemsSilentDataLoss` passes, demonstrating that `original != reloaded` for items 0, 2, 3, 4, 12, and 13.

2. **Run Master E2E Runner**:
   ```bash
   ./tests/run_all_tests.sh
   ```
   *Expected outcome*: All 7 suites execute and pass (366/366 tests).

3. **Verify On-Device Compilation**:
   ```bash
   /opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud
   ```
   *Expected outcome*: 0 errors, 0 warnings.
