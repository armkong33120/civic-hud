package com.civichud.maintenance.data

import android.content.Context
import androidx.room.withTransaction
import com.civichud.maintenance.domain.MaintenanceRule
import com.civichud.maintenance.domain.MaintenanceStatus
import com.civichud.maintenance.domain.ServiceDraft
import com.civichud.maintenance.domain.ServiceLog
import com.civichud.maintenance.domain.Vehicle
import com.civichud.maintenance.domain.VehicleProfileDraft
import com.civichud.maintenance.domain.FeedbackDraft
import com.civichud.maintenance.domain.FeedbackEntry
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

/** Durable state; callers serialize distance checkpoints with calibration and service actions. */
class TrackerStore(context: Context, val simulation: Boolean = false) {
    private val db = TrackerDatabase.get(context, simulation)
    private val dao = db.trackerDao()

    val vehicle: Flow<Vehicle?> = dao.observeVehicle().map { it?.toDomain() }
    val rules: Flow<List<MaintenanceRule>> = dao.observeRules().map { rows -> rows.map { it.toDomain() } }
    val history: Flow<List<ServiceLog>> = dao.observeHistory().map { rows -> rows.map { it.toDomain() } }
    val feedback: Flow<List<FeedbackEntry>> = dao.observeFeedback().map { rows -> rows.map { it.toDomain() } }

    suspend fun initialize() {
        db.withTransaction {
            if (dao.vehicle() != null) return@withTransaction
            val now = System.currentTimeMillis()
            val today = localDate(now)
            dao.insertVehicle(
                VehicleEntity(
                    name = "รถของฉัน", baseOdometerKm = 0.0,
                    accumulatedKm = 0.0, calibratedAt = 0, isCalibrated = false,
                    adapterAddress = "", updatedAt = now,
                ),
            )
            // Editable starting schedules, not a claim that these services were performed.
            defaultRules(today, now).forEach { dao.insertRule(it) }
        }
    }

    suspend fun currentVehicle(): Vehicle {
        initialize()
        return requireVehicle().toDomain()
    }

    suspend fun checkpoint(accumulatedKm: Double) {
        requireDistance(accumulatedKm, "Accumulated distance")
        initialize()
        db.withTransaction {
            val current = requireVehicle()
            check(current.isCalibrated) { "Calibrate the odometer before tracking distance" }
            require(accumulatedKm >= current.accumulatedKm) { "Accumulated distance cannot decrease" }
            require((current.baseOdometerKm + accumulatedKm).isFinite()) { "Odometer exceeds supported range" }
            if (current.accumulatedKm != accumulatedKm) {
                dao.updateVehicle(current.copy(accumulatedKm = accumulatedKm,
                    updatedAt = System.currentTimeMillis(), isDirty = true))
            }
        }
    }

    suspend fun calibrate(actualKm: Double) {
        requireDistance(actualKm, "Actual odometer")
        initialize()
        db.withTransaction {
            val current = requireVehicle()
            val now = System.currentTimeMillis()
            if (!current.isCalibrated) {
                dao.pendingBaselines().forEach { rule ->
                    dao.updateRule(rule.copy(lastServiceOdometerKm = actualKm,
                        baselinePendingCalibration = false, updatedAt = now, isDirty = true))
                }
            }
            dao.updateVehicle(current.copy(baseOdometerKm = actualKm, accumulatedKm = 0.0,
                calibratedAt = now, isCalibrated = true, updatedAt = now, isDirty = true))
        }
    }

    suspend fun saveAdapter(address: String) {
        initialize()
        db.withTransaction {
            val current = requireVehicle()
            dao.updateVehicle(current.copy(adapterAddress = address.trim(),
                updatedAt = System.currentTimeMillis(), isDirty = true))
        }
    }

    suspend fun saveVehicleProfile(draft: VehicleProfileDraft) {
        validateVehicleProfile(draft)
        initialize()
        db.withTransaction {
            val current = requireVehicle()
            dao.updateVehicle(updatedVehicleProfile(current, draft, System.currentTimeMillis()))
        }
    }

    /** Durable local outbox only. Consent records intent; this method collects no diagnostics. */
    suspend fun enqueueFeedback(draft: FeedbackDraft): Long {
        validateFeedbackDraft(draft)
        initialize()
        return db.withTransaction {
            val now = System.currentTimeMillis()
            dao.insertFeedback(FeedbackEntity(category = draft.category.trim(),
                title = draft.title.trim(), message = draft.message.trim(),
                replyEmail = draft.replyEmail.trim(), includeDiagnostics = draft.includeDiagnostics,
                createdAt = now, updatedAt = now))
        }
    }

    suspend fun pendingFeedback(): List<FeedbackEntry> {
        initialize()
        return dao.pendingFeedback().map { it.toDomain() }
    }

    /** Call only after an acknowledged server response; never treat a local enqueue as received. */
    suspend fun markFeedbackSent(id: Long, receiptId: String) {
        require(id > 0) { "Invalid feedback item" }
        require(receiptId.isNotBlank()) { "A server receipt is required before marking feedback sent" }
        initialize()
        db.withTransaction {
            val existing = requireNotNull(dao.feedback(id)) { "Feedback item no longer exists" }
            val updated = acknowledgedFeedback(existing, receiptId, System.currentTimeMillis())
            if (updated != existing) dao.updateFeedback(updated)
        }
    }

    suspend fun deleteFeedback(id: Long) {
        require(id > 0) { "Invalid feedback item" }
        initialize()
        db.withTransaction { dao.deleteFeedback(id) }
    }

    suspend fun saveRule(rule: MaintenanceRule) {
        validateRule(rule)
        initialize()
        db.withTransaction {
            val now = System.currentTimeMillis()
            if (rule.id == 0L) {
                dao.insertRule(RuleEntity(name = rule.name.trim(), intervalKm = rule.intervalKm,
                    intervalMonths = rule.intervalMonths, lastServiceOdometerKm = rule.lastServiceOdometerKm,
                    lastServiceDate = rule.lastServiceDate, warningKm = rule.warningKm,
                    warningDays = rule.warningDays, cycle = 0, updatedAt = now))
            } else {
                val existing = requireNotNull(dao.rule(rule.id)) { "Maintenance item no longer exists" }
                // An editor may have opened before a service or first calibration. Changing a
                // schedule must retain the latest baseline committed by those operations.
                dao.updateRule(updatedRuleSettings(existing, rule, now))
            }
        }
    }

    suspend fun deleteRule(id: Long) {
        require(id > 0) { "Invalid maintenance item" }
        initialize()
        // Service history keeps the item name and does not cascade when a schedule is removed.
        db.withTransaction { dao.deleteRule(id) }
    }

    suspend fun logService(ruleId: Long, odometerKm: Double, draft: ServiceDraft) {
        require(ruleId > 0) { "Invalid maintenance item" }
        requireDistance(odometerKm, "Service odometer")
        validateDraft(draft)
        initialize()
        db.withTransaction {
            check(requireVehicle().isCalibrated) { "Calibrate the odometer before recording a service" }
            val rule = requireNotNull(dao.rule(ruleId)) { "Maintenance item no longer exists" }
            check(rule.cycle < Long.MAX_VALUE) { "Service cycle exceeds supported range" }
            val now = System.currentTimeMillis()
            dao.insertLog(ServiceLogEntity(itemId = rule.id, itemName = rule.name,
                odometerKm = odometerKm, performedAt = now, latitude = draft.latitude,
                longitude = draft.longitude, locationText = draft.locationText.trim(),
                mechanicName = draft.mechanicName.trim(), phone = draft.phone.trim(),
                cost = draft.cost, notes = draft.notes, updatedAt = now))
            dao.updateRule(rule.copy(lastServiceOdometerKm = odometerKm,
                lastServiceDate = localDate(now), cycle = rule.cycle + 1,
                baselinePendingCalibration = false, updatedAt = now, isDirty = true))
        }
    }

    /** Atomically reserves one notification per status per cycle, before posting to Android. */
    suspend fun markNotified(ruleId: Long, cycle: Long, status: MaintenanceStatus): Boolean {
        require(ruleId > 0 && cycle >= 0) { "Invalid notification cycle" }
        initialize()
        return db.withTransaction {
            val current = requireVehicle()
            val rule = dao.rule(ruleId)
            if (!current.isCalibrated || rule == null || rule.cycle != cycle) {
                false
            } else {
                dao.reserveNotification(NotificationReservationEntity(ruleId = ruleId,
                    cycle = cycle, status = status.name, updatedAt = System.currentTimeMillis())) != -1L
            }
        }
    }

    private suspend fun requireVehicle(): VehicleEntity = checkNotNull(dao.vehicle()) {
        "Tracker database has not been initialized"
    }

    private fun localDate(epochMs: Long): LocalDate =
        Instant.ofEpochMilli(epochMs).atZone(ZoneId.systemDefault()).toLocalDate()

    private fun defaultRules(today: LocalDate, now: Long): List<RuleEntity> = listOf(
        Triple("น้ำมันเครื่อง", 10_000.0, 6),
        Triple("น้ำมันเกียร์", 40_000.0, 24),
        Triple("กรองอากาศ", 20_000.0, 12),
        Triple("หัวเทียน", 40_000.0, 24),
        Triple("น้ำยาหล่อเย็น", 100_000.0, 60),
        Triple("ผ้าเบรก", 40_000.0, 24),
        Triple("สลับยาง", 10_000.0, 6),
    ).map { (name, km, months) ->
        RuleEntity(name = name, intervalKm = km, intervalMonths = months,
            lastServiceOdometerKm = 0.0, lastServiceDate = today, warningKm = 1_000.0,
            warningDays = 30, cycle = 0, baselinePendingCalibration = true, updatedAt = now)
    }
}

internal fun updatedRuleSettings(existing: RuleEntity, submitted: MaintenanceRule, now: Long): RuleEntity =
    existing.copy(name = submitted.name.trim(), intervalKm = submitted.intervalKm,
        intervalMonths = submitted.intervalMonths, warningKm = submitted.warningKm,
        warningDays = submitted.warningDays, updatedAt = now, isDirty = true)

internal fun updatedVehicleProfile(existing: VehicleEntity, draft: VehicleProfileDraft, now: Long): VehicleEntity =
    existing.copy(name = draft.name.trim(), manufacturer = draft.manufacturer.trim(),
        model = draft.model.trim(), modelYear = draft.modelYear,
        registration = draft.registration.trim(), fuelType = draft.fuelType.trim(),
        updatedAt = now, isDirty = true)

internal fun acknowledgedFeedback(existing: FeedbackEntity, receiptId: String, now: Long): FeedbackEntity {
    val receipt = receiptId.trim()
    require(receipt.isNotEmpty()) { "A server receipt is required before marking feedback sent" }
    if (existing.status == "SENT") {
        require(existing.receiptId == receipt) { "Feedback already has a different receipt" }
        return existing
    }
    return existing.copy(status = "SENT", receiptId = receipt, updatedAt = now, isDirty = true)
}

internal fun validateVehicleProfile(draft: VehicleProfileDraft) {
    require(draft.name.isNotBlank() && draft.name.trim().length <= 120) { "Vehicle name must contain 1–120 characters" }
    require(draft.manufacturer.trim().length <= 80 && draft.model.trim().length <= 80) { "Manufacturer and model must be at most 80 characters" }
    require(draft.registration.trim().length <= 40 && draft.fuelType.trim().length <= 40) { "Registration and fuel type must be at most 40 characters" }
    draft.modelYear?.let { require(it in 1886..(LocalDate.now().year + 2)) { "Invalid vehicle model year" } }
}

internal fun validateFeedbackDraft(draft: FeedbackDraft) {
    require(draft.category.isNotBlank() && draft.category.trim().length <= 80) { "Choose a feedback category" }
    require(draft.title.isNotBlank() && draft.title.trim().length <= 160) { "Feedback title must contain 1–160 characters" }
    require(draft.message.isNotBlank() && draft.message.trim().length <= 10_000) { "Feedback message must contain 1–10000 characters" }
    val email = draft.replyEmail.trim()
    require(email.isEmpty() || (email.length <= 254 && Regex("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$").matches(email))) { "Invalid reply email" }
}

internal fun requireDistance(value: Double, label: String) {
    require(value.isFinite() && value >= 0.0) { "$label must be finite and nonnegative" }
}

internal fun validateRule(rule: MaintenanceRule) {
    require(rule.id >= 0) { "Invalid maintenance item" }
    require(rule.name.isNotBlank()) { "Maintenance item name is required" }
    requireDistance(rule.intervalKm, "Distance interval")
    require(rule.intervalMonths >= 0) { "Month interval must be nonnegative" }
    require(rule.intervalKm > 0.0 || rule.intervalMonths > 0) { "Choose a distance or month interval" }
    requireDistance(rule.lastServiceOdometerKm, "Starting odometer")
    requireDistance(rule.warningKm, "Distance warning")
    require(rule.warningDays >= 0) { "Day warning must be nonnegative" }
    require(rule.cycle >= 0) { "Invalid service cycle" }
}

internal fun validateDraft(draft: ServiceDraft) {
    draft.cost?.let { requireDistance(it, "Service cost") }
    require((draft.latitude == null) == (draft.longitude == null)) { "Both location coordinates are required" }
    draft.latitude?.let { require(it.isFinite() && it in -90.0..90.0) { "Invalid latitude" } }
    draft.longitude?.let { require(it.isFinite() && it in -180.0..180.0) { "Invalid longitude" } }
}
