package com.civichud.maintenance.data

import com.civichud.maintenance.domain.FeedbackDraft
import com.civichud.maintenance.domain.VehicleProfileDraft
import java.time.LocalDate
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class ProfileFeedbackTest {
    @Test fun profileEditRetainsAllTrackingAndCalibrationState() {
        val current = VehicleEntity(name = "Old name", baseOdometerKm = 123456.7,
            accumulatedKm = 0.1006, calibratedAt = 1234, isCalibrated = true,
            adapterAddress = "AA:BB:CC:DD:EE:FF", updatedAt = 10, isDirty = false)
        val draft = VehicleProfileDraft(" My car ", " Honda ", " Civic ", 2017,
            " กข 1234 ", " Petrol ")
        validateVehicleProfile(draft)
        val saved = updatedVehicleProfile(current, draft, 20)
        assertEquals(current.id, saved.id)
        assertEquals(current.baseOdometerKm, saved.baseOdometerKm, 0.0)
        assertEquals(current.accumulatedKm, saved.accumulatedKm, 0.0)
        assertEquals(current.calibratedAt, saved.calibratedAt)
        assertEquals(current.isCalibrated, saved.isCalibrated)
        assertEquals(current.adapterAddress, saved.adapterAddress)
        assertEquals(current.syncId, saved.syncId)
        assertEquals("My car", saved.toDomain().name)
        assertEquals("Honda", saved.toDomain().manufacturer)
        assertEquals("Civic", saved.toDomain().model)
        assertEquals(2017, saved.toDomain().modelYear)
        assertEquals("กข 1234", saved.toDomain().registration)
        assertEquals("Petrol", saved.toDomain().fuelType)
        assertEquals(20L, saved.updatedAt)
        assertEquals(true, saved.isDirty)
    }

    @Test fun profileDetailsAreOptionalButNameAndYearMustBeValid() {
        validateVehicleProfile(VehicleProfileDraft("รถของฉัน"))
        validateVehicleProfile(VehicleProfileDraft("รถ", modelYear = LocalDate.now().year + 2))
        invalid { validateVehicleProfile(VehicleProfileDraft("  ")) }
        invalid { validateVehicleProfile(VehicleProfileDraft("x".repeat(121))) }
        invalid { validateVehicleProfile(VehicleProfileDraft("รถ", manufacturer = "x".repeat(81))) }
        invalid { validateVehicleProfile(VehicleProfileDraft("รถ", registration = "x".repeat(41))) }
        invalid { validateVehicleProfile(VehicleProfileDraft("รถ", modelYear = 0)) }
        invalid { validateVehicleProfile(VehicleProfileDraft("รถ", modelYear = LocalDate.now().year + 3)) }
    }

    @Test fun feedbackRequiresCategoryTitleMessageButNoEmailOrDiagnostics() {
        validateFeedbackDraft(FeedbackDraft("idea", "Title", "Message"))
        validateFeedbackDraft(FeedbackDraft("bug", "Title", "Message", " person+reply@example.com ", true))
        invalid { validateFeedbackDraft(FeedbackDraft(" ", "Title", "Message")) }
        invalid { validateFeedbackDraft(FeedbackDraft("bug", " ", "Message")) }
        invalid { validateFeedbackDraft(FeedbackDraft("bug", "Title", " ")) }
        invalid { validateFeedbackDraft(FeedbackDraft("bug", "x".repeat(161), "Message")) }
        invalid { validateFeedbackDraft(FeedbackDraft("bug", "Title", "x".repeat(10001))) }
        invalid { validateFeedbackDraft(FeedbackDraft("bug", "Title", "Message", "person@example")) }
        invalid { validateFeedbackDraft(FeedbackDraft("bug", "Title", "Message", "person @example.com")) }
    }

    @Test fun localFeedbackStartsPendingWithoutServerReceipt() {
        val entry = pending()
        val domain = entry.toDomain()
        assertEquals("PENDING", domain.status)
        assertEquals("", domain.receiptId)
        assertEquals(entry.syncId.toString(), domain.syncId)
        assertEquals(false, domain.includeDiagnostics)
        assertEquals(100L, domain.createdAt)
    }

    @Test fun blankReceiptCannotMarkFeedbackSent() {
        val entry = pending()
        invalid { acknowledgedFeedback(entry, "", 200) }
        invalid { acknowledgedFeedback(entry, "  \n ", 200) }
        assertEquals("PENDING", entry.status)
        assertEquals("", entry.receiptId)
    }

    @Test fun acknowledgmentPreservesPayloadIdentityConsentAndCreationTime() {
        val entry = pending().copy(includeDiagnostics = true)
        val sent = acknowledgedFeedback(entry, " receipt-123 ", 200)
        assertEquals("SENT", sent.status)
        assertEquals("receipt-123", sent.receiptId)
        assertEquals(200L, sent.updatedAt)
        assertEquals(entry, sent.copy(status = entry.status, receiptId = entry.receiptId,
            updatedAt = entry.updatedAt))
    }

    @Test fun repeatedAcknowledgmentIsIdempotentAndCannotOverwriteReceipt() {
        val sent = acknowledgedFeedback(pending(), "receipt-123", 200)
        assertEquals(sent, acknowledgedFeedback(sent, " receipt-123 ", 300))
        invalid { acknowledgedFeedback(sent, "receipt-other", 300) }
    }

    private fun pending() = FeedbackEntity(id = 1, category = "idea", title = "Title",
        message = "Message", replyEmail = "", includeDiagnostics = false,
        createdAt = 100, updatedAt = 100)

    private fun invalid(block: () -> Unit) {
        assertThrows(IllegalArgumentException::class.java) { block() }
    }
}
