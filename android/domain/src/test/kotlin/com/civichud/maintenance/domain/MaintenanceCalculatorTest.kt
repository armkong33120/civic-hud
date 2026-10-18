package com.civichud.maintenance.domain

import java.time.LocalDate
import org.junit.Assert.*
import org.junit.Test

class MaintenanceCalculatorTest {
    private val baseline = LocalDate.of(2024, 1, 31)
    private fun rule() = MaintenanceRule(1, "Oil", 10_000.0, 6, 50_000.0, baseline, 500.0, 14, 0)

    @Test fun `normal evaluates both dimensions and greatest used fraction`() {
        val result = MaintenanceCalculator.evaluate(rule(), 55_000.0, baseline)
        assertEquals(5_000.0, result.remainingKm!!, 0.0)
        assertEquals(182L, result.remainingDays)
        assertEquals(0.5, result.usedFraction, 0.0)
        assertEquals(MaintenanceStatus.NORMAL, result.status)
    }

    @Test fun `distance warning boundary is inclusive`() {
        assertEquals(MaintenanceStatus.NORMAL, MaintenanceCalculator.evaluate(rule(), 59_499.0, baseline).status)
        assertEquals(MaintenanceStatus.APPROACHING, MaintenanceCalculator.evaluate(rule(), 59_500.0, baseline).status)
    }

    @Test fun `time warning can trigger with distance still normal`() {
        val due = baseline.plusMonths(6)
        assertEquals(MaintenanceStatus.NORMAL, MaintenanceCalculator.evaluate(rule(), 50_000.0, due.minusDays(15)).status)
        assertEquals(MaintenanceStatus.APPROACHING, MaintenanceCalculator.evaluate(rule(), 50_000.0, due.minusDays(14)).status)
    }

    @Test fun `either dimension due is overdue including exact boundary`() {
        assertEquals(MaintenanceStatus.OVERDUE, MaintenanceCalculator.evaluate(rule(), 60_000.0, baseline).status)
        assertEquals(MaintenanceStatus.OVERDUE, MaintenanceCalculator.evaluate(rule(), 50_000.0, baseline.plusMonths(6)).status)
        assertEquals(MaintenanceStatus.OVERDUE, MaintenanceCalculator.evaluate(rule(), 65_000.0, baseline.plusMonths(7)).status)
    }

    @Test fun `calendar months preserve month end including leap February`() {
        val leap = rule().copy(intervalKm = 0.0, intervalMonths = 1)
        val result = MaintenanceCalculator.evaluate(leap, 50_000.0, LocalDate.of(2024, 2, 28))
        assertEquals(1L, result.remainingDays)
        assertEquals(MaintenanceStatus.APPROACHING, result.status)
        val ordinary = leap.copy(lastServiceDate = LocalDate.of(2023, 1, 31))
        assertEquals(0L, MaintenanceCalculator.evaluate(ordinary, 50_000.0, LocalDate.of(2023, 2, 28)).remainingDays)
    }

    @Test fun `progress clamped before baseline and past due`() {
        assertEquals(0.0, MaintenanceCalculator.evaluate(rule(), 49_000.0, baseline.minusDays(1)).usedFraction, 0.0)
        assertEquals(1.0, MaintenanceCalculator.evaluate(rule(), 70_000.0, baseline.plusYears(2)).usedFraction, 0.0)
    }

    @Test fun `calendar progress uses actual days in period`() {
        val monthly = rule().copy(intervalKm = 0.0, intervalMonths = 1)
        assertEquals(14.0 / 29, MaintenanceCalculator.evaluate(monthly, 50_000.0, baseline.plusDays(14)).usedFraction, 1e-12)
    }

    @Test fun `disabled dimensions have null remaining and do not trigger warnings`() {
        val kmOnly = MaintenanceCalculator.evaluate(rule().copy(intervalMonths = 0), 50_000.0, baseline.plusYears(4))
        assertNull(kmOnly.remainingDays)
        assertEquals(MaintenanceStatus.NORMAL, kmOnly.status)
        val dateOnly = MaintenanceCalculator.evaluate(rule().copy(intervalKm = 0.0), 100_000.0, baseline)
        assertNull(dateOnly.remainingKm)
        assertEquals(MaintenanceStatus.NORMAL, dateOnly.status)
        val disabled = MaintenanceCalculator.evaluate(rule().copy(intervalKm = 0.0, intervalMonths = 0), 100_000.0, baseline.plusYears(4))
        assertEquals(MaintenanceStatus.NORMAL, disabled.status)
        assertEquals(0.0, disabled.usedFraction, 0.0)
    }

    @Test fun `service reset advances cycle and both baselines retaining user configuration`() {
        val original = rule().copy(cycle = 8)
        val date = LocalDate.of(2026, 10, 18)
        val reset = ServiceCycle.reset(original, 62_000.0, date)
        assertEquals(original.copy(lastServiceOdometerKm = 62_000.0, lastServiceDate = date, cycle = 9), reset)
        assertEquals(MaintenanceStatus.NORMAL, MaintenanceCalculator.evaluate(reset, 62_000.0, date).status)
        assertEquals(50_000.0, original.lastServiceOdometerKm, 0.0)
    }

    @Test fun `calibration offset and baseline preserve other vehicle fields`() {
        val vehicle = Vehicle(name = "Car", baseOdometerKm = 50_000.0, accumulatedKm = 200.5, calibratedAt = 1, isCalibrated = false, adapterAddress = "address")
        assertEquals(49.5, Calibration.offsetKm(50_250.0, vehicle.estimatedOdometerKm), 0.0)
        assertEquals(-200.5, Calibration.offsetKm(50_000.0, vehicle.estimatedOdometerKm), 0.0)
        val calibrated = Calibration.calibrate(vehicle, 50_250.0, 123)
        assertEquals(50_250.0, calibrated.estimatedOdometerKm, 0.0)
        assertEquals(0.0, calibrated.accumulatedKm, 0.0)
        assertTrue(calibrated.isCalibrated)
        assertEquals(123L, calibrated.calibratedAt)
        assertEquals(vehicle.adapterAddress, calibrated.adapterAddress)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `negative calibration rejected`() { Calibration.offsetKm(-1.0, 0.0) }

    @Test(expected = IllegalArgumentException::class)
    fun `nonfinite calibration rejected`() { Calibration.offsetKm(Double.NaN, 0.0) }

    @Test(expected = IllegalArgumentException::class)
    fun `negative service mileage rejected`() { ServiceCycle.reset(rule(), -1.0, baseline) }
}
