package com.sterownikco.pro.core

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmMonitorStartGateTest {
    @Test
    fun disabledPreferencePreventsMonitorStartEvenWithSession() {
        assertFalse(AlarmMonitorStartGate.canStart(monitored = false, refreshToken = "refresh-token"))
    }

    @Test
    fun missingOrBlankSessionPreventsMonitorStart() {
        assertFalse(AlarmMonitorStartGate.canStart(monitored = true, refreshToken = null))
        assertFalse(AlarmMonitorStartGate.canStart(monitored = true, refreshToken = ""))
        assertFalse(AlarmMonitorStartGate.canStart(monitored = true, refreshToken = "  "))
    }

    @Test
    fun enabledMonitorWithSessionCanStart() {
        assertTrue(AlarmMonitorStartGate.canStart(monitored = true, refreshToken = "refresh-token"))
    }
}
