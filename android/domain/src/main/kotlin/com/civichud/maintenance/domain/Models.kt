package com.civichud.maintenance.domain

import java.time.LocalDate

data class Vehicle(
    val id: Long = 1,
    val name: String,
    val baseOdometerKm: Double,
    val accumulatedKm: Double,
    val calibratedAt: Long,
    val isCalibrated: Boolean,
    val adapterAddress: String,
    val manufacturer: String = "",
    val model: String = "",
    val modelYear: Int? = null,
    val registration: String = "",
    val fuelType: String = "",
) {
    val estimatedOdometerKm: Double get() = baseOdometerKm + accumulatedKm
}

data class VehicleProfileDraft(
    val name: String,
    val manufacturer: String = "",
    val model: String = "",
    val modelYear: Int? = null,
    val registration: String = "",
    val fuelType: String = "",
)

data class FeedbackDraft(
    val category: String,
    val title: String,
    val message: String,
    val replyEmail: String = "",
    val includeDiagnostics: Boolean = false,
)

data class FeedbackEntry(
    val id: Long,
    val syncId: String,
    val category: String,
    val title: String,
    val message: String,
    val replyEmail: String,
    val includeDiagnostics: Boolean,
    val createdAt: Long,
    val status: String,
    val receiptId: String = "",
)

data class MaintenanceRule(
    val id: Long,
    val name: String,
    val intervalKm: Double,
    val intervalMonths: Int,
    val lastServiceOdometerKm: Double,
    val lastServiceDate: LocalDate,
    val warningKm: Double,
    val warningDays: Long,
    val cycle: Long,
)

data class ServiceLog(
    val id: Long,
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
)

data class ServiceDraft(
    val latitude: Double? = null,
    val longitude: Double? = null,
    val locationText: String = "",
    val mechanicName: String = "",
    val phone: String = "",
    val cost: Double? = null,
    val notes: String = "",
)

enum class MaintenanceStatus { NORMAL, APPROACHING, OVERDUE }

data class MaintenanceResult(
    val rule: MaintenanceRule,
    val remainingKm: Double?,
    val remainingDays: Long?,
    val usedFraction: Double,
    val status: MaintenanceStatus,
)

enum class TrackingStatus {
    STOPPED, SCANNING, CONNECTING, INITIALIZING, CONNECTED, DATA_STALE,
    DISCONNECTED, RECONNECT_WAIT,
}

data class TrackingTelemetry(
    val status: TrackingStatus,
    val speedKmh: Double?,
    val sessionDistanceKm: Double,
    val accumulatedKm: Double,
    val detail: String,
    val retryAttempt: Int = 0,
    val nextRetryMs: Long = 0,
)
