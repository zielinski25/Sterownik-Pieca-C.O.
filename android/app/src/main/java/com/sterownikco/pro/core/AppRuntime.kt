package com.sterownikco.pro.core

import android.content.Context
import android.os.Process
import android.os.SystemClock

/* ══════════════════════════════════════════════════════════════════════════
   [0.31.62] STAN, KTÓRY MUSI PRZEŻYĆ ODTWORZENIE AKTYWNOŚCI

   Problem (zgłoszenie 30.10): „gdy odpakuję aplikację, wejdę za parę sekund,
   ona ładuje się od nowa na dashboard, a po wejściu w Wykresy znowu kilka
   sekund czyta historię".

   Przyczyna: `MainActivity.onCreate` tworzy `AppModel(applicationContext, …)`
   przy KAŻDYM odtworzeniu aktywności (powrót z tła po zabiciu procesu,
   „Nie zachowuj aktywności" w opcjach programisty, klik w powiadomienie
   alarmu z FLAG_ACTIVITY_CLEAR_TOP). Nowy `AppModel` = `page = 0` (pulpit),
   `telemetry` puste i `TelemetryCache` z pustą pamięcią RAM → pełne wczytanie.

   Rozwiązanie:
   1) `telemetryCache()` — JEDEN obiekt pamięci historii na proces. Odtworzenie
      aktywności nie czyta pliku z dysku od nowa.
   2) `lastTelemetry` / `lastLive` / `lastRangeSec` — ostatnio pokazany wykres
      zostaje w procesie, więc po powrocie rysuje się natychmiast, bez dysku.
   3) `processTag()` — PID i wiek procesu do dziennika. To rozstrzyga, czy
      Android naprawdę zabił proces (PID inny), czy tylko odtworzył aktywność
      (PID ten sam) — bez tego każda diagnoza byłaby zgadywaniem.

   Czego to nie obejmuje: po PRAWDZIWYM zabiciu procesu pamięć RAM znika.
   Wtedy ratują nas: zapamiętana zakładka (Prefs.K_LAST_PAGE) i pamięć historii
   na dysku (`historia.tsv.gz`), z której 0.31.61 rysuje wykres od razu.
   ══════════════════════════════════════════════════════════════════════════ */

object AppRuntime {

    private var cache: TelemetryCache? = null

    /** Wspólna pamięć historii telemetrii — jedna na proces, nie na aktywność. */
    @Synchronized
    fun telemetryCache(ctx: Context): TelemetryCache {
        val existing = cache
        if (existing != null) return existing
        val created = TelemetryCache(ctx.applicationContext ?: ctx)
        cache = created
        return created
    }

    /** Ostatnio pokazany wykres (historia) — przeżywa odtworzenie aktywności. */
    @Volatile var lastTelemetry: List<TelemPoint> = emptyList()

    /** Próbki „live" doklejone do wykresu. */
    @Volatile var lastLive: List<TelemPoint> = emptyList()

    /** Zakres, dla którego policzono [lastTelemetry]. */
    @Volatile var lastRangeSec: Int = 0

    /** Kiedy ostatnio udanie wczytano historię (elapsedRealtime). */
    @Volatile var lastLoadedAtMs: Long = 0L

    /** Wiek procesu w ms (0, gdy system nie poda) — odróżnia restart od rekreacji. */
    fun processAgeMs(): Long = try {
        val age = SystemClock.elapsedRealtime() - Process.getStartElapsedRealtime()
        if (age < 0L) 0L else age
    } catch (e: Exception) {
        0L
    }

    /** Krótki opis do dziennika: „pid=1234 wiek=37s". */
    fun processTag(): String = "pid=" + Process.myPid() + " wiek=" + (processAgeMs() / 1000L) + "s"
}
