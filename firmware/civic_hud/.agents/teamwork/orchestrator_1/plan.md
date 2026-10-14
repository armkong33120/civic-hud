# Concrete Execution Plan: Honda Civic FC 1.8 Maintenance HUD Firmware

## Goal
Audit, fix, test, and flash the Honda Civic FC 1.8 (LPG) maintenance-countdown HUD firmware on CYD ESP32-2432S028 matching approved Layout C, full persistence, robust OBD-II/BT, and verified on physical hardware.

## Phase 0: Survey & Scope Exploration (Parallel)
- **Explorer 1 (Spec Miner)**: Deep dive into `/Users/arm/.gemini/antigravity/brain/223a3194-ff5d-4613-9761-24750508b680/ui_layout_c.html` and `civic_fc_maintenance_display_plan.md`. Extract full UI/UX design specs, Thai text strings, color palette, modal layout, slide-to-confirm mechanics, and Web Settings requirements.
- **Explorer 2 (Codebase & Toolchain Explorer)**: Investigate `/Users/arm/Documents/Arduino/civic_hud`, existing files, current compilation status with `arduino-cli`, libraries installed, memory/partition status, display driver configuration, and serial flash capability on `/dev/cu.usbserial-11220`.
- **Explorer 3 (OBD-II & Architecture Explorer)**: Deep dive into OBD-II Bluetooth Classic SPP state machine, ELMDuino quirks, PID 01 31 handling, speed integration fallback, and review `/Users/arm/.gemini/antigravity/scratch/civic-hud/` (including `test/test_core_engines.cpp`).

## Phase 1: Architecture Synthesis & Dual-Track Initialization
- Synthesize Explorer reports into `PROJECT.md` (Feature Inventory, Milestones, Interface Contracts, Code Layout).
- Spawn E2E Testing Track to create `TEST_INFRA.md` and test harnesses (ELM327 stream simulator, persistence harness, web endpoint validator).

## Phase 2: Implementation Track Milestones
- **Milestone 1 (OBD & BT Subsystem)**:
  - Non-blocking BT Classic SPP state machine.
  - Auto-reconnect <= 10s on drop.
  - PID 01 31 + speed fallback odometer engine.
  - Verification with automated ELM327 simulator.
- **Milestone 2 (Storage & Web Settings Subsystem)**:
  - Genuine NVS / LittleFS persistence for all items, intervals, visibility, history, odometer, BT credentials.
  - Web Settings server at 192.168.1.1 (all items editable, add/hide, history log).
  - Persistence survives reboot.
- **Milestone 3 (Display & UI Subsystem)**:
  - Legible Thai font rendering on CYD display.
  - Approved Layout C card list, urgency sorting, color semantics.
  - Previous service history modal & slide-right-to-confirm reset.
- **Milestone 4 (Final Integration, Hardware Verification & Flashing)**:
  - Pass 100% of E2E tests (Tiers 1-4).
  - Adversarial hardening (Tier 5).
  - Compile with `arduino-cli` and upload to `/dev/cu.usbserial-11220`.
  - Serial verification: >= 60s runtime without panic/crash.
  - On-device screen capture PNGs verified against rubric.
  - Persistence round-trip verification on physical board.
  - Deliverable: `REPORT.md` in Thai.
