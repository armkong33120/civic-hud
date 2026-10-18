# Smart Vehicle Maintenance Tracker / CivicHUD Android

Native Android MVP for Honda Civic FC 1.8 (2017) maintenance tracking, built against API36 and tested on Samsung SM-S901E Android16. All core features work offline. Cloud auth/sync is deliberately not implemented in this phase.

## Build

JDK17, AndroidSDK36, Gradle8.13. Set ANDROID_HOME or put your SDK path in local.properties (`sdk.dir=...`).

```sh
./gradlew :domain:test testDebugUnitTest assembleDebug assembleRelease
```

Debug APK: app/build/outputs/apk/debug/app-debug.apk. Release output is unsigned; production signing keys are not included. Wrapper and dependency versions are pinned in project files. Do not commit local.properties or build caches.

## Architecture

- `domain`: pure Kotlin models, monotonic trapezoidal speed integration, PID010D parser, dual distance/date maintenance rules, calibration/service helpers and deterministic tests.
- `data`: Room entities/DAOs/store; atomic service-log+cycle update; seven editable Thai starting schedules; syncId,updatedAt,isDirty metadata. Actual vehicle and debug simulation have separate database files.
- `app/service/obd`: BluetoothSocket SPP transport, prompt-framed ELM327 protocol, bounded initialization/reads, reconnect backoff and connectedDevice foreground service.
- `app/service/TrackingSession`: serialized live odometer/calibration/service operations; checkpoint at0.5km/60seconds or state changes. UI state can update without SQLite writes per frame.
- `app/presentation`: state aggregation and user actions. `app/ui`: Compose state and callbacks only; no Bluetooth/DAO/parser calls.
- `app/platform`: paired-device listing and optional GPS-on-tap.
- `app/service`: notification channels, cycle/status dedup and periodic12-hour date check.

The Android path uses speed integration only. It does not reuse the old ESP32 ECU-distance-counter implementation. The lifetime odometer shown is an estimate: user dashboard calibration + observed travel. A gap is marked and not extrapolated.

## Safety/correctness properties

A valid speed frame seeds or advances an adjacent interval no longer than3000ms. Invalid responses, disconnect, stale/future timestamps and clock reversal cut the endpoint. Timer ticks never advance distance. Restart restores the last durable checkpoint and requires a new sample endpoint. Unexpected force-kill may lose at most the uncheckpointed time/distance window; explicit Stop/calibration/service actions flush first.

Default schedules begin at setup/calibration, not evidence that the vehicle was serviced. Confirm intervals against the vehicle's maintenance schedule and adjust baseline usage before relying on due dates. No fabricated service records are seeded.

The real transport uses standard ELM327 auto protocol selection; actual car/adapter compatibility and distance accuracy must be checked in the car. Foreground service and wakelock improve continuity but do not guarantee immunity to every Android/OEM process restriction. WorkManager date alerts are inexact, not an exact midnight alarm.

## Debug HIL

Only debug manifest contains `com.civichud.maintenance.DEBUG_OBD_INJECT`, protected by `android.permission.DUMP` for ADB shell. Release has no exported injector. Debug simulation traffic and service entries use `vehicle_tracker_simulation.db`; real data uses `vehicle_tracker.db`. Notifications from simulation are labeled as tests.

With the app visible and runtime permissions granted:

```sh
adb shell am broadcast --receiver-foreground -a com.civichud.maintenance.DEBUG_OBD_INJECT -n com.civichud.maintenance/.debug.DebugObdReceiver --es command MODE
adb shell am broadcast --receiver-foreground -a com.civichud.maintenance.DEBUG_OBD_INJECT -n com.civichud.maintenance/.debug.DebugObdReceiver --es command CALIBRATE --es odometerKm 100000
adb shell am broadcast --receiver-foreground -a com.civichud.maintenance.DEBUG_OBD_INJECT -n com.civichud.maintenance/.debug.DebugObdReceiver --es command START
adb shell am broadcast --receiver-foreground -a com.civichud.maintenance.DEBUG_OBD_INJECT -n com.civichud.maintenance/.debug.DebugObdReceiver --es command FRAME --es raw '41 0D 78'
```

Inject fresh frames every0.5–1s for simulation. Supported controls include MODE,START,STOP,CALIBRATE,STATE,FLUSH,CHECK_ALERTS,REAL_MODE; FRAME raw may be a speed response,NO DATA,?,DISCONNECT,RECONNECT. Use STOP then REAL_MODE to return to real-data setup. Do not use these controls to fabricate actual vehicle data.

`verification/` contains actual device artifacts and audit reports. Read `MORNING_TEST_GUIDE.md` for car instructions and tested limitations. Project state tracker is maintained at the parent workspace root as requested and copied into the final deliverable.
