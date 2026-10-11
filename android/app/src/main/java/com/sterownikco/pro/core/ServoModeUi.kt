package com.sterownikco.pro.core

/** Kept as the servo-specific API; shared validation/presentation lives in ModeSelectionUi. */
internal data class ServoModePresentation(
    val mode: Int?,
    val awaitingStatus: Boolean
)

internal fun presentServoMode(acknowledgedMode: Int?, reportedMode: Int?): ServoModePresentation {
    val mode = presentModeSelection(acknowledgedMode, reportedMode, 1..3)
    return ServoModePresentation(mode.mode, mode.awaitingStatus)
}
