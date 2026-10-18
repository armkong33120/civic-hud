package com.civichud.maintenance.ui

import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

internal val GoodGreen = Color(0xFF25814A)
internal val WarningAmber = Color(0xFF986000)
internal val DangerRed = Color(0xFFD43E41)

val LocalReducedMotion = staticCompositionLocalOf { false }

@Composable
internal fun TrackerTheme(themeMode: String = "system", reduceMotion: Boolean = false, content: @Composable () -> Unit) {
    val dark = when(themeMode) { "dark" -> true; "light" -> false; else -> isSystemInDarkTheme() }
    CompositionLocalProvider(LocalReducedMotion provides reduceMotion) { TrackerTheme(dark, content) }
}

@Composable
internal fun TrackerTheme(dark: Boolean, content: @Composable () -> Unit) {
    val colors = if (dark) darkColorScheme(
        primary = Color(0xFF8CDAAA), secondary = Color(0xFFFFC772),
        primaryContainer = Color(0xFF214B33), onPrimaryContainer = Color(0xFFD2F5DC),
        background = Color(0xFF101713), surface = Color(0xFF18221C),
        surfaceVariant = Color(0xFF29372E),
        surfaceContainer = Color(0xFF1B2922), surfaceContainerLow = Color(0xFF18251E),
        surfaceContainerHigh = Color(0xFF223329), surfaceContainerHighest = Color(0xFF293D31),
        outline=Color(0xFF729380), outlineVariant=Color(0xFF405D4B), onSurface = Color(0xFFE5EFE8), onSurfaceVariant=Color(0xFFB7CCBE), onPrimary=Color(0xFF06381D), error = Color(0xFFFFB3B3),
    ) else lightColorScheme(
        primary = GoodGreen, secondary = WarningAmber,
        primaryContainer = Color(0xFFD8F1DD), onPrimaryContainer = Color(0xFF14391F),
        background = Color(0xFFF1F5F3), surface = Color(0xFFFFFFFF),
        surfaceVariant = Color(0xFFE4EEE6),
        surfaceContainer = Color(0xFFFFFFFF), surfaceContainerLow = Color(0xFFF9FCFA),
        surfaceContainerHigh = Color(0xFFEDF4EF), surfaceContainerHighest = Color(0xFFE4EEE6),
        outline=Color(0xFF718678), outlineVariant=Color(0xFFC2D4C7), onSurface=Color(0xFF172B20), onSurfaceVariant=Color(0xFF4B6153), onPrimary=Color.White, error = DangerRed,
    )
    MaterialTheme(colorScheme = colors, shapes = Shapes(small=androidx.compose.foundation.shape.RoundedCornerShape(12.dp), medium=androidx.compose.foundation.shape.RoundedCornerShape(20.dp), large=androidx.compose.foundation.shape.RoundedCornerShape(28.dp)), content = content)
}
