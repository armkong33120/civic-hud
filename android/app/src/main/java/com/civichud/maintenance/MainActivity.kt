package com.civichud.maintenance

import android.Manifest
import android.os.Build
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.civichud.maintenance.presentation.TrackerViewModel
import com.civichud.maintenance.ui.*

class MainActivity:ComponentActivity() {
 private val model:TrackerViewModel by viewModels()
 private var pendingBluetooth:(()->Unit)?=null
 private val bluetoothPermission=registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { grants->
  if(grants.values.all{it}) pendingBluetooth?.invoke()
  else (application as TrackerApplication).graph.message.value="ต้องอนุญาต Bluetooth เพื่อเชื่อม OBD2"
  pendingBluetooth=null
 }
 private val notificationPermission=registerForActivityResult(ActivityResultContracts.RequestPermission()) {}
 private val locationPermission=registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()){grants->
  if(grants.values.any{it})model.captureLocation() else (application as TrackerApplication).graph.message.value="ไม่อนุญาตตำแหน่ง กรอกสถานที่เองได้"
 }
 private fun withBluetooth(action:()->Unit) {
  if(Build.VERSION.SDK_INT>=31 && (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)!=android.content.pm.PackageManager.PERMISSION_GRANTED || checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)!=android.content.pm.PackageManager.PERMISSION_GRANTED)) {
   pendingBluetooth=action;bluetoothPermission.launch(arrayOf(Manifest.permission.BLUETOOTH_CONNECT,Manifest.permission.BLUETOOTH_SCAN))
  }else action()
 }
 override fun onCreate(savedInstanceState:Bundle?) {
  super.onCreate(savedInstanceState)
  window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
  if(Build.VERSION.SDK_INT>=33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=android.content.pm.PackageManager.PERMISSION_GRANTED)notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
  setContent {
   val state=model.state.collectAsStateWithLifecycle().value
   TrackerScreen(state,TrackerActions(
    onRefreshDevices={withBluetooth{model.refreshDevices()}},onConnect={address->withBluetooth{model.connect(address)}},
    onStartSimulation={withBluetooth{model.startSimulation()}},onStop=model::stop,onCalibrate=model::calibrate,
    onSaveRule=model::saveRule,onDeleteRule=model::deleteRule,onLogService=model::logService,
    onCaptureLocation={
     if(checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION)==android.content.pm.PackageManager.PERMISSION_GRANTED)model.captureLocation()
     else locationPermission.launch(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION,Manifest.permission.ACCESS_COARSE_LOCATION))
    },onClearMessage=model::clearMessage
   ))
  }
 }
}
