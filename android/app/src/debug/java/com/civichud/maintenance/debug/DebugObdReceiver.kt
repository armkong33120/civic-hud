package com.civichud.maintenance.debug

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import androidx.core.content.ContextCompat
import com.civichud.maintenance.TrackerApplication
import com.civichud.maintenance.domain.*
import com.civichud.maintenance.service.obd.ObdTrackingForegroundService
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.first
import org.json.JSONObject

/** Debug manifest requires DUMP: ADB shell can inject, ordinary apps cannot. Absent from release. */
class DebugObdReceiver:BroadcastReceiver() {
 override fun onReceive(context:Context,intent:Intent) {
  if(intent.action!="com.civichud.maintenance.DEBUG_OBD_INJECT")return
  val pending=goAsync();val graph=(context.applicationContext as TrackerApplication).graph
  graph.scope.launch {
   try {
    graph.ready.await()
    when(val command=intent.getStringExtra("command")?:"FRAME") {
     "MODE"->{check(!graph.serviceRunning);graph.selectMode(true)}
     "REAL_MODE"->{check(!graph.serviceRunning);graph.selectMode(false);androidx.core.app.NotificationManagerCompat.from(context).cancelAll()}
     "CHECK_ALERTS"->{check(graph.simulation.value);val v=graph.store.value.currentVehicle();graph.notifier.evaluate(graph.store.value,v.baseOdometerKm+graph.session.telemetry.value.accumulatedKm)}
     "START"->ContextCompat.startForegroundService(context,Intent(context,ObdTrackingForegroundService::class.java).putExtra("simulation",true))
     "STOP"->context.startService(Intent(context,ObdTrackingForegroundService::class.java).setAction(ObdTrackingForegroundService.ACTION_STOP))
     "CALIBRATE"->{check(graph.simulation.value);graph.session.calibrate(intent.getStringExtra("odometerKm")?.toDoubleOrNull()?:0.0)}
     "FRAME"->{check(graph.simulation.value);graph.simulationTransport.inject(intent.getStringExtra("raw").orEmpty())}
     "FLUSH"->{check(graph.simulation.value);graph.session.flush()}
     "STATE"->{
      val store=graph.store.value;val v=store.currentVehicle();val t=graph.session.telemetry.value
      val rules=store.rules.first().map {MaintenanceCalculator.evaluate(it,v.baseOdometerKm+t.accumulatedKm,java.time.LocalDate.now())}
      val record=JSONObject().put("event","DEBUG_STATE").put("simulation",graph.simulation.value).put("status",t.status.name)
       .put("estimatedOdometerKm",v.baseOdometerKm+t.accumulatedKm).put("persistedOdometerKm",v.estimatedOdometerKm)
       .put("accumulatedKm",t.accumulatedKm).put("sessionKm",t.sessionDistanceKm).put("historyCount",store.history.first().size)
       .put("firstRemainingKm",rules.firstOrNull()?.remainingKm).put("firstStatus",rules.firstOrNull()?.status?.name)
       .put("retry",t.retryAttempt).put("retryDelayMs",t.nextRetryMs)
      Log.i("CivicTracker",record.toString())
     }
     else->error("Unknown debug command $command")
    }
   }catch(e:Exception){Log.e("CivicTracker","DEBUG_ERROR: ${e.message}")}
   finally{pending.finish()}
  }
 }
}
