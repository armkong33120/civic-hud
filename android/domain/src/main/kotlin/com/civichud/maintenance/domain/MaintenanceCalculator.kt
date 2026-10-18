package com.civichud.maintenance.domain

import java.time.LocalDate
import java.time.temporal.ChronoUnit

object MaintenanceCalculator {
    /** Disabled dimensions (nonpositive interval) return null and do not influence status. */
    fun evaluate(rule: MaintenanceRule, odometerKm: Double, today: LocalDate): MaintenanceResult {
        require(odometerKm.isFinite() && odometerKm >= 0)
        val hasKm = rule.intervalKm.isFinite() && rule.intervalKm > 0
        val dueDate = if (rule.intervalMonths > 0) rule.lastServiceDate.plusMonths(rule.intervalMonths.toLong()) else null
        val remainingKm = if (hasKm) rule.lastServiceOdometerKm + rule.intervalKm - odometerKm else null
        val remainingDays = dueDate?.let { ChronoUnit.DAYS.between(today, it) }
        val kmFraction = if (hasKm) (odometerKm - rule.lastServiceOdometerKm) / rule.intervalKm else 0.0
        val dateFraction = if (dueDate != null) {
            val duration = ChronoUnit.DAYS.between(rule.lastServiceDate, dueDate)
            ChronoUnit.DAYS.between(rule.lastServiceDate, today).toDouble() / duration
        } else 0.0
        val status = when {
            (remainingKm != null && remainingKm <= 0) || (remainingDays != null && remainingDays <= 0) -> MaintenanceStatus.OVERDUE
            (remainingKm != null && remainingKm <= rule.warningKm.coerceAtLeast(0.0)) ||
                (remainingDays != null && remainingDays <= rule.warningDays.coerceAtLeast(0)) -> MaintenanceStatus.APPROACHING
            else -> MaintenanceStatus.NORMAL
        }
        return MaintenanceResult(rule, remainingKm, remainingDays, maxOf(kmFraction, dateFraction).coerceIn(0.0, 1.0), status)
    }
}

object Calibration {
    fun offsetKm(actualKm: Double, estimatedKm: Double): Double {
        require(actualKm.isFinite() && actualKm >= 0)
        require(estimatedKm.isFinite() && estimatedKm >= 0)
        return actualKm - estimatedKm
    }

    /** Calibration establishes the exact entered baseline without rewriting service history. */
    fun calibrate(vehicle: Vehicle, actualKm: Double, calibratedAt: Long): Vehicle {
        offsetKm(actualKm, vehicle.estimatedOdometerKm)
        return vehicle.copy(baseOdometerKm = actualKm, accumulatedKm = 0.0, calibratedAt = calibratedAt, isCalibrated = true)
    }
}

object ServiceCycle {
    fun reset(rule: MaintenanceRule, odometerKm: Double, date: LocalDate): MaintenanceRule {
        require(odometerKm.isFinite() && odometerKm >= 0)
        require(rule.cycle < Long.MAX_VALUE)
        return rule.copy(lastServiceOdometerKm = odometerKm, lastServiceDate = date, cycle = rule.cycle + 1)
    }
}
