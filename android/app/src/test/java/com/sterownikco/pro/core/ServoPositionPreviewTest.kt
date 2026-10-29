package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ServoPositionPreviewTest {
    @Test
    fun percentCommandsMapToTheSameIntegerDegreesAsFirmware() {
        for (percent in 0..100) {
            val flap = parseServoPositionCommand("klapa $percent")!!.preview(1_000L)
            val damper = parseServoPositionCommand("syberek $percent")!!.preview(1_000L)

            assertEquals(percent, flap.requestedPercent)
            assertEquals(percent * 180 / 100, flap.targetDegrees)
            assertEquals(percent, damper.requestedPercent)
            assertEquals(percent * 90 / 100, damper.targetDegrees)
        }
    }

    @Test
    fun parserAcceptsPercentSuffixAndClampsLikeFirmware() {
        assertEquals(
            ServoPositionCommand(ServoPositionAxis.KLAPA, 13),
            parseServoPositionCommand(" KLAPA 13% ")
        )
        assertEquals(0, parseServoPositionCommand("klapa -7")?.percent)
        assertEquals(100, parseServoPositionCommand("syberek 140")?.percent)
        assertNull(parseServoPositionCommand("klapa nope"))
        assertNull(parseServoPositionCommand("pompa_wl"))
    }

    @Test
    fun targetRemainsPendingUntilMatchingAngleArrivesOrTimesOut() {
        val target = parseServoPositionCommand("klapa 13")!!.preview(5_000L)

        assertEquals(23, target.targetDegrees)
        assertTrue(shouldKeepServoPositionPreview(target, reportedDegrees = 0, nowElapsedMs = 5_001L))
        assertTrue(shouldKeepServoPositionPreview(target, reportedDegrees = null, nowElapsedMs = 20_000L))
        assertFalse(shouldKeepServoPositionPreview(target, reportedDegrees = 23, nowElapsedMs = 20_000L))
        assertFalse(shouldKeepServoPositionPreview(target, reportedDegrees = 0, nowElapsedMs = 35_000L))
    }

    @Test
    fun ackRefreshCoversServoPumpMixerAndSettingsTiles() {
        listOf(
            "klapa 13", "syberek 50", "pompa_wl", "mieszadlo_wyl",
            "ustaw wybor 2", "symuluj ogrz 60 10", "rozpalanie_serwa",
            "alarm_sound dym syrena", "alarm_sound ogrz syrena_ratunkowa",
            "alarm_sound oba system"
        ).forEach { assertTrue("$it should refresh dashboard state", commandChangesDashboardState(it)) }

        listOf("status", "diag remote cat SERVO on", "tg_config token chat 1", "alarm_sound")
            .forEach { assertFalse("$it should not trigger a device-state refresh", commandChangesDashboardState(it)) }
    }
}
