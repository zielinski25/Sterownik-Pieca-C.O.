package com.sterownikco.pro.core

import java.time.Instant
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TelemetryTimeRepairTest {
    private val boot = Instant.parse("2026-10-15T10:00:00Z").epochSecond
    private val uptimeSeconds = 1_800.0
    private val statusAtMs = (boot + uptimeSeconds.toLong()) * 1000L

    private fun row(session: String, ts: Long, monoMs: Long, seq: Long = 1) = TelemRow(
        id = "$session-${seq.toString(16).padStart(8, '0')}",
        seq = seq,
        ts = ts,
        mono = monoMs,
        a = listOf(100.0, 500.0, 700.0, 700.0, 400.0, 300.0, 200.0, 100.0, 500.0, 10000.0, 100.0),
        k = 90,
        s = 45,
        state = 1,
        q = 0,
        sim = 0,
        stateKnown = true
    )

    private fun currentSessionRows() = listOf(
        row("current", boot + 1_740, 1_740_000, 30),
        row("current", boot + 1_800, 1_800_000, 31)
    )

    @Test
    fun correctsOnlyASeparateLegacySessionWhoseRawTimesCrossTheCurrentBoot() {
        // Stary zapis traktował warszawskie pola zegara jako UTC (+2 h w październiku).
        val legacyRows = listOf(
            row("legacy", boot + 6_600, 3_000_000, 90), // rzeczywisty czas: boot - 10 min
            row("legacy", boot + 7_140, 3_540_000, 91)  // rzeczywisty czas: boot - 1 min
        )
        val input = currentSessionRows() + legacyRows

        val result = TelemetryTimeRepair.normalize(
            rows = input,
            firmware = "v3.47.3",
            deviceUptimeSeconds = uptimeSeconds,
            statusObservedAtMs = statusAtMs
        )

        val correctedLegacy = result.rows.filter { it.id.startsWith("legacy-") }
        assertEquals(setOf("legacy"), result.legacySessionIds)
        assertEquals(listOf(boot - 600, boot - 60), correctedLegacy.map { it.ts })
        assertEquals(listOf(boot + 1_740, boot + 1_800), result.rows.filter { it.id.startsWith("current-") }.map { it.ts })
        assertTrue(result.rows.zipWithNext().all { (a, b) -> a.ts <= b.ts })
    }

    @Test
    fun correctsLegacyRowsUsingTheWinterWarsawOffset() {
        val winterBoot = Instant.parse("2026-01-15T10:00:00Z").epochSecond
        val winterUptime = 1_800.0
        val winterStatusAtMs = (winterBoot + winterUptime.toLong()) * 1000L
        val current = listOf(
            row("wintercurrent", winterBoot + 1_740, 1_740_000, 30),
            row("wintercurrent", winterBoot + 1_800, 1_800_000, 31)
        )
        val legacy = listOf(
            row("winterlegacy", winterBoot + 3_000, 3_000_000, 90),
            row("winterlegacy", winterBoot + 3_540, 3_540_000, 91)
        )

        val result = TelemetryTimeRepair.normalize(
            current + legacy, "v3.47.3", winterUptime, winterStatusAtMs
        )

        assertEquals(setOf("winterlegacy"), result.legacySessionIds)
        assertEquals(listOf(winterBoot - 600, winterBoot - 60), result.rows
            .filter { it.id.startsWith("winterlegacy-") }.map { it.ts })
    }

    @Test
    fun leavesAWellOrderedPreviousSessionUntouched() {
        val previous = listOf(
            row("previous", boot - 600, 3_000_000, 90),
            row("previous", boot - 60, 3_540_000, 91)
        )
        val input = currentSessionRows() + previous

        val result = TelemetryTimeRepair.normalize(
            input, "v3.47.3", uptimeSeconds, statusAtMs
        )

        assertTrue(result.legacySessionIds.isEmpty())
        assertEquals(previous.map { it.ts }, result.rows.filter { it.id.startsWith("previous-") }.map { it.ts })
    }

    @Test
    fun leavesTheCurrentSessionAndItsTelemetryValuesUntouched() {
        val current = currentSessionRows()

        val result = TelemetryTimeRepair.normalize(
            current, "v3.47.3", uptimeSeconds, statusAtMs
        )

        assertTrue(result.legacySessionIds.isEmpty())
        assertEquals(current, result.rows)
    }

    @Test
    fun refusesToGuessWhenTwoSessionIdsMatchTheCurrentStatus() {
        // Graniczny przypadek: poprzednia sesja zaczęła się dokładnie 2 h wcześniej.
        // Jej legacy ts/mono mogą wtedy naśladować sesję bieżącą; wynik ma pozostać bez zmian.
        val ambiguousLegacy = listOf(
            row("ambiguous", boot + 1_740, 1_740_000, 70),
            row("ambiguous", boot + 1_800, 1_800_000, 71)
        )
        val input = currentSessionRows() + ambiguousLegacy

        val result = TelemetryTimeRepair.normalize(
            input, "v3.47.3", uptimeSeconds, statusAtMs
        )

        assertTrue(result.legacySessionIds.isEmpty())
        assertEquals(input.map { it.ts }, result.rows.associateBy { it.id }.let { rows -> input.map { rows.getValue(it.id).ts } })
    }

    @Test
    fun doesNotRepairWhenCurrentSessionCannotBeIdentified() {
        val onlyLegacy = listOf(
            row("legacy", boot + 6_600, 3_000_000, 90),
            row("legacy", boot + 7_140, 3_540_000, 91)
        )

        val result = TelemetryTimeRepair.normalize(
            onlyLegacy, "v3.47.3", uptimeSeconds, statusAtMs
        )

        assertTrue(result.legacySessionIds.isEmpty())
        assertEquals(onlyLegacy, result.rows)
    }

    @Test
    fun doesNotCorrectARecordInAnAmbiguousDstHour() {
        val dstBoot = Instant.parse("2026-10-25T01:35:00Z").epochSecond
        val dstUptime = 600.0
        val dstStatusAtMs = (dstBoot + dstUptime.toLong()) * 1000L
        val current = listOf(
            row("dstcurrent", dstBoot + 540, 540_000, 30),
            row("dstcurrent", dstBoot + 600, 600_000, 31)
        )
        val ambiguousLegacy = listOf(
            row("dstlegacy", Instant.parse("2026-10-25T02:29:00Z").epochSecond, 1_740_000, 89),
            row("dstlegacy", Instant.parse("2026-10-25T02:30:00Z").epochSecond, 1_800_000, 90)
        )

        val result = TelemetryTimeRepair.normalize(
            current + ambiguousLegacy, "v3.47.3", dstUptime, dstStatusAtMs
        )

        assertTrue(result.legacySessionIds.isEmpty())
        assertEquals(
            ambiguousLegacy.map { it.ts },
            result.rows.filter { it.id.startsWith("dstlegacy-") }.map { it.ts }
        )
    }

    @Test
    fun firmwareGateAndQueryLookaheadAreConservative() {
        val end = boot + 1_800

        assertEquals(end, TelemetryTimeRepair.queryEndSeconds(end, "v3.47.1", uptimeSeconds, statusAtMs))
        assertEquals(end + 7_200, TelemetryTimeRepair.queryEndSeconds(end, "v3.47.2", uptimeSeconds, statusAtMs))
        assertEquals(end, TelemetryTimeRepair.queryEndSeconds(end, "v3.47.3", Double.NaN, statusAtMs))
        assertEquals(end, TelemetryTimeRepair.queryEndSeconds(end, "v3.47.3", uptimeSeconds, null))
        assertFalse(TelemetryTimeRepair.canAttempt("unknown", uptimeSeconds, statusAtMs))
    }
}
