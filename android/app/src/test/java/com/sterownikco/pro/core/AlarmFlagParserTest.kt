package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AlarmFlagParserTest {
    @Test
    fun acceptsBooleanAndZeroOrOneNumbers() {
        assertEquals(true, parseAlarmFlag(true))
        assertEquals(false, parseAlarmFlag(false))
        assertEquals(true, parseAlarmFlag(1))
        assertEquals(false, parseAlarmFlag(0L))
        assertEquals(true, parseAlarmFlag(1.0))
        assertEquals(false, parseAlarmFlag(0.0f))
    }

    @Test
    fun acceptsTrimmedCaseInsensitiveStringFlags() {
        assertEquals(true, parseAlarmFlag(" TRUE "))
        assertEquals(false, parseAlarmFlag("False"))
        assertEquals(true, parseAlarmFlag(" 1 "))
        assertEquals(false, parseAlarmFlag("0"))
    }

    @Test
    fun rejectsNumbersOutsideBooleanDomain() {
        assertNull(parseAlarmFlag(2))
        assertNull(parseAlarmFlag(-1))
        assertNull(parseAlarmFlag(0.5))
        assertNull(parseAlarmFlag(Double.NaN))
        assertNull(parseAlarmFlag(Double.POSITIVE_INFINITY))
    }

    @Test
    fun preservesUnknownForMissingOrMalformedValues() {
        assertNull(parseAlarmFlag(null))
        assertNull(parseAlarmFlag("yes"))
        assertNull(parseAlarmFlag(""))
        assertNull(parseAlarmFlag(Any()))
    }
}
