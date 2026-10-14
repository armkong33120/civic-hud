# E2E Test Infra: Honda Civic FC 1.8 Maintenance HUD

## Test Philosophy
- **Requirement-Driven & Opaque-Box**: Derived strictly from `ORIGINAL_REQUEST.md` and approved design `ui_layout_c.html`.
- **Methodology**: 4-Tier Test Framework (Category-Partition, Boundary Value Analysis, Pairwise Combinations, and Real-World Workload Testing) + Tier 5 Adversarial Coverage Hardening.
- **Host & Hardware Verification**: Host tests compile with `/Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17`. On-device tests verify hardware SPI, NVS reboot persistence, and physical display captures over `/dev/cu.usbserial-11220`.

## Feature Inventory Test Coverage Matrix
| # | Feature | Tier 1 (Feature) | Tier 2 (Boundary) | Tier 3 (Combinations) | Tier 4 (Real-World) |
|---|---------|:----------------:|:-----------------:|:---------------------:|:-------------------:|
| 1 | Boot Panic Elimination | 5 | 5 | ✓ | ✓ |
| 2 | Non-Blocking BT State Machine | 5 | 5 | ✓ | ✓ |
| 3 | Auto-Reconnect on Drop | 5 | 5 | ✓ | ✓ |
| 4 | SSP Pairing & PIN 1234 | 5 | 5 | ✓ | ✓ |
| 5 | ELM327 Clone Quirk Handling | 5 | 5 | ✓ | ✓ |
| 6 | PID 01 31 Distance Tracking | 5 | 5 | ✓ | ✓ |
| 7 | Speed Integration Fallback | 5 | 5 | ✓ | ✓ |
| 8 | ECU Reset / Wrap Handling | 5 | 5 | ✓ | ✓ |
| 9 | Automated ELM327 Simulator | 5 | 5 | ✓ | ✓ |
| 10 | NVS Scalar Persistence | 5 | 5 | ✓ | ✓ |
| 11 | NVS Maintenance Items Blob | 5 | 5 | ✓ | ✓ |
| 12 | LittleFS Service History Log | 5 | 5 | ✓ | ✓ |
| 13 | Web Settings UI & Form | 5 | 5 | ✓ | ✓ |
| 14 | Web Settings "Save All" & Sync | 5 | 5 | ✓ | ✓ |
| 15 | Web Settings History View | 5 | 5 | ✓ | ✓ |
| 16 | Zero-Risk Web Verification Bridge | 5 | 5 | ✓ | ✓ |
| 17 | Thai Bitmap Font Engine | 5 | 5 | ✓ | ✓ |
| 18 | Thai Combining Glyph Compositor | 5 | 5 | ✓ | ✓ |
| 19 | Touchscreen Axis Swap | 5 | 5 | ✓ | ✓ |
| 20 | Layout C Status Bar | 5 | 5 | ✓ | ✓ |
| 21 | Layout C Service Cards & Colors | 5 | 5 | ✓ | ✓ |
| 22 | Pagination & Reachability | 5 | 5 | ✓ | ✓ |
| 23 | Driving Touch Lockout | 5 | 5 | ✓ | ✓ |
| 24 | Previous Service Modal | 5 | 5 | ✓ | ✓ |
| 25 | Formatted Thai Date Strings | 5 | 5 | ✓ | ✓ |
| 26 | Slide-to-Confirm Reset Slider | 5 | 5 | ✓ | ✓ |
| 27 | Partial Sprite Buffering | 5 | 5 | ✓ | ✓ |

## Test Architecture
- **Host Test Runner**:
  - Location: `tests/host_runner.cpp`, `tests/test_elm_simulator.cpp`, `tests/test_storage_web.cpp`, `tests/test_thai_engine.cpp`
  - Invocations:
    ```bash
    /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17 tests/test_elm_simulator.cpp src/core/OdometerEngine.cpp -Isrc/core -Iinclude -o /tmp/test_elm && /tmp/test_elm
    ```
- **On-Device Verification Runner**:
  - Python runner at `/Users/arm/.platformio/penv/bin/python` interacting over `/dev/cu.usbserial-11220`.
  - Serial command harness:
    - `CMD:PERSIST_TEST`: Exercises reset, interval change, hide, add; verifies via NVS dump across physical reboot.
    - `CMD:SNAPSHOT`: Dumps physical ILI9341 GRAM buffer; converted by script to PNG for visual inspection.

## Real-World Application Scenarios (Tier 4)
| # | Scenario | Features Exercised | Pass Criteria |
|---|----------|--------------------|---------------|
| 1 | Typical Highway Commute | F1, F2, F6, F8, F20, F21 | Odometer advances smoothly by 25 km; countdowns decrement; status bar updates; 0 panics. |
| 2 | Tunnel / Signal Loss & Reconnect | F2, F3, F5, F7, F20 | ELM327 drops during drive; fallback engages; dongle reappears; reconnects in $\le 10$ s without UI hitch. |
| 3 | Oil Change Service Reset | F24, F25, F26, F11, F12, F21 | Tap oil card -> modal displays previous 10,000 km history -> slide confirmed -> logs history -> card re-sorts to bottom. |
| 4 | Web Settings Reconfiguration & Reboot | F10, F11, F13, F14, F15, F31 | Edit oil interval to 15,000 km, hide spark plugs, add custom item, Save All -> verify immediate screen refresh -> reboot -> all state intact. |
| 5 | ECU Diagnostic Tool Reset Event | F6, F7, F8, F20 | Mechanic clears DTC codes resetting PID 01 31 to 0; HUD preserves accumulated odometer without loss or glitch. |

## Coverage Thresholds
- Tier 1 (Feature Coverage): $\ge 5 \times 27 = 135$ test cases
- Tier 2 (Boundary & Corner Cases): $\ge 5 \times 27 = 135$ test cases
- Tier 3 (Cross-Feature Combinations): $\ge 27$ pairwise combination tests
- Tier 4 (Real-World Application Scenarios): $\ge 5$ end-to-end integration workflows
- Phase 2 (Adversarial Coverage Hardening): Tier 5 white-box mutation and gap analysis
