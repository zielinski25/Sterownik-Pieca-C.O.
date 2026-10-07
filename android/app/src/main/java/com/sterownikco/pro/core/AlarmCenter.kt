package com.sterownikco.pro.core

/* ══════════════════════════════════════════════════════════════════════════
   Centrum alarmów dymu i przegrzania (APK-only, brak odpowiednika w HTML).

   Działa w DWÓCH miejscach z tą samą logiką:
   • na pierwszym planie — AppModel.poll() po każdym odczycie /piec/status,
   • w tle — AlarmMonitorService na strumieniu SSE tego samego węzła.
   Krawędź (edge) wykrywana jest przez sygnaturę zapisaną w prefs, więc
   podwójne odpalenie jest niemożliwe nawet gdy oba źródła pracują naraz.

   Tryby ciszy (oba przeżyją restart aplikacji i telefonu):
   • drzemka czasowa (10 min / 1 h) — po jej końcu AKTYWNY alarm dzwoni znowu,
   • wyciszenie do następnego alarmu — nowa krawędź (inna sygnatura) dzwoni.
   ══════════════════════════════════════════════════════════════════════════ */

/** Opis odpalonego alarmu — `sig` to sygnatura krawędzi ("D", "O" lub "D+O"). */
data class AlarmInfo(val kind: String, val title: String, val msg: String, val sig: String)

object AlarmCenter {
    const val K_MONITOR = "piec_alarm_monitor"       // "1"/"0", domyślnie "1"
    const val K_SOUND = "piec_alarm_sound"           // syrena|dzwonek|pikanie|system
    const val K_SNOOZE_UNTIL = "piec_alarm_snooze"   // epoch ms jako String
    const val K_MUTED_SIG = "piec_alarm_muted_sig"   // sygnatura wyciszona "do następnego"
    const val K_LAST_SIG = "piec_alarm_last_sig"     // ostatnia widziana sygnatura
    const val K_LAST_FIRE = "piec_alarm_last_fire"   // epoch ms ostatniego dzwonka

    /** Powtórka dzwonka, gdy alarm wisi aktywny i NIC nie jest wyciszone. */
    const val REPEAT_MS = 30L * 60_000

    fun isMonitored(p: Prefs): Boolean = p.get(K_MONITOR) != "0"
    fun setMonitored(p: Prefs, on: Boolean) = p.set(K_MONITOR, if (on) "1" else "0")

    fun sound(p: Prefs): String = (p.get(K_SOUND) ?: "syrena").lowercase().let {
        if (it in setOf("syrena", "dzwonek", "pikanie", "system")) it else "syrena"
    }
    fun setSound(p: Prefs, s: String) = p.set(K_SOUND, s)

    /** Drzemka czasowa — liczy od teraz. */
    fun snooze(p: Prefs, minutes: Int) {
        p.setLong(K_SNOOZE_UNTIL, System.currentTimeMillis() + minutes * 60_000L)
    }

    /** Całkowite wyciszenie bieżącej krawędzi — zadzwoni dopiero NOWY alarm. */
    fun muteUntilNext(p: Prefs) {
        val sig = p.get(K_LAST_SIG) ?: ""
        if (sig.isNotEmpty()) p.set(K_MUTED_SIG, sig)
        p.remove(K_SNOOZE_UNTIL)
    }

    /** Ręczne zakończenie drzemki / wyciszenia (przycisk w arkuszu). */
    fun unsilence(p: Prefs) {
        p.remove(K_SNOOZE_UNTIL); p.remove(K_MUTED_SIG)
    }

    fun snoozeLeftMin(p: Prefs): Int {
        val left = p.getLong(K_SNOOZE_UNTIL, 0L) - System.currentTimeMillis()
        return if (left > 0) ((left + 59_999) / 60_000).toInt() else 0
    }

    fun isMutedNow(p: Prefs): Boolean {
        val last = p.get(K_LAST_SIG) ?: ""
        return last.isNotEmpty() && (p.get(K_MUTED_SIG) ?: "") == last
    }

    fun statusText(p: Prefs): String = when {
        !isMonitored(p) -> "Monitoring w tle WYŁĄCZONY"
        snoozeLeftMin(p) > 0 -> "Drzemka — cisza jeszcze ${snoozeLeftMin(p)} min"
        isMutedNow(p) -> "Wyciszony do następnego alarmu"
        else -> "Aktywny — zadzwoni przy dymie / przegrzaniu"
    }

    /**
     * Ocena migawki statusu. Zwraca [AlarmInfo] TYLKO gdy trzeba fizycznie
     * zadzwonić (nowa krawędź, koniec drzemki przy wiszącym alarmie albo
     * 30-minutowa powtórka); w przeciwnym razie null = cisza.
     */
    fun evaluate(p: Prefs, dym: Boolean, ogrz: Boolean, tOgrz: Double, dymAdc: Double): AlarmInfo? {
        if (!isMonitored(p)) return null
        val now = System.currentTimeMillis()
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
