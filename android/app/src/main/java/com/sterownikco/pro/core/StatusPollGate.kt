package com.sterownikco.pro.core

import kotlinx.coroutines.sync.Mutex

/**
 * Serializes foreground status reads without letting an in-flight routine poll discard
 * an explicit post-ACK refresh. Routine ticks skip when busy; ACK refreshes queue behind
 * the active request and then read the controller again.
 */
internal class StatusPollGate {
    private val mutex = Mutex()

    /** Returns null only when [skipIfBusy] is true and another poll already owns the gate. */
    suspend fun <T> withPoll(skipIfBusy: Boolean, action: suspend () -> T): T? {
        if (skipIfBusy) {
            if (!mutex.tryLock()) return null
        } else {
            mutex.lock()
        }
        return try {
            action()
        } finally {
            mutex.unlock()
        }
    }
}
