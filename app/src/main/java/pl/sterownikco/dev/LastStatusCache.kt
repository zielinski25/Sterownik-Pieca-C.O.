package pl.sterownikco.dev

import android.content.Context
import org.json.JSONObject

/**
 * STEROWNIK CO — Offline cache for last known status.
 * Fix: App shows last data even without network.
 */
object LastStatusCache {

    private const val PREFS_NAME = "last_status_cache"

    fun save(context: Context, status: JSONObject) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit()
            .putString("status", status.toString())
            .putLong("timestamp", System.currentTimeMillis())
            .apply()
    }

    fun load(context: Context): Pair<JSONObject, Long>? {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val str = prefs.getString("status", null) ?: return null
        val ts = prefs.getLong("timestamp", 0L)
        return try {
            JSONObject(str) to ts
        } catch (e: Exception) {
            null
        }
    }

    fun getAgeSeconds(context: Context): Long {
        val ts = load(context)?.second ?: return Long.MAX_VALUE
        return (System.currentTimeMillis() - ts) / 1000L
    }

    fun clear(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().clear().apply()
    }
}
