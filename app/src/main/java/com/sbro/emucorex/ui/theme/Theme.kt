package com.sbro.emucorex.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val DarkColorScheme = darkColorScheme(
    primary = AccentPrimary,
    onPrimary = OnAccent,
    primaryContainer = AccentPrimaryContainer,
    onPrimaryContainer = AccentPrimaryLight,
    secondary = SecondaryAccent,
    onSecondary = OnAccent,
    secondaryContainer = SecondaryContainer,
    onSecondaryContainer = SecondaryAccent,
    tertiary = TertiaryAccent,
    onTertiary = OnAccent,
    background = DarkBackground,
    onBackground = DarkOnBackground,
    surface = DarkSurface,
    onSurface = DarkOnSurface,
    surfaceVariant = DarkSurfaceVariant,
    onSurfaceVariant = DarkOnSurfaceVariant,
    outline = DarkOutline,
    outlineVariant = DarkOutline,
    error = ErrorRed,
    onError = OnAccent,
    errorContainer = ErrorContainer,
    onErrorContainer = ErrorRed,
    scrim = DarkScrim
)

private val ProColorScheme = darkColorScheme(
    primary = ProPrimary,
    onPrimary = OnAccent,
    primaryContainer = ProPrimaryContainer,
    onPrimaryContainer = ProOnPrimaryContainer,
    secondary = ProSecondary,
    onSecondary = Color(0xFF21181C),
    secondaryContainer = ProSecondaryContainer,
    onSecondaryContainer = Color(0xFFE6D8DD),
    tertiary = ProTertiary,
    onTertiary = Color(0xFF1A0D00),
    background = ProBackground,
    onBackground = ProOnBackground,
    surface = ProSurface,
    onSurface = ProOnSurface,
    surfaceVariant = ProSurfaceVariant,
    onSurfaceVariant = ProOnSurfaceVariant,
    outline = ProOutline,
    outlineVariant = ProOutline,
    surfaceTint = Color.Transparent,
    error = ErrorRed,
    onError = OnAccent,
    errorContainer = ErrorContainer,
    onErrorContainer = ErrorRed,
    scrim = ProScrim
)

private val LightColorScheme = lightColorScheme(
    primary = AccentPrimaryDark,
    onPrimary = OnAccent,
    primaryContainer = AccentPrimaryLightContainer,
    onPrimaryContainer = AccentPrimaryDark,
    secondary = SecondaryAccentDark,
    onSecondary = OnAccent,
    secondaryContainer = SecondaryLightContainer,
    onSecondaryContainer = SecondaryAccentDark,
    tertiary = TertiaryDark,
    onTertiary = OnAccent,
    background = LightBackground,
    onBackground = LightOnBackground,
    surface = LightSurface,
    onSurface = LightOnSurface,
    surfaceVariant = LightSurfaceVariant,
    onSurfaceVariant = LightOnSurfaceVariant,
    outline = LightOutline,
    outlineVariant = LightOutline,
    error = ErrorRedLight,
    onError = OnAccent,
    errorContainer = ErrorLightContainer,
    onErrorContainer = ErrorRedLight,
)

enum class ThemeMode {
    SYSTEM, LIGHT, DARK, PRO
}

@Composable
fun EmuCoreXTheme(
    themeMode: ThemeMode = ThemeMode.SYSTEM,
    content: @Composable () -> Unit
) {
    val darkTheme = when (themeMode) {
        ThemeMode.SYSTEM -> isSystemInDarkTheme()
        ThemeMode.LIGHT -> false
        ThemeMode.DARK -> true
        ThemeMode.PRO -> true
    }

    val colorScheme = when (themeMode) {
        ThemeMode.PRO -> ProColorScheme
        else -> if (darkTheme) DarkColorScheme else LightColorScheme
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = Typography,
        content = content
    )
}
