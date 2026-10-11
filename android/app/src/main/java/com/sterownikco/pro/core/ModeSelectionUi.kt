package com.sterownikco.pro.core

/**
 * Presentation of a remote mode while an accepted command waits for the next
 * authoritative status snapshot. The ACK may lead status, but never changes the
 * reported PiecState itself.
 */
internal data class ModeSelectionPresentation(
    val mode: Int?,
    val awaitingStatus: Boolean
)

internal fun presentModeSelection(
    acknowledgedMode: Int?,
    reportedMode: Int?,
    allowedModes: IntRange
): ModeSelectionPresentation {
    val acknowledged = acknowledgedMode?.takeIf { it in allowedModes }
    val reported = reportedMode?.takeIf { it in allowedModes }
    return ModeSelectionPresentation(
        mode = acknowledged ?: reported,
        awaitingStatus = acknowledged != null && acknowledged != reported
    )
}
