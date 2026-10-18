package com.civichud.maintenance.service.obd

import android.os.SystemClock
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.withTimeoutOrNull
import java.io.IOException

data class ObdResponse(val raw: String, val receivedAtMs: Long = SystemClock.elapsedRealtime())
interface ObdTransport {
 suspend fun connect()
 suspend fun command(command: String, timeoutMs: Long = 3500): ObdResponse
 suspend fun close()
}

/** Selected only by an explicit debug action, with its own database. */
class SimulationObdTransport : ObdTransport {
 private val frames = Channel<ObdResponse>(Channel.UNLIMITED)
 @Volatile private var available = true
 fun inject(raw: String) {
  if (raw == "DISCONNECT") available = false
  if (raw == "RECONNECT") { available = true; return }
  frames.trySend(ObdResponse(raw))
 }
 override suspend fun connect() { if (!available) throw IOException("SIMULATED_LINK_DOWN") }
 override suspend fun command(command: String, timeoutMs: Long): ObdResponse {
  if (!available) throw IOException("SIMULATED_LINK_DOWN")
  if (command.startsWith("AT")) return ObdResponse(if(command == "ATZ") "ELM327 simulator" else "OK")
  val frame = withTimeoutOrNull(timeoutMs) { frames.receive() } ?: return ObdResponse("NO DATA")
  if (frame.raw == "DISCONNECT") throw IOException("SIMULATED_LINK_DOWN")
  return frame
 }
 override suspend fun close() { while (frames.tryReceive().isSuccess) { /* discard old frames on session transition */ } }
}
