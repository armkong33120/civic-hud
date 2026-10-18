package com.civichud.maintenance.service.obd

import kotlinx.coroutines.delay
import java.io.IOException

class Elm327ProtocolHandler(private val transport: ObdTransport) {
 suspend fun initialize() {
  for(command in listOf("ATZ", "ATE0", "ATL0", "ATH0", "ATSP0")) {
   val reply=transport.command(command, if(command=="ATZ") 6000 else 3500).raw.trim()
   if(reply.isEmpty() || listOf("?","NO DATA","UNABLE","ERROR").any { reply.contains(it,true) }) throw IOException("ELM327 $command failed")
   if(command=="ATZ") {
    if(reply.isEmpty() || reply.contains("UNABLE",true)) throw IOException("ELM327 reset failed")
    delay(250)
   } else if(!reply.contains("OK",true)) throw IOException("ELM327 $command rejected: ${reply.take(80)}")
  }
 }
 suspend fun readSpeed() = transport.command("010D", 3500)
}
