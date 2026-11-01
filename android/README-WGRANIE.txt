Sterownik CO - aplikacja Android 0.31.76 (pelny projekt)
=========================================================

Co nowego w 0.31.76
-------------------
W POZIOMIE PASKI NAWIGACJI TO SAME IKONY: pas ikon 68 dp -> 56 dp.
Pulpit dostaje 12 dp (19 px) wiecej szerokosci.

PUNKT 1 - co to za komponent
----------------------------
To NIE jest NavigationRail z Material 3. To wlasna funkcja `NavRail` w
`app/src/main/java/com/sterownikco/pro/ui/components/Chrome.kt` — zwykla
`Column` z szescioma linijkami wlasnego rysowania (gradient, obramowanie,
wybor). Material 3 nie jest tu uzywany do nawigacji w ogole.

PUNKT 2 - tryb poziomy (co dokladnie zmienione)
-----------------------------------------------
  szerokosc pasa ikon ............ 56 dp  (Dimens.railWCompact)
  pole dotyku pozycji ............ 48 x 48 dp  (bez zmian!)
  ikona .......................... 24 dp  (bylo 22)
  margines po bokach ............. 2 x 4 dp (56 - 8 = 48 dp na kwadrat)
  padding pionowy ................ 8 dp
  odstep miedzy pozycjami ........ 4 dp
  lewy padding ................... WindowInsets.displayCutout (wyciecie
                                   kamery), nigdy stala liczba

Pozycje maja teraz stale 48 dp zamiast `weight(1f)` (wczesniej rozciagaly sie
na cala wysokosc), wiec ukladam je symetrycznie w pionie.

PUNKT 3 - rozgalezienie: rozmiar okna, nie orientacja
-----------------------------------------------------
W `SterownikApp.kt`:
        val compactRail = maxWidth > maxHeight     // BoxWithConstraints
Dlaczego nie `LocalConfiguration.current.orientation`:
  1. od 0.31.69 uklad zalezy od ROZMIARU OKNA — jeden powod decyzji, nie dwa,
     ktore moga sie rozjechac (np. okno dzielone: orientacja dokumentowa,
     a okno pionowe),
  2. na skladanych telefonach i w trybie wielu okien orientacja nie opisuje
     okna, `maxWidth/maxHeight` opisuja je zawsze,
  3. od Androida 16 (targetSdk 36) system ignoruje `screenOrientation`, wiec
     obrot przestaje cokolwiek gwarantowac.
W pionie nic sie nie zmienia: tam nawigacja to dolny pasek `NavBar` z podpisami.

PUNKT 4 - podpisy ukryte, ale dostepne
--------------------------------------
  - przytrzymanie palca na ikonie: `combinedClickable(onLongClick = ...)`
    uruchamia dymek `TooltipBox` z nazwa pozycji,
  - TalkBack czyta nazwe z `contentDescription` + `role = Tab` + `selected`,
  - bez tego "Wiecej" i "Ustawienia" bylyby nierozroznialne.

PUNKT 5 - czy pulpit ma weight(1f)
----------------------------------
Tak: kolumna tresci to `Modifier.weight(1f).fillMaxHeight()` (SterownikApp.kt),
dzieki czemu dostaje cala uwolniona szerokosc. Podwojnego paddingu poziomego
nie ma — sprawdzone pomiarem zrzutu: tresc zaczyna sie 22 px (14 dp) od szyny,
czyli dokladnie `Dimens.pagePadH`, bez drugiego wciecia (to usunela 0.31.74).

PUNKT 6 - prawa strona: zmierzone, przyczyny NIE ma w kodzie
------------------------------------------------------------
Zmierzone na Twoim zrzucie (1280 x 576 px, gestosc 1,5607 px/dp):
  prawa krawedz karty ....... 1186 px
  krawedz tresci (pasek naw.) 1209 px  -> margines 23 px = 15 dp  OK
  brak `horizontalScroll` i `requiredWidth` w korzeniu (SterownikApp.kt)
  kafelki meteo: `MetricGrid` uklada je `weight(1f)`, wiec zawijaja sie
  do dostepnej szerokosci, nie wystaja.
`horizontalScroll` jest w dwoch miejscach i tam jest celowy: pasek chipow
parametrow (Temperatura/Wiatr/Opady/Zachmurzenie) i pasek prognozy godzinowej.
To tasmy, ktore maja sie przewijac w bok — nie karty.
To, co widac na gorze ("duze / brak opadow / niski" bez gornej krawedzi), to
rzedy kafelkow ucieta przez GORNA krawedz obszaru przewijania, czyli normalne
przewiniecie strony o pol kafelka. Nie jest to wystawanie poza ekran.

PUNKT 8 - pomiar pasa
---------------------
PRZED (Twoj zrzut, 0.31.75): pas 167 px / 1,5607 = 107 dp
                             (68 dp ikon z podpisami + 39 dp kamery)
PO (0.31.76, do potwierdzenia zrzutem): 56 + 39 = 95 dp = 148 px
Uwaga: 56 dp to pas ikon. Drugie 39 dp to wyciecie kamery — tego nie da sie
zwolnic, ikony pod obiektywem bylyby niewidoczne. Dlatego "100 dp -> 56 dp"
w praktyce wychodzi 107 dp -> 95 dp. Zeby zejsc nizej niz 95 dp, trzeba by
zlamac wymog 48 x 48 dp pola dotyku.

PUNKT 9 - wersja
----------------
versionCode 31076 / versionName 0.31.76 (app/build.gradle.kts)
Firmware BEZ ZMIAN: FIRMWARE_VERSION zostaje 3.57.9, pieca nie ruszamy.
APK: buduje sie z tego projektu (Gradle). W srodowisku, w ktorym powstal
pakiet, nie ma Javy ani Android SDK, wiec APK powstaje u Ciebie jedna komenda
z instrukcji — i wtedy ma ten sam numer 0.31.76.

Co sprawdzic na telefonie
-------------------------
1. W poziomie: same ikony, brak podpisow, pasek wezszy niz byl.
2. Przytrzymaj palec na ikonie — pojawia sie dymek z nazwa.
3. Kazda ikona nadal latwo trafiana palcem (48 x 48 dp).
4. W pionie: dolny pasek z podpisami bez zmian.
5. Prawa strona: karty koncza sie marginesem, nic nie wychodzi za ekran.

Jak cofnac
----------
W SterownikApp.kt: `compact = compactRail` zamienic na `compact = false`
(podpisy wroca, szyna 68 dp). W MainActivity.kt jest drugi, grubszy revert:
`setDecorFitsSystemWindows(window, true)` = powrot do zachowania 0.31.71.

Weryfikacja bez telefonu
------------------------
Brama statyczna (narzedzia/sprawdz_uklad.py): 64 OK / 0 BŁĄD.
Ta sama brama na 0.31.75: 17 BŁĄD (brak trybu kompaktowego, brak dymkow,
brak contentDescription).

Pliki zmienione (4)
-------------------
app/build.gradle.kts
app/src/main/java/com/sterownikco/pro/ui/SterownikApp.kt
app/src/main/java/com/sterownikco/pro/ui/components/Chrome.kt
app/src/main/java/com/sterownikco/pro/ui/theme/Theme.kt
