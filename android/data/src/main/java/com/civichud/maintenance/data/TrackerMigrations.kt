package com.civichud.maintenance.data

import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase

/** Additive upgrade: never rewrite mileage, calibration, profile name or service records. */
internal val MIGRATION_1_2_SQL = listOf(
    "ALTER TABLE vehicles ADD COLUMN manufacturer TEXT NOT NULL DEFAULT ''",
    "ALTER TABLE vehicles ADD COLUMN model TEXT NOT NULL DEFAULT ''",
    "ALTER TABLE vehicles ADD COLUMN modelYear INTEGER DEFAULT NULL",
    "ALTER TABLE vehicles ADD COLUMN registration TEXT NOT NULL DEFAULT ''",
    "ALTER TABLE vehicles ADD COLUMN fuelType TEXT NOT NULL DEFAULT ''",
    "CREATE TABLE IF NOT EXISTS feedback_outbox (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, category TEXT NOT NULL, title TEXT NOT NULL, message TEXT NOT NULL, replyEmail TEXT NOT NULL, includeDiagnostics INTEGER NOT NULL, createdAt INTEGER NOT NULL, status TEXT NOT NULL, receiptId TEXT NOT NULL, syncId TEXT NOT NULL, updatedAt INTEGER NOT NULL, isDirty INTEGER NOT NULL)",
    "CREATE INDEX IF NOT EXISTS index_feedback_outbox_status_createdAt ON feedback_outbox (status, createdAt)",
)

internal val MIGRATION_1_2: Migration = object : Migration(1, 2) {
    override fun migrate(db: SupportSQLiteDatabase) {
        MIGRATION_1_2_SQL.forEach(db::execSQL)
    }
}
