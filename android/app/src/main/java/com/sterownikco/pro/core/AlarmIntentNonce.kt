package com.sterownikco.pro.core

/** Rejects forged or stale alarm-open intents without trusting exported-activity extras. */
internal object AlarmIntentNonce {
    fun matches(supplied: String?, expected: String?): Boolean =
        !supplied.isNullOrBlank() && supplied == expected
}
