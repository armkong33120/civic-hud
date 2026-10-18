package com.civichud.maintenance.service

import android.content.Context
import androidx.work.*
import com.civichud.maintenance.data.TrackerStore
import java.util.concurrent.TimeUnit

class MaintenanceWorker(context:Context,parameters:WorkerParameters):CoroutineWorker(context,parameters) {
 override suspend fun doWork():Result = try {
  // Date reminders always use real vehicle data; simulation is confined to debug sessions.
  val store=TrackerStore(applicationContext,false);store.initialize()
  MaintenanceNotifier(applicationContext).evaluate(store);Result.success()
 } catch(_:Exception) { Result.retry() }
 companion object {
  fun schedule(context:Context) {
   WorkManager.getInstance(context).enqueueUniquePeriodicWork("maintenance-date-check",ExistingPeriodicWorkPolicy.KEEP,
    PeriodicWorkRequestBuilder<MaintenanceWorker>(12,TimeUnit.HOURS).build())
  }
 }
}
