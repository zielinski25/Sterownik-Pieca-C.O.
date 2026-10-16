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

/**
 * The controller reports the mixer's manual-override timer and the relay state,
 * rather than a dedicated mixer mode. A positive timer plus relay state tells
 * us which force button is active; a zero timer means control has returned to Auto.
 */
internal fun presentMixerModeSelection(
    acknowledgedMode: Int?,
    reportedOverrideMinutes: Int?,
    reportedMixerOn: Boolean?
): ModeSelectionPresentation {
    val reportedMode = when {
        reportedOverrideMinutes == null || reportedOverrideMinutes < 0 -> null
        reportedOverrideMinutes == 0 -> 0 // Auto
        reportedMixerOn == true -> 1      // Force ON
        reportedMixerOn == false -> 2     // Force OFF
        else -> null
    }
    return presentModeSelection(
        acknowledgedMode = acknowledgedMode,
        reportedMode = reportedMode,
        allowedModes = 0..2
    )
}
