Sterownik CO - aplikacja Android 0.31.72 (pelny projekt)
=========================================================

Co nowego w 0.31.72
-------------------
PAS 37 dp PRZY LEWEJ KRAWEDZI — wlasciwa przyczyna znaleziona i usunieta.

DOWOD, ZE TO NIE BYL NASZ PADDING (pomiar zrzutu 12:30, wersja 0.31.71)
-----------------------------------------------------------------------
W 0.31.71 szyna miala:
    Modifier.width(Dimens.railW + startInset)      // 37 + 80 = 117 dp
    ...padding(start = 8.dp + startInset, ...)
czyli jej obramowanie mialo wypasc przy x=0, a tlo (gradient) ciagnac sie
od samej krawedzi. Tymczasem na zrzucie:

    x=0..58    #060C18   <- kolor tla (@color/bg), nie szyny
    x=59       #1B2633   <- OBRAMOWANIE SZYNY (Pal.NavBorder)
    x=60..184  gradient  <- szyna, 80 dp

Obramowanie przy x=59 dowodzi, ze szyna ZACZYNA SIE 37 dp od krawedzi, mimo
ze nasz kod kazal jej zaczac sie od zera. Skoro tak, to odstep dodawal nie
nasz kod, tylko SYSTEM.

PRZYCZYNA
---------
Aplikacja dzialala w trybie domyslnym (decorFitsSystemWindows = true).
W tym trybie system rozciaga OKNO na caly ekran (dlatego po 0.31.70 czarny
pas zniknal), ale WIDOK TRESCI ustawia 37 dp od wyciecia kamery. Compose
dostaje juz zwezone pole i wszystko, co rysujemy — szyna, tlo, paski —
zaczyna sie od x=59 px. Dlatego ZADNA zmiana w naszym ukladzie nie mogla
tego ruszyc: walczylismy z odstepem, ktorego nie rysowalismy.

CO ZMIENIONE
------------
1. MainActivity.kt - tryb edge-to-edge:

       WindowCompat.setDecorFitsSystemWindows(window, false)

   Teraz tresc rysuje pod paskami systemowymi, a odstepy ustawiamy sami.

2. ui/SterownikApp.kt - pas pod paskiem statusu malujemy sami:

       Box(Modifier.fillMaxWidth()
           .windowInsetsTopHeight(WindowInsets.statusBars)
           .background(Brush.linearGradient(listOf(Pal.Top, Pal.Top2))))

   oraz tresc omija pasek nawigacji Androida: .navigationBarsPadding().
   Szyna nadal pochłania wciecie wyciecia (z 0.31.71), wiec jej tlo dochodzi
   do samej krawedzi, a ikony sa odsuniete od obiektywu.

3. ui/components/Chrome.kt
   - TopBar: wciecie paska statusu PRZED wysokoscia 62 dp, nie w srodku.
     Bez tego pasek tracilby 37 dp (w trybie edge-to-edge tresc zaczyna sie
     od y=0) i tytul by sie scisnal.
   - NavRail: .statusBarsPadding() przed tlem — gorny pas szyny ma kolor
     paska, nie tla.
   - Sheet: .navigationBarsPadding() — dolny arkusz nie wchodzi pod pasek
     nawigacji.

CO ZOSTAJE BEZ ZMIAN
--------------------
- Motyw i shortEdges z 0.31.70.
- Uklad z 0.31.69 (szyna gdy wysokosc < 480 dp i szerokosc >= 600 dp).
- Kolory: Pal.Bg #060D18, Pal.Nav #081421, Pal.Top #071421.

WERSJA
------
versionCode 31072 / versionName 0.31.72
Numer widac w aplikacji: Ustawienia -> Aktualizacja firmware -> wiersz
"APLIKACJA WEB PRO".

JAK ZBUDOWAC
------------
    .\gradlew.bat --no-daemon testDebugUnitTest assembleDebug

UWAGA: kompilacji nie sprawdzono tam, gdzie kod powstal (brak Javy/Gradle).
Brama statyczna: 42 OK / 0 BŁĄD (ta sama brama na 0.31.71: 8 BŁĄD).

CO SPRAWDZIC
------------
1. Poziom: szyna od samej lewej krawedzi, ZERO pionowej linii przy krawedzi.
2. Poziom: pasek gorny ma normalna wysokosc (tytul nie scisniety).
3. Pion: bez zmian; dolny pasek nie wchodzi pod pasek nawigacji.
4. Arkusze (np. Ustawienia -> Aktualizacja) dosuniete do dolu, nad paskiem.

JAK COFNAC (gdyby edge-to-edge cos zepsul)
------------------------------------------
W MainActivity.kt zmienic jedna linie na:
       WindowCompat.setDecorFitsSystemWindows(window, true)
Wtedy wraca zachowanie 0.31.71 (z pasem 37 dp).
