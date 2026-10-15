package com.sterownikco.pro.core

/** Pure startup policy for the background alarm monitor. */
internal object AlarmMonitorStartGate {
    /**
     * [lanAvailable] = włączone połączenie lokalne i znany adres centrali: czuwanie działa
     * wtedy także bez sesji Firebase (status z centrali w sieci lokalnej, bez internetu).
     */
    fun canStart(monitored: Boolean, refreshToken: String?, lanAvailable: Boolean = false): Boolean =
        monitored && (!refreshToken.isNullOrBlank() || lanAvailable)
}
