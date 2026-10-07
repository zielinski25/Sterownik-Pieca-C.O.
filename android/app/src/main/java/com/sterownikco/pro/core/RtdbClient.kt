package com.sterownikco.pro.core

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit
import kotlin.math.max

/* ══════════════════════════════════════════════════════════════════════════
   Klient Firebase RTDB (REST) + lokalnego API ESP32 — odpowiednik
   `fbGet / fbPatch / fetchWithTimeout / doFirebaseLogin / pollFirebase`
   oraz endpointów `http://<ip>/api/...` z Piec.html.
   ══════════════════════════════════════════════════════════════════════════ */

object Rtdb {
    const val DB_URL = "https://akwarium-367be-default-rtdb.europe-west1.firebasedatabase.app"
    const val AUTH_SIGNIN_URL = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key="
    const val AUTH_REFRESH_URL = "https://securetoken.googleapis.com/v1/token?key="

    private val JSON = "application/json; charset=utf-8".toMediaType()

    private val http: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .connectTimeout(8, TimeUnit.SECONDS)
            .readTimeout(10, TimeUnit.SECONDS)
            .callTimeout(15, TimeUnit.SECONDS)
            .build()
    }

    @Volatile var idToken: String = ""
    @Volatile var refreshToken: String = ""
    @Volatile var apiKey: String = Prefs.DEFAULT_FB_API_KEY
    @Volatile var cmdToken: String = Prefs.DEFAULT_CMD_TOKEN
    @Volatile var email: String = Prefs.DEFAULT_FB_EMAIL

    private var cmdSeq = 0
    fun makeFireCmdId(): String {
        cmdSeq = (cmdSeq + 1) and 0xFFFF
        val rnd = java.util.UUID.randomUUID().toString().replace("-", "").take(12)
        return java.lang.Long.toString(System.currentTimeMillis(), 36) + "-" +
            Integer.toString(cmdSeq, 36) + "-" + rnd
    }

    private fun authQuery(): String = if (idToken.isNotEmpty()) "?auth=" + java.net.URLEncoder.encode(idToken, "UTF-8") else ""

    private suspend fun execute(req: Request): Pair<Int, String> = withContext(Dispatchers.IO) {
        try {
            http.newCall(req).execute().use { r -> r.code to (r.body?.string() ?: "") }
        } catch (e: Exception) {
            -1 to (e.message ?: "błąd sieci")
        }
    }

    /** GET z automatycznym odświeżeniem idToken przy 401/403 (jak fbGet). */
    suspend fun get(path: String): GetResult {
        val (code, body) = execute(Request.Builder().url(DB_URL + path + authQuery()).header("Cache-Control", "no-store").build())
        if (code == 401 || code == 403) {
            if (refreshToken.isNotEmpty() && apiKey.isNotEmpty()) {
                refreshIdToken()
                val r2 = execute(Request.Builder().url(DB_URL + path + authQuery()).header("Cache-Control", "no-store").build())
                return if (r2.first in 200..299) GetResult(true, r2.second, null)
                else GetResult(false, "", "HTTP " + r2.first)
            }
            return GetResult(false, "", "Wymagana autoryzacja Firebase")
        }
        return if (code in 200..299) GetResult(true, body, null) else GetResult(false, body, "HTTP " + code)
    }

    data class GetResult(val ok: Boolean, val body: String, val error: String?)

    suspend fun patch(path: String, body: JSONObject): GetResult {
        val req = Request.Builder().url(DB_URL + path + authQuery())
            .patch(body.toString().toRequestBody(JSON)).build()
        val (code, text) = execute(req)
        return if (code in 200..299) GetResult(true, text, null) else GetResult(false, text, "HTTP $code")
    }

    suspend fun put(path: String, body: JSONObject): GetResult {
        val req = Request.Builder().url(DB_URL + path + authQuery())
            .put(body.toString().toRequestBody(JSON)).build()
        val (code, text) = execute(req)
        return if (code in 200..299) GetResult(true, text, null) else GetResult(false, text, "HTTP $code")
    }

    /** `accounts:signInWithPassword` — zwraca idToken/refreshToken. */
    suspend fun signIn(apiKey: String, emailIn: String, password: String): Result<JSONObject> =
        withContext(Dispatchers.IO) {
            try {
                val body = JSONObject()
                    .put("email", emailIn).put("password", password).put("returnSecureToken", true)
                val req = Request.Builder().url(AUTH_SIGNIN_URL + java.net.URLEncoder.encode(apiKey, "UTF-8"))
                    .post(body.toString().toRequestBody(JSON)).build()
                val (code, text) = execute(req)
                val j = JSONObject(text)
                if (code in 200..299 && j.has("idToken")) Result.success(j)
                else Result.failure(Exception(j.optJSONObject("error")?.optString("message") ?: "Błąd autoryzacji Firebase"))
            } catch (e: Exception) {
                Result.failure(e)
            }
        }

    suspend fun refreshIdToken() {
        if (apiKey.isEmpty() || refreshToken.isEmpty()) return
        withContext(Dispatchers.IO) {
            try {
                val body = "grant_type=refresh_token&refresh_token=" + java.net.URLEncoder.encode(refreshToken, "UTF-8")
                val req = Request.Builder().url(AUTH_REFRESH_URL + java.net.URLEncoder.encode(apiKey, "UTF-8"))
                    .post(body.toRequestBody("application/x-www-form-urlencoded".toMediaType())).build()
                val (code, text) = execute(req)
                if (code in 200..299) {
                    val j = JSONObject(text)
                    if (j.has("id_token")) {
                        idToken = j.getString("id_token")
                        if (j.has("refresh_token")) refreshToken = j.getString("refresh_token")
                    }
                }
            } catch (e: Exception) { /* ciche jak w oryginale */ }
        }
    }

    suspend fun pollStatus(): JSONObject? {
        val r = get("/piec/status.json")
        if (!r.ok) return null
        return try { if (r.body.isBlank()) null else JSONObject(r.body) } catch (e: Exception) { null }
    }

    /** `POST /piec/cmd.json` + oczekiwanie na `GET /piec/ack.json` (max 12 s). */
    suspend fun sendCommand(id: String, cmd: String): Ack? {
        val body = JSONObject().put("id", id).put("cmd", cmd).put("token", cmdToken).put("ts", System.currentTimeMillis())
        val p = patch("/piec/cmd.json", body)
        if (!p.ok) throw java.io.IOException(p.error ?: "błąd wysyłki")
        val start = System.currentTimeMillis()
        while (System.currentTimeMillis() - start < 12_000) {
            try {
                val a = get("/piec/ack.json")
                if (a.ok && a.body.isNotBlank()) {
                    val j = JSONObject(a.body)
                    if (j.optString("cmdId") == id) {
                        return Ack(j.optBoolean("ok"), j.optString("error").ifEmpty { "błąd" })
                    }
                }
            } catch (e: Exception) { /* continue */ }
            delay(1000)
        }
        return null
    }

    data class Ack(val ok: Boolean, val error: String)

    // ── lokalne API centrali (ESP32) ─────────────────────────────────────────
    private fun espUrl(ip: String, path: String): String? = if (ip.isBlank()) null else "http://" + ip.trim() + path

    private suspend fun espGet(ip: String, path: String): JSONObject? = espUrl(ip, path)?.let { u ->
        val (code, text) = execute(Request.Builder().url(u).build())
        if (code in 200..299) try { JSONObject(text) } catch (e: Exception) { null } else null
    }

    private suspend fun espPost(ip: String, path: String, body: JSONObject): JSONObject? = espUrl(ip, path)?.let { u ->
        val (code, text) = execute(Request.Builder().url(u).post(body.toString().toRequestBody(JSON)).build())
        if (code in 200..299) try { JSONObject(text) } catch (e: Exception) { null } else null
    }

    suspend fun wifiList(ip: String): JSONArray? = espGet(ip, "/api/wifi/list")?.optJSONArray("networks")
    suspend fun wifiAdd(ip: String, ssid: String, pass: String): JSONObject? =
        espPost(ip, "/api/wifi/add", JSONObject().put("ssid", ssid).put("pass", pass))
    suspend fun wifiDelete(ip: String, ssid: String): JSONObject? =
        espPost(ip, "/api/wifi/delete", JSONObject().put("ssid", ssid))
    /** `POST /api/wifi/scan/start` — true gdy centrala przyjęła zlecenie skanu. */
    suspend fun wifiScanStart(ip: String): Boolean =
        espUrl(ip, "/api/wifi/scan/start") != null &&
            withContext(Dispatchers.IO) {
                try {
                    val r = http.newCall(Request.Builder()
                        .url(espUrl(ip, "/api/wifi/scan/start")!!)
                        .post("".toRequestBody(null)).build()).execute()
                    r.use { it.isSuccessful }
                } catch (e: Exception) { false }
            }

    /** `GET /api/wifi/scan/result` — `{status:"scanning"|"done", networks:[…]}`. */
    suspend fun wifiScanResult(ip: String): JSONObject? = espGet(ip, "/api/wifi/scan/result")
    suspend fun rtcSync(ip: String, y: Int, mo: Int, d: Int, h: Int, mi: Int, s: Int): JSONObject? =
        espGet(ip, "/api/rtc-sync?rok=$y&miesiac=$mo&dzien=$d&godzina=$h&minuta=$mi&sekunda=$s")
    suspend fun terminalSend(ip: String, line: String): JSONObject? =
        espPost(ip, "/api/terminal", JSONObject().put("cmd", line))

    suspend fun latestRelease(): JSONObject? = withContext(Dispatchers.IO) {
        try {
            val req = Request.Builder()
                .url("https://api.github.com/repos/zielinski25/Sterownik-Pieca-C.O./releases/latest")
                .header("Accept", "application/vnd.github+json")
                .header("User-Agent", "SterownikPiecaHMI").build()
            val (code, text) = execute(req)
            if (code in 200..299) JSONObject(text) else null
        } catch (e: Exception) { null }
    }

    // ── telemetria historyczna (szardy dzienne + paginacja po `ts`) ──────────
    private const val HISTORY_PAGE_SIZE = 300
    private var historyIndexFallback = false

    private fun shardPath(y: Int, m: Int, d: Int) =
        "/piec/telemetry/1m/v1/$y/" + PiecState.pad2(m) + "/" + PiecState.pad2(d)

    private fun dayRange(startTs: Long, endTs: Long): List<LongArray> {
        val out = ArrayList<LongArray>()
        val cal = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone("UTC"))
        cal.timeInMillis = startTs * 1000
        cal.set(java.util.Calendar.HOUR_OF_DAY, 0); cal.set(java.util.Calendar.MINUTE, 0)
        cal.set(java.util.Calendar.SECOND, 0); cal.set(java.util.Calendar.MILLISECOND, 0)
        val lastCal = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone("UTC"))
        lastCal.timeInMillis = endTs * 1000
        lastCal.set(java.util.Calendar.HOUR_OF_DAY, 0); lastCal.set(java.util.Calendar.MINUTE, 0)
        lastCal.set(java.util.Calendar.SECOND, 0); lastCal.set(java.util.Calendar.MILLISECOND, 0)
        var cur = cal.timeInMillis
        while (cur <= lastCal.timeInMillis) {
            val c = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone("UTC"))
            c.timeInMillis = cur
            out.add(longArrayOf(cur, c.get(java.util.Calendar.YEAR).toLong(), (c.get(java.util.Calendar.MONTH) + 1).toLong(), c.get(java.util.Calendar.DAY_OF_MONTH).toLong()))
            cur += 86_400_000L
        }
        return out
    }

    private suspend fun fetchShard(path: String, startTs: Long, endTs: Long): List<TelemRow> {
        if (historyIndexFallback) return fetchShardFull(path, startTs, endTs)
        val rows = ArrayList<TelemRow>()
        var cursor = startTs
        for (guard in 0 until 100) {
            val q = path + ".json?orderBy=" + java.net.URLEncoder.encode("\"ts\"", "UTF-8") +
                "&startAt=" + cursor + "&endAt=" + endTs + "&limitToFirst=" + HISTORY_PAGE_SIZE
            val res = get(q)
            if (!res.ok) {
                val msg = res.error ?: ""
                if (msg.contains("Index not defined", true) || msg.contains("indexOn", true) || msg.contains("400")) {
                    historyIndexFallback = true
                    return fetchShardFull(path, startTs, endTs)
                }
                throw java.io.IOException(msg)
            }
            val page = ArrayList<TelemRow>()
            if (res.body.isNotBlank()) {
                val data = JSONObject(res.body)
                for (id in data.keys()) {
                    val r = parseRecord(id, data.optJSONObject(id))
                    if (r != null && r.ts in startTs..endTs) page.add(r)
                }
            }
            if (page.isEmpty()) break
            rows.addAll(page)
            if (page.size < HISTORY_PAGE_SIZE) break
            val maxTs = page.maxOf { it.ts }
            if (maxTs < cursor) break
            cursor = maxTs + 1
        }
        return uniqueSort(rows)
    }

    private suspend fun fetchShardFull(path: String, startTs: Long, endTs: Long): List<TelemRow> {
        val res = get(path + ".json")
        val rows = ArrayList<TelemRow>()
        if (res.ok && res.body.isNotBlank()) {
            val data = JSONObject(res.body)
            for (id in data.keys()) {
                val r = parseRecord(id, data.optJSONObject(id))
                if (r != null && r.ts in startTs..endTs) rows.add(r)
            }
        }
        return uniqueSort(rows)
    }

    private fun parseRecord(id: String, obj: JSONObject?): TelemRow? {
        if (obj == null) return null
        val arr = obj.optJSONArray("a") ?: JSONArray()
        val a = ArrayList<Double>(arr.length())
        for (i in 0 until arr.length()) a.add(arr.optDouble(i, 0.0))
        return TelemRow(
            id = id, seq = obj.optLong("seq", 0), ts = obj.optLong("ts", 0), mono = obj.optLong("mono", 0),
            a = a, k = obj.optLong("k", 0), s = obj.optLong("s", 0),
            state = obj.optLong("state", 0), q = obj.optLong("q", 0), sim = obj.optLong("sim", 0)
        )
    }

    private fun uniqueSort(rows: List<TelemRow>): List<TelemRow> {
        val map = LinkedHashMap<String, TelemRow>()
        rows.forEach { r -> map[r.id] = r }
        return map.values.sortedWith(compareBy({ it.ts }, { it.seq }))
    }

    /** `fetchTelemetryFromFirebase(rangeSec)` — zwraca gotowe serie lub null. */
    suspend fun fetchTelemetry(rangeSec: Int): List<TelemPoint>? {
        if (idToken.isEmpty()) return null
        val end = System.currentTimeMillis() / 1000
        val start = end - rangeSec
        val all = ArrayList<TelemRow>()
        for (day in dayRange(start, end)) {
            val dayStart = max(start, day[0] / 1000)
            val dayEnd = minOf(end, day[0] / 1000 + 86_400 - 1)
            val path = shardPath(day[1].toInt(), day[2].toInt(), day[3].toInt())
            try { all.addAll(fetchShard(path, dayStart, dayEnd)) } catch (e: Exception) { /* pusty dzień */ }
        }
        val sorted = uniqueSort(all)
        if (sorted.isEmpty()) return null
        return sorted.map { r -> r.toPoint() }
    }
}
