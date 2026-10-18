# Agent loop state — Smart Vehicle Maintenance Tracker

Latest directive: 18 October 2026. Android project: outputs/smart-vehicle-tracker.

## Loop status
- Loop 0 COMPLETE: handoff read, existing ESP32 preserved; JDK17, SDK36, Gradle8.13 located; S22 ADB online; stay-on and timeout requested.
- Loop 1 COMPLETE: 3-module contracts in docs/CONTRACTS.md; debug DUMP-protected receiver and isolated simulation DB.
- Loop 2 COMPLETE: domain, Room and Compose specialists running; root integrates transport/foreground service.
- Loop 3 COMPLETE: debug APK built; 39 domain +9 storage +7 protocol tests passed. GPS type issue and compatibility warning corrected.
- Loop 4 COMPLETE: A-E, actualRoom snapshots, customrules/notifications, malformedreply, freshfirstsample and nonzero coldrestart passed. GPS-on-tap gracefully timedout without erasing form; actualfix not obtained.
- Loop 5 COMPLETE: independent domain/service and storage critics signed off within physical S22 simulation scope. Actual car/adapter and outdoor GPS fix remain real-world validation tasks, documented in morning guide.

## Requirements / evidence gates
- [x] Modular Kotlin/Compose + pure domain + Room data modules.
- [x] Only PID010D distance; no PID0131 in Android production calculation.
- [x] Validated AT init, Bluetooth Classic SPP, bounded framing/timeouts/reconnect.
- [x] Monotonic speed integration, invalid/disconnect/stall >3000ms cuts gap.
- [x] Room checkpoint every0.5km/60s/transitions, sync-ready metadata.
- [x] Calibration, custom dual km/month intervals, service transaction/history/optional fields.
- [x] GPS-on-tap and graceful permission handling.
- [x] Dual warning notifications and periodic date worker.
- [x] Debug-only shell-protected simulator uses isolated DB; release merged manifest has no injection receiver.
- [x] Unit tests and debug/release compile clean:55tests,0failures,0Kotlinwarnings.
- [x] S22 A normal / B dropout / C background idle / D reconnect / E UI+restart persistence passed.
- [x] No fatal crashes in scoped test logs; review actual device evidence.
- [x] Morning guide accurately distinguishes simulation from actual car validation.

## Defects found and repaired during loops
- Freshness checked before integration; old/future frames rejected.
- Start/stop controls serialized; calibration and service save share session mutex.
- AT initialization rejects explicit error responses; calibration flushes checkpoint.
- Notification permission/channel enabled checked before dedup reservation; simulation notifications clearly labeled.
- Adapter-controlled percent text formatted separately to prevent notification crash; unsolicited serial drain bounded.
- Stale interval editor preserves latest service baseline/cycle.

## Constraints
No cloud/auth implementation. No real ECU test claims from simulation. Do not use power/lock key. Keep PIN private and out of artifacts. Never alter ESP32 data or vehicle history to simulate tests.

## Final evidence and scope
55unit tests,0failures;debug/releasebuild,0Kotlinwarnings;A-Epass on SM-S901E Android16. Crashbuffer has0ownedFATAL. Actual carECU anddeepDoze not falsely claimed; GPSfix unavailable indoors, manualform andtimeout verified. No cloud/auth or ESP32 changes.
