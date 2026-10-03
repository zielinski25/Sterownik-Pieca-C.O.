package pl.sterownikco.dev

import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL

class FirebaseAuthRest {
    data class Session(
        val idToken: String,
        val refreshToken: String,
        val expiresAtMs: Long,
        val localId: String?
    )

    fun signIn(email: String, password: String): Session {
        val body = JSONObject()
            .put("email", email)
            .put("password", password)
            .put("returnSecureToken", true)
        return parseSignIn(postJson(
            "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=${FirebaseDevConfig.WEB_API_KEY}",
            body.toString()
        ))
    }

    fun refresh(refreshToken: String): Session {
        val connection = URL(
            "https://securetoken.googleapis.com/v1/token?key=${FirebaseDevConfig.WEB_API_KEY}"
        ).openConnection() as HttpURLConnection
        connection.requestMethod = "POST"
        connection.connectTimeout = 10000
        connection.readTimeout = 10000
        connection.doOutput = true
        connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
        val encoded = java.net.URLEncoder.encode(refreshToken, "UTF-8")
        connection.outputStream.use { it.write("grant_type=refresh_token&refresh_token=$encoded".toByteArray(Charsets.UTF_8)) }
        val response = readResponse(connection)
        if (connection.responseCode !in 200..299) throw firebaseError(response, connection.responseCode, "odświeżania sesji")
        val json = JSONObject(response)
        return Session(
            idToken = json.getString("id_token"),
            refreshToken = json.getString("refresh_token"),
            expiresAtMs = System.currentTimeMillis() + json.optLong("expires_in", 3600) * 1000L,
            localId = json.optString("user_id").takeIf { it.isNotBlank() }
        )
    }

    fun getStatus(idToken: String): JSONObject {
        val url = "${FirebaseDevConfig.DATABASE_URL}/piec/status.json?auth=${java.net.URLEncoder.encode(idToken, "UTF-8")}"
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.requestMethod = "GET"
        connection.connectTimeout = 10000
        connection.readTimeout = 10000
        val response = readResponse(connection)
        if (connection.responseCode !in 200..299) throw firebaseError(response, connection.responseCode, "odczytu /piec/status")
        return JSONObject(response)
    }

    private fun postJson(url: String, body: String): JSONObject {
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.requestMethod = "POST"
        connection.connectTimeout = 10000
        connection.readTimeout = 10000
        connection.doOutput = true
        connection.setRequestProperty("Content-Type", "application/json")
        connection.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }
        val response = readResponse(connection)
        if (connection.responseCode !in 200..299) throw firebaseError(response, connection.responseCode, "logowania")
        return JSONObject(response)
    }

    private fun readResponse(connection: HttpURLConnection): String {
        val stream = if (connection.responseCode in 200..299) connection.inputStream else connection.errorStream
        return stream?.use { input -> BufferedReader(InputStreamReader(input, Charsets.UTF_8)).use { it.readText() } } ?: "{}"
    }

    private fun firebaseError(response: String, code: Int, action: String): FirebaseAuthException {
        val parsed = runCatching { JSONObject(response).optJSONObject("error") }.getOrNull()
        val message = parsed?.optString("message")?.takeIf { it.isNotBlank() }
            ?: "Błąd $action (HTTP $code)"
        val firebaseCode = message
        return FirebaseAuthException(firebaseCode, code, message)
    }

    class FirebaseAuthException(
        val firebaseCode: String,
        val httpCode: Int,
        message: String,
    ) : IllegalStateException(message)

    private fun parseSignIn(json: JSONObject): Session = Session(
        idToken = json.getString("idToken"),
        refreshToken = json.getString("refreshToken"),
        expiresAtMs = System.currentTimeMillis() + json.optLong("expiresIn", 3600) * 1000L,
        localId = json.optString("localId").takeIf { it.isNotBlank() }
    )
    class HistoryQueryUnsupported : IllegalStateException()

    fun historyDayUrl(day: String): String =
        "${FirebaseDevConfig.DATABASE_URL}/piec/telemetry/1m/v1/$day.json"

    fun historyUrl(day: String, startTs: Long, endTs: Long): String =
        historyDayUrl(day) + "?orderBy=%22ts%22&startAt=$startTs&endAt=$endTs"

    fun getJson(url: String, idToken: String): JSONObject? {
        val separator = if (url.contains('?')) '&' else '?'
        val full = "$url${separator}auth=${java.net.URLEncoder.encode(idToken, "UTF-8")}"
        val connection = URL(full).openConnection() as HttpURLConnection
        connection.requestMethod = "GET"
        connection.connectTimeout = 15000
        connection.readTimeout = 20000
        val response = readResponse(connection)
        if (connection.responseCode == 400 && url.contains("orderBy")) throw HistoryQueryUnsupported()
        if (connection.responseCode !in 200..299) throw firebaseError(response, connection.responseCode, "odczytu historii")
        if (response.isBlank() || response == "null") return null
        return JSONObject(response)
    }

}
