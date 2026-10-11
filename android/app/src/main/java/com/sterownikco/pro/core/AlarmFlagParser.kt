package com.sterownikco.pro.core

/**
 * Parses a wire-format alarm flag without collapsing malformed or missing input to false.
 * Callers must keep `null` as an unknown state and only clear an active alarm when the
 * relevant flags are explicitly known to be false.
 */
internal fun parseAlarmFlag(raw: Any?): Boolean? = when (raw) {
    is Boolean -> raw
    is Number -> raw.toDouble().takeIf { it.isFinite() && (it == 0.0 || it == 1.0) }?.let { it == 1.0 }
    is String -> when (raw.trim().lowercase()) {
        "1", "true" -> true
        "0", "false" -> false
        else -> null
    }
    else -> null
}
