package com.sterownikco.pro.core

/**
 * URL policy for the ESP32's plain-HTTP API. Status comes from RTDB, so its
 * `ip` field is untrusted input: accept only literal RFC1918 IPv4 hosts and
 * simple API paths. This prevents a bad/stale status value from turning a
 * local credential-bearing request into an arbitrary Internet request.
 */
internal object LocalControllerAddress {
    fun canonicalPrivateIpv4(raw: String): String? {
        val host = raw.trim()
        if (host.isEmpty() || host.length > 15) return null
        val fields = host.split('.')
        if (fields.size != 4) return null

        val octets = IntArray(4)
        for (i in fields.indices) {
            val field = fields[i]
            if (field.isEmpty() || (field.length > 1 && field[0] == '0') ||
                field.any { it !in '0'..'9' }
            ) return null
            octets[i] = field.toIntOrNull()?.takeIf { it in 0..255 } ?: return null
        }

        val privateAddress = octets[0] == 10 ||
            (octets[0] == 172 && octets[1] in 16..31) ||
            (octets[0] == 192 && octets[1] == 168)
        return if (privateAddress) octets.joinToString(".") else null
    }

    fun url(ip: String, path: String): String? {
        val host = canonicalPrivateIpv4(ip) ?: return null
        if (path.isEmpty() || path[0] != '/' || path.startsWith("//")) return null
        if (path.count { it == '?' } > 1) return null
        if (path.any { ch ->
                ch !in 'a'..'z' && ch !in 'A'..'Z' && ch !in '0'..'9' &&
                    ch !in "/._?=&+-"
            }
        ) return null
        val route = path.substringBefore('?').removePrefix("/")
        if (route.split('/').any { it.isEmpty() || it == "." || it == ".." }) return null
        return "http://$host$path"
    }
}
