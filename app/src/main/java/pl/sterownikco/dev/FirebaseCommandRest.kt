package pl.sterownikco.dev

import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import java.util.UUID

class FirebaseCommandRest {
    data class Ack(val ok: Boolean, val error: String?)

    // Matches the controller's existing DEV Firebase mailbox contract.
    private val commandToken = "sterownikco-cmd-2026"

    fun send(idToken: String, command: String): String {
        val cmdId = "android-${UUID.randomUUID()}"
        val body = JSONObject()
            .put("id", cmdId)
            .put("cmd", command)
            .put("token", commandToken)
            .put("ts", System.currentTimeMillis())
            .toString()
        val url = "${FirebaseDevConfig.DATABASE_URL}/piec/cmd.json?auth=${URLEncoder.encode(idToken, "UTF-8")}"
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.requestMethod = "PUT"
        connection.connectTimeout = 10000
        connection.readTimeout = 10000
        connection.doOutput = true
        connection.setRequestProperty("Content-Type", "application/json")
        connection.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }
        val response = readResponse(connection)
        if (connection.responseCode !in 200..299) throw IllegalStateException("Błąd wysyłki komendy: HTTP ${connection.responseCode}")
        return cmdId
    }

    fun readAck(idToken: String, cmdId: String): Ack? {
        val url = "${FirebaseDevConfig.DATABASE_URL}/piec/ack/${URLEncoder.encode(cmdId, "UTF-8")}.json?auth=${URLEncoder.encode(idToken, "UTF-8")}"
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.requestMethod = "GET"
        connection.connectTimeout = 10000
        connection.readTimeout = 10000
        val response = readResponse(connection)
        if (connection.responseCode !in 200..299) throw IllegalStateException("Błąd odczytu ACK: HTTP ${connection.responseCode}")
        if (response.isBlank() || response == "null") return null
        val json = JSONObject(response)
        return Ack(json.optBoolean("ok", false), json.optString("error").takeIf { it.isNotBlank() })
    }

    private fun readResponse(connection: HttpURLConnection): String {
        val stream = if (connection.responseCode in 200..299) connection.inputStream else connection.errorStream
        return stream?.use { input -> BufferedReader(InputStreamReader(input, Charsets.UTF_8)).use { it.readText() } } ?: ""
    }
}
