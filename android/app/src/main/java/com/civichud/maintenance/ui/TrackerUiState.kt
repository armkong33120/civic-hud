package com.civichud.maintenance.ui

import com.civichud.maintenance.domain.*

data class PairedObdDevice(val name: String, val address: String)
data class CapturedLocation(val latitude: Double, val longitude: Double, val address: String)

data class TrackerUiState(
    val vehicle: Vehicle? = null,
    val rules: List<MaintenanceResult> = emptyList(),
    val history: List<ServiceLog> = emptyList(),
    val telemetry: TrackingTelemetry = TrackingTelemetry(
        TrackingStatus.STOPPED, null, 0.0, 0.0, ""
    ),
    val simulation: Boolean = false,
    val devices: List<PairedObdDevice> = emptyList(),
    val message: String = "",
    val gpsBusy: Boolean = false,
    val gpsLocation: CapturedLocation? = null,
)

data class TrackerActions(
    val onRefreshDevices: () -> Unit,
    val onConnect: (String) -> Unit,
    val onStartSimulation: () -> Unit,
    val onStop: () -> Unit,
    val onCalibrate: (Double) -> Unit,
    val onSaveRule: (MaintenanceRule) -> Unit,
    val onDeleteRule: (Long) -> Unit,
    val onLogService: (Long, ServiceDraft) -> Unit,
    val onCaptureLocation: () -> Unit,
    val onClearMessage: () -> Unit,
)
