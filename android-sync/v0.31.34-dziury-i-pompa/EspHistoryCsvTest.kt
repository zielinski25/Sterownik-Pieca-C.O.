package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * [v0.31.33 ESP-HISTORY] Parser lokalnego archiwum centrali (`GET /api/history`).
 *
 * Kolumny (1:1 z `historiaLiniaCsv()` w main_centrala.cpp):
 * 0 etykieta, 1 t_zewn, 2 t_bojler, 3 t_ogrz, 4 t_ogrz_sr, 5 t_powrot, 6 t_panel,
 * 7 t_pokoj, 8 pompa, 9 cisnienie, 10 wilgotnosc, 11 klapa %, 12 syberka %, 13 czas_unix.
 */
class EspHistoryCsvTest {

    @Test
    fun parsesFourteenColumnRows() {
        val csv = "2026-10-24 11:06,5.3,58.2,61.0,60.5,45.1,32.4,21.7,1,1002.4,54,37,12,1761303960\n" +
            "2026-10-24 11:07,5.3,58.1,61.2,60.6,45.2,32.9,21.6,0,1002.5,54,37,12,1761304020\n"
        val rows = parseEspHistoryCsv(csv)
        assertEquals(2L, rows.size.toLong())
        val r = rows[0]
        assertEquals(1761303960L, r.ts)
        assertEquals(5.3, r.t_zewn!!, 1e-9)
        assertEquals(58.2, r.t_bojler!!, 1e-9)
        assertEquals(61.0, r.t_ogrz!!, 1e-9)
        assertEquals(60.5, r.t_ogrz_sr!!, 1e-9)
        assertEquals(45.1, r.t_powrot!!, 1e-9)
        assertEquals(32.4, r.t_panel!!, 1e-9)
        assertEquals(21.7, r.t_pokoj!!, 1e-9)
        assertEquals(1002.4, r.cisnienie!!, 1e-9)
        assertEquals(54.0, r.wilgotnosc!!, 1e-9)
        assertEquals(37.0, r.klapa!!, 1e-9)
        assertEquals(12.0, r.syberka!!, 1e-9)
        assertEquals(true, r.pumpOn)
        assertEquals(false, rows[1].pumpOn)
        // Pola, których bufor krótki nie ma, zostają puste — nie zmyślamy zer.
        assertNull(r.t_trociny)
        assertNull(r.dym)
    }

    @Test
    fun usesLastColumnUnixEvenWhenValuesContainIntegers() {
        // Wiersze mają w środku liczby całkowite (pompa, wilgotność, serwa) —
        // czas musi pochodzić z kolumny 13, nie z pierwszej „podobnej do unix".
        val csv = "2026-10-24 11:06,5.3,58.2,61.0,60.5,45.1,32.4,21.7,1,1002.4,54,37,12,1761303960\n"
        assertEquals(1761303960L, parseEspHistoryCsv(csv).single().ts)
    }

    @Test
    fun fallsBackToLabelWhenUnixColumnIsZero() {
        // Znany błąd starych plików: ostatnia kolumna bywa „,0" → czas z etykiety.
        val csv = "2026-10-24 11:06,5.3,58.2,61.0,60.5,45.1,32.4,21.7,1,1002.4,54,37,12,0\n"
        val row = parseEspHistoryCsv(csv).single()
        val cal = java.util.Calendar.getInstance().apply { timeInMillis = row.ts * 1000L }
        assertEquals(2026L, cal.get(java.util.Calendar.YEAR).toLong())
        assertEquals(java.util.Calendar.OCTOBER.toLong(), cal.get(java.util.Calendar.MONTH).toLong())
        assertEquals(24L, cal.get(java.util.Calendar.DAY_OF_MONTH).toLong())
        assertEquals(11L, cal.get(java.util.Calendar.HOUR_OF_DAY).toLong())
        assertEquals(6L, cal.get(java.util.Calendar.MINUTE).toLong())
    }

    @Test
    fun skipsBrokenAndShortLines() {
        val csv = "\n" +
            "2026-10-24 11:06,5.3\n" +                                   // za mało kolumn
            "to nie jest wiersz csv\n" +
            "2026-10-24 11:06,5.3,58.2,61.0,60.5,45.1,32.4,21.7,1,1002.4,54,37,12,1761303960\n"
        assertEquals(1, parseEspHistoryCsv(csv).size)
    }

    @Test
    fun sortsByTimeAndKeepsOneRowPerTimestamp() {
        val csv = "2026-10-24 11:08,5.3,58.1,61.2,60.6,45.2,32.9,21.6,0,1002.5,54,37,12,1761304080\n" +
            "2026-10-24 11:06,5.3,58.2,61.0,60.5,45.1,32.4,21.7,1,1002.4,54,37,12,1761303960\n" +
            "2026-10-24 11:06,5.3,58.2,61.0,60.5,45.1,32.4,21.7,1,1002.4,54,37,12,1761303960\n"
        val rows = parseEspHistoryCsv(csv)
        assertEquals(2L, rows.size.toLong())
        assertTrue(rows[0].ts < rows[1].ts)
    }

    @Test
    fun handlesEmptyInput() {
        assertTrue(parseEspHistoryCsv("").isEmpty())
        assertTrue(parseEspHistoryCsv("   \n\n").isEmpty())
    }

    @Test
    fun keepsTemperatureColumnsIndependentOfHeaderLine() {
        val csv = "etykieta,t_zewn,t_bojler,t_ogrz,t_ogrz_sr,t_powrot,t_panel,t_pokoj,pompa,cisnienie,wilgotnosc,klapa,syberka,czas_unix\n" +
            "2026-10-24 11:06,5.3,58.2,61.0,60.5,45.1,32.4,21.7,1,1002.4,54,37,12,1761303960\n"
        val rows = parseEspHistoryCsv(csv)
        assertEquals(1L, rows.size.toLong())
        assertNotNull(rows[0].t_panel)
    }
}
