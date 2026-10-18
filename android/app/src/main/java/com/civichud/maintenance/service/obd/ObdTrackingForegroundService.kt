package com.civichud.maintenance.service.obd

import android.app.*
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import com.civichud.maintenance.*
import com.civichud.maintenance.domain.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlin.math.min

class ObdTrackingForegroundService:Service() {
 private val scope=CoroutineScope(SupervisorJob()+Dispatchers.Main.immediate)
 private val graph get()=(application as TrackerApplication).graph
 private val controls=Mutex()
 private var worker:Job?=null
 private var watchdog:Job?=null
 private var alerts:Job?=null
 private var transport:ObdTransport?=null
 private var wakeLock:PowerManager.WakeLock?=null
 override fun onBind(intent:Intent?):IBinder?=null
 override fun onCreate() {
  super.onCreate()
  // Construct channels before startForeground; never launch location FGS for optional GPS-on-tap.
  graph.notifier
  ServiceCompat.startForeground(this,42,notification(graph.session.telemetry.value),ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
  scope.launch { graph.session.telemetry.collectLatest { getSystemService(NotificationManager::class.java).notify(42,notification(it)) } }
 }
 override fun onStartCommand(intent:Intent?,flags:Int,startId:Int):Int {
  if(intent?.action==ACTION_STOP) {
   scope.launch { controls.withLock { stopTracking();getSharedPreferences("tracker",MODE_PRIVATE).edit().putBoolean("trackingWanted",false).apply();stopForeground(STOP_FOREGROUND_REMOVE);stopSelf(startId) } }
   return START_NOT_STICKY
  }
  val prefs=getSharedPreferences("tracker",MODE_PRIVATE)
  val simulation=intent?.getBooleanExtra("simulation",false) ?: (BuildConfig.DEBUG && prefs.getBoolean("simulation",false))
  val address=intent?.getStringExtra("address") ?: prefs.getString("adapter","").orEmpty()
  if(intent==null && !prefs.getBoolean("trackingWanted",false)) {stopSelf(startId);return START_NOT_STICKY}
  scope.launch { controls.withLock {
   graph.ready.await()
   stopTracking()
   try {
    graph.selectMode(simulation)
    if(!graph.store.value.currentVehicle().isCalibrated) {
     graph.message.value="ตั้งเลขไมล์หน้าปัดก่อนเริ่ม";stopForeground(STOP_FOREGROUND_REMOVE);stopSelf(startId);return@launch
    }
    if(!simulation) graph.store.value.saveAdapter(address)
    prefs.edit().putBoolean("trackingWanted",true).putString("adapter",address).apply()
    graph.serviceRunning=true
    val selected=if(simulation)graph.simulationTransport else BluetoothClassicObdTransport(this@ObdTrackingForegroundService,address)
    transport=selected
    wakeLock=getSystemService(PowerManager::class.java).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,"CivicHUD:ObdTracking").apply { setReferenceCounted(false);acquire(600000) }
    watchdog=scope.launch { var renewed=android.os.SystemClock.elapsedRealtime();while(isActive) {
     graph.session.tick();delay(250)
     if(android.os.SystemClock.elapsedRealtime()-renewed>300000) { wakeLock?.acquire(600000);renewed=android.os.SystemClock.elapsedRealtime() }
    } }
    alerts=scope.launch {while(isActive){
     runCatching { val v=graph.store.value.currentVehicle();graph.notifier.evaluate(graph.store.value,v.baseOdometerKm+graph.session.telemetry.value.accumulatedKm) }
     delay(15000)
    } }
    worker=scope.launch { runConnection(selected) }
   } catch(e:Exception) {
    if(e is CancellationException) throw e
    graph.message.value=e.message?:"เริ่มติดตามไม่สำเร็จ";stopTracking();stopForeground(STOP_FOREGROUND_REMOVE);stopSelf(startId)
   }
  } }
  return START_STICKY
 }
 private suspend fun runConnection(selected:ObdTransport) {
  var failures=0
  while(currentCoroutineContext().isActive) {
   var consecutiveValid=0
   try {
    graph.session.status(TrackingStatus.SCANNING,"ค้นหาอุปกรณ์ที่เลือก")
    graph.session.status(TrackingStatus.CONNECTING,"กำลังเชื่อม Bluetooth")
    selected.connect()
    graph.session.status(TrackingStatus.INITIALIZING,"ตั้งค่า ELM327")
    val protocol=Elm327ProtocolHandler(selected);protocol.initialize()
    graph.session.status(TrackingStatus.DATA_STALE,"รอความเร็วจาก ECU")
    while(currentCoroutineContext().isActive) {
     val reply=protocol.readSpeed()
     graph.session.frame(reply.raw,reply.receivedAtMs)
     if(Elm327ResponseParser.parseSpeedKmh(reply.raw)==null) throw java.io.IOException("ELM: ${reply.raw.take(60)}")
     consecutiveValid++
     if(consecutiveValid>=5) failures=0
     delay(250)
    }
   } catch(e:Exception) {
    if(e is CancellationException) throw e
    graph.session.status(TrackingStatus.DISCONNECTED,e.message?:"การเชื่อมต่อขาด")
    selected.close()
    val retryMs=min(30000L,1000L shl min(failures,5));failures++
    graph.session.status(TrackingStatus.RECONNECT_WAIT,"รอเชื่อมใหม่ ${retryMs/1000} วินาที",failures,retryMs)
    delay(retryMs)
   }
  }
 }
 private suspend fun stopTracking() {
  worker?.cancel();transport?.close();worker?.join();worker=null;transport=null
  watchdog?.cancelAndJoin();watchdog=null;alerts?.cancelAndJoin();alerts=null
  graph.session.status(TrackingStatus.STOPPED,"หยุดติดตาม")
  graph.session.flush();graph.serviceRunning=false
  wakeLock?.let { if(it.isHeld)it.release() };wakeLock=null
 }
 private fun notification(t:TrackingTelemetry):Notification {
  val open=PendingIntent.getActivity(this,0,Intent(this,MainActivity::class.java),PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
  val stop=PendingIntent.getService(this,1,Intent(this,javaClass).setAction(ACTION_STOP),PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
  val title=when(t.status){TrackingStatus.CONNECTED->"OBD เชื่อมต่อ · ข้อมูลสด";TrackingStatus.DATA_STALE->"OBD ข้อมูลขาด · หยุดนับระยะ";TrackingStatus.STOPPED->"หยุดติดตาม";TrackingStatus.RECONNECT_WAIT->"รอเชื่อม OBD ใหม่";else->"กำลังเชื่อม OBD"}
  val distance="%.3f".format(t.sessionDistanceKm)
  return NotificationCompat.Builder(this,"tracking").setSmallIcon(android.R.drawable.ic_menu_compass).setContentTitle(title)
   .setContentText("${t.speedKmh?.let{"%.0f กม./ชม.".format(it)}?:"ไม่มีความเร็วสด"} · เที่ยวนี้ $distance กม. · ${t.detail}")
   .setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true).addAction(0,"หยุด",stop).build()
 }
 override fun onDestroy() {
  worker?.cancel();watchdog?.cancel();alerts?.cancel()
  // Graceful Stop already checkpoints. Unexpected process death is bounded by periodic checkpoints.
  val current=transport
  scope.launch { current?.close();graph.session.flush();scope.cancel() }
  wakeLock?.let {if(it.isHeld)it.release()};graph.serviceRunning=false
  super.onDestroy()
 }
 companion object {const val ACTION_STOP="com.civichud.maintenance.STOP_TRACKING"}
}
