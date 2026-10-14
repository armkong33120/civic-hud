# Original User Request

## 2026-10-13T14:56:05Z

Audit, fix and finalize the Honda Civic FC 1.8 (LPG) maintenance-countdown HUD firmware running on a CYD ESP32-2432S028 (2.8" 240x320 ILI9341 + XPT2046 touch) so that the on-device UX/UI matches the approved design, every designed feature genuinely works end-to-end — including the Bluetooth Classic (SPP) link to an ELM327 OBD-II dongle with PIN `1234` — and the verified firmware is flashed onto the board currently connected to this Mac. The owner will do the final in-car test themselves, so everything that can be proven without a car must be proven before handing off.

Working directory: `~/Documents/Arduino/civic_hud`
Integrity mode: development

## Reference material
- **Approved design (source of truth for UX/UI):** `/Users/arm/.gemini/antigravity/brain/223a3194-ff5d-4613-9761-24750508b680/ui_layout_c.html` — Layout C service list + tap → modal with previous-service history → slide-right-to-confirm, plus the Web Settings page at `http://192.168.1.1`.
- **Architecture/plan:** `/Users/arm/.gemini/antigravity/brain/223a3194-ff5d-4613-9761-24750508b680/civic_fc_maintenance_display_plan.md`
- **Older PlatformIO copy (reference only):** `/Users/arm/.gemini/antigravity/scratch/civic-hud/` (contains a host-side unit test `test/test_core_engines.cpp`).

## Known gaps found in a pre-audit (must be resolved, not an exhaustive list)
1. Item names are Thai + emoji, but the display uses built-in Latin-only fonts → Thai text renders as garbage on the real screen.
2. The OBD task only polls speed / coolant / voltage; it never reads PID `01 31` and never feeds the odometer engine, so on-screen mileage never changes while driving.
3. Bluetooth connects once in `setup()` and never retries — no auto-reconnect after ignition-off/on or signal loss; the connect call also blocks boot.
4. `StorageManager::loadItems/saveItems` are stubs — slide-to-confirm resets, interval changes and hidden items are lost on reboot.
5. The on-board web page at 192.168.1.1 only has ODO + BT fields; it lacks the designed interval editing (e.g. 10,000 → 15,000 km), hide/show items, add item, service-history log, and the BT name/PIN fields don't reflect saved values.
6. The modal records the service date as the literal string `"TODAY"`.

## Requirements

### R1. On-device UX/UI matches the approved design
The physical screen must faithfully implement Layout C from `ui_layout_c.html`: status bar (OBD connection state, voltage, odometer), urgency-sorted service cards (Overdue › Due Soon › OK) with the "usage" progress bar semantics (short green = fresh, near-full yellow = due soon, full red = overdue), ability to reach all visible items (not only the top 5), tap-a-card modal showing previous service (date + km + km travelled since) and the new record, and a slide-right-to-confirm control that resets the item. Thai item names must render correctly on the real screen.

### R2. All designed features actually work and persist
Every feature in the design works end-to-end on the board: slide-to-confirm reset + history record, Web Settings at `http://192.168.1.1` (Wi-Fi AP "CivicHUD") with ODO sync, per-item interval editing, hide/show, add item, BT device name/PIN, a single "Save All" button whose changes appear on the screen immediately, and a viewable service-history log. All user data (odometer, item state/intervals/visibility/custom items, history, BT settings) must survive a reboot / power loss.

### R3. OBD-II over Bluetooth works for real
With an ELM327-compatible dongle advertising as the configured name (default `OBDII`, PIN `1234`), the board must pair/connect automatically without user interaction, initialize the ELM327, read distance via PID `01 31` (with speed-integration fallback when 0x31 is unsupported), advance the odometer and maintenance countdowns, survive ECU counter resets, show connection state on screen, never block the UI while connecting, and automatically reconnect after the dongle disappears and reappears (ignition off/on). Behaviour must be robust to typical ELM327 clone quirks (echo, `SEARCHING...`, `NO DATA`, `?`, `>` prompt, slow responses).

### R4. Final firmware flashed to the board
After all verification passes, the final firmware is compiled and flashed to the CYD on `/dev/cu.usbserial-11220`, and the board boots cleanly into the finished UI.

### Out of scope (owner deferred)
Real date/time synchronisation. The team may store whatever placeholder date representation is cleanest, but must not ship the literal `"TODAY"` string as a date; leave a clear hook for adding real time later.

## Environment facts & constraints
- Known-good toolchain: `/opt/homebrew/bin/arduino-cli`, core `esp32:esp32@3.3.10`, FQBN `esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800` (921600 baud fails on this board). Libraries installed: TFT_eSPI 2.5.43 (configured via `User_Setups/Setup_CYD_ESP32_2432S028.h`), XPT2046_Touchscreen 1.4, ELMDuino 3.4.1. Additional libraries may be installed with `arduino-cli lib install`.
- The bundled x86 `ctags` was replaced with a native arm64 build; keep it working.
- **Do not** run `sudo`, `brew install`, or anything that triggers the Xcode license prompt (it hangs). For host-side C/C++ use `/Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`. Python with pyserial/esptool is at `/Users/arm/.platformio/penv/bin/python`.
- PlatformIO package downloads stall on this network — don't depend on them.
- **Do not switch this Mac's Wi-Fi onto the board's "CivicHUD" AP** (it would cut the team's internet). Verify the web UI by other means.
- No car and no ELM327 dongle are available during this work — OBD behaviour must be proven with a simulator.

## Acceptance Criteria

### Build & flash
- [ ] `arduino-cli compile` with the FQBN above succeeds with zero errors from a clean build cache.
- [ ] Final `arduino-cli upload` to `/dev/cu.usbserial-11220` succeeds with "Hash of data verified", and a serial capture after reset shows the boot reaching the ready state with no panic/Guru Meditation/WDT reset for ≥ 60 s of runtime.

### OBD-II / Bluetooth (simulated)
- [ ] An automated ELM327 simulator test (runnable with one command, on the host and/or on-device) feeds realistic ELM327 response streams — including echo, `SEARCHING...`, `NO DATA`, `?`, split/slow responses and the `>` prompt — and asserts the parsed PID `01 31` / `01 0D` values.
- [ ] Test proves: odometer advances by the exact delta of PID 0x31 across a sequence; an ECU counter reset (value drops) does not lose or double-count km; speed-integration fallback engages when 0x31 returns `NO DATA`.
- [ ] Test proves the connection state machine: dongle absent → retries without blocking the UI loop (UI/touch loop keeps servicing at least every 100 ms during connect attempts); dongle appears → connected; dongle drops → detected within ≤ 10 s and reconnect resumes automatically.
- [ ] The configured BT name/PIN saved from the web page are the ones actually used on the next connect attempt (demonstrated by test or serial log).

### Persistence
- [ ] On the real board: perform a slide-to-confirm reset, an interval change, a hide, and a custom-item add (via a test/serial hook or web handler invocation), hardware-reset the board, and show via serial dump that all four changes plus odometer and history survived.

### UX/UI evidence on the real screen
- [ ] Provide image captures of what is actually rendered on the physical display (main list, scrolled list, modal, slider mid-drag/confirmed state) saved as PNG files in the working directory. Captures must come from the device, not a re-rendering on the host.
- [ ] Thai item names are legible and correctly shaped in those captures (no tofu boxes/garbage).
- [ ] An independent reviewer agent compares the captures against `ui_layout_c.html` using this rubric and every item passes: (1) status bar shows OBD state, voltage, ODO; (2) cards sorted Overdue › Due Soon › OK; (3) colour + bar semantics match (short green / near-full yellow / full red); (4) all visible items reachable; (5) modal shows previous service date, previous km, km travelled since; (6) slide-right confirm resets the item and it re-sorts to the bottom; (7) text not clipped or overlapping.

### Web Settings
- [ ] Automated checks against the board's HTTP handlers (via whatever method avoids moving the Mac onto the AP) confirm: page lists every item with editable interval and visibility; changing oil interval 10,000 → 15,000 km and pressing Save All updates the item's next-due/ordering on the device; hidden items disappear from the screen; added item appears; BT name/PIN round-trip; history log is shown.

### Deliverable report
- [ ] A short Thai-language `REPORT.md` in the working directory listing what was fixed, how each criterion was verified (commands + evidence files), and a step-by-step checklist for the owner's in-car test (pairing, first connect, odometer sync, what to look for on screen).


## 2026-10-13T16:38:09Z

User explicitly approved: "ลุยต่อเลย" (Continue and finish the remaining milestones). Please resume execution immediately: finalize Milestone 2 (harmonize BUG-M2-01 test assertions and pass M2 gate), execute Milestone 3 (Thai font engine 16x16, 4-tier compositor, pagination, touch coordinates, Layout C cards & modal), execute Milestone 4 (pass 100% test suite, compile and flash to CYD on /dev/cu.usbserial-11220, perform on-device validation and screen captures, and produce REPORT.md in Thai). Keep going until complete!
