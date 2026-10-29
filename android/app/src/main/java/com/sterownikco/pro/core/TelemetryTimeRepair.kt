package com.sterownikco.pro.core

import java.time.LocalDateTime
import java.time.ZoneId
import java.time.ZoneOffset
import kotlin.math.abs

/**
 * Ostrożna korekta archiwalnych znaczników czasu RTDB zapisanych przez firmware
 * starszy niż v3.47.2. Nie uznaje samego uptime'u za dowód: wymaga jednoznacznego
 * dopasowania sesji aktywnej na podstawie `ts - mono`, uptime'u i świeżej próbki.
 * Koryguje tylko inną sesję, której surowy czas wychodzi po starcie sesji aktywnej,
 * a interpretacja pól czasu jako Europe/Warsaw cofa cały jej odcinek przed ten start.
 */
object TelemetryTimeRepair {
    const val MAX_QUERY_LOOKAHEAD_SECONDS = 2L * 60L * 60L

    private const val MAX_MONO_MS = 0xFFFF_FFFFL
    private const val MAX_MONO_SECONDS = MAX_MONO_MS / 1000.0
    private const val BOOT_MATCH_TOLERANCE_SECONDS = 180.0
    private const val FRESH_SAMPLE_MAX_AGE_SECONDS = 300.0
    private const val FUTURE_TIMESTAMP_EVIDENCE_SECONDS = 300.0
    private const val NORMALIZED_END_TOLERANCE_SECONDS = 120.0
    private const val MIN_WARSAW_OFFSET_SECONDS = 3_600L
    private const val MAX_WARSAW_OFFSET_SECONDS = 7_200L

    private val MIN_FIXED_UTC_FIRMWARE = Triple(3, 47, 2)
    private val WARSAW: ZoneId = ZoneId.of("Europe/Warsaw")

    data class Result(
        val rows: List<TelemRow>,
        /** Sesje uznane za legacy na podstawie dowodów; przydatne dla czytnika inkrementalnego. */
        val legacySessionIds: Set<String>
    )

    /** Czy bieżący status pozwala bezpiecznie próbować rozpoznania starszej sesji. */
    fun canAttempt(
        firmware: String?,
        deviceUptimeSeconds: Double?,
        statusObservedAtMs: Long?
    ): Boolean {
        if (!firmwareHasUtcTelemetry(firmware)) return false
        val uptime = deviceUptimeSeconds ?: return false
        val observedAt = statusObservedAtMs ?: return false
        return uptime.isFinite() && uptime > 0.0 && uptime <= MAX_MONO_SECONDS && observedAt > 0L
    }

    /** Rozszerza zapytanie o najwyżej 2 h, żeby pobrać legacy wiersze przed ich korektą. */
    fun queryEndSeconds(
        endSeconds: Long,
        firmware: String?,
        deviceUptimeSeconds: Double?,
        statusObservedAtMs: Long?
    ): Long = if (canAttempt(firmware, deviceUptimeSeconds, statusObservedAtMs)) {
        endSeconds + MAX_QUERY_LOOKAHEAD_SECONDS
    } else {
        endSeconds
    }

    /** Zwraca kopie wierszy z poprawionym `ts` tylko dla jednoznacznie rozpoznanych poprzednich sesji. */
    fun normalize(
        rows: List<TelemRow>,
        firmware: String?,
        deviceUptimeSeconds: Double?,
        statusObservedAtMs: Long?,
        zone: ZoneId = WARSAW
    ): Result {
        if (rows.isEmpty() || !canAttempt(firmware, deviceUptimeSeconds, statusObservedAtMs)) {
            return Result(rows, emptySet())
        }

        val uptime = deviceUptimeSeconds!!
        val statusAtSeconds = statusObservedAtMs!! / 1000.0
        val expectedBootSeconds = statusAtSeconds - uptime
        val timestampCache = HashMap<Long, Long?>()
        fun normalizedTimestamp(raw: Long): Long? {
            if (timestampCache.containsKey(raw)) return timestampCache[raw]
            val value = legacyWarsawTimestamp(raw, zone)
            timestampCache[raw] = value
            return value
        }
        val grouped = LinkedHashMap<String, MutableList<TelemRow>>()
        rows.forEach { row ->
            val id = sessionId(row.id) ?: return@forEach
            if (row.ts > 0L) grouped.getOrPut(id) { ArrayList() }.add(row)
        }

        val sessions = grouped.mapNotNull { (id, sessionRows) ->
            val samples = sessionRows.mapNotNull { row ->
                val mono = row.mono
                if (mono !in 0L..MAX_MONO_MS) return@mapNotNull null
                val monoSeconds = mono / 1000.0
                SessionSample(row, monoSeconds, row.ts.toDouble() - monoSeconds)
            }
            if (samples.isEmpty()) null else Session(id, sessionRows, samples)
        }

        val activeCandidates = sessions.filter { session ->
            session.bootSpreadSeconds <= BOOT_MATCH_TOLERANCE_SECONDS &&
                abs(session.medianBootSeconds - expectedBootSeconds) <= BOOT_MATCH_TOLERANCE_SECONDS &&
                session.samples.any { sample ->
                    val ageAtStatus = statusAtSeconds - sample.row.ts
                    ageAtStatus >= -BOOT_MATCH_TOLERANCE_SECONDS &&
                        ageAtStatus <= FRESH_SAMPLE_MAX_AGE_SECONDS &&
                        abs(sample.monoSeconds + ageAtStatus - uptime) <= BOOT_MATCH_TOLERANCE_SECONDS
                }
        }

        // Jeśli brak dopasowania albo dwa losowe identyfikatory pasują (np. przy
        // granicznym zbiegu uptime'u i przesunięcia UTC), nie rozstrzygamy zgadywaniem.
        val activeSessionId = activeCandidates.singleOrNull()?.id
        val activeBootSeconds = activeCandidates.singleOrNull()?.medianBootSeconds
        val recognized = LinkedHashSet<String>()

        if (activeSessionId != null && activeBootSeconds != null) {
            for (oldSession in sessions) {
                if (oldSession.id == activeSessionId || oldSession.id in recognized) continue
                if (oldSession.samples.size < 2) continue
                if (oldSession.bootSpreadSeconds > BOOT_MATCH_TOLERANCE_SECONDS) continue
                val maxRawTs = oldSession.rows.maxOfOrNull { it.ts }?.toDouble() ?: continue
                if (maxRawTs < activeBootSeconds + FUTURE_TIMESTAMP_EVIDENCE_SECONDS) continue

                val correctedTimestamps = ArrayList<Long>(oldSession.rows.size)
                var allRowsConvertible = true
                for (row in oldSession.rows) {
                    val newTs = normalizedTimestamp(row.ts)
                    if (newTs == null) {
                        allRowsConvertible = false
                        break
                    }
                    val shift = row.ts - newTs
                    if (shift !in MIN_WARSAW_OFFSET_SECONDS..MAX_WARSAW_OFFSET_SECONDS) {
                        allRowsConvertible = false
                        break
                    }
                    correctedTimestamps.add(newTs)
                }
                val correctedMax = correctedTimestamps.maxOrNull()?.toDouble()
                if (allRowsConvertible && correctedMax != null &&
                    correctedMax <= activeBootSeconds + NORMALIZED_END_TOLERANCE_SECONDS) {
                    recognized.add(oldSession.id)
                }
            }
        }

        if (recognized.isEmpty()) return Result(rows, emptySet())

        val repaired = rows.map { row ->
            val id = sessionId(row.id)
            if (id == null || id !in recognized || id == activeSessionId) return@map row
            val newTs = normalizedTimestamp(row.ts) ?: return@map row
            val shift = row.ts - newTs
            if (shift !in MIN_WARSAW_OFFSET_SECONDS..MAX_WARSAW_OFFSET_SECONDS) row else row.copy(ts = newTs)
        }.sortedWith(compareBy<TelemRow> { it.ts }.thenBy { it.seq })

        return Result(repaired, recognized)
    }

    private data class SessionSample(
        val row: TelemRow,
        val monoSeconds: Double,
        val bootSeconds: Double
    )

    private data class Session(
        val id: String,
        val rows: List<TelemRow>,
        val samples: List<SessionSample>
    ) {
        private val bootEstimates: List<Double> get() = samples.map { it.bootSeconds }
        val medianBootSeconds: Double
            get() = bootEstimates.sorted()[samples.size / 2]
        val bootSpreadSeconds: Double
            get() = (bootEstimates.maxOrNull() ?: Double.POSITIVE_INFINITY) -
                (bootEstimates.minOrNull() ?: Double.NEGATIVE_INFINITY)
    }

    private fun sessionId(recordId: String): String? =
        recordId.substringBefore('-', missingDelimiterValue = "").takeIf { it.isNotBlank() }

    private fun firmwareHasUtcTelemetry(firmware: String?): Boolean {
        val match = Regex("^\\s*v?(\\d+)\\.(\\d+)\\.(\\d+)", RegexOption.IGNORE_CASE)
            .find(firmware.orEmpty()) ?: return false
        val major = match.groupValues[1].toIntOrNull() ?: return false
        val minor = match.groupValues[2].toIntOrNull() ?: return false
        val patch = match.groupValues[3].toIntOrNull() ?: return false
        val version = Triple(major, minor, patch)
        return when {
            version.first != MIN_FIXED_UTC_FIRMWARE.first -> version.first > MIN_FIXED_UTC_FIRMWARE.first
            version.second != MIN_FIXED_UTC_FIRMWARE.second -> version.second > MIN_FIXED_UTC_FIRMWARE.second
            else -> version.third >= MIN_FIXED_UTC_FIRMWARE.third
        }
    }

    /** `ts` legacy ma pola zegara lokalnego potraktowane jak UTC; odczytujemy je jako Warsaw. */
    private fun legacyWarsawTimestamp(rawTimestampSeconds: Long, zone: ZoneId): Long? = try {
        val localFields = LocalDateTime.ofEpochSecond(rawTimestampSeconds, 0, ZoneOffset.UTC)
        // Przesunięta/przestawiana godzina przy zmianie DST nie ma jednoznacznego
        // odwzorowania bez dodatkowych danych; w takim wierszu rezygnujemy z korekty.
        val offsets = zone.rules.getValidOffsets(localFields)
        if (offsets.size != 1) null else {
            val normalized = localFields.toEpochSecond(offsets.single())
            val offset = rawTimestampSeconds - normalized
            normalized.takeIf { offset in MIN_WARSAW_OFFSET_SECONDS..MAX_WARSAW_OFFSET_SECONDS }
        }
    } catch (_: Exception) {
        null
    }
}
