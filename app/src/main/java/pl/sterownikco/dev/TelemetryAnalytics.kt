package pl.sterownikco.dev

import kotlin.math.abs
import kotlin.math.pow
import kotlin.math.sqrt

/** Pure history calculations. RAW Firebase telemetry is never changed. */
object TelemetryAnalytics {
    data class SeriesStats(val min: Double, val max: Double, val average: Double, val first: Double, val last: Double, val delta: Double, val stdev: Double)
    data class CycleStats(val cycles: Int, val averageMinutes: Double, val longestMinutes: Double, val currentState: String)

    fun stats(values: List<Double>): SeriesStats? {
        if (values.isEmpty()) return null
        val min = values.minOrNull() ?: return null
        val max = values.maxOrNull() ?: return null
        val average = values.average()
        val variance = values.map { (it - average).pow(2) }.average()
        return SeriesStats(min, max, average, values.first(), values.last(), values.last() - values.first(), sqrt(variance))
    }

    /** History-only cycle detection; it is not a controller setting. */
    fun heatingCycles(rows: List<TelemetryRecord>, channel: Int, threshold: Double = 45.0, hysteresis: Double = 2.0): CycleStats {
        if (rows.size < 2) return CycleStats(0, 0.0, 0.0, "BRAK DANYCH")
        val sorted = rows.sortedBy { it.ts }
        var active = false
        var start = 0L
        val durations = mutableListOf<Double>()
        for (r in sorted) {
            val v = r.a.getOrNull(channel)?.takeIf { it.isFinite() }?.times(0.1) ?: continue
            if (!active && v >= threshold) { active = true; start = r.ts }
            else if (active && v <= threshold - hysteresis) {
                durations += (r.ts - start).coerceAtLeast(0L) / 60.0
                active = false
            }
        }
        if (active && start > 0L) durations += (sorted.last().ts - start).coerceAtLeast(0L) / 60.0
        return CycleStats(durations.size, if (durations.isEmpty()) 0.0 else durations.average(), durations.maxOrNull() ?: 0.0, if (active) "PRACA" else "POSTÓJ")
    }

    fun subtract(a: List<Double>, b: List<Double>): List<Double> {
        val n = minOf(a.size, b.size)
        return (0 until n).map { a[it] - b[it] }
    }

    fun mad(values: List<Double>): Double? {
        if (values.isEmpty()) return null
        val ordered = values.sorted()
        val median = ordered[ordered.size / 2]
        val deviations = ordered.map { abs(it - median) }.sorted()
        return deviations[deviations.size / 2]
    }
}
