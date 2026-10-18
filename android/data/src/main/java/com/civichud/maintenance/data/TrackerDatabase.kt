package com.civichud.maintenance.data

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.TypeConverter
import androidx.room.TypeConverters
import java.time.LocalDate
import java.util.UUID

internal class TrackerConverters {
    @TypeConverter fun dateToEpochDay(value: LocalDate): Long = value.toEpochDay()
    @TypeConverter fun epochDayToDate(value: Long): LocalDate = LocalDate.ofEpochDay(value)
    @TypeConverter fun uuidToString(value: UUID): String = value.toString()
    @TypeConverter fun stringToUuid(value: String): UUID = UUID.fromString(value)
}

@Database(
    entities = [VehicleEntity::class, RuleEntity::class, ServiceLogEntity::class,
        NotificationReservationEntity::class, FeedbackEntity::class],
    version = 2,
    exportSchema = false,
)
@TypeConverters(TrackerConverters::class)
internal abstract class TrackerDatabase : RoomDatabase() {
    abstract fun trackerDao(): TrackerDao

    companion object {
        private val instances = mutableMapOf<String, TrackerDatabase>()

        fun get(context: Context, simulation: Boolean): TrackerDatabase = synchronized(instances) {
            val appContext = context.applicationContext
            val filename = if (simulation) "vehicle_tracker_simulation.db" else "vehicle_tracker.db"
            val key = appContext.getDatabasePath(filename).absolutePath
            instances.getOrPut(key) {
                Room.databaseBuilder(appContext, TrackerDatabase::class.java, filename)
                    .addMigrations(MIGRATION_1_2).build()
            }
        }
    }
}
