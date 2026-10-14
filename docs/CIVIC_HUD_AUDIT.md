# รายงานตรวจรับช่วงงาน Civic HUD

วันที่ 13 ตุลาคม 2026 — ตรวจจากโค้ดจริง รายงานทีมเดิม การคอมไพล์ และการทดสอบบน Mac

## ข้อสรุปของ Lead Agent

ยังไม่พร้อมประกาศเสร็จหรือแฟลชรุ่นสุดท้าย งาน M1 ต้องเปิดแก้เรื่องความถูกต้องของเลขไมล์, M2 มีการแก้บางส่วนแต่ยังมีปัญหาการย้ายข้อมูลและการรับข้อความ, M3 ยังไม่เชื่อมการแสดงผลภาษาไทย/การแบ่งหน้าเข้ากับเฟิร์มแวร์จริง, M4 ยังไม่มีหลักฐานยืนยันครบตามเกณฑ์

รอบนี้ใช้ Lead 1 คนและ Sub-Agent 3 คนตรวจ OBD, Storage/Web และ UI แยกกัน รายงานเก่าที่ระบุอนุมัติ 5 เสียงเป็นประวัติของ Antigravity ไม่ใช่การอนุมัติของทีมปัจจุบัน

## ผลตรวจจริง

- คอมไพล์เฟิร์มแวร์ปัจจุบันสำเร็จ: โปรแกรม 2,027,560 / 3,145,728 ไบต์ (64%), global RAM 70,292 / 327,680 ไบต์ (21%) ไม่รวม heap/stack ขณะทำงาน
- Master runner: 365 ผ่าน / 1 ไม่ผ่าน ก่อนหยุดที่ suite 6; suite 7 ไม่ถูกเรียกเพราะ runner หยุดเมื่อเจอความผิดพลาด
- รัน suite 7 แยก: 19 ผ่าน / 4 ไม่ผ่าน
- รวม 7 suite ที่รัน: 384 ผ่าน / 5 ไม่ผ่าน / 389 กรณี ตัวเลขเหล่านี้ไม่ใช่หลักฐานว่าบอร์ดหรือ Bluetooth จริงผ่านทั้งหมด
- ข้อที่ไม่ผ่าน 5 ข้ออ้างอิงขนาดบัฟเฟอร์เก่าหรือพฤติกรรม CSV เก่าที่แก้ไปบางส่วน ต้องปรับเกณฑ์ให้ตรงข้อกำหนดและเพิ่มการตรวจ UTF-8/การย้ายข้อมูลจริง ไม่ใช่เพียงลดความเข้มของการทดสอบ
- ฟอนต์/หน้าจอหลายข้อทดสอบแบบจำลองแยก ไม่ได้เรียก sketch ที่ลงบอร์ด ส่วน worker Bluetooth ถูกตัดออกเมื่อทดสอบบน host

## ลำดับแก้ที่แนะนำ

1. ป้องกันเลขไมล์เพิ่มผิดเมื่อล้าง ECU, ตรวจข้อมูล PID อย่างเข้มงวด และออกแบบการบันทึกเลขไมล์คู่กับ baseline ที่สอดคล้องกัน
2. ป้องกันการทับข้อมูลเดิมเมื่อเปลี่ยน schema บันทึกจาก 100 เป็น 132 ไบต์; ตรวจผลเขียนจริงและเส้นทางกู้ข้อมูล
3. ประเมินรอบบำรุงรักษาใหม่เมื่อเลขไมล์เพิ่ม และยืนยันบริการด้วย ID คงที่ + การรูดจริงจากต้นแถบ
4. จัดการข้อมูล Bluetooth ข้าม Core อย่างปลอดภัย และแยกสถานะ BT เชื่อมต่อ / ECU พร้อมใช้งาน / ข้อมูลหมดอายุ
5. กำหนดขีดจำกัดชื่อเป็นไบต์หรือตัวอักษรให้ชัด, ไม่ตัดกลาง UTF-8, แก้การ decode ฟอร์มซ้ำ, escape HTML และตรวจช่วงค่าตัวเลข
6. ทำ renderer ไทย การจัดวางข้อความ และการแบ่งหน้าบนโค้ดจริง พร้อมทดสอบสัมผัสจริง
7. ปรับการทดสอบให้ใช้ logic ที่เฟิร์มแวร์เรียกจริง แล้วตรวจ persistence บนบอร์ด, ความนิ่ง, ภาพหน้าจอ และจัดรายงานส่งมอบ

### วันที่และเวลา

คำสั่งเดิมเลื่อนระบบซิงก์วันเวลาจริงออกนอกขอบเขต จึงไม่บังคับเพิ่มระบบเวลาในรอบนี้ แต่ต้องเลิกบันทึกวันที่ตายตัวว่า 13 ต.ค. 26 เสมือนเป็นวันจริง ใช้สถานะไม่ทราบวันที่และวาง hook ไว้ให้เพิ่มภายหลัง รายการที่ครบกำหนดตามวันต้องแสดงข้อจำกัดนี้อย่างตรงไปตรงมา

## SD Card ที่เพิ่งเสียบ

โปรแกรมปัจจุบันมี pin mapping แต่ยังไม่มี SD.begin หรือเส้นทางอ่าน SD การเสียบในช่องบอร์ดไม่ทำให้ Mac เห็นเป็นไดรฟ์ USB โปรแกรมขนาด 2.03 MB ยังไม่ต้องย้ายไฟล์ออกจาก Flash โดย SD สามารถใช้เก็บฟอนต์/รูป/ประวัติ แต่ไม่ใช่พื้นที่ให้ ESP32 บูตเฟิร์มแวร์โดยตรง ผลตรวจฮาร์ดแวร์ SD จะเพิ่มท้ายรายงานเมื่อเสร็จ

## รายงาน Sub-Agent โดยละเอียด

ข้อค้นพบด้านล่างเป็นผลตรวจโค้ดและการจำลองที่ระบุไว้ ไม่ใช่ผลทดสอบรถหรือหัว OBD จริง

# OBD / Bluetooth / odometer independent audit

Read-only audit, 2026-10-13. No firmware edits, flash, or physical RF test performed. Root source: `/Users/arm/Documents/Arduino/civic_hud`.

## Confirmed required fixes

1. **P1 — ECU-clear inflation.** `/Users/arm/Documents/Arduino/civic_hud/OdometerEngine.cpp:59` treats any decrease from >60000 to <5000 as rollover. Clearing DTCs at PID31=65000 followed by 0 adds 536 km with no travel. Reproduced against production OdometerEngine: calibrated 100000 becomes 100536. Need detect ambiguous resets with plausibility/time/speed evidence, avoid inventing distance, and confirm actual ECU counter behavior rather than assume rollover. Historical reviewer_m1_2 also noted this but approved M1 anyway.

2. **P1 — Reboot mileage loss despite existing persistence APIs.** `/Users/arm/Documents/Arduino/civic_hud/civic_hud.ino:73` restores only integer odometer; line 134 saves only every 5 km. `StorageManager::loadLastPid31/saveLastPid31` have no callers anywhere in production. First post-boot PID31 is silently set as fresh baseline (`OdometerEngine.cpp:44`), so <5 km since last save is lost on each power cycle, plus distance while board is off. Reproduced: persisted 100000, travel 4 km, reboot at PID31=104, next +1 km gives 100001 instead of 100005. Need coordinated odometer+PID31 persistence/snapshot and explicit fallback fractional-loss policy. Saving independently without matching baseline would itself risk duplicate distance.

3. **P1 — Changed Bluetooth credentials race with worker.** `/Users/arm/Documents/Arduino/civic_hud/WebSettings.h:201` calls begin on Core1 while worker on Core0 uses `m_btDeviceName.c_str()/m_btPin.c_str()` in `ObdTask.cpp:403`. `begin` writes both std::strings at lines 141–142 without synchronization. Reallocation during printf/connect can invalidate pointers. It also updates stack PIN from UI task while worker may be pairing. Transfer validated configuration through worker-owned command/queue, reconnect deliberately when device changes. Current begin does not disconnect an already connected old dongle; immediate application is incomplete.

4. **P2 — PID31 permanently disabled by one NO DATA.** `/Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp:512` sets pid31Supported=false, while line 491 requires true to query it again. INITIALIZING_ELM never resets support on reconnect. A transient ECU startup NO DATA causes speed-only mode until board reboot. Add capability re-probe with recovery baseline handling; avoid double-counting already integrated distance when PID31 returns.

5. **P2 — Reconnection <=10 s is not implemented as a bound.** `/Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp:404` uses connect by device name without deadline. Installed ESP32 3.3.10 primary source `/Users/arm/Library/Arduino15/packages/esp32/hardware/esp32/3.3.10/libraries/BluetoothSerial/src/BluetoothSerial.cpp:73` defines SCAN_TIMEOUT=32 s and line 989 waits that long. 3.5 s backoff cannot prove recovery <=10 s; if dongle appears during a timed-out attempt, retries can be delayed. Prefer cached address / event-driven bounded discovery and explicitly define timing from adapter availability. This blocks OBD worker, not UI Core1.

6. **P2 — ELM initialization always reports success.** `/Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp:85` returns true regardless of all command timeouts/errors; worker at line 426 ignores return and marks obdConnected=true at 438 without a valid ECU response. ATSP6 '?' or silent dongle gives false OBD OK. Validate AT acknowledgements, retain separate BT-link vs ECU-ready state, retry failed init.

7. **P2 — Parser accepts signed nonhex bytes.** `/Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp:256` parses two-char chunks with strtoul and checks only final pointer. '-1' is accepted as a byte. Actual production repro `parsePid0131("41 31 -1 00")` returned OK / 65280, which can massively inflate mileage. Validate exactly two hex digits before conversion in all PID parsers; reject malformed tokens.

8. **P2 — Cross-core reads not synchronized.** `/Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp:210` exposes raw telemetry const reference, `isConnected` reads shared bool, worker writes fields. Odometer getters lines 117–150 do not take the writer's critical-section lock, so multi-field trip/baseline snapshots can mix updates. Historical reviewer_m1_2 explicitly identified telemetry gap. Use copy snapshots under lock; mutex-protected writers alone do not protect readers.

9. **P2 — Independent PID failures mask each other.** `/Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp:472` resets shared error count on every valid speed; line 499 resets on every valid PID31. If speed works but PID31 times out indefinitely, count never reaches 4 and odometer stays frozen in supported mode. If speed fails while PID31 works, stale speed persists and failures are periodically erased. Track failures/staleness per signal and explicit degraded modes.

10. **P2 — Failed speed polls discard time in fallback.** `/Users/arm/Documents/Arduino/civic_hud/ObdTask.cpp:465` computes dt since previous poll then line 466 moves timestamp even if query fails. Next valid speed integrates only the latest polling interval; elapsed failed intervals disappear. Need defined integration using valid sample times and bounded gap handling instead of assuming every command succeeds.

## Evidence and test scope

- Historical M1 gate says PASS with five approvals / CLEAN. `reviewer_m1_2/handoff.md` nevertheless records telemetry race, ECU-reset ambiguity, mock isolation; these remain in current sources. Approvals cannot establish finished behavior.
- `tests/run_all_tests.sh:58` ELM suite compiles test_elm_simulator.cpp + OdometerEngine.cpp only; own `ElmDriverSimulator` supplies Bluetooth and protocol state transitions.
- Challenger BT suite links ObdTask.cpp, but `tests/test_challenger_bt_speed.cpp:96` defines its own `SimulatedObdStateMachine`. Recovery and dual-thread tests use this class. Actual `ObdTask::workerLoop` is entirely inside #ifdef ARDUINO and is empty in host build (`ObdTask.cpp:385–572`); host begin simply sets connected=true (`:190`). Thus host pass supports parser/math portions, not actual FreeRTOS/BT worker, reconnect bound, stack behavior, or runtime races.
- `tests/run_all_tests.sh:111–115` prints hardcoded test totals, not measured aggregate counts. Avoid citing 328/366 as proof of live behavior. Parent team is checking complete latest test outcomes.
- Reproductions saved in `work/audit_obd_repro.cpp`; built with installed clang using explicit macOS SDK sysroot and current production ObdTask.cpp/OdometerEngine.cpp. Executed outputs:
  - ECU clear 65000 -> 0: 100536 (expected 100000 for no travel)
  - Reboot before save: before 100004, after +1 km 100001 (expected 100005)
  - Malformed PID31 '-1 00': result 0 / OK, distance 65280 (expected ERROR_RESP)

## Recommended order

First fix odometer integrity (reset plausibility, matched persistence, strict parser), then worker-owned Bluetooth configuration and snapshots. Extract worker transitions behind injected clock/transport so tests execute actual state machine, cover temporary NO DATA and partial PID failures, then verify board compile, on-device stability, and actual dongle reconnect timings. Do not mark M1 100% based only on old gate.


# Storage / Web / Maintenance audit (read-only)

Source: `/Users/arm/Documents/Arduino/civic_hud`; audited production sources, old M2 handoffs, current tests. No firmware files changed, no device flash or hardware test. Probe programs and logs kept in this chat's `work/`.

## Verified findings needing correction

1. **P1: Schema upgrade silently discards saved maintenance configuration.** `StorageManager.h:9-25` changed packed record from 100 to 132 bytes. `StorageManager.cpp:124-129` only accepts the new size, with no schema/version/migration. `civic_hud.ino:81-83` interprets rejection as first boot, loads defaults and writes over old data. Existing board running previous firmware will lose custom names, intervals, visibility and service baselines on upgrade. Probe injected a 1400-byte/14-item legacy blob: load=false. Add versioned migration from legacy record and preserve a recovery copy; never overwrite malformed storage automatically.

2. **P1: Mileage changes do not recompute maintenance alerts.** `civic_hud.ino:118-139` updates OBD, draws cached state and persists mileage, but never evaluates maintenance. `drawServiceList:249` only sorts cached items. `evaluate` only called setup (85), web save (`WebSettings.h:233`) or service reset (`MaintenanceEngine.cpp:125`). Crossing a service threshold during normal driving leaves alert, remaining km, RGB and ordering stale until save/reset/reboot. Reevaluate against a consistent current odometer before UI updates and notify alert changes.

3. **P1: Date-based service alerts cannot progress.** setup and web evaluation pass day0 (`civic_hud.ino:85`, `WebSettings.h:233`); service reset also day0 and fixed `13 ต.ค. 26` (`civic_hud.ino:426`), custom creation fixed same date (`WebSettings.h:224`). Defaults use arbitrary day offsets 62,300,... rather than a clear absolute epoch contract (`MaintenanceEngine.cpp:13-26`). Real days never advance; battery/PRB alerts remain healthy indefinitely. Choose clock source and absolute-day representation, migrate defaults, provide date setting/offline behavior, and use actual date on reset.

4. **P1: Stored script/attribute injection.** `WebSettings.h:93-94,115,137-138` inserts BT name/PIN, item name and history fields raw into HTML attributes/text. Probe confirmed custom `<script>alert(1)</script>` appears executable in rendered page; BT `x' autofocus onfocus='alert(1)` escapes the attribute. Existing `test_challenger_m2_web.cpp:596-613` incorrectly calls this safe and explicitly requires raw `<script>`. Escape all HTML contexts; test escaped output and literal display. Applies to portal/own LAN context, not a proven remote internet exposure.

5. **P1/P2: Hardware HTTP route corrupts literal form values.** `WebSettings.h:301-310` normally takes `server.arg()` values that the WebServer already decoded, joins them with raw `&`/`=`, then `handleSavePayload` percent-decodes again (164-169). Names such as `A&B`, `A+B` or literal `%20` are split or changed; separator-containing values can alter other parameters. Accept decoded map directly, or percent-encode each reconstructed name/value. Current host tests bypass Arduino route, so fail to cover it. This is source-verified routing bug; physical HTTP not tested here.

6. **P2: Name80 means 79 UTF-8 bytes, not 80 Thai characters; truncation splits code points and differs across reboot.** `StorageManager.h:12`, `StorageManager.cpp:201` use raw byte strncpy. `MaintenanceEngine.cpp:157` retains full unbounded name in RAM prefixed with a 5-byte emoji+space; reboot loads 79-byte prefix. Probe with 40 Thai ก yielded RAM125 vs NVS79; last byte0xB8 is incomplete UTF-8 (5+74=79, Thai triples cut after two bytes). Buffer does fit all current defaults (largest68 bytes), fixing old BUG-M2-01; broader custom contract not implemented. Define max bytes/codepoints explicitly, reject/normalize before RAM mutation, truncate only complete codepoints/graphemes, account for prefix, migrate schema if 80 characters truly required. Same risk for lastDate/id raw truncation.

7. **P2: Save paths claim success regardless of flash failure; “atomic save” is not atomic.** `StorageManager.cpp:28,56-59,86-90,112-116,214-216` ignore Preferences return values. Items count and blob are independent writes; loss of power between them creates a mismatched record and subsequent default overwrite. Web ignores all save failures (`WebSettings.h:181,200,230`) and always redirects success (236). Service reset ignores history/item saves (`civic_hud.ino:427-428`). `LittleFS.begin(true)` autoformats on mount failure (29), risking history erasure. Use checked commits, a single versioned blob or recoverable dual slot, expose save errors and avoid formatting history automatically.

8. **P2: Negative/oversized interval parsing accepts invalid values, leading to wrong arithmetic.** `WebSettings.h:210,222` std::stoul accepts -1; assigning to uint32_t yields4294967295 (probe confirmed). Prefix values `100abc` accepted, platform long width differs, no explicit uint32 bounds. `MaintenanceEngine.cpp:47-48` casts interval and used distance to int32 before subtraction, so extreme accepted settings produce invalid remaining distances and can invoke signed overflow. Validate entire digits-only input and sensible bounded ranges before any mutation; use widened arithmetic.

9. **P2: CSV multiline names cannot reload.** Writer quotes newlines (`StorageManager.cpp:225-243`), but loader splits physical lines before parsing quotes (317,323), so `%0A` name allowed from web produces a history entry absent on load. Probe append with `before\nafter` =>0 loaded records. Parse whole logical CSV records or reject control characters at input. Comma/quote fixes are now present and work; old tests asserting comma data loss must be reversed.

10. **P2: History grows without bound and is fully duplicated in RAM.** append never limits log; `exportHistoryCsv:302` reads entire file to Arduino String then std::string, `loadHistoryLogs:312` duplicates it again and creates vector; HTML appends all history rows. Long-lived usage risks flash exhaustion/heap allocation failures and UI stalls. Use retention/rotation plus streaming/pagination; verify controlled failure on full filesystem. Hardware exhaustion not tested.

11. **P2/P3: Partial save hides every existing item.** `WebSettings.h:206-216` sets visibility=false for absent checkbox keys, including empty/malformed payload or BT-only update/serial bridge commands. Full browser form intentionally omits unchecked boxes; need explicit full-form marker/scope, otherwise preserve visibility for partial updates. Existing tests accept this behavior rather than checking intended partial-update semantics.

12. **P3: ID reuse after removal.** `MaintenanceEngine.cpp:154` generates `custom_(size+1)`. Adding custom15, custom16, removing custom15 then adding gives second custom16. Duplicate POST keys affect both items; reset hits first match. Not currently exposed via a web removal control, but public remove API supports this path. Use max existing suffix+1 or stored unique counter; enforce uniqueness on load.

## Current verification results

Production-linked dedicated web challenger: **29/30 pass**, failure `test_challenger_m2_web.cpp:638` still expects <=48 rather than current79. Production-linked storage challenger: **19/23 pass**. Failures at lines289,368,403,677 assert old 48-byte truncation or old comma loss; these demonstrate stale tests after the partial repair, not proof of new corruption for defaults. Logs: `work/test_challenger_web.log`, `work/test_challenger_storage.log`.

Standalone production-linked probe: long Thai truncation125→79/incomplete UTF8; raw script and BT-attribute rendering=true; negative interval4294967295; multiline history lost; old blob load=false. Source `work/storage_probe.cpp`.

`tests/test_storage_web.cpp` defines its own `StorageSim::PackedItemRecord name[48]` and mock store (lines15-40), not actual StorageManager. It can pass despite production regressions. Old challenger handoff claims 366/366, injection-safe,100-byte record and 48-byte safety; current sources are132-byte record and raw injection. Runner summary hardcodes366 and stops early on failure via set-e, so claimed total must not be reused.

## Meaningful next verification

- Production tests of UTF8 boundary cases79/80 bytes, combining marks, emoji, exactly80 Thai characters per agreed contract, rejecting embedded NUL/control chars, RAM vs reboot equality.
- Legacy100→132 migration with customized intervals/visibility/service baselines; corrupt/partial read and count/blob mismatch recovery.
- Injected NVS write failure and power-loss boundaries, reboot in a new process/device (host map persists only inside process).
- Real HTTP POST through WebServer parsing for &, +, %, equals, Thai, duplicate parameters and malformed escapes; host handleSavePayload is insufficient.
- Date/clock progression and mileage threshold crossing through actual firmware loop, not tests calling evaluate manually.
- CSV quotes/commas/newlines, bounded history and filesystem-full behavior; HTML context escaping.
- Independent reviewers inspect repaired behavior; do not grant five unanimous approvals by copying old records.


# Read-only M3/M4 audit

Audited 2026-10-13. No firmware edits, no hardware access. Primary source: `/Users/arm/Documents/Arduino/civic_hud`; approved requirements: `prompt_draft.md` and `ui_layout_c.html` under Antigravity brain `223a3194-ff5d-4613-9761-24750508b680`.

## Conclusion

M3 remains unfinished in production. Existing code implements an English-only approximation of Layout C, color cards, progress bars, and a modal. It does not contain the promised Thai bitmap renderer, combining compositor, pagination, portrait axis swap, voltage display, current-date handling, or partial sprites. M4 is unverified; no final report or physical display PNG evidence exists in the production project. PROJECT.md:58–59 itself marks M3/M4 PLANNED.

## Concrete findings

1. **P1: Maintenance status does not advance during driving.** `civic_hud.ino:119` updates OBD/odometer, but the loop never calls `g_maintEngine.evaluate`. Only initial setup (`:85`), web save (`WebSettings.h:233`), and reset (`MaintenanceEngine.cpp:125`) reevaluate. Header ODO can increase while remaining km, progress bars, urgency ordering, and LED remain stale. Reevaluate on changed ODO/day before rendering.
2. **P1: Single right-side tap records a service; no slide gesture is required.** `civic_hud.ino:415–428` maps any contact in y=205..255 to slider position and immediately commits when x≈178 or greater. No start-on-thumb, continuous drag, release confirmation, or release reset exists. `handleTouch:388` returns immediately on no contact. Persisted maintenance history can be accidentally altered by one tap. Implement an actual gesture state machine and test right-edge tap rejection, interrupted drags, release-before-threshold, and one-shot commit.
3. **P1: Modal identity is a sorted list index.** `civic_hud.ino:312–314,422–424,454` rereads sorted items using `g_modalItemIdx`. While modal is open, a web save can reorder/hide/add items, causing the displayed or reset item to change. Hold stable item ID, revalidate existence, and cancel cleanly on removal.
4. **P1: Thai text cannot render through the production path.** `civic_hud.ino:278,322` passes Thai+emoji names directly to built-in TFT fonts 2/4. No font data, loadFont, ThaiFont, compositor, or DisplayEngine file exists. Defaults in `MaintenanceEngine.cpp:13–26` contain Thai+emoji. Implement Thai rendering with real glyphs and combining rules and verify on-device text; do not use the model test pass as evidence.
5. **P1: All items after index 4 are unreachable on the screen.** `civic_hud.ino:252–255` renders only first 5; `:452` hit-tests only 5. Footer `:304–308` is a static string. `g_scrollOffset` is unused. Implement dynamic pages for 0..24 visible items, navigation, and correct card hit tests; 14 defaults require three pages, 24 custom-inclusive items require five.
6. **P1: Real time/date system absent.** Setup `civic_hud.ino:85` and reset `:426` use epoch day zero; every service record uses literal `13 ต.ค. 26`. Defaults `MaintenanceEngine.cpp:13–26` store prototype ages (e.g. 62, 760) in lastEpoch fields, but production `evaluate:52` treats them as epochs and clamps elapsed to zero. Time-based intervals never expire. Need explicit clock/date source and migration/semantics for initial service dates, including no-valid-clock state.
7. **P2: Portrait touch mapping disagrees with planned mapping.** Production `civic_hud.ino:393–394` maps p.x→x,p.y→y. PROJECT.md:37 and test model `tests/test_thai_engine.cpp:277–284` require rawY reversed→x,rawX→y. Actual calibration must be proven on the device, not copied blindly from a simulated equation.
8. **P2: Modal/date/time UI incomplete and wrong for time-only items.** `civic_hud.ino:329–357` shows only previous km, delta km, current km, next km. Does not display lastDate or new date and always displays next=current+intervalKm (incorrect for intervalKm=0). List `:282–288` always shows km label, even when `isLimitedByTime` is true, so tax/battery time-only cards show LEFT 0km.
9. **P2: Unsigned underflow in modal traveled distance.** `civic_hud.ino:340` directly subtracts lastKm from current unsigned ODO; setting ODO below previous service km displays a huge value. Core reset already clamps at `MaintenanceEngine.cpp:107`, so modal and record disagree.
10. **P2: Stale cards remain after hiding items.** `drawServiceList:249–302` redraws present cards without clearing the viewport. A web hide reducing list length leaves previous card pixels; `handleTouch:452–458` can open an out-of-range index, clear screen, and have drawModal return blank. Clear old card slots and bounds-check against current visible-page entries.
11. **P2: Long name and status overlap.** Names start at x14 (`:278`) and urgency text ends x226 on the same y (`:288`) without width limits. Modal name font4 starts x18 next to close button x180 (`:322–327`). Need clipping/ellipsis or separate text rows; required 80-character storage names must remain legible within UI width.
12. **P2: Voltage missing.** Header `:214–240` has OBD+ODO only although requirement R1 includes battery voltage. Telemetry already has `batteryVoltage` in `ObdTask.h:30`.
13. **P2: UI blocking and sprites absent.** Service success `civic_hud.ino:432,436` delays main loop 380ms; serial `:111` can wait for input timeout. Full card primitives redraw every ~800ms; no `TFT_eSprite` exists. Host passing UI response tests cannot prove <100ms hardware servicing or flicker elimination.
14. **P2: Driving touch lockout absent.** PROJECT.md:41 specifies speed>5km/h lockout; production `handleTouch` does not inspect telemetry speed.

## Test coverage integrity

`tests/test_thai_engine.cpp` defines its own `ThaiUiSim::ThaiCompositor` and `LayoutCModel` (lines 13–296) and includes only the test framework/standard library. `tests/run_all_tests.sh:77–79` compiles that single file without production sketch/renderer/touch code. Its tests prove the independent model, not the board implementation. Examples of self-confirming assertions:

- `:586–590` sets modalW=224 and asserts it is 224 (production modalW=220).
- `:624–631` assigns literal date then asserts same literal/not TODAY.
- `:697–707` calculates theoretical buffer size and subtracts from a constant heap number.
- `:717–719` assigns atomicPushSupported=true and asserts true (production has no sprite).

Required repair: factor actual production UI state/gesture/calibration/date logic into shared host-compilable modules and test those; use production renderer with a framebuffer adapter for layout checks where feasible; still obtain actual device captures for accepted Thai glyph/rendering proof.

## M4 gaps/evidence limits

- `WebSettings.h:240–274` implements only CMD:WEB_SAVE and CMD:DUMP_STORAGE. The CMD:SNAPSHOT and CMD:PERSIST_TEST claimed by TEST_INFRA.md:49–50 do not exist.
- No PNG captures or REPORT.md in `/Users/arm/Documents/Arduino/civic_hud` and no project PNG evidence in the inspected brain/scratch roots. Historical walkthrough documents older PlatformIO work and host unit tests; it is not proof of final integrated Arduino firmware/hardware completion.
- This audit intentionally did not compile/upload/reset the board or touch its serial port. Clean build, upload hash verification, >=60s serial stability, reboot persistence, and screen captures remain to be established by the lead.

## Recommended sequencing

1. Address stale evaluation, stable modal ID, actual slider gesture, viewport clearing, and time semantics.
2. Implement shared production Thai renderer/pagination/calibration/layout and replace disconnected test coverage with tests of actual code.
3. Complete clean build, device persistence tests, device captures with independent visual comparison, stability run, and Thai report. Avoid labeling M3/M4 complete based on models or old upload claims.
