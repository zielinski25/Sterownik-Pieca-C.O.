package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CommandRedactorTest {
    @Test
    fun labelsTelegramConfigurationWithoutExposingCredentials() {
        val raw = "tg_config 123456:placeholder-token 987654321 1"
        val display = CommandRedactor.forDisplay(raw)

        assertTrue(CommandRedactor.isSensitive(raw))
        assertEquals("Konfiguracja Telegrama", display)
        assertFalse(display.contains("tg_config"))
        assertFalse(display.contains("placeholder-token"))
        assertFalse(display.contains("987654321"))
    }

    @Test
    fun labelsEveryAppSettingInPolishInsteadOfShowingVariableNames() {
        val expectedLabels = mapOf(
            "ustaw wybor 1" to "Tryb pracy pompy: Trociniak",
            "ustaw czasOn 12" to "Czas pracy pompy: 12 min",
            "ustaw czasOff 20" to "Czas postoju pompy: 20 min",
            "ustaw tempOn 65" to "Temperatura załączenia pompy: 65 °C",
            "ustaw tempOff 40" to "Temperatura wyłączenia pompy: 40 °C",
            "ustaw antystopWlaczony 0" to "Funkcja Antystop: Wyłączona",
            "ustaw antystopDni 7" to "Czas bezruchu przed uruchomieniem Antystopu: 7 dni",
            "ustaw trybSerwa 3" to "Tryb serwa: Bezpieczna",
            "ustaw tempZadServo 45" to "Temperatura zadana serwa: 45 °C",
            "ustaw histerServo 3" to "Histereza serwa: 3 °C",
            "ustaw skokKlapy 10" to "Skok klapy: 10%",
            "ustaw skokSyberka 15" to "Skok syberka: 15%",
            "ustaw odchylTemp 12" to "Odchylenie przyspieszające: 12 °C",
            "ustaw mnoznik 2" to "Mnożnik korekty kroku: 2×",
            "ustaw mieszadloWlaczony 1" to "Mieszadło: Włączone",
            "ustaw mieszadloCzasOn 30" to "Czas pracy mieszadła: 30 s",
            "ustaw mieszadloCzasOff 5" to "Przerwa pracy mieszadła: 5 min",
            "ustaw progAlarmTemp 90" to "Próg alarmu przegrzania: 90 °C",
            "ustaw progAlarmDym 1200" to "Próg alarmu czujnika dymu: 1200 ADC",
            "ustaw dymProgTemp 45" to "Temperatura aktywacji czujnika dymu: 45 °C",
            "ustaw dymTrybPracy 1" to "Tryb czujnika dymu: Ciągły",
            "ustaw dymCzasOn 60" to "Czas pracy czujnika dymu: 60 s",
            "ustaw dymCzasOff 30" to "Przerwa między pomiarami czujnika dymu: 30 s",
            "ustaw dymCzasStabilizacji 20" to "Czas stabilizacji czujnika dymu: 20 s"
        )

        expectedLabels.forEach { (command, label) ->
            assertEquals(command, label, CommandRedactor.forDisplay(command))
        }
    }

    @Test
    fun labelsCommonActionsAndSensorSimulationsInPolish() {
        assertEquals("Pompa CO: ręczne włączenie", CommandRedactor.forDisplay("pompa_wl"))
        assertEquals("Pozycja klapy: 42%", CommandRedactor.forDisplay("klapa 42"))
        assertEquals("Synchronizacja zegara centrali", CommandRedactor.forDisplay("rtc_sync 2026 10 10 12 30 0"))
        assertEquals(
            "Symulacja: Temperatura zewnętrzna — 12.5 °C · 15 min",
            CommandRedactor.forDisplay("symuluj zewn 12.5 15")
        )
        assertEquals(
            "Wyłączenie symulacji: Czujnik dymu i spalin",
            CommandRedactor.forDisplay("symuluj_stop dym")
        )
        assertEquals("Zdalny odbiór logów: włączony", CommandRedactor.forDisplay("diag remote on"))
        assertEquals(
            "Logi z kategorii czujniki: wyłączone",
            CommandRedactor.forDisplay("diag remote cat SENSOR off")
        )
        assertEquals(
            "Szczegółowość logów: diagnostyczny",
            CommandRedactor.forDisplay("diag remote level DEBUG")
        )
    }

    @Test
    fun doesNotExposeUnknownCommandOrSettingNamesAndValues() {
        assertEquals("Polecenie sterownika", CommandRedactor.forDisplay("internal_variable SECRET"))
        assertEquals("Zmiana ustawień sterownika", CommandRedactor.forDisplay("ustaw privateSetting SECRET"))
        assertEquals("Tryb serwa", CommandRedactor.forDisplay("ustaw trybSerwa secret"))
    }

    @Test
    fun matchesTheVerbCaseInsensitivelyAndWithWhitespace() {
        assertTrue(CommandRedactor.isSensitive("  TG_CONFIG\ttoken chat 1"))
        assertFalse(CommandRedactor.isSensitive("tg_configure token chat 1"))
        assertEquals("Tryb serwa: Bezpieczna", CommandRedactor.forDisplay("  USTAW\tTRYBSERWA 3  "))
    }
}
