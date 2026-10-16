package com.sterownikco.pro.core

/** Pure startup policy for the background alarm monitor. */
internal object AlarmMonitorStartGate {
    fun canStart(monitored: Boolean, refreshToken: String?): Boolean =
        monitored && !refreshToken.isNullOrBlank()
}
