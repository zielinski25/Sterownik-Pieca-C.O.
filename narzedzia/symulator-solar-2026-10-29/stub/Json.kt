// STUB org.json — tylko metody używane przez SolarAnalytics.save()/load().
package org.json

class JSONObject {
    val map = LinkedHashMap<String, Any?>()
    fun put(k: String, v: Any?): JSONObject { map[k] = v; return this }
    fun optJSONObject(k: String): JSONObject? = map[k] as? JSONObject
    fun optDouble(k: String): Double = optDouble(k, Double.NaN)
    fun optDouble(k: String, d: Double): Double = (map[k] as? Number)?.toDouble() ?: d
    fun optInt(k: String): Int = optInt(k, 0)
    fun optInt(k: String, d: Int): Int = (map[k] as? Number)?.toInt() ?: d
    fun optLong(k: String): Long = optLong(k, 0L)
    fun optLong(k: String, d: Long): Long = (map[k] as? Number)?.toLong() ?: d
    fun optString(k: String): String = optString(k, "")
    fun optString(k: String, d: String): String = map[k]?.toString() ?: d
    fun length(): Int = map.size
    fun toString(indent: Int): String = map.toString()
}
class JSONArray(text: String? = null) {
    init { /* stub: nie odtwarza JSON z tekstu */ }
    val list = ArrayList<Any?>()
    fun put(v: Any?): JSONArray { list.add(v); return this }
    fun length(): Int = list.size
    fun optJSONObject(i: Int): JSONObject? = list.getOrNull(i) as? JSONObject
    fun optString(i: Int): String = list.getOrNull(i)?.toString() ?: ""
    fun optDouble(i: Int): Double = (list.getOrNull(i) as? Number)?.toDouble() ?: Double.NaN
    fun optInt(i: Int): Int = (list.getOrNull(i) as? Number)?.toInt() ?: 0
    fun toString(indent: Int): String = list.toString()
}
