package com.sterownikco.pro.core

/** In-memory dismissal policy for the in-app preview of a still-active alarm. */
internal class AlarmPopupSession {
    private var dismissedSignature: String? = null

    /** A new alarm notification/repeat or explicit open should be allowed to show again. */
    fun onAlarmEvent() {
        dismissedSignature = null
    }

    fun dismiss(signature: String?) {
        dismissedSignature = signature?.takeIf { it.isNotBlank() }
    }

    fun clear() {
        dismissedSignature = null
    }

    fun shouldRestore(signature: String): Boolean =
        signature.isNotBlank() && signature != dismissedSignature
}
