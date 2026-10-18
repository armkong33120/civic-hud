# Independent integration audit — domain and Android service

Date: 18 October 2026 (Asia/Bangkok). Reviewer: domain specialist reviewing root-owned service, transport, graph and their integration. Production review was read-only; this reviewer issued no phone, power/lock, ESP32, vehicle-history or database mutation commands during this audit. This is a source/evidence review, not an actual-car acceptance test.

## Final conclusion and signoff

The reviewed implementation correctly gates mileage on fresh valid PID `010D` samples, cuts integration at missing/invalid/disconnected/stale boundaries, restores accumulated mileage without restoring a speed endpoint, and serializes calibration and service recording with live distance. Earlier integration defects listed below have been repaired in source.

**PASS for all Loop 5 gates within the authorized physical-S22 simulation scope.** Both final findings were repaired and rechecked in source. The final debug/release build and all 55 tests pass. Physical scenarios A–E, a consumed first fresh frame, malformed-percent handling, nonzero accumulated-distance cold restart, custom intervals, displayed approaching/overdue notifications and the scoped zero-fatal check have now been reviewed. No unresolved critical domain/service defect was found in the reviewed final source and evidence.

This acceptance covers the actual Android app on the physical S22 using isolated injected simulation responses. Actual ELM327 pairing, ECU communication and driving accuracy remain untested and are the task owner's next-morning car validation. No simulation result is presented as a real-car test.

## Final findings and closure

1. **Closed P1 — Adapter text could crash notification formatting.** `app/src/main/java/com/civichud/maintenance/service/obd/ObdTrackingForegroundService.kt`, `notification`, now formats `sessionDistanceKm` independently before concatenating adapter-controlled detail. `malformed-percent.json` shows the running app survived the injected `50% BUSY` response, entered RECONNECT_WAIT and retained exactly `0.10060000000000001` km; `logcat-final.txt` records the corresponding DATA_STALE FRAME with delta 0. The scoped crash result reports zero fatal exceptions.
2. **Closed P2 — Pre-command input draining was not bounded.** `BluetoothClassicObdTransport.command` now limits draining to 8192 bytes and 250 ms, checks coroutine cancellation and throws an I/O error on overflow. The final source was inspected directly and rebuilt successfully. Continuous real-radio input was not physically generated; this closure rests on source and compile evidence rather than an adapter stress test.

An additional conservative accuracy limitation is intentional in the current integrator: its monotonic high-water mark can reject a frame received just before a watchdog tick but delivered after that tick. This prevents old queued frames from restarting distance accumulation, but scheduler contention can undercount. It cannot add phantom distance. Longer real-adapter sampling is still needed to characterize this effect.

## Immutable Loop 5 gates

| Gate | Evidence and result |
| --- | --- |
| No PID0131 distance | Search of `app/src/main`, `data/src/main` and `domain/src/main` found no PID0131/4131 command or decode path. `Elm327ProtocolHandler.readSpeed` sends only `010D`. Parser accepts only `410DXX`; non-speed PIDs cannot produce speed. **Source passes.** |
| Invalid, disconnect and >3 s stale data halt accumulation | `TrackingSession.frame` validates `0 <= atMs <= now` and `now-atMs <= 3000` before `acceptSpeed`. Parser null and invalid timestamps pass null and clear the endpoint. Every connection-status transition resets the endpoint. `tick` adds exactly zero and cuts gaps strictly greater than 3000 ms. Trapezoids require adjacent valid samples with positive elapsed time <=3000 ms. **Source + unit tests pass; device gap evidence agrees.** |
| Real Bluetooth and required AT initialization implemented | Production creates `BluetoothSocket` with RFCOMM SPP UUID `00001101-0000-1000-8000-00805F9B34FB`, closes failed sockets and retries with capped exponential backoff. Initialization sends `ATZ`, `ATE0`, `ATL0`, `ATH0`, `ATSP0` in that order and rejects explicit invalid/mixed-error replies. Prompt framing is capped at 8192 characters; connect/response timeouts and bounded input draining exist. **Source + protocol unit tests pass; actual adapter remains pending.** |
| Calibration, custom km/month intervals, progress colors and local warnings | Calibration flushes pending distance, writes actual baseline, resets accumulated/session distance and clears the endpoint. Maintenance uses `LocalDate.plusMonths`, independent km/day limits, inclusive warning/due boundaries and progress clamped to [0,1]. UI maps NORMAL/APPROACHING/OVERDUE to green/amber/red and labels estimated odometer. Notification dedup keys include rule/cycle/status; runtime permission, global notification enabled state and channel state are checked before reservation. Periodic work evaluates real-vehicle date reminders. Persisted QA rule has 4 km/2 month intervals; Android notification dumps contain both approaching and overdue titles for QA-Interval. **Source + unit + physical-device QA evidence pass.** |
| Room service history with optional fields | Service save and calibration share the session mutex. Room transaction inserts history and advances both service baselines/cycle; no deletion cascade removes historic item names. Optional coordinates, address text, mechanic, phone, cost and notes are represented. GPS is requested on tap with a location timeout and manual-text fallback. Scenario E stores a UI-entered service and matching cycle reset, with manual location, mechanic, phone, cost and notes, and retains them after restart. Calibration does not rewrite history. Sync UUID/updatedAt/isDirty are present. **Source + device Room readbacks pass. Nine data JVM tests validate inputs; GPS coordinate acquisition was not established by the reviewed artifacts.** |
| Real and simulation isolation | Separate Room filenames are selected explicitly; mode publication follows successful target-store initialization. Debug raw-frame receiver is in `src/debug` and requires `android.permission.DUMP`; merged release manifest has no debug receiver. No production mileage seed is present; new vehicle requires calibration. **Source and release-manifest inspection pass.** |
| User stop, reconnect and cold restore | Start/stop transitions use a control mutex; stop cancels worker, closes transport, joins tracking tasks, flushes, clears tracking preference and releases wakelock. Cold startup awaits `graph.ready`; initial uncalibrated integrator cannot checkpoint zero over persisted mileage. Preparing a session restores only accumulated distance, with session=0 and no speed endpoint. Reconnect statuses cut endpoints. Final before/after cold-restart records preserve nonzero accumulated distance exactly while resetting session to zero; STOPPED persists. **Source + device A–E/final checks pass; no rapid-control runtime stress claim.** |
| Unit/build plus physical S22 A–E, background and zero fatal errors | 55 tests are confirmed in XML: 39 domain, 7 protocol, 9 data validation; all failures/errors/skips=0. Final `verification/build-and-tests.log` ends BUILD SUCCESSFUL in 11 s, includes debug/release assembly and test tasks, and has no warning lines. A–E and final-check artifacts are physical-phone simulation. `crash-check.json` reports zero package-owned fatal exceptions; reviewed final log contains no owned fatal block. **Pass for the recorded run and stated background scope.** |

## Reviewed physical-phone simulation evidence

`scenarios-a-d.json` was read directly. Every scenario state is explicitly `simulation=true`; no state is evidence of a real vehicle or ECU connection.

- **A:** Total distance increased from 0.3641666667 to 0.7296333333 km while status stayed CONNECTED; the first rule's remaining distance decreased by the same amount. This supports normal frame-driven updates, not driving accuracy against a real odometer.
- **B:** Before and after the six-second gap, accumulated distance remained exactly 0.8385333333 km, with DATA_STALE and a persisted checkpoint. Later fresh driving resumed. The early `b-first-resumed-frame.json` has RECONNECT_WAIT and was insufficient as consumed-frame proof. This evidence gap is now closed by `fresh-first-sample.json`: status CONNECTED, accumulated/session 0, speed frame consumed; `logcat-final.txt` records its CONNECTED FRAME with speed 60 and delta 0. `final-checks.json` also records the explicit predicate as passed.
- **C:** The reported process ID stayed unchanged and distance increased from 0.9503333333 to 1.1534666667 km during the stated HOME + `am make-uid-idle` scope. The evidence itself correctly says this is not forced deep Doze or a screen-lock test.
- **D:** Link-down states show retry delays of 4000 and 8000 ms with unchanged accumulated mileage; subsequent CONNECTED state increases distance to 1.3054333333 km. This supports reconnect/backoff and later recovery.
- **E:** `scenario-e.json` records a UI-entered service, cycle reset and saved odometer `123456.7`, unchanged after force-stop/relaunch, with history count 1. `final-checks.json` adds the stronger nonzero-distance cold restart: odometer `123459.8006` and accumulated `0.10060000000000001` km are identical before/after; session resets from the same nonzero value to 0. This establishes checkpoint restoration without carrying a previous speed endpoint. The real-store final state is independently uncalibrated, base/accumulated/session 0, history count 0 and STOPPED.

Additional final QA confirms the custom 4 km/2 month rule persists and both Android approaching/overdue notification titles are present in `notifications-approaching.txt` and `notifications-overdue.txt`, with history preserved. These alerts deliberately use simulation QA data. The service row includes manual optional fields and null GPS coordinates; it is not a claim of an actual garage visit or successful GPS fix.

## Closed earlier findings rechecked in source

- Freshness is now checked before integration, including rejection of future timestamps.
- Start/stop setup is serialized with the service control mutex.
- Calibration flushes before writing its new baseline.
- Service history save now uses the same session mutex as integration/calibration.
- AT initialization rejects `?`, `NO DATA`, `UNABLE`, empty and mixed `OK`/error replies.
- Mode selection stages the target database before publishing it; UI guards against mixed source/telemetry during transition.
- Notifications check runtime/global/channel eligibility before reserving a cycle/status notification.
- Notification numeric formatting is separated from raw adapter detail, and the malformed-percent regression ran without a crash.
- Input draining is bounded by elapsed time, bytes and cancellation before the prompt-framed command read begins.

## Honest limits for handoff

Simulation can establish parser/integration behavior, UI response, foreground service operation, gap handling and persistence on this S22. It does not establish ELM327 clone compatibility, Bluetooth radio stability in the car, ECU response behavior, exact wall-clock travel distance, OEM battery behavior over hours, full forced Doze, or next-day notification delivery. Unexpected process death can lose distance since the last checkpoint (up to the specified 0.5 km/60 s cadence); no synthetic backfill is attempted. Actual-car validation must remain marked pending.

**Signed off by the domain/service audit agent after read-only review of final production source, the final build/test results, A–E physical-S22 simulation artifacts, final regression records and scoped crash evidence.** No phone commands were issued by this reviewer. The signoff accepts the completed implementation and recorded HIL simulation gates; it does not substitute for the task owner's actual-car test.
