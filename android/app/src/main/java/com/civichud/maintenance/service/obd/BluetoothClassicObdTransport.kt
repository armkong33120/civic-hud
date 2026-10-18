package com.civichud.maintenance.service.obd

import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.os.SystemClock
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.ensureActive
import java.io.IOException
import java.util.UUID

@SuppressLint("MissingPermission")
class BluetoothClassicObdTransport(context: Context, private val address: String) : ObdTransport {
 private val adapter = context.getSystemService(BluetoothManager::class.java)?.adapter
 @Volatile private var socket: BluetoothSocket? = null
 override suspend fun connect(): Unit = coroutineScope {
  val bt = adapter ?: throw IOException("เครื่องนี้ไม่มี Bluetooth")
  if (!bt.isEnabled) throw IOException("กรุณาเปิด Bluetooth")
  if (!android.bluetooth.BluetoothAdapter.checkBluetoothAddress(address)) throw IOException("ที่อยู่ Bluetooth ไม่ถูกต้อง")
  bt.cancelDiscovery()
  val pending = bt.getRemoteDevice(address).createRfcommSocketToServiceRecord(UUID.fromString("00001101-0000-1000-8000-00805F9B34FB"))
  socket = pending
  val timeout = launch { delay(12000); runCatching { pending.close() } }
  try { withContext(Dispatchers.IO) { pending.connect() } } finally { timeout.cancel() }
 }
 override suspend fun command(command: String, timeoutMs: Long): ObdResponse = withContext(Dispatchers.IO) {
  val current = socket ?: throw IOException("Bluetooth disconnected")
  val input = current.inputStream
  // The previous response is prompt-framed; discard unsolicited leftovers before a new request.
  val drainDeadline=SystemClock.elapsedRealtime()+250
  var drained=0
  while(input.available() > 0) {
   kotlinx.coroutines.currentCoroutineContext().ensureActive()
   if(++drained>8192 || SystemClock.elapsedRealtime()>drainDeadline)throw IOException("ELM unsolicited data overflow")
   input.read()
  }
  current.outputStream.write((command + "\r").toByteArray(Charsets.US_ASCII))
  current.outputStream.flush()
  val end = SystemClock.elapsedRealtime() + timeoutMs
  val response = StringBuilder()
  while (SystemClock.elapsedRealtime() < end) {
   if (!current.isConnected) throw IOException("Bluetooth disconnected")
   if (input.available() == 0) { delay(10); continue }
   val value = input.read()
   if(value < 0) throw IOException("Bluetooth EOF")
   if(value == '>'.code) return@withContext ObdResponse(response.toString())
   response.append(value.toChar())
   if(response.length > 8192) throw IOException("ELM response too large")
  }
  throw IOException("ELM response timed out")
 }
 override suspend fun close() { val old=socket;socket=null;withContext(Dispatchers.IO) { runCatching { old?.close() } }; }
}
