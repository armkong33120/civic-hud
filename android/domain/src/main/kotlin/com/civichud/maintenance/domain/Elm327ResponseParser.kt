package com.civichud.maintenance.domain

/** Parses one prompt-framed ELM327 ASCII response to the 010D speed command. */
object Elm327ResponseParser {
    private val error = Regex("NO\\s*DATA|UNABLE\\s*TO\\s*CONNECT|STOPPED|CAN\\s*ERROR|BUS\\s*(?:INIT[^\\r\\n]*ERROR|ERROR)|BUFFER\\s*FULL|LV\\s*RESET|ERROR|\\?", RegexOption.IGNORE_CASE)
    private val speedFrame = Regex("410D([0-9A-F]{2})")

    /**
     * Accepts compact/spaced payloads, optional command echo and SEARCHING chatter.
     * Rejects mixed ECU speed values, malformed speed frames, error replies and other PIDs.
     * It deliberately never interprets a distance-since-code-clear PID as odometer mileage.
     */
    fun parseSpeedKmh(rawResponse: String): Double? {
        val normalized = rawResponse.uppercase()
        if (error.containsMatchIn(normalized)) return null
        val values = mutableListOf<Int>()
        for (rawLine in normalized.split('\r', '\n', '>')) {
            val line = rawLine.replace("SEARCHING...", "").replace("SEARCHING", "").trim()
            if (line.isEmpty()) continue
            val compact = line.replace(Regex("\\s+"), "")
            if (compact == "010D") continue
            // Framed transport is configured with ATH0: never guess at CAN headers or DLC.
            if (!compact.matches(Regex("[0-9A-F]+"))) return null
            if (compact.startsWith("7F")) return null // Negative OBD response.
            if (!compact.startsWith("410D")) continue
            val match = speedFrame.matchEntire(compact) ?: return null
            values += match.groupValues[1].toInt(16)
        }
        return values.distinct().singleOrNull()?.toDouble()
    }
}
