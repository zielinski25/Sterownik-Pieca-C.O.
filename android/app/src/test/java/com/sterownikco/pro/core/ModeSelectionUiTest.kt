package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ModeSelectionUiTest {
    @Test
    fun acknowledgedChoiceWinsUntilStatusCatchesUp() {
        val state = presentModeSelection(acknowledgedMode = 3, reportedMode = 1, allowedModes = 1..3)

        assertEquals(3, state.mode)
        assertTrue(state.awaitingStatus)
    }

    @Test
    fun matchingStatusReplacesPendingPresentation() {
        val state = presentModeSelection(acknowledgedMode = 2, reportedMode = 2, allowedModes = 1..3)

        assertEquals(2, state.mode)
        assertFalse(state.awaitingStatus)
    }

    @Test
    fun reportedModeIsUsedWithoutPendingAck() {
        val state = presentModeSelection(acknowledgedMode = null, reportedMode = 1, allowedModes = 1..3)

        assertEquals(1, state.mode)
        assertFalse(state.awaitingStatus)
    }

    @Test
    fun invalidModeValuesAreNotDisplayed() {
        val state = presentModeSelection(acknowledgedMode = 4, reportedMode = 0, allowedModes = 1..3)

        assertNull(state.mode)
        assertFalse(state.awaitingStatus)
    }

    @Test
    fun zeroIsValidForTheSmokeSensorModeRange() {
        val state = presentModeSelection(acknowledgedMode = 0, reportedMode = 1, allowedModes = 0..1)

        assertEquals(0, state.mode)
        assertTrue(state.awaitingStatus)
    }

    @Test
    fun mixerModeIsDerivedFromOverrideTimerAndRelayState() {
        assertEquals(0, presentMixerModeSelection(null, 0, null).mode) // Auto
        assertEquals(1, presentMixerModeSelection(null, 12, true).mode) // Force ON
        assertEquals(2, presentMixerModeSelection(null, 12, false).mode) // Force OFF
        assertNull(presentMixerModeSelection(null, 12, null).mode) // insufficient status
    }

    @Test
    fun acknowledgedMixerModeWinsUntilReportedStatusCatchesUp() {
        val state = presentMixerModeSelection(acknowledgedMode = 1, reportedOverrideMinutes = 10, reportedMixerOn = false)

        assertEquals(1, state.mode)
        assertTrue(state.awaitingStatus)
    }
}
