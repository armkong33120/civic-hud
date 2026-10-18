package com.civichud.maintenance.data

import android.content.Context
import androidx.room.withTransaction
import com.civichud.maintenance.domain.MaintenanceRule
import com.civichud.maintenance.domain.MaintenanceStatus
import com.civichud.maintenance.domain.ServiceDraft
import com.civichud.maintenance.domain.ServiceLog
import com.civichud.maintenance.domain.Vehicle
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

    suspend fun initialize() {
        db.withTransaction {
            if (dao.vehicle() != null) return@withTransaction
            val now = System.currentTimeMillis()
            val today = localDate(now)
            dao.insertVehicle(
                VehicleEntity(
                    name = "Honda Civic FC 1.8 2017", baseOdometerKm = 0.0,
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
