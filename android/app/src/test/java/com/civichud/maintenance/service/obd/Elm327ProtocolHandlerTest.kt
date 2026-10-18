package com.civichud.maintenance.service.obd

import java.io.IOException
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class Elm327ProtocolHandlerTest {
    private val initialization = listOf("ATZ", "ATE0", "ATL0", "ATH0", "ATSP0")

    @Test fun `initialization validates commands in exact order with bounded timeouts`() = runBlocking {
        val transport = FakeTransport()
        Elm327ProtocolHandler(transport).initialize()

        assertEquals(initialization, transport.calls.map { it.first })
        assertEquals(listOf(6_000L, 3_500L, 3_500L, 3_500L, 3_500L), transport.calls.map { it.second })
        assertFalse(transport.calls.any { it.first == "010D" })
    }

    @Test fun `only speed PID is polled after initialization`() = runBlocking {
        val transport = FakeTransport()
        val protocol = Elm327ProtocolHandler(transport)
        protocol.initialize()
        repeat(3) {
            val reply = protocol.readSpeed()
            assertEquals("41 0D 3C", reply.raw)
            assertEquals(0L, reply.receivedAtMs)
        }

        assertEquals(initialization + listOf("010D", "010D", "010D"), transport.calls.map { it.first })
        assertEquals(listOf(3_500L, 3_500L, 3_500L), transport.calls.drop(5).map { it.second })
    }

    @Test fun `invalid reset replies fail before later initialization commands`() = runBlocking {
        for (badReply in listOf("?", "NO DATA", "UNABLE TO CONNECT", "", "   \r\n", "OK\rERROR")) {
            val transport = FakeTransport(overrides = mapOf("ATZ" to badReply))
            assertRejected(Elm327ProtocolHandler(transport), badReply)
            assertEquals(listOf("ATZ"), transport.calls.map { it.first })
        }
    }

    @Test fun `invalid configuration replies abort initialization immediately`() = runBlocking {
        for (badReply in listOf("?", "NO DATA", "UNABLE TO CONNECT", "", "OK\rERROR")) {
            val transport = FakeTransport(overrides = mapOf("ATE0" to badReply))
            assertRejected(Elm327ProtocolHandler(transport), badReply)
            assertEquals(listOf("ATZ", "ATE0"), transport.calls.map { it.first })
        }
    }

    @Test fun `each configuration command requires acknowledgement`() = runBlocking {
        for (command in initialization.drop(1)) {
            val transport = FakeTransport(overrides = mapOf(command to "ELM327 v2.1"))
            assertRejected(Elm327ProtocolHandler(transport), command)
            assertEquals(initialization.take(initialization.indexOf(command) + 1), transport.calls.map { it.first })
        }
    }

    @Test fun `mixed OK and error replies cannot count as success at any stage`() = runBlocking {
        for (command in initialization) {
            val transport = FakeTransport(overrides = mapOf(command to "OK\rCAN ERROR"))
            assertRejected(Elm327ProtocolHandler(transport), command)
            assertEquals(command, transport.calls.last().first)
            assertTrue(transport.calls.none { it.first == "010D" })
        }
    }

    @Test fun `speed error replies remain available to session parser unchanged`() = runBlocking {
        val transport = FakeTransport(overrides = mapOf("010D" to "NO DATA"))
        val reply = Elm327ProtocolHandler(transport).readSpeed()
        assertEquals("NO DATA", reply.raw)
        assertEquals(listOf("010D" to 3_500L), transport.calls)
    }

    private suspend fun assertRejected(protocol: Elm327ProtocolHandler, description: String) {
        try {
            protocol.initialize()
            fail("Expected initialization to reject: $description")
        } catch (_: IOException) {
            // Expected protocol failure rather than an Android stub or missing clock failure.
        }
    }

    private class FakeTransport(private val overrides: Map<String, String> = emptyMap()) : ObdTransport {
        val calls = mutableListOf<Pair<String, Long>>()
        override suspend fun connect() = Unit
        override suspend fun close() = Unit
        override suspend fun command(command: String, timeoutMs: Long): ObdResponse {
            calls += command to timeoutMs
            val raw = overrides[command] ?: when (command) {
                "ATZ" -> "ELM327 v2.1"
                "ATE0", "ATL0", "ATH0", "ATSP0" -> "OK"
                "010D" -> "41 0D 3C"
                else -> error("Unexpected command: $command")
            }
            // Explicit timestamp avoids Android SystemClock in local JVM tests.
            return ObdResponse(raw, 0L)
        }
    }
}
