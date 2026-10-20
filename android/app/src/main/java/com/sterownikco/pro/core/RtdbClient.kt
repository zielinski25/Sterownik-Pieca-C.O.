package com.sterownikco.pro.core

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.job
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import android.util.Base64
import okhttp3.FormBody
import okhttp3.HttpUrl.Companion.toHttpUrl
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

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
                // Brak połączenia (np. brak internetu): IOException, żeby logowanie automatyczne
                // nie zasłaniało działającego trybu LAN oknem logowania.
                if (code < 0) return@withContext Result.failure(java.io.IOException(text))
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

    /** [via] = droga potwierdzenia: [VIA_FIREBASE] (/piec/ack) albo [VIA_LAN] (/api/lan/ack). */
    data class Ack(val ok: Boolean, val error: String, val via: String = VIA_FIREBASE)

    const val VIA_FIREBASE = "FIREBASE"
    const val VIA_LAN = "LAN"

    // ── [LAN] centrala w tej samej sieci — działa także bez internetu ────────
    /** Sondowanie LAN: krótkie limity — telefon poza siecią centrali nie czeka 15 s. */
    private val lanHttp: OkHttpClient by lazy {
        espHttp.newBuilder()
            .connectTimeout(1500, TimeUnit.MILLISECONDS)
            .readTimeout(4, TimeUnit.SECONDS)
            .callTimeout(6, TimeUnit.SECONDS)
            .build()
    }

    private suspend fun executeWith(client: OkHttpClient, req: Request): Pair<Int, String> = withContext(Dispatchers.IO) {
        try {
            val call = client.newCall(req)
            coroutineContext.job.invokeOnCompletion { try { call.cancel() } catch (e: Exception) { /* ignore */ } }
            call.execute().use { r -> r.code to (r.body?.string() ?: "") }
        } catch (e: Exception) {
            -1 to (e.message ?: "błąd sieci")
        }
    }

    private suspend fun lanGet(ip: String, path: String, client: OkHttpClient = lanHttp): Pair<Int, String> {
        val url = LocalControllerAddress.url(ip, path) ?: return -2 to ""
        return executeWith(client, Request.Builder().url(url).header("Cache-Control", "no-store").build())
    }

    private fun jsonOrNull(text: String): JSONObject? =
        try { if (text.isBlank()) null else JSONObject(text) } catch (e: Exception) { null }

    data class LanStatus(val ip: String, val json: JSONObject, val fullApi: Boolean)

    /**
     * Status centrali pod [ip]: `GET /api/lan/status` (firmware v3.47.0+, ten sam JSON co
     * /piec/status); starszy firmware — `/api/status` (bez ustawień, tylko odczyt).
     * Starszy firmware odpowiada na nieznane trasy 404 albo przekierowaniem na „/”
     * (captive portal) — oba oznaczają brak /api/lan/… — null = pod adresem nie ma centrali.
     */
    suspend fun lanStatus(ip: String): LanStatus? {
        val (code, text) = lanGet(ip, "/api/lan/status")
        val fullApi: Boolean
        val json: JSONObject? = when {
            code in 200..299 -> { fullApi = true; jsonOrNull(text) }
            code == 404 || code in 300..399 -> {
                fullApi = false
                val (legacyCode, legacyText) = lanGet(ip, "/api/status")
                if (legacyCode in 200..299) jsonOrNull(legacyText) else null
            }
            else -> { fullApi = false; null }
        }
        if (json == null) return null
        val controllerFields = json.has("t_ogrz") || json.has("dym_alarm") || json.has("tryb_serwa")
        if (!LanCandidates.looksLikeController(json.optString("wifi_mode"), controllerFields)) return null
        return LanStatus(ip, json, fullApi)
    }

    /** Pierwszy osiągalny adres z [candidates] (kolejność = pierwszeństwo; sprawdzane równolegle). */
    suspend fun lanFind(candidates: List<String>): LanStatus? = coroutineScope {
        val probes = candidates.map { ip ->
            async {
                try { lanStatus(ip) } catch (e: CancellationException) { throw e } catch (e: Exception) { null }
            }
        }
        probes.map { it.await() }.firstOrNull { it != null }
    }

    /** Polecenie na pewno NIE dotarło do centrali w LAN — wolno spróbować przez Firebase. */
    class LanNotDelivered(message: String) : java.io.IOException(message)

    /**
     * `POST /api/lan/cmd` (token w nagłówku X-Piec-Token) + oczekiwanie na `GET /api/lan/ack`
     * (max 12 s) — odpowiednik [sendCommand] w sieci lokalnej: ten sam wykonawca w centrali
     * i ten sam token. null = brak potwierdzenia; [LanNotDelivered] = polecenie nie dotarło.
     */
    suspend fun lanSendCommand(ip: String, id: String, cmd: String): Ack? {
        val url = LocalControllerAddress.url(ip, "/api/lan/cmd") ?: throw LanNotDelivered("nieprawidłowy adres centrali")
        val ackPath = "/api/lan/ack?id=$id"
        val form = FormBody.Builder().add("id", id).add("cmd", cmd).build()
        val (code, text) = executeWith(lanHttp, Request.Builder().url(url)
            .header("X-Piec-Token", cmdToken).header("Cache-Control", "no-store").post(form).build())
        val reply = jsonOrNull(text)
        when (code) {
            200, 202 -> Unit
            400 -> return Ack(false, reply?.optString("error")?.ifEmpty { null } ?: "nieprawidłowe polecenie", VIA_LAN)
            403 -> return Ack(false, "nieprawidłowy token poleceń", VIA_LAN)
            409 -> return Ack(false, "centrala wykonuje poprzednie polecenie — spróbuj za chwilę", VIA_LAN)
            in 300..399, 404 -> throw LanNotDelivered("firmware centrali bez /api/lan/cmd")
            else -> {
                // Zerwane połączenie: centrala mogła jednak przyjąć ten cmdId — sprawdź, zanim użyjemy Firebase.
                val (probeCode, _) = lanGet(ip, ackPath)
                if (probeCode != 200) throw LanNotDelivered(if (code < 0) "brak połączenia z centralą" else "HTTP $code")
            }
        }
        if (reply?.optBoolean("done") == true) return lanAck(reply)
        val start = System.currentTimeMillis()
        while (System.currentTimeMillis() - start < 12_000) {
            delay(300)
            val (ackCode, ackText) = lanGet(ip, ackPath)
            if (ackCode == 404 || ackCode in 300..399) return null
            val ack = jsonOrNull(ackText) ?: continue
            if (ack.optBoolean("done")) return lanAck(ack)
        }
        return null
    }

    private fun lanAck(j: JSONObject): Ack = Ack(j.optBoolean("ok"), j.optString("error").ifEmpty { "błąd" }, VIA_LAN)

    /** `GET /api/history` (bufor minutowy ~24 h) albo `/api/history/long` (wiele dni) — CSV centrali. */
    suspend fun lanHistoryCsv(ip: String, long: Boolean): String? {
        val (code, text) = lanGet(ip, if (long) "/api/history/long" else "/api/history", client = espHttp)
        return if (code in 200..299) text else null
    }

    /**
     * Polecenie do centrali: w sieci lokalnej (świeże łącze [LanLink] z pełnym API), inaczej
     * przez Firebase. Gdy LAN zawiedzie, zanim polecenie dotrze do centrali — Firebase.
     */
    suspend fun sendCommandAuto(id: String, cmd: String): Ack? {
        val lan = LanLink.fresh(System.currentTimeMillis())
        if (lan != null && lan.fullApi) {
            try {
                return lanSendCommand(lan.ip, id, cmd)
            } catch (e: LanNotDelivered) {
                LanLink.markLost(lan.ip)
                if (refreshToken.isBlank()) throw java.io.IOException("Brak połączenia z centralą w sieci lokalnej (${e.message})")
            }
        } else if (lan != null && refreshToken.isBlank()) {
            throw java.io.IOException("Starszy firmware centrali — sterowanie w sieci lokalnej wymaga v3.47.0")
        }
        if (refreshToken.isBlank()) throw java.io.IOException("Brak połączenia ze sterownikiem — ani sieci lokalnej, ani sesji Firebase")
        return sendCommand(id, cmd)
    }

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

    // ── telemetria historyczna (szardy dzienne UTC + lokalna pamięć dób) ─────
    /** Strona zapytania `orderBy="ts"`; pełna doba to ~1440 rekordów = 2 strony. */
    private const val HISTORY_PAGE_SIZE = 1000
    @Volatile private var historyIndexFallback = false

    /** Reguły bez `".indexOn": ["ts"]` — RTDB odrzuca zapytanie po `ts` (HTTP 400). */
    private class HistoryIndexMissing : Exception()

    private suspend fun fetchShardPage(day: TelemDay, cursor: Long): TelemShardPage {
        val res = get(day.shardPath + ".json", mapOf(
            "orderBy" to "\"ts\"",
            "startAt" to cursor.toString(),
            "endAt" to day.endTs.toString(),
            "limitToFirst" to HISTORY_PAGE_SIZE.toString()
        ))
        if (!res.ok) {
            val msg = res.error ?: ""
            if (msg.contains("400") || res.body.contains("Index not defined", true) || res.body.contains("indexOn", true)) {
                throw HistoryIndexMissing()
            }
            throw java.io.IOException("Historia " + day.date + ": " + msg)
        }
        return parseShardBody(res.body, day)
    }

    /** `null` / `{}` = doba bez rekordów (poprawny, pusty wynik); zepsuty JSON = błąd doby. */
    private fun parseShardBody(body: String, day: TelemDay): TelemShardPage {
        if (isEmptyRtdbBody(body)) return TelemShardPage(emptyList(), 0)
        val data = JSONObject(body)
        val rows = ArrayList<TelemRow>(data.length())
        for (id in data.keys()) {
            val r = parseRecord(id, data.optJSONObject(id))
            if (r != null && r.ts in day.startTs..day.endTs) rows.add(r)
        }
        return TelemShardPage(rows, data.length())
    }

    /**
     * Rekordy doby z `ts >= fromTs` (rzuca przy błędzie sieci/HTTP — doba nie zostanie
     * wtedy uznana za kompletną). Bez indeksu `ts` w regułach: cały shard doby.
     */
    private suspend fun fetchShardRows(day: TelemDay, fromTs: Long): List<TelemRow> {
        if (!historyIndexFallback) {
            try {
                return fetchDayPaged(day, fromTs, HISTORY_PAGE_SIZE) { cursor -> fetchShardPage(day, cursor) }
            } catch (e: HistoryIndexMissing) {
                historyIndexFallback = true
            }
        }
        val res = get(day.shardPath + ".json")
        if (!res.ok) throw java.io.IOException("Historia " + day.date + ": " + (res.error ?: ""))
        return uniqueSortedRows(parseShardBody(res.body, day).rows)
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

    /**
     * `fetchTelemetryFromFirebase(rangeSec)` z lokalną pamięcią dób ([store]): doby
     * kompletne idą z telefonu, z Firebase dociągane są tylko nowe rekordy (zwykle
     * dzisiejsze). Zwraca null bez sesji Firebase. Niczego w bazie nie usuwa ani nie zmienia.
     */
    internal suspend fun fetchTelemetry(
        rangeSec: Int,
        store: TelemetryDayStore,
        onProgress: ((done: Int, total: Int) -> Unit)? = null
    ): TelemRangeResult? {
        if (idToken.isEmpty()) return null
        val end = System.currentTimeMillis() / 1000
        val start = end - rangeSec
        return store.load(start, end, onProgress) { day, fromTs -> fetchShardRows(day, fromTs) }
    }
}
