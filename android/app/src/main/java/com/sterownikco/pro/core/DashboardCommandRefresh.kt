package com.sterownikco.pro.core

/** Commands that change dashboard values or synchronized device settings. */
internal fun commandChangesDashboardState(command: String): Boolean {
    val normalized = command.trim().lowercase(java.util.Locale.ROOT)
    return normalized.startsWith("ustaw ") ||
        normalized.startsWith("alarm_sound ") ||
        parseServoPositionCommand(normalized) != null ||
        normalized.startsWith("symuluj ") || normalized.startsWith("symuluj_stop ") ||
        normalized in setOf(
            "pompa_wl", "pompa_wyl", "pompa_auto",
            "mieszadlo_wl", "mieszadlo_wyl", "mieszadlo_auto",
            "rozpalanie_serwa", "rozpalanie_stop", "reset_alarmow",
            "wycisz", "wycisz_ogrz"
        )
}
