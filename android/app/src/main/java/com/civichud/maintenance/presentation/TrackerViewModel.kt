package com.civichud.maintenance.presentation

import android.app.Application
import android.content.Intent
import androidx.core.content.ContextCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.civichud.maintenance.*
import com.civichud.maintenance.domain.*
import com.civichud.maintenance.platform.LocationCapture
import com.civichud.maintenance.platform.PairedDeviceRepository
import com.civichud.maintenance.service.obd.ObdTrackingForegroundService
import com.civichud.maintenance.ui.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import java.time.LocalDate

@OptIn(ExperimentalCoroutinesApi::class)
class TrackerViewModel(application:Application):AndroidViewModel(application) {
 private val graph=(application as TrackerApplication).graph
 private val devices=MutableStateFlow<List<PairedObdDevice>>(emptyList())
 private val gpsBusy=MutableStateFlow(false)
 private val gpsLocation=MutableStateFlow<CapturedLocation?>(null)
 private data class Stored(val source:com.civichud.maintenance.data.TrackerStore,val vehicle:Vehicle?,val rules:List<MaintenanceRule>,val history:List<ServiceLog>)
 private val stored=graph.store.flatMapLatest { store -> combine(store.vehicle,store.rules,store.history){v,r,h->Stored(store,v,r,h)} }
 private val extra=combine(devices,graph.message,gpsBusy,gpsLocation){d,m,b,l->TrackerUiState(devices=d,message=m,gpsBusy=b,gpsLocation=l)}
 val state=combine(stored,graph.session.telemetry,graph.simulation,extra){s,t,sim,extras->
  val liveVehicle=if(s.source!==graph.session.currentStore)null else if(!graph.session.initialized)s.vehicle else s.vehicle?.copy(accumulatedKm=t.accumulatedKm)
  extras.copy(vehicle=liveVehicle,rules=s.rules.map {MaintenanceCalculator.evaluate(it,liveVehicle?.estimatedOdometerKm?:0.0,LocalDate.now())},history=s.history,telemetry=t,simulation=sim)
 }.stateIn(viewModelScope,SharingStarted.WhileSubscribed(5000),TrackerUiState())
 private fun act(block:suspend ()->Unit){viewModelScope.launch {try{graph.ready.await();block()}catch(e:Exception){if(e is CancellationException)throw e;graph.message.value=e.message?:"ทำรายการไม่สำเร็จ"}}}
 fun refreshDevices(){act {devices.value=PairedDeviceRepository(getApplication()).list();if(devices.value.isEmpty())graph.message.value="ยังไม่มีอุปกรณ์ที่จับคู่ ให้จับคู่ OBD2 ใน Bluetooth ของมือถือก่อน"}}
 fun connect(address:String){startService(false,address)}
 fun startSimulation(){if(BuildConfig.DEBUG)startService(true,"")}
 private fun startService(simulation:Boolean,address:String){act {
  ContextCompat.startForegroundService(getApplication(),Intent(getApplication(),ObdTrackingForegroundService::class.java).putExtra("simulation",simulation).putExtra("address",address))
 }}
 fun stop(){act {if(graph.serviceRunning)getApplication<Application>().startService(Intent(getApplication(),ObdTrackingForegroundService::class.java).setAction(ObdTrackingForegroundService.ACTION_STOP))}}
 fun calibrate(value:Double){act{graph.session.calibrate(value);graph.notifier.evaluate(graph.store.value);graph.message.value="บันทึกเลขไมล์แล้ว"}}
 fun saveRule(rule:MaintenanceRule){act{graph.store.value.saveRule(rule);graph.message.value="บันทึกรอบแล้ว";graph.notifier.evaluate(graph.store.value,state.value.vehicle?.estimatedOdometerKm)}}
 fun deleteRule(id:Long){act{graph.store.value.deleteRule(id)}}
 fun logService(id:Long,draft:ServiceDraft){act {
  graph.session.logService(id,draft);graph.message.value="บันทึกการเปลี่ยนถ่ายแล้ว"
 }}
 fun captureLocation(){act {gpsLocation.value=null;gpsBusy.value=true;try{gpsLocation.value=LocationCapture(getApplication()).capture()}finally{gpsBusy.value=false}}}
 fun clearMessage(){graph.message.value=""}
}
