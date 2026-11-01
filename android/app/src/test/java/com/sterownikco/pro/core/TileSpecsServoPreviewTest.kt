package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TileSpecsServoPreviewTest {
    @Test
    fun flapAndDamperUseTheirOwnAngularRanges() {
        val meters = buildServoPositionMeters(klapaDegrees = 90, syberkaDegrees = 45)

        assertEquals(listOf("KLAPA", "SYBEREK"), meters.map { it.label })
        assertEquals(50, meters[0].percentage)
        assertEquals(50, meters[1].percentage)
        assertEquals(0.5, meters[0].fraction!!, 0.0001)
        assertEquals("90°", meters[0].angleText)
        assertEquals("45°", meters[1].angleText)
        assertEquals("50%", meters[0].percentageText)
    }

    @Test
    fun zeroIsAValidPositionAndEndpointsFillTheMeters() {
        val atZero = buildServoPositionMeters(klapaDegrees = 0, syberkaDegrees = 0)
        val atMaximum = buildServoPositionMeters(klapaDegrees = 180, syberkaDegrees = 90)

        assertEquals(0, atZero[0].percentage)
        assertEquals(0, atZero[1].percentage)
        assertEquals(100, atMaximum[0].percentage)
        assertEquals(100, atMaximum[1].percentage)
        assertEquals(1.0, atMaximum[0].fraction!!, 0.0001)
        assertEquals(1.0, atMaximum[1].fraction!!, 0.0001)
    }

    @Test
    fun missingOrOutOfRangeReadingsStayUnknown() {
        val meters = buildServoPositionMeters(klapaDegrees = null, syberkaDegrees = 91)

        assertNull(meters[0].percentage)
        assertNull(meters[0].fraction)
        assertEquals("—", meters[0].angleText)
        assertEquals("—", meters[0].percentageText)
        assertNull(meters[1].percentage)
        assertNull(meters[1].degrees)
        assertTrue(meters.mapNotNull { it.fraction }.all { it in 0.0..1.0 })
    }

    @Test
    fun acknowledgedTargetMeterIsMarkedPendingAndOtherMeterStaysReported() {
        val preview = parseServoPositionCommand("klapa 13")!!.preview(10L)
        val meters = buildServoPositionMeters(
            klapaDegrees = 0,
            syberkaDegrees = 24,
            klapaPreview = preview
        )

        assertEquals(23, meters[0].degrees)
        assertEquals(13, meters[0].percentage)
        assertEquals(0.13, meters[0].fraction!!, 0.0001)
        assertTrue(meters[0].pendingTarget)
        assertEquals(24, meters[1].degrees)
        assertEquals(27, meters[1].percentage)
        assertTrue(!meters[1].pendingTarget)
    }
}
