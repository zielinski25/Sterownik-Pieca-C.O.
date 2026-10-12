package com.sterownikco.pro.core

import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PiecStateIgnitionFieldsTest {
    @Test
    fun servoIgnitionStateIsIndependentFromMixerIgnition() {
        val state = PiecState()
        state.applyIncomingData(
            JSONObject(
                """{"rozpalanie":true,"rozpalanie_serwa_aktywne":true,"rozpalanie_serwa_dostepne":true,"rozpalanie_serwa_blokada_dymu":false}"""
            )
        )

        assertTrue(state.rozpalanie) // existing mixer override
        assertTrue(state.rozpalanie_serwa_aktywne)
        assertTrue(state.rozpalanie_serwa_dostepne)
        assertFalse(state.rozpalanie_serwa_blokada_dymu)
        assertTrue(state.hasAllData("rozpalanie", "rozpalanie_serwa_aktywne", "rozpalanie_serwa_dostepne"))
    }

    @Test
    fun absentServoIgnitionStateIsNotReportedAsReceived() {
        val state = PiecState()
        state.applyIncomingData(JSONObject("""{"rozpalanie":false}"""))

        assertFalse(state.rozpalanie_serwa_aktywne)
        assertFalse(state.hasData("rozpalanie_serwa_aktywne"))
        assertFalse(state.hasData("rozpalanie_serwa_dostepne"))
    }
}
