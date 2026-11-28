package com.sterownikco.pro.core

/** Merge overlapping RTDB archive reads by record id, preserving a stable time order. */
object ChartArchive {
    fun merge(
        existing: List<TelemPoint>,
        incoming: List<TelemPoint>,
        startMs: Long,
        endMs: Long
    ): List<TelemPoint> {
        val byId = LinkedHashMap<String, TelemPoint>()
        (existing.asSequence() + incoming.asSequence()).forEach { point ->
            if (point.ts !in startMs..endMs) return@forEach
            val key = point.sourceId.takeIf { it.isNotBlank() } ?: "${point.ts}:${point.seq}"
            byId[key] = point
        }
        return byId.values.sortedWith(compareBy<TelemPoint>({ it.ts }, { it.seq }, { it.sourceId }))
    }
}
