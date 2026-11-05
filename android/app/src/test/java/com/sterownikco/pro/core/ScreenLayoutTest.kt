package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Brama na błąd z 0.31.68: aplikacja „nie rozciągała się” po obrocie telefonu,
 * bo sztywna ramka 412 dp włączała się od samej szerokości (`maxWidth > 560.dp`).
 *
 * Wymiary w dp wzięte z realnych okien (px / density):
 *  - telefon użytkownika: 735×331 dp w poziomie (ze zrzutu: ramka 412 dp = 1210 px,
 *    czyli 2,94 px/dp; ekran 2158 px ≈ 735 dp),
 *  - Pixel 7: 915×412 dp w poziomie,
 *  - tablet 10": 1280×800 dp,
 *  - split-screen: 851×400 dp.
 */
class ScreenLayoutTest {

    // [PRÓBA BRAMKI 0.31.82 — TYMCZASOWE, DO USUNIĘCIA W NASTĘPNYM COMMICIE]
    // Wymuszony błąd testu. Cel: sprawdzić na żywym runnerze, że czerwony test
    // naprawdę zatrzymuje wydanie — nie powstaje commit z APK ani CI-TESTY.txt.
    @Test
    fun probaBramkiWymuszonyBlad() {
        error("proba bramki 0.31.82 - zamierzony blad, do cofniecia")
    }

    @Test
    fun portraitPhoneKeepsBottomBar() {
        // 331×735 dp — najczęstszy przypadek, musi zostać bez zmian.
        assertFalse(ScreenLayout.useNavRail(windowWidthDp = 331, windowHeightDp = 735))
    }

    @Test
    fun landscapePhoneSwitchesToRail() {
        assertTrue(ScreenLayout.useNavRail(windowWidthDp = 735, windowHeightDp = 331))
        assertTrue(ScreenLayout.useNavRail(windowWidthDp = 915, windowHeightDp = 412))
        assertTrue(ScreenLayout.useNavRail(windowWidthDp = 851, windowHeightDp = 400))
    }

    @Test
    fun tallLandscapeTabletKeepsBottomBar() {
        // 1280×800 dp ma dużo pionu (800 dp), więc dolny pasek niczego nie zabiera:
        // 800 - 62 - 74 = 664 dp treści, wykres 220 dp mieści się z zapasem.
        // Szyna jest odpowiedzią na BRAK PIONU, a nie na samą szerokość — zmiana
        // nawigacji na szerokich tabletach to osobna decyzja (krok 3), nie ta poprawka.
        assertFalse(ScreenLayout.useNavRail(windowWidthDp = 1280, windowHeightDp = 800))
        assertTrue(
            ScreenLayout.contentHeightDp(800, topBarDp = 62, navBarDp = 74, rail = false) >= 220
        )
    }

    @Test
    fun narrowAndShortWindowKeepsBottomBar() {
        // Tablet w postawie „tabletop” / małe okno: szyna zabrałaby zbyt dużo szerokości.
        assertFalse(ScreenLayout.useNavRail(windowWidthDp = 500, windowHeightDp = 400))
        assertFalse(ScreenLayout.useNavRail(windowWidthDp = 599, windowHeightDp = 400))
    }

    @Test
    fun breakpointIsInclusiveOnWidthAndExclusiveOnHeight() {
        assertTrue(ScreenLayout.useNavRail(windowWidthDp = 600, windowHeightDp = 479))
        assertFalse(ScreenLayout.useNavRail(windowWidthDp = 600, windowHeightDp = 480))
    }

    @Test
    fun railGivesBackEnoughHeightForTheChart() {
        // Wykres ma stałą wysokość 220 dp (Dimens.chartH). Przy dolnym pasku
        // 331 - 62 - 74 = 195 dp — za mało. Przy szynie 331 - 62 = 269 dp.
        val bottomBar = ScreenLayout.contentHeightDp(331, topBarDp = 62, navBarDp = 74, rail = false)
        val rail = ScreenLayout.contentHeightDp(331, topBarDp = 62, navBarDp = 74, rail = true)

        assertEquals(195, bottomBar)
        assertEquals(269, rail)
        assertTrue("z szyną wykres 220 dp musi się zmieścić", rail >= 220)
        assertTrue("z dolnym paskiem się nie mieści — i to jest powód szyny", bottomBar < 220)
    }
}
