package com.sterownikco.pro.core

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class StatusPollGateTest {
    @Test
    fun routinePollsCoalesceButPostAckPollWaitsForFreshRead() = runBlocking {
        val gate = StatusPollGate()
        val routineStarted = CompletableDeferred<Unit>()
        val finishRoutine = CompletableDeferred<Unit>()
        val acknowledgedPollStarted = CompletableDeferred<Unit>()

        val routinePoll = async(start = CoroutineStart.UNDISPATCHED) {
            gate.withPoll(skipIfBusy = true) {
                routineStarted.complete(Unit)
                finishRoutine.await()
                "routine"
            }
        }
        assertTrue(routineStarted.isCompleted)

        var skippedActionRan = false
        val skippedPoll = gate.withPoll(skipIfBusy = true) {
            skippedActionRan = true
            "should not run"
        }
        assertNull(skippedPoll)
        assertFalse(skippedActionRan)

        val postAckPoll = async(start = CoroutineStart.UNDISPATCHED) {
            gate.withPoll(skipIfBusy = false) {
                acknowledgedPollStarted.complete(Unit)
                "post-ack"
            }
        }
        assertFalse(acknowledgedPollStarted.isCompleted)

        finishRoutine.complete(Unit)
        assertEquals("routine", routinePoll.await())
        assertEquals("post-ack", postAckPoll.await())
        assertTrue(acknowledgedPollStarted.isCompleted)
    }

    @Test
    fun cancellationReleasesThePollGate() = runBlocking {
        val gate = StatusPollGate()
        val pollStarted = CompletableDeferred<Unit>()
        val activePoll = async(start = CoroutineStart.UNDISPATCHED) {
            gate.withPoll(skipIfBusy = false) {
                pollStarted.complete(Unit)
                awaitCancellation<String>()
            }
        }
        assertTrue(pollStarted.isCompleted)

        activePoll.cancelAndJoin()
        assertEquals("next", gate.withPoll(skipIfBusy = false) { "next" })
    }
}
