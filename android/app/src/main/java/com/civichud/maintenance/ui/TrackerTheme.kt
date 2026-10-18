package com.civichud.maintenance.ui

import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

internal val GoodGreen = Color(0xFF25814A)
internal val WarningAmber = Color(0xFF986000)
internal val DangerRed = Color(0xFFD43E41)

@Composable
internal fun TrackerTheme(dark: Boolean, content: @Composable () -> Unit) {
    val colors = if (dark) darkColorScheme(
        primary = Color(0xFF8CDAAA), secondary = Color(0xFFFFC772),
        primaryContainer = Color(0xFF214B33), onPrimaryContainer = Color(0xFFD2F5DC),
        background = Color(0xFF101713), surface = Color(0xFF18221C),
        surfaceVariant = Color(0xFF29372E), error = Color(0xFFFFB3B3),
    ) else lightColorScheme(
        primary = GoodGreen, secondary = WarningAmber,
        primaryContainer = Color(0xFFD8F1DD), onPrimaryContainer = Color(0xFF14391F),
        background = Color(0xFFF5F8F5), surface = Color(0xFFFFFFFF),
        surfaceVariant = Color(0xFFE4EEE6), error = DangerRed,
    )
    MaterialTheme(colorScheme = colors, content = content)
}
