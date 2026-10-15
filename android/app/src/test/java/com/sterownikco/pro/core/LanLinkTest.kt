package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class LanLinkTest {
    @Test
    fun candidatesArePrivateDistinctAndOrderedByPriority() {
        assertEquals(
            listOf("192.168.1.77", "192.168.1.50", LanCandidates.AP_DEFAULT_IP),
            LanCandidates.build(" 192.168.1.77 ", "192.168.1.50")
        )
        // publiczny adres, śmieci i duplikaty odpadają; hotspot zostaje zawsze
        assertEquals(listOf("10.0.0.5", LanCandidates.AP_DEFAULT_IP), LanCandidates.build("8.8.8.8", "10.0.0.5"))
        assertEquals(listOf(LanCandidates.AP_DEFAULT_IP), LanCandidates.build(null, "192.168.4.1"))
        assertEquals(listOf(LanCandidates.AP_DEFAULT_IP), LanCandidates.build("abc", ""))
    }

    @Test
    fun onlyControllerStatusIsAccepted() {
        assertTrue(LanCandidates.looksLikeController("STA", hasControllerFields = true))
        assertTrue(LanCandidates.looksLikeController("AP", hasControllerFields = true))
        assertFalse(LanCandidates.looksLikeController("STA", hasControllerFields = false))
        assertFalse(LanCandidates.looksLikeController("", hasControllerFields = true))
        assertFalse(LanCandidates.looksLikeController(null, hasControllerFields = true))
    }

    @Test
    fun probingIsThrottledWhenControllerIsMissing() {
        val schedule = LanProbeSchedule(retryMs = 30_000)
        assertTrue(schedule.shouldProbe(1_000))
        schedule.onMissing(1_000)
        assertFalse(schedule.shouldProbe(20_000))
        assertTrue(schedule.shouldProbe(20_000, force = true))
        assertTrue(schedule.shouldProbe(31_000))
        schedule.onMissing(31_000)
        schedule.probeSoon()
        assertTrue(schedule.shouldProbe(31_001))
        schedule.onMissing(40_000)
        schedule.onFound()
        assertTrue(schedule.shouldProbe(40_001))
    }

    @Test
    fun sharedLinkExpiresAndIsClearedOnlyForTheLostAddress() {
        LanLink.markLost()
        LanLink.markOk("192.168.1.50", fullApi = true, nowMs = 100_000)
        assertEquals("192.168.1.50", LanLink.fresh(130_000)?.ip)
        assertNull(LanLink.fresh(200_000))                 // starsze niż 45 s
        LanLink.markLost("192.168.4.1")                    // inny adres — bez zmian
        assertEquals("192.168.1.50", LanLink.fresh(100_500)?.ip)
        LanLink.markLost("192.168.1.50")
        assertNull(LanLink.active)
    }

    @Test
    fun alarmMonitorCanRunOnLanWithoutFirebaseSession() {
        assertTrue(AlarmMonitorStartGate.canStart(monitored = true, refreshToken = null, lanAvailable = true))
        assertFalse(AlarmMonitorStartGate.canStart(monitored = false, refreshToken = null, lanAvailable = true))
        assertFalse(AlarmMonitorStartGate.canStart(monitored = true, refreshToken = " ", lanAvailable = false))
    }
}
