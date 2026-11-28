package com.sterownikco.pro.core

import kotlin.math.roundToLong

/** Time-based chart viewport. Offset 1 shows the oldest edge; offset 0 the newest edge. */
data class ChartTimeWindow(
    val domainStartMs: Long,
    val domainEndMs: Long,
    val startMs: Long,
    val endMs: Long
) {
    val domainSpanMs: Long get() = (domainEndMs - domainStartMs).coerceAtLeast(1L)
    val visibleSpanMs: Long get() = (endMs - startMs).coerceAtLeast(1L)
    val startFraction: Float
        get() = ((startMs - domainStartMs).toDouble() / domainSpanMs).toFloat().coerceIn(0f, 1f)
    val visibleFraction: Float
        get() = (visibleSpanMs.toDouble() / domainSpanMs).toFloat().coerceIn(0f, 1f)
    val maxStartFraction: Float get() = (1f - visibleFraction).coerceAtLeast(0f)

    companion object {
        fun calculate(domainStartMs: Long, requestedEndMs: Long, zoom: Float, offset: Float): ChartTimeWindow {
            val domainEndMs = if (requestedEndMs > domainStartMs) requestedEndMs else domainStartMs + 1L
            val domainSpanMs = (domainEndMs - domainStartMs).coerceAtLeast(1L)
            val safeZoom = zoom.takeIf { it.isFinite() && it > 0f }?.coerceIn(1f, 16f) ?: 1f
            val visibleSpanMs = (domainSpanMs.toDouble() / safeZoom.toDouble())
                .roundToLong().coerceIn(1L, domainSpanMs)
            val maxStartMs = domainSpanMs - visibleSpanMs
            val safeOffset = offset.takeIf { it.isFinite() }?.coerceIn(0f, 1f) ?: 1f
            val position = ((1f - safeOffset) * maxStartMs).roundToLong().coerceIn(0L, maxStartMs)
            val startMs = domainStartMs + position
            return ChartTimeWindow(domainStartMs, domainEndMs, startMs, startMs + visibleSpanMs)
        }
    }
}

/** Maps a sample timestamp into the visible plot interval (not into its list index). */
fun chartTimestampFraction(timestampMs: Long, startMs: Long, endMs: Long): Float {
    val spanMs = (endMs - startMs).coerceAtLeast(1L)
    return ((timestampMs - startMs).toDouble() / spanMs).toFloat().coerceIn(0f, 1f)
}

/** Nearest entry in a timestamp-sorted list; ties resolve to the earlier sample. */
fun nearestChartTimestampIndex(sortedTimestamps: List<Long>, targetMs: Long): Int? {
    if (sortedTimestamps.isEmpty()) return null
    var low = 0
    var high = sortedTimestamps.lastIndex
    while (low < high) {
        val middle = (low + high) ushr 1
        if (sortedTimestamps[middle] < targetMs) low = middle + 1 else high = middle
    }
    if (low > 0 && targetMs - sortedTimestamps[low - 1] <= sortedTimestamps[low] - targetMs) return low - 1
    return low
}
