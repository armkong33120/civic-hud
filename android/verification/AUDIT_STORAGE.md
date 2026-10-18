# Storage audit

## Result and evidence boundaries

**Storage sign-off: PASS for the reviewed implementation and observed S22 scenarios.** The current source meets the Room storage contract; nine data-unit tests pass; the physical-device app readbacks and independently inspected device database snapshots agree. The checks and their limits are separated below.

The root agent owns the Samsung S22 test run; this audit did not operate the phone or interfere with that run. Device-produced snapshots were inspected only through Python SQLite URI connections with `mode=ro`. Local unit tests do **not** open Room on Android. Earlier checks that executed Room-generated schema SQL with host SQLite also do **not** constitute Android Room runtime evidence; the later S22 artifacts provide that distinct evidence.

## Source review

| Concern | Finding |
| --- | --- |
| Database separation | `TrackerDatabase.get` selects `vehicle_tracker.db` or `vehicle_tracker_simulation.db`; cached instances are keyed by the full database path. No destructive-migration fallback is enabled. Schema version is 1 and export is disabled. |
| Initialization | One transaction creates the uncalibrated vehicle at base 0 / accumulated 0 and seven editable Thai schedules. Setup dates use the local setup date. Initialization inserts no service logs and does not restore deleted schedules on later startup. |
| Initial calibration | One transaction aligns pending setup baselines to the actual odometer, clears their pending flags, and sets vehicle base to actual / accumulated to 0. It does not insert a service event. |
| Later calibration | Changes vehicle calibration only; existing service baselines, cycles, and log rows are retained. The session flushes and calibrates under one mutex, then clears its speed endpoint and live accumulated distance. |
| Checkpoints | Require calibrated, finite, nonnegative, monotonic accumulated distance and a finite resulting odometer. Equal checkpoints do not write another vehicle row. The session owns 0.5 km / 60 second debounce and flushes at state changes and Stop. |
| Service recording | The session mutex covers flush, odometer snapshot and storage call. Room atomically inserts history and resets the matching rule's odometer/date while incrementing its cycle. Failure rolls back both database mutations. Optional details are supported; invalid cost/coordinates are rejected. |
| Concurrent schedule editing | Existing-rule saves change settings only. They preserve the transaction's latest service baseline, cycle and pending-calibration flag, preventing a stale editor from reverting a service or calibration. |
| Deletion/history | Rule deletion does not cascade into logs; each log retains the original item name. IDs auto-increment, so ordinary deletion does not reuse a removed rule's identity. |
| Notifications | A unique `(ruleId, cycle, status)` index plus insert-ignore in a transaction allows one reservation per status per cycle. Missing rules, uncalibrated vehicles and stale cycles are rejected. App/runtime/channel notification eligibility is checked before reserving. Reservations intentionally provide at-most-once posting attempts: a process death after reservation and before posting can suppress that attempt. |
| Sync metadata | Vehicle, rule, service log and notification reservation entities each persist UUID `syncId`, epoch-millisecond `updatedAt`, and `isDirty`. Updates retain UUID and mark dirty. This is metadata preparation, not an implemented cloud sync system. |
| Lifecycle transitions | Service command transitions now use a control mutex. Session preparation loads the next database before committing its current-store/integrator state. UI gates live odometer substitution by the store identity and initialized state. |

Reviewed implementation: `data/src/main/java/com/civichud/maintenance/data/{TrackerStore,TrackerDatabase,TrackerDao,TrackerEntities}.kt`, `app/src/main/java/com/civichud/maintenance/service/{TrackingSession,MaintenanceNotifier}.kt`, and integration call sites in the foreground service and view model.

## Local test evidence

`data/build/test-results/testDebugUnitTest/TEST-com.civichud.maintenance.data.TrackerValidationTest.xml` records nine tests, no failures/errors. Coverage includes valid distance/calendar schedules; invalid, negative and nonfinite inputs; optional service details; coordinate bounds; date/UUID conversion; stale-edit preservation of a newer service baseline; and preservation of setup calibration state during schedule edits.

The Kotlin/Room annotation processor and generated DAO compiled successfully. These checks establish source and generated-code validity; they do not replace actual Room execution, transaction rollback fault injection, or concurrent Android instrumentation.

## Device evidence received so far

The device debug-state records are emitted by the running app and read `TrackerStore.currentVehicle()` plus flows. They provide actual app/Room read evidence, with live telemetry and persisted odometer reported separately:

- `b-before-gap.json`: live accumulated distance `0.8385333333333334` km, persisted odometer `100000.80850555556` km.
- `b-after-six-second-gap.json`: status `DATA_STALE`, unchanged accumulated distance `0.8385333333333334` km, persisted odometer `100000.83853333333` km. This demonstrates no distance added through the observed stale gap and a transition checkpoint visible through the app's Room read path.
- The received A/B snapshots report `historyCount: 0`; simulation driving has not manufactured service history in those observations.

`scenarios-a-d.json` has now been reviewed. All four recorded scenario predicates pass. A shows live distance increasing while the persisted value may lag within the checkpoint policy. B records `gapAddedKm: 0.0` through the stale gap. C keeps the same PID (`24820`) through HOME + `am make-uid-idle` while live distance continues; this is background-service evidence, **not** process-restart durability, screen-lock or forced-Doze proof. D checkpoints `100001.15346666667` km at link loss, then reconnects and resumes live accumulation. No scenario claims a real Bluetooth/ELM link: these are injected simulation frames on a physical Android phone.

`tracking-stopped.json` reports `STOPPED`, live and persisted odometer both `100001.30543333333` km, accumulated distance `1.3054333333333334` km and history count 0. This is direct on-device app/Room readback that Stop flushed the observed distance.

Scenario E deliberately recalibrated simulation to `123456.7` km and recorded a service through the UI. `scenario-e.json`, `e-before-force-stop.json` and `e-after-restart.json` show that base/estimated/persisted odometer `123456.7`, accumulated distance 0, history count 1 and the reset first-item remaining distance survive the root agent's force-stop and relaunch. The history UI was also captured as `e-history-after-restart.xml` and `history-after-restart.png`. This establishes restart persistence for the calibrated vehicle and recorded service.

The later `final-checks.json` adds the previously missing nonzero accumulated-distance cold-restart case. `positiveBefore` and `positiveAfter` exactly match on accumulated distance `0.10060000000000001` km, estimated and persisted odometer `123459.8006` km, and history count 1. Session distance correctly resets from `0.10060000000000001` to 0 after relaunch. The independent final database snapshot matches those exact accumulated and total odometer values. `readyReal` additionally reports real mode uncalibrated at 0 with history count 0.

## Direct device-database verification

The root agent supplied `vehicle_tracker.db`, `vehicle_tracker_simulation.db` and their matching WAL/SHM files from the stopped S22 app. Both supplied WAL files are empty; committed content is present in the main files. Read-only inspection passed `PRAGMA integrity_check` for both, with schema version 1. Detailed assertions, row counts, selected records and SHA-256 snapshot hashes are saved in `storage-device-verification.json`.

| Check | Actual device-produced state |
| --- | --- |
| Real database isolation | Vehicle base and accumulated distance both 0, uncalibrated, calibration timestamp 0; seven setup rules at baseline 0 / cycle 0 / pending initial calibration; no service history and no notification reservations. The simulation QA service did not enter real history. |
| Simulation calibration | Vehicle base `123456.7`, accumulated 0, calibrated flag true, positive calibration timestamp. |
| Service transaction result | Exactly one log: item 1 / น้ำมันเครื่อง at `123456.7` km. Rule 1 has matching baseline `123456.7`, cycle 1, pending flag false and the same update timestamp as the service's performed timestamp. |
| Optional service details | Location `QA-Lab`; mechanic `QA-Mechanic`; phone `0800000000`; cost `1234.5`; notes `Isolated-HIL-test`; both GPS coordinates null. These are deliberate isolated QA inputs, not production vehicle history. |
| Other schedules preserved | Rules 2–7 retain the original first-calibration baseline `100000.0` km and cycle 0 after later recalibration and item-1 service, demonstrating that later calibration did not rewrite their baselines. |
| Metadata | Every persisted row in all four entity tables has a syntactically valid UUID, positive integer `updatedAt`, and `isDirty=1`; the schema marks all three columns non-null. UUIDs are unique within each inspected database. |
| Notification reservations | Three simulation reservations exist: `(1,0,OVERDUE)`, `(3,0,OVERDUE)`, `(7,0,OVERDUE)`; no duplicate tuple. The actual stored schema contains the unique rule/cycle/status index. These rows precede the item-1 cycle reset and remain as prior-cycle reservations. |

## Final-state follow-up

The later `final-state/vehicle_tracker_simulation.db` and its supplied sidecars were also inspected with `mode=ro`; `PRAGMA integrity_check` returned `ok`. The WAL is empty. `storage-final-verification.json` records the assertions and snapshot hashes.

- The vehicle is calibrated at base `123459.7` km with accumulated `0.10060000000000001` km, totaling `123459.8006` km. This independently matches the nonzero-distance pre/post-restart app records.
- Eight schedules are persisted. Custom rule 8, `QA-Interval`, retains its edited `4.0` km / `2` month interval, starting baseline `123456.7`, cycle 0 and sync identity. Its QA warning values are persisted as `5003.0` km / 7 days.
- Exactly one service remains, including the original `123456.7` km odometer and all isolated QA details. Editing the custom interval and recalibrating/driving did not fabricate or erase service history.
- Five reservations exist without duplicate rule/cycle/status tuples. Rule 8 has both `APPROACHING` and `OVERDUE` in cycle 0, each exactly once. All rows retain valid UUID, positive timestamp and dirty metadata.
- `notifications-approaching.txt` and `notifications-overdue.txt` contain active Android `NotificationRecord` entries for package `com.civichud.maintenance`, maintenance channel, importance 3, and the specific titles `ทดสอบ · ใกล้กำหนด: QA-Interval` and `ทดสอบ · ถึง/เกินกำหนด: QA-Interval`. `custom-and-alerts.json` asserts both specific titles and preserved history. This provides physical-device Android posting evidence for both alert states, beyond database reservations alone; it does not claim a repeated-notification concurrent stress exercise.

This independent inspection proves committed state in actual Android Room output and agrees with post-restart app readback, including nonzero accumulated mileage. No unresolved storage defect was found. Remaining limits: no on-device simultaneous-writer stress test or transaction rollback fault injection was performed. Real Bluetooth/ELM behavior and malformed/reconnect scenario acceptance belong to the domain/service audit.

Signed off by the storage audit agent after read-only inspection of the supplied S22 snapshots and scenario E evidence.
