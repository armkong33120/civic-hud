package com.civichud.maintenance.domain

/** A snapshot after one sample or clock tick. Only acceptSpeed can return a positive delta. */
data class IntegrationResult(
    val deltaKm: Double,
    val sessionDistanceKm: Double,
    val accumulatedKm: Double,
    val speedKmh: Double?,
    val isStale: Boolean,
)

/**
 * Integrates adjacent PID 010D samples using caller-supplied monotonic milliseconds.
 * No wall clock and no extrapolation are used. Restore persisted accumulated mileage in
 * the constructor; a new instance always starts a fresh session and has no speed endpoint.
 */
class SpeedDistanceIntegrator(
    initialAccumulatedKm: Double = 0.0,
    val staleAfterMs: Long = 3_000,
) {
    init {
        require(initialAccumulatedKm.isFinite() && initialAccumulatedKm >= 0)
        require(staleAfterMs > 0)
    }

    var accumulatedKm: Double = initialAccumulatedKm
        private set
    var sessionDistanceKm: Double = 0.0
        private set
    var speedKmh: Double? = null
        private set
    private var lastSampleMs: Long? = null
    private var lastObservedMs: Long? = null

    fun acceptSpeed(speedKmh: Double?, nowMs: Long): IntegrationResult {
        if (!observeTime(nowMs) || speedKmh == null || !speedKmh.isFinite() || speedKmh !in 0.0..255.0) {
            resetEndpoint()
            return snapshot(0.0, true)
        }
        val previousTime = lastSampleMs
        val previousSpeed = this.speedKmh
        if (previousTime != null && nowMs <= previousTime) {
            resetEndpoint()
            return snapshot(0.0, true)
        }
        val delta = if (previousTime != null && previousSpeed != null &&
            nowMs > previousTime && nowMs - previousTime <= staleAfterMs
        ) {
            (previousSpeed + speedKmh) / 2.0 * (nowMs - previousTime).toDouble() / 3_600_000.0
        } else 0.0
        accumulatedKm += delta
        sessionDistanceKm += delta
        this.speedKmh = speedKmh
        lastSampleMs = nowMs
        return snapshot(delta, false)
    }

    /** Checks freshness only. Timer calls never add mileage, even with a last known speed. */
    fun tick(nowMs: Long): IntegrationResult {
        val validTime = observeTime(nowMs)
        val sampleTime = lastSampleMs
        if (!validTime || sampleTime == null || nowMs - sampleTime > staleAfterMs) {
            resetEndpoint()
            return snapshot(0.0, true)
        }
        return snapshot(0.0, false)
    }

    /** Call immediately on a parse/transport error, stop, or disconnect. Keeps mileage. */
    fun resetEndpoint() {
        speedKmh = null
        lastSampleMs = null
    }

    fun disconnect() = resetEndpoint()

    /** After a calibration, atomically discard any old speed endpoint and replace the total. */
    fun resetAccumulated(totalKm: Double = 0.0) {
        require(totalKm.isFinite() && totalKm >= 0)
        accumulatedKm = totalKm
        sessionDistanceKm = 0.0
        resetEndpoint()
    }

    private fun observeTime(nowMs: Long): Boolean {
        val previous = lastObservedMs
        if (nowMs < 0 || (previous != null && nowMs < previous)) return false
        // Retain a high-water mark: queued old frames must not move the clock backward
        // and establish a new integration interval behind an already observed stale tick.
        lastObservedMs = nowMs
        return true
    }

    private fun snapshot(delta: Double, stale: Boolean) = IntegrationResult(
        delta, sessionDistanceKm, accumulatedKm, speedKmh, stale,
    )
}
