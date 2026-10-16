package com.sterownikco.pro.core

import org.json.JSONObject

/** Mute fields echoed by the controller in /piec/status. Null means the old firmware did not report it. */
internal data class ControllerAlarmMuteStatus(
    val smokeMuted: Boolean?,
    val smokeRemainingMs: Long?,
    val overheatMuted: Boolean?,
    val overheatRemainingMs: Long?,
    val controllerUpdatedAtMs: Long? = null
) {
    val hasAnyField: Boolean
        get() = smokeMuted != null || smokeRemainingMs != null ||
            overheatMuted != null || overheatRemainingMs != null
}

internal fun controllerAlarmMuteStatus(d: JSONObject): ControllerAlarmMuteStatus {
    fun millis(key: String): Long? {
        if (!d.has(key) || d.isNull(key)) return null
        val value = (d.opt(key) as? Number)?.toDouble() ?: return null
        if (!value.isFinite() || value % 1.0 != 0.0) return null
        val longValue = value.toLong()
        return longValue.takeIf { it in -1L..3_600_000L }
    }
    fun nonNegativeLong(key: String): Long? {
        if (!d.has(key) || d.isNull(key)) return null
        val value = (d.opt(key) as? Number)?.toDouble() ?: return null
        if (!value.isFinite() || value !in 0.0..4_294_967_295.0 || value % 1.0 != 0.0) return null
        return value.toLong()
    }

    val smokeRemaining = millis("alarm_dym_wyciszenie_ms")
    val overheatRemaining = millis("alarm_ogrz_wyciszenie_ms")
    val smokeFlag = if (d.has("alarm_dym_wyciszony")) parseAlarmFlag(d.opt("alarm_dym_wyciszony")) else null
    val overheatFlag = if (d.has("alarm_ogrz_wyciszony")) parseAlarmFlag(d.opt("alarm_ogrz_wyciszony")) else null
    return ControllerAlarmMuteStatus(
        smokeMuted = smokeFlag ?: smokeRemaining?.let { it != 0L },
        smokeRemainingMs = smokeRemaining,
        overheatMuted = overheatFlag ?: overheatRemaining?.let { it != 0L },
        overheatRemainingMs = overheatRemaining,
        controllerUpdatedAtMs = nonNegativeLong("updatedAt")
    )
}

/** Builds only recognized controller commands; the controller accepts one alarm class per command. */
internal object AlarmMuteCommands {
    fun signature(dym: Boolean?, ogrz: Boolean?, previousSignature: String = ""): String {
        val previous = previousSignature.split('+').toSet()
        val smokeActive = dym ?: ("D" in previous)
        val overheatActive = ogrz ?: ("O" in previous)
        return listOfNotNull(if (smokeActive) "D" else null, if (overheatActive) "O" else null)
            .joinToString("+")
    }

    fun forMute(signature: String, seconds: Int?): List<String> {
        if (seconds != null && seconds !in 0..3_600) return emptyList()
        val signatureParts = signature.split('+').map { it.trim() }
        if (signatureParts.isEmpty() || signatureParts.any { it !in setOf("D", "O") }) return emptyList()
        val targets = signatureParts.map {
            when (it) {
                "D" -> "dym"
                else -> "ogrz"
            }
        }.distinct()
        if (targets.isEmpty()) return emptyList()
        val action = seconds?.toString() ?: "off"
        val target = if (targets.size == 2) "oba" else targets.single()
        return listOf("alarm_mute $target $action")
    }
}
