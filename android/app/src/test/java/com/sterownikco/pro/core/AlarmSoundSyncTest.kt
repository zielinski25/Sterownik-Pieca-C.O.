package com.sterownikco.pro.core

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmSoundSyncTest {
    private class MemoryAlarmPrefs : AlarmPrefsStore {
        private val values = mutableMapOf<String, String>()
        override fun get(key: String): String? = values[key]
        override fun set(key: String, value: String) { values[key] = value }
        override fun remove(key: String) { values.remove(key) }
        override fun getLong(key: String, def: Long): Long = values[key]?.toLongOrNull() ?: def
        override fun setLong(key: String, v: Long) { values[key] = v.toString() }
    }

    @Test
    fun commandUsesFirmwareTargetsForAllThreeAlarmKinds() {
        assertEquals("alarm_sound dym syrena", AlarmSoundSync.command("dym", "syrena"))
        assertEquals("alarm_sound ogrz system", AlarmSoundSync.command("ogrz", "system"))
        assertEquals("alarm_sound oba gong_ostrzegawczy", AlarmSoundSync.command("dym+ogrz", "gong_ostrzegawczy"))
        assertEquals("alarm_sound oba dwutonowy", AlarmSoundSync.command("oba", "DWUTONOWY"))
    }

    @Test
    fun commandRejectsUnknownKindsAndSoundIdsInsteadOfSendingThem() {
        assertNull(AlarmSoundSync.command("dymu", "syrena"))
        assertNull(AlarmSoundSync.command("dym", "unknown"))
        assertNull(AlarmSoundSync.command("", "system"))
        assertEquals(12, AlarmSoundSync.SOUND_IDS.size)
    }

    @Test
    fun statusSoundIdsAreValidatedAndMissingValuesAreNotFreshData() {
        val state = PiecState()
        state.applyIncomingData(
            JSONObject(
                """{"alarm_sound_dym":"SYRENA","alarm_sound_ogrz":"dzwonek","alarm_sound_both":"system"}"""
            )
        )

        assertEquals("syrena", state.alarm_sound_dym)
        assertEquals("dzwonek", state.alarm_sound_ogrz)
        assertEquals("system", state.alarm_sound_both)
        assertTrue(state.hasAllData("alarm_sound_dym", "alarm_sound_ogrz", "alarm_sound_both"))

        state.applyIncomingData(JSONObject("""{"alarm_sound_dym":"not-a-sound"}"""))
        assertEquals("", state.alarm_sound_dym)
        assertFalse(state.hasData("alarm_sound_dym"))
        assertFalse(state.hasData("alarm_sound_ogrz"))
        assertFalse(state.hasData("alarm_sound_both"))
    }

    @Test
    fun controllerSnapshotSynchronizesOnlyValidSoundPreferences() {
        val prefs = MemoryAlarmPrefs()
        AlarmSoundSync.applyStatus(
            prefs,
            JSONObject(
                """{"alarm_sound_dym":"pikanie","alarm_sound_ogrz":"system","alarm_sound_both":"invalid"}"""
            ),
            nowElapsedMs = 10_000L
        )

        assertEquals("pikanie", AlarmCenter.sound(prefs, "dym"))
        assertEquals("system", AlarmCenter.sound(prefs, "ogrz"))
        assertEquals("dwutonowy", AlarmCenter.sound(prefs, "dym+ogrz"))
    }

    @Test
    fun staleControllerSnapshotCannotOverwriteRecentPhoneSelection() {
        val prefs = MemoryAlarmPrefs()
        val target = AlarmSoundSync.targetFor("dym")!!
        val selected = "syrena_ratunkowa"
        AlarmCenter.setSound(prefs, target.preferenceKind, selected)
        AlarmSoundSync.rememberSelection(target, selected, nowElapsedMs = 1_000L)

        AlarmSoundSync.applyStatus(
            prefs,
            JSONObject("""{"alarm_sound_dym":"pikanie"}"""),
            nowElapsedMs = 2_000L
        )
        assertEquals(selected, AlarmCenter.sound(prefs, "dym"))

        AlarmSoundSync.applyStatus(
            prefs,
            JSONObject("""{"alarm_sound_dym":"$selected"}"""),
            nowElapsedMs = 2_001L
        )
        assertEquals(selected, AlarmCenter.sound(prefs, "dym"))
    }

    @Test
    fun controllerSettingWinsAfterAcknowledgementWindowExpires() {
        val prefs = MemoryAlarmPrefs()
        val target = AlarmSoundSync.targetFor("ogrz")!!
        AlarmCenter.setSound(prefs, target.preferenceKind, "narastajacy")
        AlarmSoundSync.rememberSelection(target, "narastajacy", nowElapsedMs = 0L)

        AlarmSoundSync.applyStatus(
            prefs,
            JSONObject("""{"alarm_sound_ogrz":"system"}"""),
            nowElapsedMs = AlarmSoundSync.STATUS_TIMEOUT_MS
        )
        assertEquals("system", AlarmCenter.sound(prefs, "ogrz"))
    }
}
