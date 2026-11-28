package com.sterownikco.pro.core

/* ══════════════════════════════════════════════════════════════════════════
   Centrum alarmów dymu i przegrzania (APK-only, brak odpowiednika w HTML).

   Działa w DWÓCH miejscach z tą samą logiką:
   • na pierwszym planie — AppModel.poll() po każdym odczycie /piec/status,
   • w tle — AlarmMonitorService odpytuje status co 60 s.
   Krawędź (edge) wykrywana jest przez sygnaturę zapisaną w prefs, więc
   podwójne odpalenie jest niemożliwe nawet gdy oba źródła pracują naraz.

   Dźwięk aktywnego, nieuciszonego alarmu jest zapętlany przez AlarmMonitorService.
   Wyciszenie jest lokalne natychmiast, a aplikacja synchronizuje je z centralą;
   raport /piec/status jest źródłem wspólnego stanu dla pozostałych telefonów.
   Tryby ciszy (przetrwają restart aplikacji i telefonu):
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
    const val K_CONTROLLER_DYM_MUTE_UNTIL = "piec_alarm_controller_dym_until"
    const val K_CONTROLLER_OGRZ_MUTE_UNTIL = "piec_alarm_controller_ogrz_until"
    const val K_CONTROLLER_STATUS_MARKER = "piec_alarm_controller_status_marker"

    /** Ponowne zgłoszenie powiadomienia; właściwy dźwięk alarmu gra w pętli w AlarmMonitorService. */
    const val REPEAT_MS = 30L * 60_000

    private val soundIds = AlarmSoundSync.SOUND_IDS

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

    /**
     * Applies the controller's shared mute report to this phone. Timed values are durations
     * (not controller millis timestamps); -1 means muted until the alarm clears. A transition
     * from muted to audible rearms an alarm that is still active on this phone.
     */
    internal fun applyControllerMuteSnapshot(
        p: AlarmPrefsStore,
        status: ControllerAlarmMuteStatus,
        currentSignature: String,
        completeAlarmFlags: Boolean,
        nowMs: Long
    ) {
        val effectiveStatus = if (status.hasAnyField) {
            ageRepeatedControllerMuteStatus(p, status, nowMs)
        } else {
            status
        }
        val wasMuted = storedControllerMuteCoversSignature(p, currentSignature)
        if (!effectiveStatus.hasAnyField) {
            // A full status from pre-sync firmware must not leave an old indefinite mute cached.
            if (completeAlarmFlags) {
                writeControllerMuteUntil(p, K_CONTROLLER_DYM_MUTE_UNTIL, 0L)
                writeControllerMuteUntil(p, K_CONTROLLER_OGRZ_MUTE_UNTIL, 0L)
                if (p.get(K_CONTROLLER_STATUS_MARKER) != null) p.remove(K_CONTROLLER_STATUS_MARKER)
            }
        } else {
            applyControllerMuteType(
                p, K_CONTROLLER_DYM_MUTE_UNTIL, effectiveStatus.smokeMuted,
                effectiveStatus.smokeRemainingMs, nowMs
            )
            applyControllerMuteType(
                p, K_CONTROLLER_OGRZ_MUTE_UNTIL, effectiveStatus.overheatMuted,
                effectiveStatus.overheatRemainingMs, nowMs
            )
        }
        val isMuted = controllerMuteForSignatureAt(p, currentSignature, nowMs, expire = false)
        if (wasMuted && !isMuted && currentSignature.isNotEmpty() &&
            p.get(K_LAST_SIG) == currentSignature
        ) {
            p.setLong(K_LAST_FIRE, 0L)
        }
    }

    private fun ageRepeatedControllerMuteStatus(
        p: AlarmPrefsStore,
        status: ControllerAlarmMuteStatus,
        nowMs: Long
    ): ControllerAlarmMuteStatus {
        val sourceTime = status.controllerUpdatedAtMs ?: return status
        val marker = p.get(K_CONTROLLER_STATUS_MARKER)?.split(':')
        val previousSourceTime = marker?.getOrNull(0)?.toLongOrNull()
        val previousReceivedAt = marker?.getOrNull(1)?.toLongOrNull()
        if (previousSourceTime != sourceTime || previousReceivedAt == null) {
            p.set(K_CONTROLLER_STATUS_MARKER, "$sourceTime:$nowMs")
            return status
        }

        // RTDB can keep returning its last value while the controller is offline. Age a
        // duplicate report from its first receipt so it cannot extend a mute indefinitely.
        val ageMs = (nowMs - previousReceivedAt).coerceAtLeast(0L)
        fun age(remaining: Long?): Long? =
            if (remaining != null && remaining > 0L) (remaining - ageMs).coerceAtLeast(0L) else remaining
        return status.copy(
            smokeRemainingMs = age(status.smokeRemainingMs),
            overheatRemainingMs = age(status.overheatRemainingMs)
        )
    }

    private fun applyControllerMuteType(
        p: AlarmPrefsStore,
        key: String,
        muted: Boolean?,
        remainingMs: Long?,
        nowMs: Long
    ) {
        if (muted == null) return
        val until = when {
            !muted -> 0L
            remainingMs == null || remainingMs == -1L -> Long.MAX_VALUE
            remainingMs <= 0L -> 0L
            else -> nowMs + remainingMs.coerceAtMost(3_600_000L)
        }
        writeControllerMuteUntil(p, key, until)
    }

    private fun writeControllerMuteUntil(p: AlarmPrefsStore, key: String, until: Long) {
        val previous = p.getLong(key, 0L)
        val materiallyChanged = when {
            previous == until -> false
            previous == 0L || previous == Long.MAX_VALUE || until == 0L || until == Long.MAX_VALUE -> true
            else -> kotlin.math.abs(until - previous) > 1_000L
        }
        if (materiallyChanged) p.setLong(key, until)
    }

    private fun storedControllerMuteCoversSignature(p: AlarmPrefsStore, signature: String): Boolean {
        val activeTypes = signature.split('+').mapNotNull {
            when (it) {
                "D" -> K_CONTROLLER_DYM_MUTE_UNTIL
                "O" -> K_CONTROLLER_OGRZ_MUTE_UNTIL
                else -> null
            }
        }
        return activeTypes.isNotEmpty() && activeTypes.all { p.getLong(it, 0L) > 0L }
    }

    private fun controllerMuteForSignatureAt(
        p: AlarmPrefsStore,
        signature: String,
        nowMs: Long,
        expire: Boolean
    ): Boolean {
        val activeTypes = signature.split('+').mapNotNull {
            when (it) {
                "D" -> K_CONTROLLER_DYM_MUTE_UNTIL
                "O" -> K_CONTROLLER_OGRZ_MUTE_UNTIL
                else -> null
            }
        }
        if (activeTypes.isEmpty()) return false
        var expired = false
        val allMuted = activeTypes.all { key ->
            val until = p.getLong(key, 0L)
            when {
                until == Long.MAX_VALUE -> true
                until > nowMs -> true
                until > 0L -> {
                    expired = true
                    if (expire) p.setLong(key, 0L)
                    false
                }
                else -> false
            }
        }
        if (expire && expired && p.get(K_LAST_SIG) == signature) {
            p.setLong(K_LAST_FIRE, 0L)
        }
        return allMuted
    }

    /** Null means at least one currently active alarm class is not muted. */
    fun controllerMuteLeftMinAt(p: AlarmPrefsStore, signature: String, nowMs: Long): Int? {
        val activeTypes = signature.split('+').mapNotNull {
            when (it) {
                "D" -> K_CONTROLLER_DYM_MUTE_UNTIL
                "O" -> K_CONTROLLER_OGRZ_MUTE_UNTIL
                else -> null
            }
        }
        if (activeTypes.isEmpty()) return null
        val untils = activeTypes.map { p.getLong(it, 0L) }
        if (untils.any { it != Long.MAX_VALUE && it <= nowMs }) return null
        val finite = untils.filter { it != Long.MAX_VALUE }
        if (finite.isEmpty()) return -1
        val earliest = finite.minOrNull() ?: return null
        val left = earliest - nowMs
        return if (left > 0L) ((left + 59_999L) / 60_000L).toInt() else null
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
        return (last.isNotEmpty() && (p.get(K_MUTED_SIG) ?: "") == last) ||
            controllerMuteForSignatureAt(p, last, System.currentTimeMillis(), expire = true)
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
        if (sig.isEmpty() || (p.get(K_MUTED_SIG) ?: "") == sig) return false
        return !controllerMuteForSignatureAt(p, sig, nowMs, expire = true)
    }

    fun statusText(p: AlarmPrefsStore): String = statusTextAt(p, System.currentTimeMillis())

    internal fun statusTextAt(p: AlarmPrefsStore, nowMs: Long): String {
        val snoozeMinutes = snoozeLeftMinAt(p, nowMs)
        val lastSignature = p.get(K_LAST_SIG) ?: ""
        val controllerMuteMinutes = controllerMuteLeftMinAt(p, lastSignature, nowMs)
        val localMuted = lastSignature.isNotEmpty() && (p.get(K_MUTED_SIG) ?: "") == lastSignature
        return when {
            !isMonitored(p) -> "Monitoring w tle WYŁĄCZONY"
            snoozeMinutes > 0 -> "Drzemka — cisza jeszcze $snoozeMinutes min"
            localMuted -> "Wyciszony do następnego alarmu"
            controllerMuteMinutes == -1 -> "Centrala i telefony wyciszone do ustąpienia alarmu"
            controllerMuteMinutes != null -> "Centrala i telefony wyciszone — jeszcze $controllerMuteMinutes min"
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
        if (controllerMuteForSignatureAt(p, sig, now, expire = true)) return null
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
