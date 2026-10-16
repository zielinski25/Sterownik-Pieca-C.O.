package com.sterownikco.pro.core

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PiecStateIgnitionFieldsTest {
    @Test
    fun sharedIgnitionStateAndTimerAreParsedTogether() {
        val state = PiecState()
        state.applyIncomingData(
            JSONObject(
                """{"rozpalanie":true,"rozpalanie_potwierdzone":true,"rozpalanie_pozostalo_s":3599,"rozpalanie_serwa_aktywne":true,"rozpalanie_serwa_dostepne":true,"rozpalanie_serwa_blokada_dymu":false}"""
            )
        )

        assertTrue(state.rozpalanie)
        assertTrue(state.rozpalanie_potwierdzone)
        assertEquals(3599, state.rozpalanie_pozostalo_s)
        assertTrue(state.rozpalanie_serwa_aktywne)
        assertTrue(state.rozpalanie_serwa_dostepne)
        assertFalse(state.rozpalanie_serwa_blokada_dymu)
        assertTrue(
            state.hasAllData(
                "rozpalanie", "rozpalanie_potwierdzone", "rozpalanie_pozostalo_s"
            )
        )
    }

    @Test
    fun olderFirmwareWithoutSharedModeFieldsIsNotReportedAsSupported() {
        val state = PiecState()
        state.applyIncomingData(
            JSONObject(
                """{"rozpalanie":false,"rozpalanie_serwa_aktywne":false,"rozpalanie_serwa_dostepne":true}"""
            )
        )

        assertFalse(state.rozpalanie_potwierdzone)
        assertEquals(0, state.rozpalanie_pozostalo_s)
        assertFalse(state.hasData("rozpalanie_potwierdzone"))
        assertFalse(state.hasData("rozpalanie_pozostalo_s"))
        assertFalse(state.hasAllData("rozpalanie", "rozpalanie_potwierdzone", "rozpalanie_pozostalo_s"))
    }

    @Test
    fun timerMustBeWithinTheOneHourLimit() {
        val state = PiecState()
        state.applyIncomingData(JSONObject("""{"rozpalanie_pozostalo_s":3601}"""))

        assertEquals(0, state.rozpalanie_pozostalo_s)
        assertFalse(state.hasData("rozpalanie_pozostalo_s"))
    }
}
