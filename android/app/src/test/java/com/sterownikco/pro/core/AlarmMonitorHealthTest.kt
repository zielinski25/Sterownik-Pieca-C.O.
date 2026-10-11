package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmMonitorHealthTest {
    @Test
    fun disabledMonitoringOverridesServiceSnapshot() {
        val text = AlarmMonitorHealthText.describe(
            monitored = false,
            hasSession = true,
            health = AlarmMonitorHealth(AlarmMonitorPhase.STATUS_OK, 90_000L),
            nowElapsedMs = 100_000L
        )

        assertEquals("Monitoring alarmów jest wyłączony.", text)
    }

    @Test
    fun missingSessionDoesNotClaimServiceIsRunning() {
        val text = AlarmMonitorHealthText.describe(
            monitored = true,
            hasSession = false,
            health = AlarmMonitorHealth(AlarmMonitorPhase.STATUS_OK, 90_000L),
            nowElapsedMs = 100_000L
        )

        assertEquals("Brak zapisanej sesji Firebase — monitoring w tle nie wystartuje.", text)
    }

    @Test
    fun freshSuccessfulReadIsDescribedWithoutDeliveryGuarantee() {
        val text = AlarmMonitorHealthText.describe(
            monitored = true,
            hasSession = true,
            health = AlarmMonitorHealth(AlarmMonitorPhase.STATUS_OK, 82_000L),
            nowElapsedMs = 100_000L
        )

        assertTrue(text.contains("Usługa odebrała status z RTDB."))
        assertTrue(text.contains("Ostatni odczyt RTDB: przed chwilą."))
        assertTrue(text.contains("nie gwarantuje przyszłego alarmu"))
    }

    @Test
    fun staleStatusIsExplicitlyMarkedAsPossiblyOutdated() {
        val text = AlarmMonitorHealthText.describe(
            monitored = true,
            hasSession = true,
            health = AlarmMonitorHealth(AlarmMonitorPhase.RETRYING, 1_000L),
            nowElapsedMs = 131_000L
        )

        assertTrue(text.contains("usługa ponawia próbę"))
        assertTrue(text.contains("Ostatni odczyt RTDB: 2 min temu; dane mogą być nieaktualne."))
    }

    @Test
    fun connectingWithoutAnyReadDoesNotImplyRtdbIsReachable() {
        val text = AlarmMonitorHealthText.describe(
            monitored = true,
            hasSession = true,
            health = AlarmMonitorHealth(AlarmMonitorPhase.CONNECTING),
            nowElapsedMs = 100_000L
        )

        assertTrue(text.contains("Usługa łączy się z RTDB."))
        assertTrue(text.contains("Brak potwierdzonego odczytu RTDB."))
    }

    @Test
    fun stoppedServiceIsNotPresentedAsReady() {
        val text = AlarmMonitorHealthText.describe(
            monitored = true,
            hasSession = true,
            health = AlarmMonitorHealth(),
            nowElapsedMs = 100_000L
        )

        assertTrue(text.contains("nie jest potwierdzona jako uruchomiona"))
        assertTrue(text.contains("Brak potwierdzonego odczytu RTDB."))
    }

    @Test
    fun failedForegroundStartIsShownAsFailure() {
        val text = AlarmMonitorHealthText.describe(
            monitored = true,
            hasSession = true,
            health = AlarmMonitorHealth(AlarmMonitorPhase.START_FAILED),
            nowElapsedMs = 100_000L
        )

        assertTrue(text.contains("Nie udało się zlecić uruchomienia usługi."))
    }
}
