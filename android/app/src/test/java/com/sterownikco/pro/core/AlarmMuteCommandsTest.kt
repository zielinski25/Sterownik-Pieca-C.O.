package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmMuteCommandsTest {
    @Test
    fun combinedAlarmUsesOneAtomicControllerCommand() {
        assertEquals(
            listOf("alarm_mute oba 600"),
            AlarmMuteCommands.forMute("D+O", seconds = 600)
        )
        assertEquals(
            listOf("alarm_mute oba 0"),
            AlarmMuteCommands.forMute("O+D", seconds = 0)
        )
    }

    @Test
    fun singleAlarmUsesItsOwnControllerMuteChannel() {
        assertEquals(listOf("alarm_mute dym 3600"), AlarmMuteCommands.forMute("D", 3600))
        assertEquals(listOf("alarm_mute ogrz off"), AlarmMuteCommands.forMute("O", null))
    }

    @Test
    fun missingOrInvalidTargetsAreNeverSent() {
        assertTrue(AlarmMuteCommands.forMute("", 600).isEmpty())
        assertTrue(AlarmMuteCommands.forMute("D+X", 600).isEmpty())
        assertTrue(AlarmMuteCommands.forMute("D", 3601).isEmpty())
        assertTrue(AlarmMuteCommands.forMute("X", null).isEmpty())
    }

    @Test
    fun snapshotSignatureRetainsOnlyUnknownPreviouslyActiveFlags() {
        assertEquals("D+O", AlarmMuteCommands.signature(dym = true, ogrz = null, previousSignature = "O"))
        assertEquals("D", AlarmMuteCommands.signature(dym = null, ogrz = false, previousSignature = "D+O"))
        assertEquals("", AlarmMuteCommands.signature(dym = false, ogrz = false, previousSignature = "D+O"))
    }
}
