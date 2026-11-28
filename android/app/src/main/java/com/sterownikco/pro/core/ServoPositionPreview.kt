package com.sterownikco.pro.core

/** Klapa and syberek command ranges match the firmware's servo map(0..100, 0..maxDegrees). */
internal enum class ServoPositionAxis(val commandName: String, val maxDegrees: Int) {
    KLAPA("klapa", 180),
    SYBEREK("syberek", 90)
}

/** UI-only, ACK-confirmed target. It is not copied into PiecState's reported telemetry. */
internal data class ServoPositionPreview(
    val requestedPercent: Int,
    val targetDegrees: Int,
    val acceptedAtElapsedMs: Long
)

internal data class ServoPositionCommand(val axis: ServoPositionAxis, val percent: Int) {
    fun preview(acceptedAtElapsedMs: Long): ServoPositionPreview {
        val clampedPercent = percent.coerceIn(0, 100)
        // Arduino map() uses integer division; match it exactly rather than rounding differently.
        val targetDegrees = clampedPercent * axis.maxDegrees / 100
        return ServoPositionPreview(clampedPercent, targetDegrees, acceptedAtElapsedMs)
    }
}

internal const val SERVO_POSITION_PREVIEW_TIMEOUT_MS = 30_000L

/** Parse only a position command which the controller can accept, e.g. "klapa 13". */
internal fun parseServoPositionCommand(command: String): ServoPositionCommand? {
    val parts = command.trim().split(Regex("\\s+"), limit = 2)
    if (parts.size != 2) return null
    val axis = when (parts[0].lowercase(java.util.Locale.ROOT)) {
        "klapa" -> ServoPositionAxis.KLAPA
        "syberek" -> ServoPositionAxis.SYBEREK
        else -> return null
    }
    val rawPercent = parts[1].trim().removeSuffix("%")
    val percent = rawPercent.toIntOrNull() ?: return null
    return ServoPositionCommand(axis, percent.coerceIn(0, 100))
}

/** Keep the target visible until telemetry reaches it, or until the bounded timeout expires. */
internal fun shouldKeepServoPositionPreview(
    preview: ServoPositionPreview,
    reportedDegrees: Int?,
    nowElapsedMs: Long,
    timeoutMs: Long = SERVO_POSITION_PREVIEW_TIMEOUT_MS
): Boolean {
    if (nowElapsedMs - preview.acceptedAtElapsedMs >= timeoutMs) return false
    return reportedDegrees != preview.targetDegrees
}
