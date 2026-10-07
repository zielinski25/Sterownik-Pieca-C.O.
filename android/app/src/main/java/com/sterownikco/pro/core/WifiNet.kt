package com.sterownikco.pro.core

/** Sieć Wi-Fi zwracana przez centralę (`/api/wifi/list`, `/api/wifi/scan`). */
data class WifiNet(val ssid: String, val active: Boolean, val rssi: Int, val hasPass: Boolean, val known: Boolean = false) {
    /** `wifiSigBars` — 0..4 kreski na podstawie RSSI. */
    val level: Int get() = when {
        rssi >= -50 -> 4; rssi >= -60 -> 3; rssi >= -70 -> 2; rssi >= -80 -> 1; else -> 0
    }
}
