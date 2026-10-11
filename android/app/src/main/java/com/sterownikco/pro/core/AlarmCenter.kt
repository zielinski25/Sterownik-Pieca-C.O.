package com.sterownikco.pro.core

/* ══════════════════════════════════════════════════════════════════════════
   Centrum alarmów dymu i przegrzania (APK-only, brak odpowiednika w HTML).

   Działa w DWÓCH miejscach z tą samą logiką:
   • na pierwszym planie — AppModel.poll() po każdym odczycie /piec/status,
   • w tle — AlarmMonitorService na strumieniu SSE tego samego węzła.
   Krawędź (edge) wykrywana jest przez sygnaturę zapisaną w prefs, więc
   podwójne odpalenie jest niemożliwe nawet gdy oba źródła pracują naraz.

   Dźwięk aktywnego, nieuciszonego alarmu jest zapętlany przez AlarmMonitorService.
   Tryby ciszy (oba przeżyją restart aplikacji i telefonu):
   • drzemka czasowa (10 min / 1 h) — zatrzymuje pętlę; po jej końcu aktywny alarm wraca,
   • wyciszenie do następnego alarmu — zatrzymuje pętlę; nowa krawędź (inna sygnatura) dzwoni.
   ══════════════════════════════════════════════════════════════════════════ */

/** Opis alarmu z flag zwróconych przez sterownik. */
data class AlarmInfo(val kind: String, val title: String, val msg: String, val sig: String)

/** Stan dwóch flag; `null` pozostaje jawnie nieznanym odczytem. */
data class AlarmFlags(val dym: Boolean?, val ogrz: Boolean?)

object AlarmCenter {
    const val K_MONITOR = "piec_alarm_monitor"       // "1"/"0", domyślnie "1"
    const val K_SOUND = "piec_alarm_sound"           // legacy shared sound, used as smoke fallback
    const val K_SOUND_DYM = "piec_alarm_sound_dym"
    const val K_SOUND_OGRZ = "piec_alarm_sound_ogrz"
    const val K_SOUND_BOTH = "piec_alarm_sound_both"
    const val K_SNOOZE_UNTIL = "piec_alarm_snooze"   // epoch ms jako String
    const val K_MUTED_SIG = "piec_alarm_muted_sig"   // sygnatura wyciszona "do następnego"
    const val K_LAST_SIG = "piec_alarm_last_sig"     // ostatnia widziana sygnatura
    const val K_LAST_FIRE = "piec_alarm_last_fire"   // epoch ms ostatniego dzwonka

    /** Ponowne zgłoszenie powiadomienia; właściwy dźwięk alarmu gra w pętli w AlarmMonitorService. */
    const val REPEAT_MS = 30L * 60_000

    private val soundIds = setOf(
        "syrena", "dzwonek", "pikanie", "dwutonowy", "impulsowy", "narastajacy", "niski_puls",
        "syrena_falujaca", "syrena_ratunkowa", "klakson_pulsacyjny", "gong_ostrzegawczy", "system"
    )

    fun isMonitored(p: AlarmPrefsStore): Boolean = p.get(K_MONITOR) != "0"
    fun setMonitored(p: AlarmPrefsStore, on: Boolean) = p.set(K_MONITOR, if (on) "1" else "0")

    /**
     * Resolves an incomplete background snapshot without collapsing unknown to false.
     * A confirmed active flag is enough to process an alarm; for an unknown flag, retain
     * a previously active state when available. Otherwise keep that flag as null. A caller
     * must not clear an alarm from a partial snapshot with no confirmed active flag.
     */
    fun activeFlagsForPartialSnapshot(
        p: AlarmPrefsStore,
        dym: Boolean?,
        ogrz: Boolean?
    ): AlarmFlags? {
        if (dym != null && ogrz != null) return null
        if (dym != true && ogrz != true) return null

        val last = (p.get(K_LAST_SIG) ?: "").split('+').toSet()
        val activeDym = dym ?: if ("D" in last) true else null
        val activeOgrz = ogrz ?: if ("O" in last) true else null
        return AlarmFlags(activeDym, activeOgrz)
    }

    /** Osobny wybór dla dymu, przegrzania i alarmu łącznego; K_SOUND zachowuje starszy wybór dymu. */
    fun sound(p: AlarmPrefsStore, kind: String): String {
        val key = when (kind) {
            "ogrz" -> K_SOUND_OGRZ
            "dym+ogrz", "ogrz+dym" -> K_SOUND_BOTH
            else -> K_SOUND_DYM
        }
        val default = when (kind) {
            "ogrz" -> "dzwonek"
            "dym+ogrz", "ogrz+dym" -> "dwutonowy"
            else -> (p.get(K_SOUND) ?: "syrena").lowercase()
        }
        val candidate = (p.get(key) ?: default).lowercase()
        return candidate.takeIf { it in soundIds } ?: default.takeIf { it in soundIds } ?: "syrena"
    }

    fun sound(p: AlarmPrefsStore): String = sound(p, "dym")

    fun setSound(p: AlarmPrefsStore, kind: String, sound: String) {
        if (sound !in soundIds) return
        val key = when (kind) {
            "ogrz" -> K_SOUND_OGRZ
            "dym+ogrz", "ogrz+dym" -> K_SOUND_BOTH
            else -> K_SOUND_DYM
        }
        p.set(key, sound)
    }

    /** Zapis legacy zachowany dla kompatybilności ze starszymi UI. */
    fun setSound(p: AlarmPrefsStore, sound: String) {
        if (sound in soundIds) p.set(K_SOUND, sound)
    }

    /** Drzemka czasowa — liczy od teraz. */
    fun snooze(p: AlarmPrefsStore, minutes: Int) =
        snoozeAt(p, minutes, System.currentTimeMillis())

    /** Wariant z jawnym czasem do deterministycznych testów granic timera. */
    internal fun snoozeAt(p: AlarmPrefsStore, minutes: Int, nowMs: Long) {
        p.setLong(K_SNOOZE_UNTIL, nowMs + minutes * 60_000L)
    }

    /** Całkowite wyciszenie bieżącej krawędzi — zadzwoni dopiero NOWY alarm. */
    fun muteUntilNext(p: AlarmPrefsStore) {
        val sig = p.get(K_LAST_SIG) ?: ""
        if (sig.isNotEmpty()) p.set(K_MUTED_SIG, sig)
        p.remove(K_SNOOZE_UNTIL)
    }

    /** Ręczne zakończenie drzemki / wyciszenia (przycisk w arkuszu). */
    fun unsilence(p: AlarmPrefsStore) {
        p.remove(K_SNOOZE_UNTIL); p.remove(K_MUTED_SIG)
    }

    fun snoozeLeftMin(p: AlarmPrefsStore): Int =
        snoozeLeftMinAt(p, System.currentTimeMillis())

    /** Jawny zegar pozwala testować zaokrąglenie i dokładną granicę drzemki bez sleep(). */
    internal fun snoozeLeftMinAt(p: AlarmPrefsStore, nowMs: Long): Int {
        val left = p.getLong(K_SNOOZE_UNTIL, 0L) - nowMs
        return if (left > 0) ((left + 59_999) / 60_000).toInt() else 0
    }

    fun isMutedNow(p: AlarmPrefsStore): Boolean {
        val last = p.get(K_LAST_SIG) ?: ""
        return last.isNotEmpty() && (p.get(K_MUTED_SIG) ?: "") == last
    }

    /** Bieżący rodzaj alarmu do doboru osobnej melodii. */
    fun kindFor(dym: Boolean, ogrz: Boolean): String = when {
        dym && ogrz -> "dym+ogrz"
        dym -> "dym"
        else -> "ogrz"
    }

    /** Buduje opis aktywnego alarmu bez zmiany sygnatury, drzemki ani czasu ostatniego dzwonka. */
    fun describeActiveFlags(dym: Boolean, ogrz: Boolean, tOgrz: Double, dymAdc: Double): AlarmInfo? {
        val sig = listOfNotNull(if (dym) "D" else null, if (ogrz) "O" else null).joinToString("+")
        return sig.takeIf { it.isNotEmpty() }?.let { build(it, tOgrz, dymAdc) }
    }

    /** Ciągły dźwięk jest aktywny tylko przy alarmie nieobjętym drzemką/wyciszeniem. */
    fun shouldLoopSound(p: AlarmPrefsStore, dym: Boolean, ogrz: Boolean): Boolean =
        shouldLoopSoundAt(p, dym, ogrz, System.currentTimeMillis())

    internal fun shouldLoopSoundAt(
        p: AlarmPrefsStore, dym: Boolean, ogrz: Boolean, nowMs: Long
    ): Boolean {
        if (!isMonitored(p) || (!dym && !ogrz) || snoozeLeftMinAt(p, nowMs) > 0) return false
        val sig = listOfNotNull(if (dym) "D" else null, if (ogrz) "O" else null).joinToString("+")
        return sig.isNotEmpty() && (p.get(K_MUTED_SIG) ?: "") != sig
    }

    fun statusText(p: AlarmPrefsStore): String = statusTextAt(p, System.currentTimeMillis())

    internal fun statusTextAt(p: AlarmPrefsStore, nowMs: Long): String {
        val snoozeMinutes = snoozeLeftMinAt(p, nowMs)
        return when {
            !isMonitored(p) -> "Monitoring w tle WYŁĄCZONY"
            snoozeMinutes > 0 -> "Drzemka — cisza jeszcze $snoozeMinutes min"
            isMutedNow(p) -> "Wyciszony do następnego alarmu"
            else -> "Monitoring włączony — nie potwierdza łączności w tle"
        }
    }

    /**
     * Ocena migawki statusu. Zwraca [AlarmInfo] TYLKO gdy trzeba fizycznie
     * zadzwonić (nowa krawędź, koniec drzemki przy wiszącym alarmie albo
     * 30-minutowa powtórka); w przeciwnym razie null = cisza.
     */
    fun evaluate(p: AlarmPrefsStore, dym: Boolean, ogrz: Boolean, tOgrz: Double, dymAdc: Double): AlarmInfo? =
        evaluateAt(p, dym, ogrz, tOgrz, dymAdc, System.currentTimeMillis())

    /** Alarm reducer with an explicit timestamp for deterministic unit tests. */
    internal fun evaluateAt(
        p: AlarmPrefsStore,
        dym: Boolean,
        ogrz: Boolean,
        tOgrz: Double,
        dymAdc: Double,
        nowMs: Long
    ): AlarmInfo? {
        if (!isMonitored(p)) return null
        val now = nowMs
        val sig = listOfNotNull(if (dym) "D" else null, if (ogrz) "O" else null).joinToString("+")
        if (sig.isEmpty()) {
            // Alarmy zgasły — czyścimy stan krawędzi, żeby powrót zadzwonił od nowa.
            if ((p.get(K_LAST_SIG) ?: "").isNotEmpty()) {
                p.set(K_LAST_SIG, "")
                p.remove(K_MUTED_SIG)
                p.remove(K_SNOOZE_UNTIL)
            }
            return null
        }
        val last = p.get(K_LAST_SIG) ?: ""
        val isEdge = sig != last
        if (isEdge) {
            // Nowa sytuacja kasuje drzemkę (bezpieczeństwo ponad ciszę).
            p.remove(K_SNOOZE_UNTIL)
        } else if (now < p.getLong(K_SNOOZE_UNTIL, 0L)) {
            return null // drzemka trzyma
        } else if (p.getLong(K_SNOOZE_UNTIL, 0L) > 0L) {
            // Drzemka właśnie wygasła, a alarm wisi — dzwonimy znowu.
            p.remove(K_SNOOZE_UNTIL)
            p.set(K_LAST_SIG, sig); p.setLong(K_LAST_FIRE, now)
            return build(sig, tOgrz, dymAdc)
        }
        if (!isEdge && (p.get(K_MUTED_SIG) ?: "") == sig) return null // wyciszony do następnego
        val lastFire = p.getLong(K_LAST_FIRE, 0L)
        if (!isEdge && now - lastFire < REPEAT_MS) return null // ta sama krawędź, za wcześnie na powtórkę
        p.set(K_LAST_SIG, sig); p.setLong(K_LAST_FIRE, now)
        return build(sig, tOgrz, dymAdc)
    }

    private fun build(sig: String, tOgrz: Double, dymAdc: Double): AlarmInfo {
        fun deg(v: Double) = if (v.isFinite()) String.format(java.util.Locale.US, "%.1f", v) + "°C" else "—"
        val adc = if (dymAdc.isFinite()) dymAdc.toInt().toString() + " ADC" else "—"
        return when (sig) {
            "D+O", "O+D" -> AlarmInfo("dym+ogrz", "ALARM DYMU I PRZEGRZANIA",
                "Dym: $adc · Piec: ${deg(tOgrz)}. Sprawdź kotłownię NATYCHMIAST!", sig)
            "D" -> AlarmInfo("dym", "ALARM DYMU",
                "Czujnik dymu MQ-2: $adc. Możliwy pożar sadzy — sprawdź kocioł!", sig)
            else -> AlarmInfo("ogrz", "ALARM PRZEGRZANIA",
                "Temperatura kotła: ${deg(tOgrz)}. Ryzyko zagotowania układu!", sig)
        }
    }
}
