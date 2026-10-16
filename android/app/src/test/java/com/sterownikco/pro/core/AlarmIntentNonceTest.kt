package com.sterownikco.pro.core

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmIntentNonceTest {
    @Test
    fun missingNonceIsRejected() {
        assertFalse(AlarmIntentNonce.matches(null, "current"))
    }

    @Test
    fun blankNonceIsRejected() {
        assertFalse(AlarmIntentNonce.matches("  ", "  "))
    }

    @Test
    fun currentNonceIsAccepted() {
        assertTrue(AlarmIntentNonce.matches("current", "current"))
    }

    @Test
    fun staleNonceIsRejected() {
        assertFalse(AlarmIntentNonce.matches("previous", "current"))
    }
}
