package com.sterownikco.pro.core

/**
 * A successful command ACK is shown immediately while the periodic Firebase status
 * catches up. Once status reports the same value, the temporary ACK override is no
 * longer needed. Invalid or missing status values are never presented as real modes.
 */
internal data class ServoModePresentation(
    val mode: Int?,
    val awaitingStatus: Boolean
)

internal fun presentServoMode(acknowledgedMode: Int?, reportedMode: Int?): ServoModePresentation {
    val ack = acknowledgedMode?.takeIf { it in 1..3 }
    val reported = reportedMode?.takeIf { it in 1..3 }
    return ServoModePresentation(
        mode = ack ?: reported,
        awaitingStatus = ack != null && ack != reported
    )
}
