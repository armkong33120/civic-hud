# Pure vehicle tracking domain

Production code depends only on Kotlin/JDK classes. All public types are in
`com.civichud.maintenance.domain`; immutable model signatures follow `../docs/CONTRACTS.md`.

## Speed and distance

```kotlin
val integrator = SpeedDistanceIntegrator(initialAccumulatedKm = persistedTotal)
val speed: Double? = Elm327ResponseParser.parseSpeedKmh(rawPromptFramedResponse)
val result: IntegrationResult = integrator.acceptSpeed(speed, elapsedRealtimeMillis)
integrator.tick(elapsedRealtimeMillis) // freshness only; always returns deltaKm == 0
integrator.disconnect()               // immediately clears the previous speed endpoint
integrator.resetEndpoint()            // same cut for errors/stop
integrator.resetAccumulated(0.0)        // calibration boundary, clears session and endpoint
```

`IntegrationResult` exposes `deltaKm`, `sessionDistanceKm`, `accumulatedKm`,
`speedKmh` and `isStale`. The integrator also exposes its current
`sessionDistanceKm`, `accumulatedKm` and `speedKmh` as read-only properties.

Only adjacent valid speed samples with an increasing monotonic timestamp and a
gap of at most 3,000 ms contribute mileage, using the trapezoid rule. Samples
after startup, restore, errors, disconnection, backward timestamps or stale gaps
establish a new endpoint and contribute zero. Timer calls never extrapolate.
The monotonic clock high-water mark prevents queued old frames from restarting
integration behind an already observed time. A timer and a sample may share a
timestamp; two samples at the same timestamp cut the endpoint.

The parser consumes ATH0 payloads for PID `010D` only. It accepts whitespace,
compact payloads, command echo, `SEARCHING...`, zero speed and matching repeated
ECU replies. Invalid payloads, adapter errors, negative replies and conflicting
ECU speeds return null, which must immediately cut the integrator endpoint.
Other PIDs never supply a speed or mileage value. Framing, command timeouts,
Bluetooth and AT initialization belong to the app transport.

## Maintenance and calibration

`MaintenanceCalculator.evaluate(rule, odometerKm, today)` evaluates enabled km
and calendar-month limits independently; either can trigger a warning or an
overdue state. Zero/nonpositive intervals disable that dimension. Calendar due
dates use `LocalDate.plusMonths`, remaining days use date boundaries, and progress
is the greater used fraction clamped to `[0,1]`. Exact due boundaries are overdue;
warning boundaries are inclusive.

`Calibration.offsetKm(actualKm, estimatedKm)` calculates the signed correction.
`Calibration.calibrate(vehicle, actualKm, calibratedAt)` creates the new baseline
with accumulated mileage zero and leaves other vehicle fields intact. The caller
must checkpoint pending mileage and coordinate the integrator reset.

`ServiceCycle.reset(rule, odometerKm, date)` returns the rule with both service
baselines replaced and its cycle incremented, retaining intervals and warnings.
Persistence and history insertion must happen in the data-layer transaction.

## Verification

`./gradlew :domain:test` passes 39 JUnit tests covering arithmetic, freshness,
invalid samples, queued timestamps, parser failures and ambiguity, month ends,
dual warning thresholds, progress bounds, calibration and service resets.
