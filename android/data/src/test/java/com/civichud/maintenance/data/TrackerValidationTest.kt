package com.civichud.maintenance.data

import com.civichud.maintenance.domain.MaintenanceRule
import com.civichud.maintenance.domain.ServiceDraft
import java.time.LocalDate
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class TrackerValidationTest {
    private val rule = MaintenanceRule(0, "น้ำมันเครื่อง", 10_000.0, 6,
        120_000.0, LocalDate.of(2026, 10, 18), 1_000.0, 30, 0)

    @Test fun allowsDistanceOnlyAndCalendarOnlySchedules() {
        validateRule(rule.copy(intervalMonths = 0))
        validateRule(rule.copy(intervalKm = 0.0))
        validateRule(rule.copy(lastServiceOdometerKm = 0.0, warningKm = 0.0, warningDays = 0))
    }

    @Test fun rejectsSchedulesThatCannotBecomeDue() {
        assertInvalid { validateRule(rule.copy(intervalKm = 0.0, intervalMonths = 0)) }
        assertInvalid { validateRule(rule.copy(intervalKm = -1.0)) }
        assertInvalid { validateRule(rule.copy(intervalMonths = -1)) }
        assertInvalid { validateRule(rule.copy(intervalKm = Double.NaN)) }
        assertInvalid { validateRule(rule.copy(intervalKm = Double.POSITIVE_INFINITY)) }
    }

    @Test fun rejectsInvalidBaselinesWarningsAndIdentifiers() {
        assertInvalid { validateRule(rule.copy(name = "  ")) }
        assertInvalid { validateRule(rule.copy(id = -1)) }
        assertInvalid { validateRule(rule.copy(cycle = -1)) }
        assertInvalid { validateRule(rule.copy(lastServiceOdometerKm = -1.0)) }
        assertInvalid { validateRule(rule.copy(lastServiceOdometerKm = Double.NaN)) }
        assertInvalid { validateRule(rule.copy(warningKm = -1.0)) }
        assertInvalid { validateRule(rule.copy(warningKm = Double.POSITIVE_INFINITY)) }
        assertInvalid { validateRule(rule.copy(warningDays = -1)) }
    }

    @Test fun allServiceDetailsAreOptional() {
        validateDraft(ServiceDraft())
        validateDraft(ServiceDraft(cost = 0.0))
        validateDraft(ServiceDraft(latitude = -90.0, longitude = -180.0))
        validateDraft(ServiceDraft(latitude = 90.0, longitude = 180.0))
    }

    @Test fun rejectsInvalidCostsAndCoordinates() {
        assertInvalid { validateDraft(ServiceDraft(cost = -0.01)) }
        assertInvalid { validateDraft(ServiceDraft(cost = Double.NaN)) }
        assertInvalid { validateDraft(ServiceDraft(cost = Double.POSITIVE_INFINITY)) }
        assertInvalid { validateDraft(ServiceDraft(latitude = 13.0)) }
        assertInvalid { validateDraft(ServiceDraft(longitude = 100.0)) }
        assertInvalid { validateDraft(ServiceDraft(latitude = 91.0, longitude = 100.0)) }
        assertInvalid { validateDraft(ServiceDraft(latitude = 13.0, longitude = -181.0)) }
        assertInvalid { validateDraft(ServiceDraft(latitude = Double.NaN, longitude = 100.0)) }
    }

    @Test fun rejectsNonfiniteAndNegativeDistance() {
        requireDistance(0.0, "Odometer")
        requireDistance(123_456.789, "Odometer")
        assertInvalid { requireDistance(-1.0, "Odometer") }
        assertInvalid { requireDistance(Double.NaN, "Odometer") }
        assertInvalid { requireDistance(Double.NEGATIVE_INFINITY, "Odometer") }
        assertInvalid { requireDistance(Double.POSITIVE_INFINITY, "Odometer") }
    }

    @Test fun persistedDatesAndSyncIdsRoundTripWithoutTimezoneLoss() {
        val converters = TrackerConverters()
        val date = LocalDate.of(2024, 2, 29)
        val syncId = UUID.randomUUID()
        assertEquals(date, converters.epochDayToDate(converters.dateToEpochDay(date)))
        assertEquals(syncId, converters.stringToUuid(converters.uuidToString(syncId)))
    }

    @Test fun editingScheduleCannotRevertAConcurrentServiceReset() {
        val actualServiceDate = LocalDate.of(2026, 10, 19)
        val persisted = RuleEntity(id = 1, name = rule.name, intervalKm = 10_000.0,
            intervalMonths = 6, lastServiceOdometerKm = 121_000.0,
            lastServiceDate = actualServiceDate, warningKm = 1_000.0, warningDays = 30,
            cycle = 3, updatedAt = 10, isDirty = false)
        val edited = updatedRuleSettings(persisted, rule.copy(id = 1, name = " เปลี่ยนรอบ ",
            intervalKm = 8_000.0, intervalMonths = 5, warningKm = 500.0, warningDays = 14), 20)
        assertEquals("เปลี่ยนรอบ", edited.name)
        assertEquals(8_000.0, edited.intervalKm, 0.0)
        assertEquals(5, edited.intervalMonths)
        assertEquals(500.0, edited.warningKm, 0.0)
        assertEquals(14L, edited.warningDays)
        assertEquals(121_000.0, edited.lastServiceOdometerKm, 0.0)
        assertEquals(actualServiceDate, edited.lastServiceDate)
        assertEquals(3L, edited.cycle)
        assertEquals(persisted.syncId, edited.syncId)
        assertEquals(20L, edited.updatedAt)
        assertEquals(true, edited.isDirty)
    }

    @Test fun editingSetupSchedulePreservesItsPendingFirstCalibration() {
        val starting = RuleEntity(id = 1, name = rule.name, intervalKm = 10_000.0,
            intervalMonths = 6, lastServiceOdometerKm = 0.0,
            lastServiceDate = LocalDate.of(2026, 10, 18), warningKm = 1_000.0,
            warningDays = 30, cycle = 0, baselinePendingCalibration = true, updatedAt = 10)
        val edited = updatedRuleSettings(starting, rule.copy(id = 1), 20)
        assertEquals(0.0, edited.lastServiceOdometerKm, 0.0)
        assertEquals(starting.lastServiceDate, edited.lastServiceDate)
        assertEquals(true, edited.baselinePendingCalibration)
    }

    private fun assertInvalid(block: () -> Unit) {
        assertThrows(IllegalArgumentException::class.java) { block() }
    }
}
