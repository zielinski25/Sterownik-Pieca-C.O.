package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmCenterTest {
    private class MemoryAlarmPrefs : AlarmPrefsStore {
        private val values = mutableMapOf<String, String>()

        override fun get(key: String): String? = values[key]

        override fun set(key: String, value: String) {
            values[key] = value
        }

        override fun remove(key: String) {
            values.remove(key)
        }

        override fun getLong(key: String, def: Long): Long = values[key]?.toLongOrNull() ?: def

        override fun setLong(key: String, v: Long) {
            values[key] = v.toString()
        }
    }

    @Test
    fun monitoringDisabledDoesNotRaiseOrMutateAlarm() {
        val prefs = MemoryAlarmPrefs().apply {
            set(AlarmCenter.K_MONITOR, "0")
            set(AlarmCenter.K_LAST_SIG, "D")
        }

        assertNull(AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 80.0, dymAdc = 700.0))
        assertEquals("D", prefs.get(AlarmCenter.K_LAST_SIG))
    }

    @Test
    fun firstSmokeEdgeFiresAndDuplicateIsSuppressed() {
        val prefs = MemoryAlarmPrefs()

        val first = AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0)
        assertNotNull(first)
        assertEquals("dym", first!!.kind)
        assertEquals("D", first.sig)

        assertNull(AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0))
    }

    @Test
    fun clearingAlarmAllowsSameAlarmToFireAgain() {
        val prefs = MemoryAlarmPrefs()

        assertNotNull(AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0))
        assertNull(AlarmCenter.evaluate(prefs, dym = false, ogrz = false, tOgrz = 70.0, dymAdc = 0.0))
        assertEquals("", prefs.get(AlarmCenter.K_LAST_SIG))

        assertNotNull(AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0))
    }

    @Test
    fun combinedAlarmUsesCombinedClassification() {
        val alarm = AlarmCenter.evaluate(
            MemoryAlarmPrefs(),
            dym = true,
            ogrz = true,
            tOgrz = 95.5,
            dymAdc = 812.0
        )

        assertNotNull(alarm)
        assertEquals("dym+ogrz", alarm!!.kind)
        assertEquals("D+O", alarm.sig)
        assertEquals("ALARM DYMU I PRZEGRZANIA", alarm.title)
    }

    @Test
    fun describeActiveFlagsReturnsCombinedAlarmInfo() {
        assertNull(AlarmCenter.describeActiveFlags(dym = false, ogrz = false, tOgrz = 70.0, dymAdc = 400.0))
        val info = AlarmCenter.describeActiveFlags(dym = true, ogrz = true, tOgrz = 95.5, dymAdc = 812.0)

        val alarm = requireNotNull(info)
        assertEquals("dym+ogrz", alarm.kind)
        assertEquals("D+O", alarm.sig)
    }

    @Test
    fun partialSnapshotPreservesUnknownFlagAlongsideConfirmedActiveFlag() {
        val prefs = MemoryAlarmPrefs()

        val flags = AlarmCenter.activeFlagsForPartialSnapshot(prefs, dym = true, ogrz = null)

        assertEquals(AlarmFlags(dym = true, ogrz = null), flags)
    }

    @Test
    fun partialSnapshotPreservesPreviouslyActiveUnknownFlag() {
        val prefs = MemoryAlarmPrefs().apply { set(AlarmCenter.K_LAST_SIG, "D+O") }

        val flags = AlarmCenter.activeFlagsForPartialSnapshot(prefs, dym = true, ogrz = null)

        assertEquals(AlarmFlags(dym = true, ogrz = true), flags)
    }

    @Test
    fun partialSnapshotPreservesOtherPreviouslyActiveFlag() {
        val prefs = MemoryAlarmPrefs().apply { set(AlarmCenter.K_LAST_SIG, "D") }

        val flags = AlarmCenter.activeFlagsForPartialSnapshot(prefs, dym = null, ogrz = true)

        assertEquals(AlarmFlags(dym = true, ogrz = true), flags)
    }

    @Test
    fun partialSnapshotWithoutConfirmedActiveFlagCannotClearAlarm() {
        val prefs = MemoryAlarmPrefs().apply { set(AlarmCenter.K_LAST_SIG, "D") }

        val flags = AlarmCenter.activeFlagsForPartialSnapshot(prefs, dym = false, ogrz = null)

        assertNull(flags)
        assertEquals("D", prefs.get(AlarmCenter.K_LAST_SIG))
    }

    @Test
    fun completeSnapshotIsLeftForRegularAlarmEvaluation() {
        val flags = AlarmCenter.activeFlagsForPartialSnapshot(
            MemoryAlarmPrefs(), dym = false, ogrz = false
        )

        assertNull(flags)
    }

    @Test
    fun partialAlarmCanFireAndLaterUnknownSnapshotCannotClearIt() {
        val prefs = MemoryAlarmPrefs()
        val active = requireNotNull(
            AlarmCenter.activeFlagsForPartialSnapshot(prefs, dym = true, ogrz = null)
        )

        val info = AlarmCenter.evaluate(
            prefs, active.dym == true, active.ogrz == true, tOgrz = 70.0, dymAdc = 400.0
        )
        assertNotNull(info)
        assertEquals("D", prefs.get(AlarmCenter.K_LAST_SIG))

        val incompleteClear = AlarmCenter.activeFlagsForPartialSnapshot(prefs, dym = false, ogrz = null)
        assertNull(incompleteClear)
        assertEquals("D", prefs.get(AlarmCenter.K_LAST_SIG))
    }

    @Test
    fun snoozeSuppressesCurrentEdgeButNewAlarmEdgeOverridesIt() {
        val prefs = MemoryAlarmPrefs()
        val now = System.currentTimeMillis()
        prefs.set(AlarmCenter.K_LAST_SIG, "D")
        prefs.setLong(AlarmCenter.K_LAST_FIRE, now)
        prefs.setLong(AlarmCenter.K_SNOOZE_UNTIL, now + 10 * 60_000L)

        assertNull(AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0))

        val escalated = AlarmCenter.evaluate(prefs, dym = true, ogrz = true, tOgrz = 95.5, dymAdc = 812.0)
        assertNotNull(escalated)
        assertEquals("D+O", escalated!!.sig)
        assertEquals(0L, prefs.getLong(AlarmCenter.K_SNOOZE_UNTIL, 0L))
    }

    @Test
    fun expiredSnoozeRingsAgainWhileAlarmRemainsActive() {
        val prefs = MemoryAlarmPrefs()
        val now = System.currentTimeMillis()
        prefs.set(AlarmCenter.K_LAST_SIG, "D")
        prefs.setLong(AlarmCenter.K_LAST_FIRE, now - 20 * 60_000L)
        prefs.setLong(AlarmCenter.K_SNOOZE_UNTIL, now - 1L)

        val repeated = AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0)

        assertNotNull(repeated)
        assertEquals("D", repeated!!.sig)
        assertEquals(0L, prefs.getLong(AlarmCenter.K_SNOOZE_UNTIL, 0L))
    }

    @Test
    fun repeatThresholdSuppressesJustBeforeAndFiresAtExactBoundary() {
        val start = 1_000_000L
        val prefs = MemoryAlarmPrefs().apply {
            set(AlarmCenter.K_LAST_SIG, "D")
            setLong(AlarmCenter.K_LAST_FIRE, start)
        }

        assertNull(AlarmCenter.evaluateAt(
            prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0,
            nowMs = start + AlarmCenter.REPEAT_MS - 1L
        ))
        val repeated = AlarmCenter.evaluateAt(
            prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0,
            nowMs = start + AlarmCenter.REPEAT_MS
        )

        assertNotNull(repeated)
        assertEquals(start + AlarmCenter.REPEAT_MS, prefs.getLong(AlarmCenter.K_LAST_FIRE, 0L))
    }

    @Test
    fun snoozeSuppressesUntilExactDeadlineThenRings() {
        val start = 2_000_000L
        val deadline = start + 60_000L
        val prefs = MemoryAlarmPrefs().apply {
            set(AlarmCenter.K_LAST_SIG, "D")
            setLong(AlarmCenter.K_LAST_FIRE, start)
            setLong(AlarmCenter.K_SNOOZE_UNTIL, deadline)
        }

        assertNull(AlarmCenter.evaluateAt(
            prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0,
            nowMs = deadline - 1L
        ))
        val resumed = AlarmCenter.evaluateAt(
            prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0,
            nowMs = deadline
        )

        assertNotNull(resumed)
        assertEquals(deadline, prefs.getLong(AlarmCenter.K_LAST_FIRE, 0L))
        assertEquals(0L, prefs.getLong(AlarmCenter.K_SNOOZE_UNTIL, 0L))
    }

    @Test
    fun snoozeCountdownAndLoopPolicyUseExactInjectedTime() {
        val start = 3_000_000L
        val prefs = MemoryAlarmPrefs().apply { set(AlarmCenter.K_LAST_SIG, "D") }
        AlarmCenter.snoozeAt(prefs, 10, start)
        val deadline = start + 10 * 60_000L

        assertEquals(10, AlarmCenter.snoozeLeftMinAt(prefs, start))
        assertEquals(1, AlarmCenter.snoozeLeftMinAt(prefs, deadline - 1L))
        assertEquals(0, AlarmCenter.snoozeLeftMinAt(prefs, deadline))
        assertFalse(AlarmCenter.shouldLoopSoundAt(prefs, dym = true, ogrz = false, nowMs = deadline - 1L))
        assertTrue(AlarmCenter.shouldLoopSoundAt(prefs, dym = true, ogrz = false, nowMs = deadline))
    }

    @Test
    fun muteAppliesToCurrentSignatureUntilANewEdge() {
        val prefs = MemoryAlarmPrefs()
        prefs.set(AlarmCenter.K_LAST_SIG, "D")
        AlarmCenter.muteUntilNext(prefs)

        assertNull(AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0))

        val next = AlarmCenter.evaluate(prefs, dym = false, ogrz = true, tOgrz = 95.5, dymAdc = 0.0)
        assertNotNull(next)
        assertEquals("O", next!!.sig)
    }

    @Test
    fun clearingAlarmClearsMuteAndSnooze() {
        val prefs = MemoryAlarmPrefs()
        val now = System.currentTimeMillis()
        prefs.set(AlarmCenter.K_LAST_SIG, "D")
        prefs.set(AlarmCenter.K_MUTED_SIG, "D")
        prefs.setLong(AlarmCenter.K_SNOOZE_UNTIL, now + 60_000L)

        assertNull(AlarmCenter.evaluate(prefs, dym = false, ogrz = false, tOgrz = 70.0, dymAdc = 0.0))

        assertEquals("", prefs.get(AlarmCenter.K_LAST_SIG))
        assertNull(prefs.get(AlarmCenter.K_MUTED_SIG))
        assertEquals(0L, prefs.getLong(AlarmCenter.K_SNOOZE_UNTIL, 0L))
    }

    @Test
    fun oldAlarmMayRepeatAfterRepeatInterval() {
        val prefs = MemoryAlarmPrefs()
        val now = System.currentTimeMillis()
        prefs.set(AlarmCenter.K_LAST_SIG, "D")
        prefs.setLong(AlarmCenter.K_LAST_FIRE, now - AlarmCenter.REPEAT_MS - 1L)

        val repeated = AlarmCenter.evaluate(prefs, dym = true, ogrz = false, tOgrz = 70.0, dymAdc = 400.0)

        assertNotNull(repeated)
        assertEquals("D", repeated!!.sig)
    }

    @Test
    fun invalidSoundPreferenceFallsBackSafely() {
        val prefs = MemoryAlarmPrefs().apply {
            set(AlarmCenter.K_SOUND_DYM, "unknown-tone")
            set(AlarmCenter.K_SOUND_OGRZ, "unknown-tone")
        }

        assertEquals("syrena", AlarmCenter.sound(prefs, "dym"))
        assertEquals("dzwonek", AlarmCenter.sound(prefs, "ogrz"))
        assertFalse(AlarmCenter.sound(prefs, "dym").isBlank())
    }

    @Test
    fun newAlarmSoundChoicesCanBeSavedForEachAlarmType() {
        val prefs = MemoryAlarmPrefs()
        val newSounds = listOf(
            "syrena_falujaca", "syrena_ratunkowa", "klakson_pulsacyjny", "gong_ostrzegawczy"
        )

        for (sound in newSounds) {
            AlarmCenter.setSound(prefs, "dym", sound)
            AlarmCenter.setSound(prefs, "ogrz", sound)
            AlarmCenter.setSound(prefs, "dym+ogrz", sound)

            assertEquals(sound, AlarmCenter.sound(prefs, "dym"))
            assertEquals(sound, AlarmCenter.sound(prefs, "ogrz"))
            assertEquals(sound, AlarmCenter.sound(prefs, "dym+ogrz"))
        }
    }

    @Test
    fun activeUnsilencedAlarmLoopsSoundOnlyWhenMonitoringIsEnabled() {
        val prefs = MemoryAlarmPrefs()
        assertTrue(AlarmCenter.shouldLoopSound(prefs, dym = true, ogrz = false))

        AlarmCenter.setMonitored(prefs, false)
        assertFalse(AlarmCenter.shouldLoopSound(prefs, dym = true, ogrz = false))
    }
}
