package com.sterownikco.pro.core

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
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

    /** [v0.31.33 ESP-HISTORY] Surowy tekst z centrali — dla `/api/history` (CSV, nie JSON). */
    private suspend fun espGetText(ip: String, path: String): String? = espUrl(ip, path)?.let { u ->
        val (code, text) = executeEsp(Request.Builder().url(u).build())
        if (code in 200..299) text else null
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
    /**
     * [v0.31.33 ESP-HISTORY] Lokalne archiwum centrali: `GET /api/history`.
     * To ta sama historia, którą centrala wysyła do chmury — ale z próbkami,
     * które w chmurze się nie pojawiły (przerwy wysyłki). Zakres ~24–38 h.
     */
    suspend fun fetchEspHistory(ip: String): List<TelemPoint>? {
        val csv = espGetText(ip, "/api/history") ?: return null
        if (csv.isBlank()) return null
        val rows = withContext(Dispatchers.Default) { parseEspHistoryCsv(csv) }
        return rows.ifEmpty { null }
    }

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
    // [0.31.46] 300 -> 1000. Firebase RTDB spokojnie oddaje 1000 rekordow
    // w jednej odpowiedzi, a to 3x mniej zapytan: 7 dni = 11 zamiast 34.
    private const val HISTORY_PAGE_SIZE = 1000

    /** Ile dni pobieramy naraz. 4 to kompromis: 7 dni = 2 fale, 30 dni = 8. */
    private const val FETCH_PARALLEL_DAYS = 4

    /**
     * Po tym czasie ponawiamy paginację, nawet jeśli indeks `ts` kiedyś zawiódł.
     * Wcześniej flaga zostawała do końca procesu i jeden przypadkowy HTTP 400
     * (np. po odświeżeniu tokenu) wyłączał stronicowanie na zawsze.
     */
    private const val INDEX_FALLBACK_TTL_MS = 10L * 60_000L

    @Volatile private var historyIndexFallback = false
    @Volatile private var historyIndexFallbackUntil = 0L

    /** Dni domknięte w tym procesie — wczoraj dociągamy najwyżej raz. */
    private val finalizedDays = java.util.concurrent.ConcurrentHashMap<String, Boolean>()

    private fun indexFallbackActive(): Boolean {
        if (!historyIndexFallback) return false
        if (System.currentTimeMillis() > historyIndexFallbackUntil) {
            historyIndexFallback = false
            return false
        }
        return true
    }

    private fun enableIndexFallback() {
        historyIndexFallback = true
        historyIndexFallbackUntil = System.currentTimeMillis() + INDEX_FALLBACK_TTL_MS
    }

    /** Tylko prawdziwy brak indeksu `ts` usprawiedliwia pobranie całego dnia. */
    private fun isIndexError(error: String?, body: String): Boolean {
        val text = ((error ?: "") + " " + body).lowercase(java.util.Locale.US)
        if (text.isBlank()) return false
        return text.contains("index not defined") || text.contains("indexon")
    }

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
            // [v0.31.34 GAP-REPAIR] Błąd sieci w środku dnia nie może skasować
            // całego dnia — zachowujemy to, co już pobraliśmy. Resztę dopyta
            // repairGaps() po zebraniu wszystkich dni.
            val res = try {
                get(q)
            } catch (e: Exception) {
                if (rows.isNotEmpty()) return uniqueSort(rows)
                throw e
            }
            if (!res.ok) {
                if (isIndexError(res.error, res.body)) {
                    enableIndexFallback()
                    return fetchShardFull(path, startTs, endTs)
                }
                // Inne błędy (401, timeout, HTTP 400 bez indeksu) nie mogą
                // udawać pustego dnia — dzień zostanie ponowiony przy kolejnym wejściu.
                throw java.io.IOException(res.error ?: "błąd pobierania dnia")
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

    /**
     * [v0.31.34 GAP-REPAIR] Po pobraniu całego zakresu sprawdza, czy nie została
     * przerwa dłuższa niż 3 minuty, i dopyta dokładnie ten przedział.
     *
     * Powód: pojedyncza nieudana strona paginacji (albo krótka odpowiedź RTDB)
     * zostawiała dziurę w środku dnia, mimo że dane w chmurze są. Wtedy wykres
     * pokazywał pustkę, której w bazie nie ma. Prawdziwa przerwa w chmurze
     * zwróci 0 wierszy — wtedy dziura zostaje i jest uczciwie opisana na wykresie.
     */
    private suspend fun repairGaps(rows: List<TelemRow>): List<TelemRow> {
        if (rows.size < 2) return rows
        val holes = ArrayList<LongArray>()
        for (i in 1 until rows.size) {
            val a = rows[i - 1].ts
            val b = rows[i].ts
            if (b - a > 180L) holes.add(longArrayOf(a + 1L, b - 1L))
        }
        if (holes.isEmpty()) return rows
        val extra = ArrayList<TelemRow>()
        for (h in holes.take(40)) {
            for (day in dayRange(h[0], h[1])) {
                val ds = max(h[0], day[0] / 1000)
                val de = minOf(h[1], day[0] / 1000 + 86_400 - 1)
                if (de < ds) continue
                try {
                    extra.addAll(
                        fetchShard(shardPath(day[1].toInt(), day[2].toInt(), day[3].toInt()), ds, de)
                    )
                } catch (e: Exception) { /* przerwa zostaje — pokazujemy ją na wykresie */ }
            }
        }
        if (extra.isEmpty()) return rows
        val map = LinkedHashMap<String, TelemRow>()
        rows.forEach { map[it.id] = it }
        extra.forEach { map[it.id] = it }
        return map.values.sortedWith(compareBy({ it.ts }, { it.seq }))
    }

    /**
     * Pobiera zakres historii, korzystając z dziennego cache, jeśli został podany.
     *
     * Zasady ruchu sieciowego:
     *  - dzień bez pliku w pamięci  → pełne pobranie tego dnia i zapis,
     *  - dzień bieżący (UTC)        → wyłącznie rekordy nowsze od ostatniego zapisanego,
     *  - dzień zamknięty            → tylko z pamięci, bez sieci.
     *
     * Błąd jednego dnia nie przerywa pozostałych, ale jest zwracany w [HistoryFetch],
     * żeby UI mogło powiedzieć "brak 2 dni" zamiast milcząco pokazać dziurę.
     */
    suspend fun fetchTelemetry(
        rangeSec: Int,
        firmware: String? = null,
        deviceUptimeSeconds: Double? = null,
        statusObservedAtMs: Long? = null,
        cache: TelemetryDayCache? = null
    ): HistoryFetch = withContext(Dispatchers.IO) {
        if (idToken.isEmpty()) return@withContext HistoryFetch()

        val end = System.currentTimeMillis() / 1000
        val start = end - rangeSec
        val queryEnd = TelemetryTimeRepair.queryEndSeconds(
            end, firmware, deviceUptimeSeconds, statusObservedAtMs
        )
        val all = ArrayList<TelemRow>()
        var daysRequested = 0
        var daysFromCache = 0
        var daysDownloaded = 0
        var daysStale = 0
        var daysFailed = 0
        var firstError: String? = null
        val todayKey = TelemetryDayCache.utcDayKey(end)
        val yesterdayKey = TelemetryDayCache.utcDayKey(end - 86_400)

        for (day in dayRange(start, queryEnd)) {
            daysRequested++
            // Cały dzień, nie tylko wycinek zakresu: ten sam plik posłuży potem
            // szerszemu zakresowi. Pobranie skróconego dnia (np. przy 24 h)
            // zatrułoby pamięć i po przełączeniu na 30 dni brakowałoby próbek.
            val dayStart = day[0] / 1000
            val dayEnd = dayStart + 86_400 - 1
            val path = shardPath(day[1].toInt(), day[2].toInt(), day[3].toInt())
            val key = TelemetryDayCache.utcDayKey(dayStart)
            val cached = cache?.read(key)
            try {
                if (cached == null) {
                    val rows = fetchShard(path, dayStart, dayEnd)
                    all.addAll(rows)
                    cache?.write(key, rows)
                    finalizedDays[key] = true
                    daysDownloaded++
                } else if (key == todayKey) {
                    // Doba w toku: dociągamy tylko ogon, nie całe 24 h.
                    // `startAt` jest domknięty, więc ostatni zapisany rekord wraca
                    // zawsze — o tym, czy są nowe dane, decyduje porównanie `id`.
                    val fresh = fetchShard(path, max(dayStart, cached.maxTs), dayEnd)
                    val known = cached.rows.mapTo(HashSet()) { it.id }
                    val added = fresh.filter { it.id !in known }
                    if (added.isEmpty()) {
                        all.addAll(cached.rows)
                        daysFromCache++
                    } else {
                        val merged = uniqueSort(cached.rows + added)
                        all.addAll(merged)
                        cache?.write(key, merged)
                        daysDownloaded++
                    }
                } else if (key == yesterdayKey && !finalizedDays.containsKey(key)) {
                    // Wczoraj domykamy raz na proces: rekordy mogły dopisać się
                    // po północy UTC. Scalanie, nigdy podmiana — nie zgubimy próbek.
                    val full = fetchShard(path, dayStart, dayEnd)
                    finalizedDays[key] = true
                    val merged = uniqueSort(cached.rows + full)
                    all.addAll(merged)
                    cache?.write(key, merged)
                    daysDownloaded++
                } else {
                    all.addAll(cached.rows)
                    daysFromCache++
                }
            } catch (e: kotlinx.coroutines.CancellationException) {
                throw e
            } catch (e: Exception) {
                if (firstError == null) firstError = e.message ?: "błąd sieci"
                if (cached != null) {
                    // Dzień z pamięci jest lepszy niż dziura w wykresie,
                    // ale oznaczamy go jako nieodświeżony.
                    all.addAll(cached.rows)
                    daysStale++
                } else {
                    daysFailed++
                }
            }
        }

        val sorted = uniqueSort(all)
        if (sorted.isEmpty()) {
            return@withContext HistoryFetch(
                daysRequested = daysRequested, daysFromCache = daysFromCache,
                daysDownloaded = daysDownloaded, daysStale = daysStale,
                daysFailed = daysFailed, firstError = firstError
            )
        }
        val repaired = TelemetryTimeRepair.normalize(
            rows = sorted,
            firmware = firmware,
            deviceUptimeSeconds = deviceUptimeSeconds,
            statusObservedAtMs = statusObservedAtMs
        ).rows
        val chartPoints = repaired.asSequence()
            .filter { it.ts in start..end }
            .map { it.toPoint() }
            .filter { it.hasValues() }
            .toList()
        HistoryFetch(
            points = chartPoints,
            daysRequested = daysRequested,
            daysFromCache = daysFromCache,
            daysDownloaded = daysDownloaded,
            daysStale = daysStale,
            daysFailed = daysFailed,
            firstError = firstError
        )
    }

    /** Wynik pobrania dokładnego przedziału: próbki + informacja, czy nic nie wypadło. */
    data class WindowResult(val points: List<TelemPoint>, val complete: Boolean)

    /**
     * [0.31.43 INCREMENTAL] Pobiera DOKŁADNIE przedział [startSec, endSec].
     *
     * Po co: `fetchTelemetry(rangeSec)` zawsze brało całe okno od teraz w tył.
     * Pamięć telefonu (`TelemetryCache`) wie, czego już nie musi pobierać, więc
     * pyta tylko o brakujące fragmenty — zwykle o ogon od ostatniej próbki.
     *
     * `complete = false` znaczy, że choć jeden dzień się nie pobrał (sieć).
     * Wtedy nie wolno zapamiętywać „tu jest pusto", bo to nieprawda.
     */
    suspend fun fetchWindow(startSec: Long, endSec: Long): WindowResult {
        if (idToken.isEmpty()) return WindowResult(emptyList(), false)
        if (endSec <= startSec) return WindowResult(emptyList(), true)
        // [0.31.46 ROWNOLEGLE] Dni pobieramy po FETCH_PARALLEL_DAYS naraz zamiast
        // po kolei. Wczesniej 7 dni = 7 kolejnych dni, kazdy z wlasnymi zapytaniami
        // — sumarycznie ~34 zapytania wykonywane JEDNO PO DRUGIM (~10 s).
        // Teraz sa to 2 fale. Kolejnosc nie ma znaczenia, bo uniqueSort() i tak
        // porzadkuje po czasie, a kazdy dzien trafia do wlasnego sharda.
        val zadania = ArrayList<Pair<String, LongArray>>()
        for (day in dayRange(startSec, endSec)) {
            val dayStart = max(startSec, day[0] / 1000)
            val dayEnd = minOf(endSec, day[0] / 1000 + 86_400 - 1)
            if (dayEnd < dayStart) continue
            zadania.add(Pair(shardPath(day[1].toInt(), day[2].toInt(), day[3].toInt()),
                             longArrayOf(dayStart, dayEnd)))
        }
        // null = ten dzien sie nie udal (blad sieci). Reszta zostaje, a kompletność
        // jest falszywa — dzieki temu nie zapamietamy "tu pusto" dla brakujacego dnia.
        val czesci: List<List<TelemRow>?> = coroutineScope {
            zadania.chunked(FETCH_PARALLEL_DAYS).map { fala ->
                fala.map { zadanie ->
                    val path = zadanie.first
                    val od = zadanie.second[0]
                    val doT = zadanie.second[1]
                    async {
                        try { fetchShard(path, od, doT) } catch (e: Exception) { null }
                    }
                }.awaitAll()
            }.flatten()
        }
        val rows = ArrayList<TelemRow>()
        var complete = true
        for (czesc in czesci) {
            if (czesc == null) complete = false else rows.addAll(czesc)
        }
        val sorted = repairGaps(uniqueSort(rows))
        val pts = sorted.asSequence().map { it.toPoint() }.filter { it.hasValues() }.toList()
        return WindowResult(pts, complete)
    }
}
