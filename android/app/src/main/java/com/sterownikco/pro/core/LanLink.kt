package com.sterownikco.pro.core

/* ══════════════════════════════════════════════════════════════════════════
   SIEĆ LOKALNA (LAN) — centrala w tej samej sieci co telefon

   Gdy telefon widzi centralę w sieci lokalnej (sieć wewnętrzna budynku albo
   hotspot centrali PiecCO_AP), aplikacja czyta status i wysyła polecenia
   bezpośrednio do niej (firmware v3.47.0+: /api/lan/status, /api/lan/cmd,
   /api/lan/ack) — działa także bez internetu. Firebase pozostaje drogą zdalną.

   Ten plik: czysta logika (testy JVM) + wspólny stan łącza dla AppModel,
   usługi alarmów i widżetu (wszystkie działają w jednym procesie).
   ══════════════════════════════════════════════════════════════════════════ */

/** Adresy centrali do sprawdzenia w sieci lokalnej, w kolejności pierwszeństwa. */
internal object LanCandidates {
    /** Hotspot centrali (SSID PiecCO_AP): domyślny WiFi.softAPIP() — firmware go nie zmienia. */
    const val AP_DEFAULT_IP = "192.168.4.1"

    /** Ręcznie wpisany adres, ostatnio znany adres centrali, hotspot — wyłącznie prywatne IPv4. */
    fun build(manualIp: String?, lastKnownIp: String?): List<String> =
        listOfNotNull(manualIp, lastKnownIp, AP_DEFAULT_IP)
            .mapNotNull { LocalControllerAddress.canonicalPrivateIpv4(it) }
            .distinct()

    /** Odpowiedź wygląda na status centrali, a nie innego urządzenia pod tym samym adresem. */
    fun looksLikeController(wifiMode: String?, hasControllerFields: Boolean): Boolean =
        (wifiMode == "AP" || wifiMode == "STA") && hasControllerFields
}

/** Rzadsze sondowanie, gdy centrali nie ma w sieci — poza domem nie tracimy czasu co 8 s. */
internal class LanProbeSchedule(private val retryMs: Long = 30_000L) {
    private var nextProbeAtMs = 0L

    fun shouldProbe(nowMs: Long, force: Boolean = false): Boolean = force || nowMs >= nextProbeAtMs

    fun onFound() { nextProbeAtMs = 0L }

    fun onMissing(nowMs: Long) { nextProbeAtMs = nowMs + retryMs }

    /** Nowy adres centrali (np. z /piec/status albo wpisany ręcznie) — sprawdź go od razu. */
    fun probeSoon() { nextProbeAtMs = 0L }
}

/** Wspólny stan łącza LAN (jeden proces: AppModel, AlarmMonitorService, widżet). */
internal object LanLink {
    data class Active(val ip: String, val fullApi: Boolean, val okAtMs: Long)

    @Volatile var active: Active? = null
        private set

    fun markOk(ip: String, fullApi: Boolean, nowMs: Long) {
        active = Active(ip, fullApi, nowMs)
    }

    fun markLost(ip: String? = null) {
        val current = active ?: return
        if (ip == null || current.ip == ip) active = null
    }

    /** Łącze potwierdzone nie dawniej niż [maxAgeMs] temu (domyślnie ~5 cykli odpytywania). */
    fun fresh(nowMs: Long, maxAgeMs: Long = 45_000L): Active? =
        active?.takeIf { nowMs - it.okAtMs in 0..maxAgeMs }
}
