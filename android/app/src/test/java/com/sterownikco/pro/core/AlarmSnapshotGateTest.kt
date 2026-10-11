package com.sterownikco.pro.core

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmSnapshotGateTest {
    @Test
    fun rejectsPollResultWhenServiceSnapshotArrivesDuringRequest() {
        val gate = AlarmSnapshotGate()
        val requestRevision = gate.captureRevision()
        var pollApplied = false

        gate.applyServiceSnapshot { }
        val accepted = gate.applyPollIfCurrent(requestRevision) { pollApplied = true }

        assertFalse(accepted)
        assertFalse(pollApplied)
    }

    @Test
    fun acceptsPollResultWhenNoNewerServiceSnapshotExists() {
        val gate = AlarmSnapshotGate()
        val requestRevision = gate.captureRevision()
        var pollApplied = false

        val accepted = gate.applyPollIfCurrent(requestRevision) { pollApplied = true }

        assertTrue(accepted)
        assertTrue(pollApplied)
    }

    @Test
    fun unrelatedServiceSnapshotDoesNotInvalidateForegroundStatusPoll() {
        val gate = AlarmSnapshotGate()
        val requestRevision = gate.captureRevision()
        var serviceSnapshotApplied = false
        gate.applyServiceSnapshot(alarmFlagsMayHaveChanged = false) { serviceSnapshotApplied = true }
        var pollApplied = false
        val accepted = gate.applyPollIfCurrent(requestRevision) { pollApplied = true }

        assertTrue(serviceSnapshotApplied)
        assertTrue(accepted)
        assertTrue(pollApplied)
    }

    @Test
    fun firstAcceptedPollInvalidatesOlderOverlappingPoll() {
        val gate = AlarmSnapshotGate()
        val sharedRevision = gate.captureRevision()
        var olderPollApplied = false

        assertTrue(gate.applyPollIfCurrent(sharedRevision) { })
        val staleAccepted = gate.applyPollIfCurrent(sharedRevision) { olderPollApplied = true }

        assertFalse(staleAccepted)
        assertFalse(olderPollApplied)
    }
}
