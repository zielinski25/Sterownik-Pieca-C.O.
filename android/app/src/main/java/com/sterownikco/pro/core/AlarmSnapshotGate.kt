package com.sterownikco.pro.core

/**
 * Serializes foreground polling with snapshots accepted by the alarm-monitor service.
 * Capture a revision before network I/O; a poll result is rejected if a newer alarm snapshot
 * or foreground poll changed while that request was in flight. Unrelated status fields do not
 * invalidate a full status poll. Network calls must stay outside this lock.
 */
internal class AlarmSnapshotGate {
    private var revision = 0L

    @Synchronized
    fun captureRevision(): Long = revision

    @Synchronized
    fun applyServiceSnapshot(alarmFlagsMayHaveChanged: Boolean = true, apply: () -> Unit) {
        // Full status polls also contain unrelated controls and telemetry. A change to one of
        // those fields must not invalidate the other poll; only a newer alarm snapshot can.
        if (alarmFlagsMayHaveChanged) revision += 1L
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

    fun applyServiceSnapshot(
        alarmFlagsMayHaveChanged: Boolean = true,
        apply: () -> Unit
    ) = gate.applyServiceSnapshot(alarmFlagsMayHaveChanged, apply)

    fun applyPollIfCurrent(startRevision: Long, apply: () -> Unit): Boolean =
        gate.applyPollIfCurrent(startRevision, apply)
}
