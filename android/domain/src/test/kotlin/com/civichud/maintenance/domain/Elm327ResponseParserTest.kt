package com.civichud.maintenance.domain

import org.junit.Assert.*
import org.junit.Test

class Elm327ResponseParserTest {
    @Test fun `compact and spaced frames decode speed byte`() {
        assertEquals(60.0, Elm327ResponseParser.parseSpeedKmh("410D3C>"))
        assertEquals(60.0, Elm327ResponseParser.parseSpeedKmh("41 0D 3C\r>"))
        assertEquals(255.0, Elm327ResponseParser.parseSpeedKmh("41 0d ff"))
    }

    @Test fun `echo and searching chatter supported`() {
        assertEquals(60.0, Elm327ResponseParser.parseSpeedKmh("010D\rSEARCHING...\r41 0D 3C\r>"))
        assertEquals(60.0, Elm327ResponseParser.parseSpeedKmh("01 0D\r\nSEARCHING...41 0D 3C>"))
    }

    @Test fun `zero is a valid speed`() {
        assertEquals(0.0, Elm327ResponseParser.parseSpeedKmh("41 0D 00>"))
    }

    @Test fun `adapter and transport error replies rejected even with speed bytes`() {
        for (error in listOf("NO DATA", "?", "UNABLE TO CONNECT", "STOPPED", "CAN ERROR", "BUS ERROR", "BUFFER FULL", "ERROR")) {
            assertNull(error, Elm327ResponseParser.parseSpeedKmh(error))
            assertNull(error, Elm327ResponseParser.parseSpeedKmh("41 0D 3C\r$error>"))
        }
    }

    @Test fun `non speed PID cannot produce mileage speed`() {
        for (frame in listOf("41 31 00 3C", "41 0C 1F 40", "41 05 3C", "010D", "SEARCHING...", "", ">")) {
            assertNull(frame, Elm327ResponseParser.parseSpeedKmh(frame))
        }
    }

    @Test fun `conflicting ECU answers rejected while matching duplicate accepted`() {
        assertNull(Elm327ResponseParser.parseSpeedKmh("41 0D 3C\r41 0D 3D>"))
        assertEquals(60.0, Elm327ResponseParser.parseSpeedKmh("41 0D 3C\r41 0D 3C>"))
    }

    @Test fun `malformed and incomplete speed replies rejected`() {
        for (frame in listOf("410D", "410D3", "410D3C00", "410DGG", "junk410D3C", "41 0D -1", "7E8 03 41 0D 3C")) {
            assertNull(frame, Elm327ResponseParser.parseSpeedKmh(frame))
        }
    }

    @Test fun `negative OBD reply rejects mixed positive payload`() {
        assertNull(Elm327ResponseParser.parseSpeedKmh("7F 01 12"))
        assertNull(Elm327ResponseParser.parseSpeedKmh("41 0D 3C\r7F 01 12>"))
    }
}
