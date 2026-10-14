# Build, verification and provenance

## Firmware build environment used

- Arduino CLI; esp32:esp32 core3.3.10
- TFT_eSPI2.5.43, XPT2046_Touchscreen1.4; local installation also includes ELMDuino3.4.1 and Arduino_GFX1.6.6 (check includes before assuming every installed lib is required)
- FQBN `esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800`
- TFT setup in `hardware/Setup_CYD_ESP32_2432S028.h`: ILI9341_2, SPI write40MHz/read1MHz, pins from actual board.

Install matching dependencies, copy the TFT setup into TFT_eSPI/User_Setups, and select it in User_Setup_Select.h (disable other setup includes). Back up any existing setup first. Then from repository root:

```sh
arduino-cli compile --fqbn 'esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800' --output-dir build/civic_hud firmware/civic_hud
```

Do not erase flash/NVS to troubleshoot without backing up actual vehicle state and permission. Inspect `arduino-cli board list`; old ports changed from /dev/cu.usbserial-11220 to /dev/cu.usbserial-10, neither is guaranteed now. Stop serial monitors before upload. Upload command is intentionally not auto-run by this handoff.

## Existing tests

`firmware/civic_hud/tests/run_all_tests.sh` is a historical macOS host runner with compiler/sysroot paths. Some suites use simulated models/stubs. A passing historical count is not proof of live Bluetooth, current UI, web via real phone, or exact hardware stability. `tests/regressions` retains additional focused reproductions; some scripts still have workstation assumptions. UI stub sources are in `tests/ui_stubs`.

The portable focused color test can be built from root with:

```sh
c++ -std=c++17 tests/regressions/test_bar_appearance.cpp -o /tmp/civichud-bars
/tmp/civichud-bars
```

On this Mac, if the system git or compiler invokes an unaccepted full-Xcode license, use the already installed CommandLineTools developer directory (`DEVELOPER_DIR=/Library/Developer/CommandLineTools`) rather than changing system license/configuration.

## What evidence does show

- Historical ECU logs show real replies and changing distance during the owner's drive.
- User reported roughly100m discrepancy for the comparison trip; no universal accuracy claim.
- Historical compile/upload logs correspond to the bar upgrade, and postboot log is a short engine-off observation.
- UI transport tests checked partial redraws and bar-only blink, using stubs rather than optical measurement.
- Hotspot radio open/close was observed, real HTTP client flow is still unverified.

New export verification is recorded separately in `EXPORT_VERIFICATION.md`; do not confuse export integrity with hardware retesting. SHA256 manifest records archived files, not mutable vehicle NVS. `CivicHUD_firmware.bin` is an application image, not a complete bootloader+partition-table flash backup.
