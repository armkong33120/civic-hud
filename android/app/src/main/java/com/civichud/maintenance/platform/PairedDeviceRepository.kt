package com.civichud.maintenance.platform
import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import androidx.core.content.ContextCompat
import com.civichud.maintenance.ui.PairedObdDevice
class PairedDeviceRepository(private val context:Context) {
 @SuppressLint("MissingPermission")
 fun list():List<PairedObdDevice> {
  if(android.os.Build.VERSION.SDK_INT>=31 && ContextCompat.checkSelfPermission(context,Manifest.permission.BLUETOOTH_CONNECT)!=PackageManager.PERMISSION_GRANTED)return emptyList()
  return context.getSystemService(BluetoothManager::class.java)?.adapter?.bondedDevices.orEmpty().map {PairedObdDevice(it.name?:"อุปกรณ์ Bluetooth",it.address)}.sortedBy {it.name}
 }
}
