package com.civichud.maintenance.data

import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey
import com.civichud.maintenance.domain.MaintenanceRule
import com.civichud.maintenance.domain.ServiceLog
import com.civichud.maintenance.domain.Vehicle
import java.time.LocalDate
import java.util.UUID

@Entity(tableName = "vehicles")
internal data class VehicleEntity(
    @PrimaryKey val id: Long = 1,
    val name: String,
    val baseOdometerKm: Double,
    val accumulatedKm: Double,
    val calibratedAt: Long,
    val isCalibrated: Boolean,
    val adapterAddress: String,
    val syncId: UUID = UUID.randomUUID(),
    val updatedAt: Long,
    val isDirty: Boolean = true,
) {
    fun toDomain() = Vehicle(
        id, name, baseOdometerKm, accumulatedKm, calibratedAt, isCalibrated, adapterAddress,
    )
}

@Entity(tableName = "maintenance_rules")
internal data class RuleEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val name: String,
    val intervalKm: Double,
    val intervalMonths: Int,
    val lastServiceOdometerKm: Double,
    val lastServiceDate: LocalDate,
    val warningKm: Double,
    val warningDays: Long,
    val cycle: Long,
    // Setup baselines are not service events. Only these align with the first calibration.
    val baselinePendingCalibration: Boolean = false,
    val syncId: UUID = UUID.randomUUID(),
    val updatedAt: Long,
    val isDirty: Boolean = true,
) {
    fun toDomain() = MaintenanceRule(
        id, name, intervalKm, intervalMonths, lastServiceOdometerKm, lastServiceDate,
        warningKm, warningDays, cycle,
    )
}

@Entity(tableName = "service_logs", indices = [Index("itemId"), Index("performedAt")])
internal data class ServiceLogEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val itemId: Long,
    val itemName: String,
    val odometerKm: Double,
    val performedAt: Long,
    val latitude: Double?,
    val longitude: Double?,
    val locationText: String,
    val mechanicName: String,
    val phone: String,
    val cost: Double?,
    val notes: String,
    val syncId: UUID = UUID.randomUUID(),
    val updatedAt: Long,
    val isDirty: Boolean = true,
) {
    fun toDomain() = ServiceLog(
        id, itemId, itemName, odometerKm, performedAt, latitude, longitude,
        locationText, mechanicName, phone, cost, notes,
    )
}

@Entity(
    tableName = "notification_reservations",
    indices = [Index(value = ["ruleId", "cycle", "status"], unique = true)],
)
internal data class NotificationReservationEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val ruleId: Long,
    val cycle: Long,
    val status: String,
    val syncId: UUID = UUID.randomUUID(),
    val updatedAt: Long,
    val isDirty: Boolean = true,
)
