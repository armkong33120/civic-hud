package com.civichud.maintenance.data

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Update
import kotlinx.coroutines.flow.Flow

@Dao
internal interface TrackerDao {
    @Query("SELECT * FROM vehicles WHERE id = 1")
    fun observeVehicle(): Flow<VehicleEntity?>

    @Query("SELECT * FROM maintenance_rules ORDER BY id ASC")
    fun observeRules(): Flow<List<RuleEntity>>

    @Query("SELECT * FROM service_logs ORDER BY performedAt DESC, id DESC")
    fun observeHistory(): Flow<List<ServiceLogEntity>>

    @Query("SELECT * FROM feedback_outbox ORDER BY createdAt DESC, id DESC")
    fun observeFeedback(): Flow<List<FeedbackEntity>>

    @Query("SELECT * FROM feedback_outbox WHERE status = 'PENDING' ORDER BY createdAt ASC, id ASC")
    suspend fun pendingFeedback(): List<FeedbackEntity>

    @Query("SELECT * FROM feedback_outbox WHERE id = :id")
    suspend fun feedback(id: Long): FeedbackEntity?

    @Insert(onConflict = OnConflictStrategy.ABORT)
    suspend fun insertFeedback(entry: FeedbackEntity): Long

    @Update
    suspend fun updateFeedback(entry: FeedbackEntity)

    @Query("DELETE FROM feedback_outbox WHERE id = :id")
    suspend fun deleteFeedback(id: Long)

    @Query("SELECT * FROM vehicles WHERE id = 1")
    suspend fun vehicle(): VehicleEntity?

    @Query("SELECT * FROM maintenance_rules WHERE id = :id")
    suspend fun rule(id: Long): RuleEntity?

    @Query("SELECT * FROM maintenance_rules WHERE baselinePendingCalibration = 1")
    suspend fun pendingBaselines(): List<RuleEntity>

    @Insert(onConflict = OnConflictStrategy.ABORT)
    suspend fun insertVehicle(vehicle: VehicleEntity)

    @Update
    suspend fun updateVehicle(vehicle: VehicleEntity)

    @Insert(onConflict = OnConflictStrategy.ABORT)
    suspend fun insertRule(rule: RuleEntity): Long

    @Update
    suspend fun updateRule(rule: RuleEntity)

    @Query("DELETE FROM maintenance_rules WHERE id = :id")
    suspend fun deleteRule(id: Long)

    @Insert(onConflict = OnConflictStrategy.ABORT)
    suspend fun insertLog(log: ServiceLogEntity): Long

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun reserveNotification(reservation: NotificationReservationEntity): Long
}
