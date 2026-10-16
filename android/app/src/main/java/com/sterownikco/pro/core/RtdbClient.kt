package com.sterownikco.pro.core

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.job
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import android.util.Base64
import okhttp3.HttpUrl.Companion.toHttpUrl
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
    private val authRefreshMutex = Mutex()
    // /piec/cmd is a single-slot queue with one shared /piec/ack record.
    private val commandSendMutex = Mutex()

    private val http: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .connectTimeout(8, TimeUnit.SECONDS)
            .readTimeout(10, TimeUnit.SECONDS)
            .callTimeout(15, TimeUnit.SECONDS)
            .build()
    }

    /** Local controller requests can contain credentials: connect directly, never proxy or follow redirects. */
    private val espHttp: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .connectTimeout(8, TimeUnit.SECONDS)
            .readTimeout(10, TimeUnit.SECONDS)
            .callTimeout(15, TimeUnit.SECONDS)
            .proxy(java.net.Proxy.NO_PROXY)
            .followRedirects(false)
            .followSslRedirects(false)
            .retryOnConnectionFailure(false)
            .build()
    }

    /** Klient strumienia SSE — bez limitów czasu (jedno połączenie wisi godzinami). */
    private val streamHttp: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .connectTimeout(20, TimeUnit.SECONDS)
            .readTimeout(0, TimeUnit.SECONDS)
            .callTimeout(0, TimeUnit.SECONDS)
            .retryOnConnectionFailure(true)
            .build()
    }

    private data class AuthState(
        val apiKey: String = Prefs.DEFAULT_FB_API_KEY,
        val idToken: String = "",
        val refreshToken: String = ""
    )
    private val authStateLock = Any()
    @Volatile private var authState = AuthState()

    val idToken: String get() = authState.idToken
    val refreshToken: String get() = authState.refreshToken
    val apiKey: String get() = authState.apiKey

    /** Firebase ID token subject is the UID used by the controller's per-user diagnostics branch. */
    fun currentFirebaseUid(): String? {
        val payload = idToken.split('.').getOrNull(1) ?: return null
        return try {
            val decoded = Base64.decode(payload, Base64.URL_SAFE or Base64.NO_WRAP)
            val claims = JSONObject(String(decoded, Charsets.UTF_8))
            val uid = claims.optString("user_id").ifBlank { claims.optString("sub") }
            uid.takeIf { it.matches(Regex("[A-Za-z0-9_-]{6,128}")) }
        } catch (_: Exception) {
            null
        }
    }

    fun setAuthTokens(idToken: String, refreshToken: String) = synchronized(authStateLock) {
        authState = authState.copy(idToken = idToken, refreshToken = refreshToken)
    }

    fun setAuthSession(apiKey: String, idToken: String, refreshToken: String) = synchronized(authStateLock) {
        authState = AuthState(apiKey, idToken, refreshToken)
    }

    /** Installed by Android owners so rotated Firebase tokens survive process restart. */
    @Volatile var onAuthTokensRefreshed: ((String, String) -> Unit)? = null
    @Volatile var cmdToken: String = Prefs.DEFAULT_CMD_TOKEN
    @Volatile var email: String = Prefs.DEFAULT_FB_EMAIL

    private var cmdSeq = 0
    fun makeFireCmdId(): String {
        cmdSeq = (cmdSeq + 1) and 0xFFFF
        val rnd = java.util.UUID.randomUUID().toString().replace("-", "").take(12)
        return java.lang.Long.toString(System.currentTimeMillis(), 36) + "-" +
            Integer.toString(cmdSeq, 36) + "-" + rnd
    }

    private fun authQuery(token: String = idToken): String =
        if (token.isNotEmpty()) "?auth=" + java.net.URLEncoder.encode(token, "UTF-8") else ""

    private suspend fun execute(req: Request): Pair<Int, String> = withContext(Dispatchers.IO) {
        try {
            val call = http.newCall(req)
            coroutineContext.job.invokeOnCompletion { try { call.cancel() } catch (e: Exception) { /* ignore */ } }
            call.execute().use { r -> r.code to (r.body?.string() ?: "") }
        } catch (e: Exception) {
            -1 to (e.message ?: "błąd sieci")
        }
    }

    private suspend fun executeEsp(req: Request): Pair<Int, String> = withContext(Dispatchers.IO) {
        try {
            val call = espHttp.newCall(req)
            coroutineContext.job.invokeOnCompletion { try { call.cancel() } catch (e: Exception) { /* ignore */ } }
            call.execute().use { r -> r.code to (r.body?.string() ?: "") }
        } catch (e: Exception) {
            -1 to (e.message ?: "błąd sieci")
        }
    }

    /** GET z automatycznym, serializowanym odświeżeniem idToken przy 401/403. */
    suspend fun get(path: String): GetResult = get(path, emptyMap())

    /** GET z parametrami RTDB (np. `shallow=true`, `orderBy` i `limitToLast`). */
    suspend fun get(path: String, queryParameters: Map<String, String>): GetResult {
        val requestTokens = authState
        val (code, body) = execute(Request.Builder()
            .url(rtdbUrl(path, queryParameters, requestTokens.idToken))
            .header("Cache-Control", "no-store").build())
        if (code == 401 || code == 403) {
            if (requestTokens.refreshToken.isNotEmpty() && requestTokens.apiKey.isNotEmpty()) {
                refreshIdToken(requestTokens.idToken, requestTokens.refreshToken)
                val retryToken = authState.idToken
                val r2 = execute(Request.Builder().url(rtdbUrl(path, queryParameters, retryToken))
                    .header("Cache-Control", "no-store").build())
                return if (r2.first in 200..299) GetResult(true, r2.second, null)
                else GetResult(false, r2.second, "HTTP " + r2.first)
            }
            return GetResult(false, "", "Wymagana autoryzacja Firebase")
        }
        return if (code in 200..299) GetResult(true, body, null) else GetResult(false, body, "HTTP " + code)
    }

    private fun rtdbUrl(path: String, queryParameters: Map<String, String>, token: String): String {
        val builder = (DB_URL + path).toHttpUrl().newBuilder()
        queryParameters.forEach { (name, value) -> builder.addQueryParameter(name, value) }
        if (token.isNotEmpty()) builder.addQueryParameter("auth", token)
        return builder.build().toString()
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

    suspend fun refreshIdToken(expectedIdToken: String? = null, expectedRefreshToken: String? = null) {
        authRefreshMutex.withLock {
            val currentAuth = authState
            if (currentAuth.apiKey.isEmpty() || currentAuth.refreshToken.isEmpty()) return@withLock
            if (expectedIdToken != null && currentAuth.idToken != expectedIdToken) return@withLock
            if (expectedRefreshToken != null && currentAuth.refreshToken != expectedRefreshToken) return@withLock

            val body = "grant_type=refresh_token&refresh_token=" +
                java.net.URLEncoder.encode(currentAuth.refreshToken, "UTF-8")
            val req = Request.Builder().url(AUTH_REFRESH_URL + java.net.URLEncoder.encode(currentAuth.apiKey, "UTF-8"))
                .post(body.toRequestBody("application/x-www-form-urlencoded".toMediaType())).build()
            val (code, text) = execute(req)
            if (code !in 200..299) return@withLock
            val response = try { JSONObject(text) } catch (e: Exception) { return@withLock }
            val nextIdToken = response.optString("id_token")
            if (nextIdToken.isBlank()) return@withLock
            val nextRefreshToken = response.optString("refresh_token")
                .ifBlank { currentAuth.refreshToken }

            // Commit and persist under the same lock used by login/logout session replacement.
            val committed = synchronized(authStateLock) {
                if (authState !== currentAuth) {
                    false
                } else {
                    authState = currentAuth.copy(idToken = nextIdToken, refreshToken = nextRefreshToken)
                    try {
                        onAuthTokensRefreshed?.invoke(nextIdToken, nextRefreshToken)
                    } catch (e: Exception) { /* keep the in-memory session; persistence can be retried later */ }
                    true
                }
            }
            if (!committed) return@withLock
        }
    }

    suspend fun pollStatus(): JSONObject? {
        val r = get("/piec/status.json")
        if (!r.ok) return null
        return try { if (r.body.isBlank()) null else JSONObject(r.body) } catch (e: Exception) { null }
    }

    /**
     * Pomocniczy strumień SSE z RTDB. AlarmMonitorService celowo go nie używa:
     * monitorowanie w tle wykonuje jeden GET na minutę, zgodnie z wymaganą tolerancją opóźnienia.
     * Blokuje do anulowania korutyny / błędu sieci / `auth_revoked`.
     * [onEvent] wołane na wątku IO z eventem, JSON `data:` oraz parą tokenów używaną przez ten strumień.
     */
    internal suspend fun streamEvents(
        path: String,
        onEvent: (event: String, data: String, idToken: String, refreshToken: String) -> Unit
    ) = withContext(Dispatchers.IO) {
        val streamTokens = authState
        val req = Request.Builder().url(DB_URL + path + authQuery(streamTokens.idToken))
            .header("Accept", "text/event-stream")
            .header("Cache-Control", "no-store").build()
        val call = streamHttp.newCall(req)
        coroutineContext.job.invokeOnCompletion { try { call.cancel() } catch (e: Exception) { /* ignore */ } }
        call.execute().use { r ->
            if (!r.isSuccessful) throw java.io.IOException("HTTP " + r.code)
            val reader = r.body?.byteStream()?.bufferedReader()
                ?: throw java.io.IOException("pusty strumień")
            var ev = ""
            while (true) {
                coroutineContext.ensureActive()
                // readLine() blokuje do następnej linii — cancel() połączenia je przerywa.
                val line = try { reader.readLine() } catch (e: Exception) {
                    throw java.io.IOException("strumień przerwany")
                } ?: throw java.io.IOException("strumień zamknięty (EOF)")
                when {
                    line.startsWith("event:") -> ev = line.substring(6).trim()
                    line.startsWith("data:") -> {
                        val d = line.substring(5).trim()
                        if (ev.isNotEmpty() && d.isNotEmpty() && d != "null") {
                            onEvent(ev, d, streamTokens.idToken, streamTokens.refreshToken)
                        }
                        ev = ""
                    }
                    // puste linie = granice ramek SSE, ignorujemy
                }
            }
        }
    }

    /** `POST /piec/cmd.json` + oczekiwanie na `GET /piec/ack.json` (max 12 s). */
    suspend fun sendCommand(id: String, cmd: String): Ack? = commandSendMutex.withLock {
        val body = JSONObject().put("id", id).put("cmd", cmd).put("token", cmdToken).put("ts", System.currentTimeMillis())
        val p = patch("/piec/cmd.json", body)
        if (!p.ok) throw java.io.IOException(p.error ?: "błąd wysyłki")
        val start = System.currentTimeMillis()
        while (System.currentTimeMillis() - start < 12_000) {
            try {
                val a = get("/piec/ack.json")
                if (a.ok && a.body.isNotBlank()) {
                    val j = JSONObject(a.body)
                    if (j.optString("cmdId") == id && j.opt("ok") is Boolean) {
                        return@withLock Ack(j.getBoolean("ok"), j.optString("error").ifEmpty { "błąd" })
                    }
                }
            } catch (e: Exception) { /* continue */ }
            delay(1000)
        }
        null
    }

    data class Ack(val ok: Boolean, val error: String)

    // ── lokalne API centrali (ESP32) ─────────────────────────────────────────
    internal fun localControllerUrl(ip: String, path: String): String? =
        LocalControllerAddress.url(ip, path)

    /** Use the no-proxy local client for credential-bearing controller POSTs too. */
    internal suspend fun postLocalController(ip: String, path: String, body: String): Pair<Int, String> {
        val url = localControllerUrl(ip, path) ?: return -2 to "invalid local controller address"
        val request = Request.Builder().url(url).post(body.toRequestBody(null)).build()
        return executeEsp(request)
    }

    private fun espUrl(ip: String, path: String): String? = localControllerUrl(ip, path)

    private suspend fun espGet(ip: String, path: String): JSONObject? = espUrl(ip, path)?.let { u ->
        val (code, text) = executeEsp(Request.Builder().url(u).build())
        if (code in 200..299) try { JSONObject(text) } catch (e: Exception) { null } else null
    }

    private suspend fun espPost(ip: String, path: String, body: JSONObject): JSONObject? = espUrl(ip, path)?.let { u ->
        val (code, text) = executeEsp(Request.Builder().url(u).post(body.toString().toRequestBody(JSON)).build())
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
                    val call = espHttp.newCall(Request.Builder()
                        .url(espUrl(ip, "/api/wifi/scan/start")!!)
                        .post("".toRequestBody(null)).build())
                    coroutineContext.job.invokeOnCompletion { try { call.cancel() } catch (e: Exception) { /* ignore */ } }
                    call.execute().use { it.isSuccessful }
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
        for (i in 0 until arr.length()) {
            val raw = if (arr.isNull(i)) null else arr.opt(i)
            a.add((raw as? Number)?.toDouble()?.takeIf { it.isFinite() } ?: Double.NaN)
        }
        val simRaw = obj.opt("sim")
        val simFlagsValid = listOf("is_sim", "simulated").all { !obj.has(it) || obj.opt(it) is Boolean }
        val simMaskValue = when (simRaw) {
            is Number -> simRaw.toDouble().takeIf { it.isFinite() && it >= 0.0 && it % 1.0 == 0.0 && it < Long.MAX_VALUE.toDouble() }?.toLong()
            is String -> simRaw.trim().toLongOrNull()?.takeIf { it >= 0L }
            else -> null
        }
        val simMaskValid = !obj.has("sim") || simRaw is Boolean || simMaskValue != null
        val rowSimulated = obj.opt("is_sim") == true || obj.opt("simulated") == true || simRaw == true || !simFlagsValid || !simMaskValid
        val simMask = simMaskValue ?: 0L
        val parsedState = parseTelemetryState(obj.opt("state"))
        val stateKnown = parsedState != null
        val state = parsedState ?: 0L
        return TelemRow(
            id = id, seq = obj.optLong("seq", 0), ts = obj.optLong("ts", 0), mono = obj.optLong("mono", 0),
            a = a, k = obj.optLong("k", -1), s = obj.optLong("s", -1),
            state = state, q = obj.optLong("q", 0), sim = simMask, simulated = rowSimulated,
            stateKnown = stateKnown
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
        val chartPoints = sorted.asSequence().map { it.toPoint() }.filter { it.hasValues() }.toList()
        return chartPoints.ifEmpty { null }
    }
}
