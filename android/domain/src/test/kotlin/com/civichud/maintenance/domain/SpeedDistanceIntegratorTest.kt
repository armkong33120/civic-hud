package com.civichud.maintenance.domain

import org.junit.Assert.*
import org.junit.Test

class SpeedDistanceIntegratorTest {
    private val epsilon = 1e-10

    @Test fun `60 kmh for 60 seconds integrates exactly one km`() {
        val integrator = SpeedDistanceIntegrator()
        for (second in 0L..60L) integrator.acceptSpeed(60.0, second * 1_000)
        assertEquals(1.0, integrator.sessionDistanceKm, epsilon)
        assertEquals(1.0, integrator.accumulatedKm, epsilon)
    }

    @Test fun `ramp uses trapezoid rather than last speed`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(0.0, 0)
        val result = integrator.acceptSpeed(120.0, 1_000)
        assertEquals(60.0 / 3_600, result.deltaKm, epsilon)
    }

    @Test fun `restored total is preserved and first sample adds zero`() {
        val integrator = SpeedDistanceIntegrator(123.25)
        val result = integrator.acceptSpeed(60.0, 100_000)
        assertEquals(0.0, result.deltaKm, 0.0)
        assertEquals(123.25, result.accumulatedKm, 0.0)
        assertEquals(0.0, result.sessionDistanceKm, 0.0)
        assertFalse(result.isStale)
    }

    @Test fun `ten second dropout adds zero then resumes from new sample`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(60.0, 0)
        assertEquals(0.0, integrator.acceptSpeed(60.0, 10_000).deltaKm, 0.0)
        assertEquals(1.0 / 60, integrator.acceptSpeed(60.0, 11_000).deltaKm, epsilon)
    }

    @Test fun `exactly three seconds accepted but three seconds and one ms is cut`() {
        val accepted = SpeedDistanceIntegrator()
        accepted.acceptSpeed(60.0, 0)
        assertEquals(0.05, accepted.acceptSpeed(60.0, 3_000).deltaKm, epsilon)
        val cut = SpeedDistanceIntegrator()
        cut.acceptSpeed(60.0, 0)
        assertEquals(0.0, cut.acceptSpeed(60.0, 3_001).deltaKm, 0.0)
    }

    @Test fun `ticks never extrapolate and stale tick cuts endpoint`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(100.0, 0)
        for (tick in 1L..3_000L) {
            assertEquals(0.0, integrator.tick(tick).deltaKm, 0.0)
        }
        assertFalse(integrator.tick(3_000).isStale)
        assertTrue(integrator.tick(3_001).isStale)
        assertNull(integrator.speedKmh)
        assertEquals(0.0, integrator.acceptSpeed(100.0, 4_000).deltaKm, 0.0)
    }

    @Test fun `tick and sample may share the same monotonic time`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(60.0, 0)
        integrator.tick(1_000)
        assertEquals(1.0 / 60, integrator.acceptSpeed(60.0, 1_000).deltaKm, epsilon)
        assertFalse(integrator.tick(1_000).isStale)
    }

    @Test fun `invalid sample immediately cuts previous endpoint`() {
        for (invalid in listOf(null, -1.0, 256.0, Double.NaN, Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY)) {
            val integrator = SpeedDistanceIntegrator()
            integrator.acceptSpeed(60.0, 0)
            val failed = integrator.acceptSpeed(invalid, 500)
            assertTrue(failed.isStale)
            assertNull(failed.speedKmh)
            assertEquals(0.0, integrator.acceptSpeed(60.0, 1_000).deltaKm, 0.0)
            assertEquals(0.0, integrator.accumulatedKm, 0.0)
        }
    }

    @Test fun `zero speed is valid and produces braking trapezoid`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(60.0, 0)
        val stopped = integrator.acceptSpeed(0.0, 1_000)
        assertEquals(1.0 / 120, stopped.deltaKm, epsilon)
        assertFalse(stopped.isStale)
        assertEquals(0.0, integrator.acceptSpeed(0.0, 2_000).deltaKm, 0.0)
    }

    @Test fun `disconnect preserves mileage and resumed first sample adds zero`() {
        val integrator = SpeedDistanceIntegrator(40.0)
        integrator.acceptSpeed(60.0, 0)
        integrator.acceptSpeed(60.0, 1_000)
        val saved = integrator.accumulatedKm
        integrator.disconnect()
        assertNull(integrator.speedKmh)
        assertEquals(0.0, integrator.acceptSpeed(60.0, 2_000).deltaKm, 0.0)
        assertEquals(saved, integrator.accumulatedKm, 0.0)
        assertEquals(1.0 / 60, integrator.sessionDistanceKm, epsilon)
    }

    @Test fun `parse error reset cuts gap immediately`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(60.0, 0)
        integrator.resetEndpoint()
        assertEquals(0.0, integrator.acceptSpeed(60.0, 1_000).deltaKm, 0.0)
    }

    @Test fun `duplicate and backwards samples invalidate until next anchor`() {
        for (badTime in listOf(1_000L, 999L, -1L)) {
            val integrator = SpeedDistanceIntegrator()
            integrator.acceptSpeed(60.0, 1_000)
            assertTrue(integrator.acceptSpeed(60.0, badTime).isStale)
            assertEquals(0.0, integrator.acceptSpeed(60.0, 2_000).deltaKm, 0.0)
        }
    }

    @Test fun `backwards tick invalidates endpoint`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(60.0, 1_000)
        assertTrue(integrator.tick(900).isStale)
        assertEquals(0.0, integrator.acceptSpeed(60.0, 2_000).deltaKm, 0.0)
    }

    @Test fun `sample older than already observed tick cannot add mileage`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(60.0, 0)
        integrator.tick(2_000)
        assertTrue(integrator.acceptSpeed(60.0, 1_000).isStale)
        assertEquals(0.0, integrator.accumulatedKm, 0.0)
    }

    @Test fun `queued backwards samples cannot rewind a stale clock and integrate`() {
        val integrator = SpeedDistanceIntegrator()
        integrator.acceptSpeed(60.0, 0)
        assertTrue(integrator.tick(10_000).isStale)
        for (queuedTime in 1_000L..9_000L step 1_000L) {
            val result = integrator.acceptSpeed(60.0, queuedTime)
            assertTrue(result.isStale)
            assertEquals(0.0, result.deltaKm, 0.0)
        }
        assertEquals(0.0, integrator.acceptSpeed(60.0, 10_000).deltaKm, 0.0)
        assertEquals(1.0 / 60, integrator.acceptSpeed(60.0, 11_000).deltaKm, epsilon)
    }

    @Test fun `calibration total reset also resets session and endpoint`() {
        val integrator = SpeedDistanceIntegrator(12.0)
        integrator.acceptSpeed(60.0, 0)
        integrator.acceptSpeed(60.0, 1_000)
        integrator.resetAccumulated()
        assertEquals(0.0, integrator.accumulatedKm, 0.0)
        assertEquals(0.0, integrator.sessionDistanceKm, 0.0)
        assertEquals(0.0, integrator.acceptSpeed(60.0, 2_000).deltaKm, 0.0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `negative restore rejected`() { SpeedDistanceIntegrator(-1.0) }

    @Test(expected = IllegalArgumentException::class)
    fun `nonfinite restore rejected`() { SpeedDistanceIntegrator(Double.NaN) }
}
