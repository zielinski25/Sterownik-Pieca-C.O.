package pl.sterownikco.dev

import org.json.JSONObject
import java.time.Instant
import java.time.ZoneOffset
import java.util.Locale

/** Wire V1: seq, ts, mono, a[11], k, s, state, q, sim, age[11]. */
data class TelemetryRecord(
    val id: String,
    val session: String,
    val seq: Long,
    val ts: Long,
    val mono: Long,
    val a: List<Double>,
    val k: Double,
    val s: Double,
    val state: Long,
    val q: Long,
    val sim: Long,
    val age: List<Long>,
)

object TelemetryParser {
    fun parse(id: String, value: JSONObject): TelemetryRecord? {
        if (!value.has("ts") || !value.has("seq")) return null
        val a = buildList {
            value.optJSONArray("a")?.let { arr ->
                for (i in 0 until arr.length()) add(arr.optDouble(i, Double.NaN))
            }
        }
        val dash = id.indexOf('-')
        return TelemetryRecord(
            id = id,
            session = if (dash > 0) id.substring(0, dash) else "",
            seq = value.optLong("seq", 0L),
            ts = value.optLong("ts", 0L),
            mono = value.optLong("mono", 0L),
            a = a,
            k = value.optDouble("k", Double.NaN),
            s = value.optDouble("s", Double.NaN),
            state = value.optLong("state", 0L),
            q = value.optLong("q", 0L),
            sim = value.optLong("sim", 0L),
            age = buildList {
                value.optJSONArray("age")?.let { arr ->
                    for (i in 0 until arr.length()) add(arr.optLong(i, 65535L))
                }
            },
        )
    }
}

object TelemetryHistoryRepository {
    fun fetchRange(auth: FirebaseAuthRest, idToken: String, startTs: Long, endTs: Long): List<TelemetryRecord> {
        if (endTs <= startTs) return emptyList()
        val all = ArrayList<TelemetryRecord>()
        var day = Instant.ofEpochSecond(startTs).atZone(ZoneOffset.UTC).toLocalDate()
        val last = Instant.ofEpochSecond(endTs).atZone(ZoneOffset.UTC).toLocalDate()
        while (!day.isAfter(last)) {
            val dayPath = String.format(Locale.US, "%04d/%02d/%02d", day.year, day.monthValue, day.dayOfMonth)
            val dayStart = maxOf(startTs, day.atStartOfDay(ZoneOffset.UTC).toEpochSecond())
            val dayEnd = minOf(endTs, day.plusDays(1).atStartOfDay(ZoneOffset.UTC).toEpochSecond() - 1L)
            val data = try {
                auth.getJson(auth.historyUrl(dayPath, dayStart, dayEnd), idToken)
            } catch (_: FirebaseAuthRest.HistoryQueryUnsupported) {
                auth.getJson(auth.historyDayUrl(dayPath), idToken)
            }
            if (data != null) {
                val keys = data.keys()
                while (keys.hasNext()) {
                    val id = keys.next()
                    val obj = data.optJSONObject(id) ?: continue
                    val record = TelemetryParser.parse(id, obj) ?: continue
                    if (record.ts in startTs..endTs) all += record
                }
            }
            day = day.plusDays(1)
        }
        return all.distinctBy { it.id }.sortedWith(compareBy<TelemetryRecord> { it.ts }.thenBy { it.seq })
    }
}
