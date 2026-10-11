package com.sterownikco.pro.core

/**
 * Serializes foreground polling with snapshots accepted by the alarm-monitor service.
 * Capture a revision before network I/O; a poll result is rejected if a newer service snapshot
 * or poll result was applied while that request was in flight. Network calls must stay outside
 * this lock.
 */
internal class AlarmSnapshotGate {
    private var revision = 0L

    @Synchronized
    fun captureRevision(): Long = revision

    @Synchronized
    fun applyServiceSnapshot(apply: () -> Unit) {
        revision += 1L
        apply()
    }

    @Synchronized
    fun applyPollIfCurrent(startRevision: Long, apply: () -> Unit): Boolean {
        if (revision != startRevision) return false
        revision += 1L
        apply()
        return true
    }
}

/** One process-wide gate shared by the foreground model and background service. */
internal object AlarmSnapshotCoordinator {
    private val gate = AlarmSnapshotGate()

    fun captureRevision(): Long = gate.captureRevision()

    fun applyServiceSnapshot(apply: () -> Unit) = gate.applyServiceSnapshot(apply)

    fun applyPollIfCurrent(startRevision: Long, apply: () -> Unit): Boolean =
        gate.applyPollIfCurrent(startRevision, apply)
}
