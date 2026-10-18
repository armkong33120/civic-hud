package com.civichud.maintenance

import android.app.Application
import android.content.Context
import com.civichud.maintenance.data.TrackerStore
import com.civichud.maintenance.service.TrackingSession
import com.civichud.maintenance.service.MaintenanceNotifier
import com.civichud.maintenance.service.MaintenanceWorker
import com.civichud.maintenance.service.obd.SimulationObdTransport
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class TrackerApplication:Application() {
 lateinit var graph:AppGraph; private set
 override fun onCreate(){super.onCreate();graph=AppGraph(this);MaintenanceWorker.schedule(this)}
}
class AppGraph(private val context:Context) {
 val scope=CoroutineScope(SupervisorJob()+Dispatchers.Main.immediate)
 private val preferences=context.getSharedPreferences("tracker",Context.MODE_PRIVATE)
 private val realStore=TrackerStore(context,false)
 private val simulationStore by lazy { TrackerStore(context,true) }
 private val mutableSimulation=MutableStateFlow(BuildConfig.DEBUG && preferences.getBoolean("simulation",false))
 val simulation=mutableSimulation.asStateFlow()
 val store=MutableStateFlow(if(mutableSimulation.value) simulationStore else realStore)
 val session=TrackingSession(store.value)
 val simulationTransport=SimulationObdTransport()
 val notifier=MaintenanceNotifier(context)
 val message=MutableStateFlow("")
 var serviceRunning=false
 val ready=scope.async { session.prepare(store.value) }
 suspend fun selectMode(simulation:Boolean) {
  ready.await()
  require(!simulation || BuildConfig.DEBUG)
  val target=if(simulation) simulationStore else realStore
  session.prepare(target);store.value=target;mutableSimulation.value=simulation
  preferences.edit().putBoolean("simulation",simulation).apply()
 }
}
