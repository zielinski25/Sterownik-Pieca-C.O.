package pl.sterownikco.dev

import android.content.Context
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.*

/**
 * STEROWNIK CO — Log storage and viewer.
 * Stores diagnostic logs locally and provides filtering.
 */
object LogStorage {

    private const val PREFS_NAME = "sterownik_logs"
    private const val MAX_LOGS = 500

    enum class Level { INFO, WARN, ERROR, COMMAND }

    data class LogEntry(
        val timestamp: Long,
        val level: Level,
        val tag: String,
        val message: String
    )

    fun save(context: Context, entry: LogEntry) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val logs = loadAll(context).toMutableList()
        logs.add(0, entry)
        if (logs.size > MAX_LOGS) {
            logs.subList(MAX_LOGS, logs.size).clear()
        }
        val editor = prefs.edit()
        editor.clear()
        logs.forEachIndexed { index, log ->
            editor.putString("log_$index", serialize(log))
        }
        editor.apply()
    }

    fun loadAll(context: Context): List<LogEntry> {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val logs = mutableListOf<LogEntry>()
        var i = 0
        while (true) {
            val key = "log_$i"
            val value = prefs.getString(key, null) ?: break
            deserialize(value)?.let { logs.add(it) }
            i++
        }
        return logs
    }

    fun clear(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().clear().apply()
    }

    fun filter(context: Context, level: Level? = null, tag: String? = null): List<LogEntry> {
        return loadAll(context).filter { log ->
            (level == null || log.level == level) && (tag == null || log.tag.contains(tag, ignoreCase = true))
        }
    }

    fun info(context: Context, tag: String, message: String) {
        save(context, LogEntry(System.currentTimeMillis(), Level.INFO, tag, message))
    }

    fun warn(context: Context, tag: String, message: String) {
        save(context, LogEntry(System.currentTimeMillis(), Level.WARN, tag, message))
    }

    fun error(context: Context, tag: String, message: String) {
        save(context, LogEntry(System.currentTimeMillis(), Level.ERROR, tag, message))
    }

    fun command(context: Context, tag: String, message: String) {
        save(context, LogEntry(System.currentTimeMillis(), Level.COMMAND, tag, message))
    }

    private fun serialize(log: LogEntry): String {
        val json = JSONObject()
        json.put("ts", log.timestamp)
        json.put("level", log.level.name)
        json.put("tag", log.tag)
        json.put("message", log.message)
        return json.toString()
    }

    private fun deserialize(str: String): LogEntry? {
        return try {
            val json = JSONObject(str)
            LogEntry(
                timestamp = json.optLong("ts", 0),
                level = Level.valueOf(json.optString("level", "INFO")),
                tag = json.optString("tag", ""),
                message = json.optString("message", "")
            )
        } catch (e: Exception) {
            null
        }
    }

    fun formatTimestamp(ts: Long): String {
        val sdf = SimpleDateFormat("dd.MM HH:mm:ss", Locale.US)
        sdf.timeZone = TimeZone.getTimeZone("Europe/Warsaw")
        return sdf.format(Date(ts))
    }
}
