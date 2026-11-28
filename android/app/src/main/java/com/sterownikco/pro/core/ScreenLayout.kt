package com.sterownikco.pro.core

/**
 * Decyzja o układzie ekranu — WYŁĄCZNIE z rozmiaru okna w dp, nigdy z orientacji
 * urządzenia ani z „to tablet / to telefon”.
 *
 * Dlaczego tak (0.31.69):
 *  - Okno zmienia rozmiar przy obrocie, w split-screen, po rozłożeniu foldable
 *    i po swobodnym przeciągnięciu na ChromeOS — więc jedynym poprawnym wejściem
 *    jest bieżący rozmiar okna (wytyczne „adaptive apps”: nigdy `isTablet`, nigdy
 *    `Configuration.orientation`).
 *  - Telefon w poziomie to typowo **Medium/Expanded szerokość × Compact wysokość
 *    (< 480 dp)**. Dolny pasek zabiera wtedy 74 dp z ~330 dp wysokości i wykres
 *    (`Dimens.chartH` = 220 dp) nie mieści się na ekranie — dlatego przy takim
 *    oknie nawigacja przenosi się na bok (80 dp), co daje ~+23 % powierzchni treści.
 *
 * Do 0.31.68 obowiązywała reguła `maxWidth > 560.dp` włączająca sztywną ramkę
 * 412 dp „symulatora telefonu” (przeniesioną z trybu PC w Piec.html). Na telefonie
 * w poziomie oznaczała one 45–56 % okna wypełnionego tłem sceny i ucięte zakładki.
 */
object ScreenLayout {

    /** Górna granica klasy „Compact height” wg wytycznych Androida (telefon w poziomie). */
    const val COMPACT_HEIGHT_MAX_DP = 480

    /** Dolna granica klasy „Medium width” — poniżej niej okno jest za wąskie na szynę. */
    const val MEDIUM_WIDTH_MIN_DP = 600

    /**
     * Czy narysować nawigację jako pionową szynę z boku zamiast dolnego paska.
     *
     * Szyna tylko wtedy, gdy okno jest **niskie i jednocześnie dostatecznie szerokie**:
     *  - wysokość < [COMPACT_HEIGHT_MAX_DP] — brak pionowego miejsca na dolny pasek,
     *  - szerokość >= [MEDIUM_WIDTH_MIN_DP] — jest z czego odjąć 80 dp.
     *
     * Wąskie i niskie okno (np. tablet w postawie „tabletop”, 500×400 dp) zostaje
     * przy dolnym pasku — szyna zabrałaby mu zbyt dużą część szerokości.
     */
    fun useNavRail(windowWidthDp: Int, windowHeightDp: Int): Boolean =
        windowHeightDp < COMPACT_HEIGHT_MAX_DP && windowWidthDp >= MEDIUM_WIDTH_MIN_DP

    /** Ile pionu zostaje na treść przy danej wysokości okna — do symulacji i testów. */
    fun contentHeightDp(
        windowHeightDp: Int,
        topBarDp: Int,
        navBarDp: Int,
        rail: Boolean,
    ): Int = windowHeightDp - topBarDp - (if (rail) 0 else navBarDp)
}
