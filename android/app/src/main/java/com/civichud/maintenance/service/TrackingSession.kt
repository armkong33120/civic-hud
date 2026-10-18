package com.civichud.maintenance.service

import android.os.SystemClock
import android.util.Log
import com.civichud.maintenance.data.TrackerStore
import com.civichud.maintenance.domain.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONObject

/** Single serialization point for live odometer, checkpointing and calibration. */
class TrackingSession(private var store: TrackerStore) {
 private val mutex=Mutex()
 var initialized=false; private set
 val currentStore get()=store
 private var integrator=SpeedDistanceIntegrator()
 private var calibrated=false
 private var lastSavedKm=0.0
 private var lastSavedAt=SystemClock.elapsedRealtime()
 private val mutable=MutableStateFlow(TrackingTelemetry(TrackingStatus.STOPPED,null,0.0,0.0,"ยังไม่ได้เริ่ม"))
 val telemetry=mutable.asStateFlow()
 suspend fun prepare(nextStore:TrackerStore) = mutex.withLock {
  flushLocked(true)
  nextStore.initialize()
  val vehicle=nextStore.currentVehicle()
  store=nextStore;initialized=true
  integrator=SpeedDistanceIntegrator(vehicle.accumulatedKm); calibrated=vehicle.isCalibrated
  lastSavedKm=vehicle.accumulatedKm;lastSavedAt=SystemClock.elapsedRealtime()
  mutable.value=TrackingTelemetry(TrackingStatus.STOPPED,null,0.0,vehicle.accumulatedKm,"พร้อมเริ่ม")
 }
 suspend fun frame(raw:String,atMs:Long) = mutex.withLock {
  val speed=Elm327ResponseParser.parseSpeedKmh(raw)
  val previous=mutable.value.status
  val now=SystemClock.elapsedRealtime()
  val fresh=atMs>=0 && atMs<=now && now-atMs<=3000
  val result=integrator.acceptSpeed(if(calibrated && fresh) speed else null,if(fresh)atMs else now)
  val valid=speed!=null && calibrated && fresh && !result.isStale
  if(!valid) integrator.resetEndpoint()
  val status=if(valid) TrackingStatus.CONNECTED else TrackingStatus.DATA_STALE
  mutable.value=TrackingTelemetry(status,if(valid)speed else null,result.sessionDistanceKm,result.accumulatedKm,
   if(!calibrated) "ตั้งเลขไมล์หน้าปัดก่อนเริ่ม" else if(speed==null) "ข้อมูลขาด: ${raw.take(60)}" else if(!valid) "ข้อมูลเกิน 3 วินาที" else "รับความเร็วจาก ECU แล้ว")
  flushLocked(previous!=status)
  trace("FRAME", if(valid)result.deltaKm else 0.0)
 }
 suspend fun tick() = mutex.withLock {
  val before=mutable.value
  val result=integrator.tick(SystemClock.elapsedRealtime())
  if(before.status==TrackingStatus.CONNECTED && result.isStale) {
   mutable.value=before.copy(status=TrackingStatus.DATA_STALE,speedKmh=null,detail="ข้อมูลขาดเกิน 3 วินาที · หยุดนับระยะ")
   flushLocked(true);trace("STALE",0.0)
  } else flushLocked(false)
 }
 suspend fun status(status:TrackingStatus,detail:String,retry:Int=0,nextRetryMs:Long=0) = mutex.withLock {
  integrator.resetEndpoint()
  val old=mutable.value
  mutable.value=old.copy(status=status,speedKmh=null,detail=detail,retryAttempt=retry,nextRetryMs=nextRetryMs)
  flushLocked(old.status!=status);trace("STATUS",0.0)
 }
 suspend fun calibrate(actual:Double) = mutex.withLock {
  require(actual.isFinite() && actual>0 && actual<=10000000) { "เลขไมล์ไม่ถูกต้อง" }
  flushLocked(true);store.calibrate(actual);integrator.resetAccumulated(0.0);calibrated=true
  lastSavedKm=0.0;lastSavedAt=SystemClock.elapsedRealtime()
  mutable.value=mutable.value.copy(speedKmh=null,accumulatedKm=0.0,sessionDistanceKm=0.0,detail="ตั้งเลขไมล์แล้ว")
  trace("CALIBRATE",0.0)
 }
 suspend fun logService(id:Long,draft:ServiceDraft) = mutex.withLock {
  flushLocked(true)
  val vehicle=store.currentVehicle()
  store.logService(id,vehicle.baseOdometerKm+integrator.accumulatedKm,draft)
 }
 suspend fun flush() = mutex.withLock { flushLocked(true) }
 private suspend fun flushLocked(force:Boolean) {
  val now=SystemClock.elapsedRealtime();val total=integrator.accumulatedKm
  if(calibrated && (force || total-lastSavedKm>=0.5 || now-lastSavedAt>=60000)) {
   store.checkpoint(total);lastSavedKm=total;lastSavedAt=now
   Log.i("CivicTracker",JSONObject().put("event","CHECKPOINT").put("accumulatedKm",total).toString())
  }
 }
 private fun trace(event:String,delta:Double) {
  val t=mutable.value
  Log.i("CivicTracker",JSONObject().put("event",event).put("status",t.status.name).put("deltaKm",delta)
   .put("accumulatedKm",t.accumulatedKm).put("sessionKm",t.sessionDistanceKm).put("speedKmh",t.speedKmh)
   .put("retry",t.retryAttempt).put("retryDelayMs",t.nextRetryMs).toString())
 }
}
