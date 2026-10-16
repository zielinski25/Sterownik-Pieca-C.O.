package com.sterownikco.pro.core

enum class AlarmMonitorPhase {
    STOPPED,
    STARTING,
    CONNECTING,
    STATUS_OK,
    RETRYING,
    START_FAILED
}

data class AlarmMonitorHealth(
    val phase: AlarmMonitorPhase = AlarmMonitorPhase.STOPPED,
    /** Monotonic timestamp; meaningful only during the current device boot. */
    val lastStatusElapsedMs: Long? = null
)

/** Cautious UI copy: a recent read is evidence of reachability, not a delivery guarantee. */
internal object AlarmMonitorHealthText {
    fun describe(
        monitored: Boolean,
        hasSession: Boolean,
        health: AlarmMonitorHealth,
        nowElapsedMs: Long
    ): String {
        if (!monitored) return "Monitoring alarmów jest wyłączony."
        if (!hasSession) return "Brak zapisanej sesji Firebase — monitoring w tle nie wystartuje."

        val lastRead = health.lastStatusElapsedMs?.let { timestamp ->
            val ageMs = (nowElapsedMs - timestamp).coerceAtLeast(0L)
            when {
                ageMs < 20_000L -> "Ostatni odczyt RTDB: przed chwilą."
                ageMs < 60_000L -> "Ostatni odczyt RTDB: ${ageMs / 1_000L} s temu."
                else -> "Ostatni odczyt RTDB: ${ageMs / 60_000L} min temu; dane mogą być nieaktualne."
            }
        } ?: "Brak potwierdzonego odczytu RTDB."

        val phaseText = when (health.phase) {
            AlarmMonitorPhase.STOPPED -> "Usługa monitorowania nie jest potwierdzona jako uruchomiona."
            AlarmMonitorPhase.STARTING -> "Wysłano żądanie uruchomienia usługi; oczekiwanie na start."
            AlarmMonitorPhase.CONNECTING -> "Usługa łączy się z RTDB."
            AlarmMonitorPhase.STATUS_OK -> "Usługa odebrała status z RTDB."
            AlarmMonitorPhase.RETRYING -> "Ostatnia próba połączenia lub odczytu nie powiodła się; usługa ponawia próbę."
            AlarmMonitorPhase.START_FAILED -> "Nie udało się zlecić uruchomienia usługi."
        }
        return "$phaseText $lastRead Sam status nie gwarantuje przyszłego alarmu."
    }
}
