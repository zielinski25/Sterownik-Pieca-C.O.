package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ServoModeUiTest {
    @Test
    fun acknowledgedModeIsVisibleWhileStatusIsBehind() {
        val presentation = presentServoMode(acknowledgedMode = 2, reportedMode = 1)

        assertEquals(2, presentation.mode)
        assertTrue(presentation.awaitingStatus)
    }

    @Test
    fun matchingStatusTakesOverWithoutChangingDisplayedMode() {
        val presentation = presentServoMode(acknowledgedMode = 2, reportedMode = 2)

        assertEquals(2, presentation.mode)
        assertFalse(presentation.awaitingStatus)
    }

    @Test
    fun reportedModeIsUsedWhenThereIsNoAcknowledgedOverride() {
        val presentation = presentServoMode(acknowledgedMode = null, reportedMode = 3)

        assertEquals(3, presentation.mode)
        assertFalse(presentation.awaitingStatus)
    }

    @Test
    fun invalidModesAreNeverPresented() {
        val presentation = presentServoMode(acknowledgedMode = 4, reportedMode = 0)

        assertNull(presentation.mode)
        assertFalse(presentation.awaitingStatus)
    }
}
