package com.civichud.maintenance.service

import android.Manifest
import android.app.*
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.civichud.maintenance.MainActivity
import com.civichud.maintenance.data.TrackerStore
import com.civichud.maintenance.domain.*
import kotlinx.coroutines.flow.first
import java.time.LocalDate

class MaintenanceNotifier(private val context:Context) {
 init {
  val manager=context.getSystemService(NotificationManager::class.java)
  manager.createNotificationChannel(NotificationChannel("tracking","ติดตาม OBD",NotificationManager.IMPORTANCE_LOW))
  manager.createNotificationChannel(NotificationChannel("maintenance","เตือนบำรุงรักษา",NotificationManager.IMPORTANCE_DEFAULT))
 }
 private fun canNotify()=android.os.Build.VERSION.SDK_INT<33 || ContextCompat.checkSelfPermission(context,Manifest.permission.POST_NOTIFICATIONS)==PackageManager.PERMISSION_GRANTED
 suspend fun evaluate(store:TrackerStore,odometer:Double?=null) {
  if(!canNotify() || !NotificationManagerCompat.from(context).areNotificationsEnabled() || context.getSystemService(NotificationManager::class.java).getNotificationChannel("maintenance")?.importance==NotificationManager.IMPORTANCE_NONE) return
  val vehicle=store.currentVehicle();if(!vehicle.isCalibrated) return
  for(rule in store.rules.first()) {
   val result=MaintenanceCalculator.evaluate(rule,odometer?:vehicle.estimatedOdometerKm,LocalDate.now())
   if(result.status==MaintenanceStatus.NORMAL) continue
   if(!store.markNotified(rule.id,rule.cycle,result.status)) continue
   val body=listOfNotNull(result.remainingKm?.let { "เหลือ %.2f กม.".format(it) },result.remainingDays?.let { "เหลือ $it วัน" }).joinToString(" · ")
   val pending=PendingIntent.getActivity(context,0,Intent(context,MainActivity::class.java),PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
   val notification=NotificationCompat.Builder(context,"maintenance").setSmallIcon(android.R.drawable.ic_dialog_info)
    .setContentTitle((if(store.simulation)"ทดสอบ · " else "")+(if(result.status==MaintenanceStatus.OVERDUE) "ถึง/เกินกำหนด: " else "ใกล้กำหนด: ")+rule.name)
    .setContentText(body).setContentIntent(pending).setAutoCancel(true).build()
   if(canNotify()) NotificationManagerCompat.from(context).notify((if(store.simulation)100000 else 1000)+rule.id.toInt(),notification)
  }
 }
}
