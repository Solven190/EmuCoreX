package com.sbro.emucorex.data

enum class AppFontChoice(val preferenceValue: Int) {
    SYSTEM(0),
    RUBIK(1),
    EXO_2(2),
    CUSTOM(3);

    companion object {
        fun fromPreference(value: Int?): AppFontChoice =
            entries.firstOrNull { it.preferenceValue == value } ?: SYSTEM
    }
}

enum class HomeBackgroundType(val preferenceValue: Int) {
    NONE(0),
    IMAGE(1),
    GIF(2),
    VIDEO(3);

    companion object {
        fun fromPreference(value: Int?): HomeBackgroundType =
            entries.firstOrNull { it.preferenceValue == value } ?: NONE
    }
}

/** Visual treatment only; controller geometry and input hit targets never depend on this value. */
enum class TouchControlVisualStyle(val preferenceValue: Int) {
    CLASSIC(0),
    LEGACY(1),
    MODERN(2);

    companion object {
        fun fromPreference(value: Int?): TouchControlVisualStyle =
            entries.firstOrNull { it.preferenceValue == value } ?: CLASSIC
    }
}

enum class GameMenuTabId {
    SESSION,
    CONTROLS,
    EMULATION,
    GRAPHICS,
    FIXES,
    ACHIEVEMENTS
}

enum class GameMenuSessionSection {
    SAVE_STATES,
    AUTO_SAVE,
    QUICK_ACTIONS,
    AUTOMATION,
    GAME_PROFILE
}

val DefaultGameMenuTabOrder: List<GameMenuTabId> = GameMenuTabId.entries.toList()

fun sanitizeGameMenuTabOrder(raw: String?): List<GameMenuTabId> {
    val stored = raw.orEmpty()
        .split(',')
        .mapNotNull { token ->
            GameMenuTabId.entries.firstOrNull { it.name == token.trim().uppercase() }
        }
        .distinct()
    return (stored + DefaultGameMenuTabOrder).distinct()
}

fun sanitizeHiddenGameMenuTabs(raw: String?): Set<GameMenuTabId> = raw.orEmpty()
    .split(',')
    .mapNotNull { token ->
        GameMenuTabId.entries.firstOrNull { it.name == token.trim().uppercase() }
    }
    .filterNot { it == GameMenuTabId.SESSION }
    .toSet()

fun sanitizeHiddenGameMenuSections(raw: String?): Set<GameMenuSessionSection> = raw.orEmpty()
    .split(',')
    .mapNotNull { token ->
        GameMenuSessionSection.entries.firstOrNull { it.name == token.trim().uppercase() }
    }
    .toSet()
