package pl.sterownikco.dev

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.Executors

/**
 * STEROWNIK CO — Local history cache.
 * Stores recent telemetry locally for offline access and fast display.
 */
object TelemetryCache {

    private const val PREFS_NAME = "telemetry_cache"
    private const val MAX_RECORDS = 1000
    private const val MAX_AGE_MS = 7 * 24 * 60 * 60 * 1000L // 7 days

    @Volatile private var executor: java.util.concurrent.ExecutorService? = null

    private fun getExecutor(): java.util.concurrent.ExecutorService {
        executor?.let { if (!it.isShutdown) return it }
        val e = Executors.newSingleThreadExecutor()
        executor = e
        return e
    }

    // Fix: Memory leak — allow shutdown of thread pool
    fun shutdown() {
        executor?.shutdownNow()
        executor = null
    }

    interface Callback {
        fun onRecords(records: List<TelemetryRecord>)
    }

    fun cacheRecords(context: Context, records: List<TelemetryRecord>) {
        getExecutor().execute {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val existing = loadRecordsSync(context).toMutableList()

            // Merge new records, dedup by id
            val ids = existing.map { it.id }.toHashSet()
            for (record in records) {
                if (!ids.contains(record.id)) {
                    existing.add(record)
                    ids.add(record.id)
                }
            }

            // Sort by timestamp
            existing.sortBy { it.ts }

            // Trim old records
            val cutoff = System.currentTimeMillis() - MAX_AGE_MS
            existing.removeAll { it.ts * 1000 < cutoff }

            // Trim to max size
            if (existing.size > MAX_RECORDS) {
                existing.subList(0, existing.size - MAX_RECORDS).clear()
            }

            // Save
            val editor = prefs.edit()
            editor.clear()
            val array = JSONArray()
            for (record in existing) {
                val obj = JSONObject()
                obj.put("id", record.id)
                obj.put("ts", record.ts)
                obj.put("seq", record.seq)
                obj.put("mono", record.mono)
                obj.put("k", record.k)
                obj.put("s", record.s)
                obj.put("state", record.state)
                obj.put("q", record.q)
                obj.put("sim", record.sim)

                val aArr = JSONArray()
                record.a.forEach { if (it.isFinite()) aArr.put(it) else aArr.put(JSONObject.NULL) }
                obj.put("a", aArr)

                val ageArr = JSONArray()
                record.age.forEach { ageArr.put(it) }
                obj.put("age", ageArr)

                array.put(obj)
            }
            editor.putString("records", array.toString())
            editor.putLong("last_update", System.currentTimeMillis())
            editor.apply()
        }
    }

    fun loadRecords(context: Context, callback: Callback) {
        getExecutor().execute {
            val records = loadRecordsSync(context)
            android.os.Handler(android.os.Looper.getMainLooper()).post {
                callback.onRecords(records)
            }
        }
    }

    fun loadRecordsSync(context: Context): List<TelemetryRecord> {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val str = prefs.getString("records", null) ?: return emptyList()
        return try {
            val array = JSONArray(str)
            val records = mutableListOf<TelemetryRecord>()
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i)
                val a = mutableListOf<Double>()
                val aArr = obj.optJSONArray("a")
                if (aArr != null) {
                    for (j in 0 until aArr.length()) {
                        if (aArr.isNull(j)) a.add(Double.NaN)
                        else a.add(aArr.optDouble(j, Double.NaN))
                    }
                }
                val age = mutableListOf<Long>()
                val ageArr = obj.optJSONArray("age")
                if (ageArr != null) {
                    for (j in 0 until ageArr.length()) {
                        age.add(ageArr.optLong(j, 65535L))
                    }
                }
                records.add(
                    TelemetryRecord(
                        id = obj.optString("id", ""),
                        session = obj.optString("id", "").substringBefore("-", ""),
                        seq = obj.optLong("seq", 0L),
                        ts = obj.optLong("ts", 0L),
                        mono = obj.optLong("mono", 0L),
                        a = a,
                        k = obj.optDouble("k", Double.NaN),
                        s = obj.optDouble("s", Double.NaN),
                        state = obj.optLong("state", 0L),
                        q = obj.optLong("q", 0L),
                        sim = obj.optLong("sim", 0L),
                        age = age
                    )
                )
            }
            records
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun getLastUpdate(context: Context): Long {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getLong("last_update", 0L)
    }

    fun clear(context: Context) {
        getExecutor().execute {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            prefs.edit().clear().apply()
        }
    }
}
