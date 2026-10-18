## Latest handoff — 18 October 2026

**Android is implemented now.** Read `android/README.md`, `android/docs/CONTRACTS.md`, `android/AGENT_LOOP_STATE.md`, `android/MORNING_TEST_GUIDE.md`, and `android/verification/TEST_SUMMARY.json` first. Android uses only PID010Dspeedintegration with3s cutoff; never port the historical ESP32 counter method into it. Debug simulation is shell-protected and uses a separate Room database. Real Bluetooth SPP is implemented but actual-car testing is pending. Azure CLI was inspected; no cloud resources/auth/sync were built. Older proposal/status paragraphs below are historical and superseded.

# AI handoff — read this first

## Objective

Build on CivicHUD: maintenance reminders driven by vehicle distance. The ESP32 version works on one Civic; the next product is an Android app for approximately 20 free users, each with their own OBD2 adapter, user login and owner-controlled cloud storage.

## Reading order

1. `docs/CURRENT_STATE.md` — actual firmware behavior, caveats and evidence.
2. `docs/DECISIONS.md` — latest user choices, superseding earlier plans.
3. `docs/ANDROID_SPEC.md` — product/system design proposal; not implemented.
4. `docs/BUILD_AND_VERIFY.md` — build prerequisites and verification scope.
5. Source map below, then historical `docs/REPORT.md` only as needed.

## Source map

- `ObdTask.cpp/.h`: Classic Bluetooth SPP worker, ELM init, CAN29 clone handling, command framing, telemetry and pause/resume for hotspot.
- `OdometerEngine.cpp/.h`: PID31 delta / fallback integration / counter discontinuities.
- `StorageManager.cpp/.h`: Preferences NVS, paired CRC checkpoint and LittleFS service log.
- `MaintenanceEngine.cpp/.h`, `MaintenanceItem.h`: interval calculations, sorting, due state and service reset.
- `DisplayEngine.h`, `BarAppearance.h`, `HudPalette.h`: cached partial drawing, 24 shades, 500 km blinking.
- `ThaiFont.h`, `ThaiFontData.h`: Thai glyph bitmap/composition. Preserve provenance and review glyph rights before distributing publicly.
- `UiController.h`, `civic_hud.ino`: touch gestures, modal exit/slider, day/night switch, monitor and hotspot orchestration.
- `ResourceMonitor.h`: low-overhead CPU/core and heap min/max sampling.
- `WebSettings.h`: local settings and CSV history endpoints.
- `config.h`, `pins.h`, `hardware/Setup_CYD_ESP32_2432S028.h`: defaults and hardware wiring.

## What to do next

The owner's latest instruction is repository/Notion handoff. Do not silently resume board testing or provision Azure. For the Android follow-up, design screen flows and confirm adapter capabilities first; then implement one vehicle's offline OBD distance engine and test screen-off/restart before adding multi-user sync. Use the existing firmware as reference behavior, not as Android source code.

No AI can inherit the previous agent's live runtime, credentials, or memory simply by cloning. These files preserve the relevant decisions, implementation, evidence and caveats; the next AI must inspect its environment and validate new changes.

## Known wrong assumptions to avoid

- Original chat title mentioned ESP32-C6; the actual board has dual-core ESP32 with Classic BT.
- “OBD connected” on screen does not alone prove valid ECU telemetry. Separate adapter BT state from recent valid ECU replies.
- PID31 absolute value is not lifetime odometer. The tested display uses dashboard baseline + distance delta.
- Historical maintenance countdowns were overdue by large values because old service baselines were not confirmed. Do not invent last-service mileage to make the UI look correct.
- Historical PROJECT.md and test names sometimes overstate coverage. See limitations before quoting test counts.
- The stored APP_VERSION may still read 1.0.0; use file manifest and commit SHA for this snapshot's identity.
