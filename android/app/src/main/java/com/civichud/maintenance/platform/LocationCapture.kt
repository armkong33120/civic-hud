package com.civichud.maintenance.platform

import android.annotation.SuppressLint
import android.content.Context
import android.location.Geocoder
import android.location.LocationManager
import android.os.CancellationSignal
import com.civichud.maintenance.ui.CapturedLocation
import kotlinx.coroutines.*
import java.util.Locale
import kotlin.coroutines.resume

class LocationCapture(private val context:Context) {
 @SuppressLint("MissingPermission")
 suspend fun capture():CapturedLocation {
  val manager=context.getSystemService(LocationManager::class.java)
  val provider=listOf(LocationManager.GPS_PROVIDER,LocationManager.NETWORK_PROVIDER).firstOrNull { manager.isProviderEnabled(it) }
   ?: error("เปิดตำแหน่งบนมือถือ หรือกรอกสถานที่เอง")
  val location=withTimeoutOrNull(15000) {
   suspendCancellableCoroutine<android.location.Location?> { continuation ->
    val signal=CancellationSignal()
    if(android.os.Build.VERSION.SDK_INT>=30) {
     continuation.invokeOnCancellation {signal.cancel()}
     manager.getCurrentLocation(provider,signal,context.mainExecutor) { location -> if(continuation.isActive)continuation.resume(location) }
    } else {
     val listener=object:android.location.LocationListener {
      override fun onLocationChanged(location:android.location.Location) {manager.removeUpdates(this);if(continuation.isActive)continuation.resume(location)}
      @Suppress("OVERRIDE_DEPRECATION") override fun onStatusChanged(provider:String?,status:Int,extras:android.os.Bundle?) {}
      override fun onProviderEnabled(provider:String) {}
      override fun onProviderDisabled(provider:String) {}
     }
     continuation.invokeOnCancellation {manager.removeUpdates(listener)}
     @Suppress("DEPRECATION")
     manager.requestSingleUpdate(provider,listener,android.os.Looper.getMainLooper())
    }
   }
  } ?: error("ยังหาตำแหน่งไม่ได้ภายใน 15 วินาที กรอกสถานที่เองได้")
  @Suppress("DEPRECATION")
  val address=withContext(Dispatchers.IO) { withTimeoutOrNull(4000) { runCatching { Geocoder(context,Locale("th")).getFromLocation(location.latitude,location.longitude,1)?.firstOrNull()?.getAddressLine(0) }.getOrNull() } }.orEmpty()
  return CapturedLocation(location.latitude,location.longitude,address)
 }
}
