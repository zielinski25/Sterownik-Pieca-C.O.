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
        // PUT writes the command object directly to /piec/cmd (single-slot mailbox).
        // Firmware polls with GET /piec/cmd and parses the object for token/cmd/id fields.
        // POST would create /piec/cmd/{pushKey} which the simple indexOf() parser
        // would still find, but PUT is the cleaner match for the single-object contract.
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
        // Firmware writes a single ACK object directly to /piec/ack (not /piec/ack/{cmdId}).
        // GET /piec/ack.json returns: {"cmdId":"...","cmd":"...","ok":true,"error":"","ts":12345}
        val url = "${FirebaseDevConfig.DATABASE_URL}/piec/ack.json?auth=${URLEncoder.encode(idToken, "UTF-8")}"
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.requestMethod = "GET"
        connection.connectTimeout = 10000
        connection.readTimeout = 10000
        val response = readResponse(connection)
        if (connection.responseCode !in 200..299) throw IllegalStateException("Błąd odczytu ACK: HTTP ${connection.responseCode}")
        if (response.isBlank() || response == "null") return null
        val json = JSONObject(response)
        // Verify this ACK is for our command (single-slot mailbox, may be stale)
        val ackCmdId = json.optString("cmdId", "")
        if (ackCmdId.isNotEmpty() && ackCmdId != cmdId) return null
        return Ack(json.optBoolean("ok", false), json.optString("error").takeIf { it.isNotBlank() })
    }

    private fun readResponse(connection: HttpURLConnection): String {
        val stream = if (connection.responseCode in 200..299) connection.inputStream else connection.errorStream
        return stream?.use { input -> BufferedReader(InputStreamReader(input, Charsets.UTF_8)).use { it.readText() } } ?: ""
    }
}
