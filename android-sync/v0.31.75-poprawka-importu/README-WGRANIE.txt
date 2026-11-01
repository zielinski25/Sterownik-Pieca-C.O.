Sterownik CO - aplikacja Android 0.31.75 (pelny projekt)
=========================================================

Co nowego w 0.31.75
-------------------
SZERSZE MIEJSCE NA PULPIT: 158 dp po lewej -> 107 dp.

POPRAWKA BLEDU KOMPILACJI Z 0.31.74 (juz naprawiona)
----------------------------------------------------
0.31.74 nie skompilowala sie:
    SterownikApp.kt:24:43 Unresolved reference 'calculateBottomPadding'.

Przyczyna: `calculateBottomPadding()` jest METODA interfejsu PaddingValues,
a nie funkcja-rozszerzeniem. Rozszerzeniami w foundation.layout sa tylko
`calculateStartPadding` i `calculateEndPadding` - i tylko te wolno importowac.
Import `androidx.compose.foundation.layout.calculateBottomPadding` nie istnieje,
choc samo wywolanie `navVals.calculateBottomPadding()` jest poprawne (metoda
interfejsu nie wymaga importu).

W 0.31.75: usuniety ten jeden import. Cala reszta ukladu bez zmian
wobec 0.31.74.

Dlaczego pas po lewej byl taki szeroki (pomiar Twojego zrzutu 12:45):

    x = 0 .. 186 px   szyna (119 dp = 80 dp ikon + 39 dp wyciecia kamery)
    x = 187 .. 245 px PUSTKA - 39 dp tla strony
    x = 246 px        dopiero tu zaczynal sie pasek gorny i karty

Te 39 dp pustki to wciecie kamery policzone DRUGI RAZ. Szyna juz je
pochlania (dlatego ma 119 dp zamiast 80 dp), a dodatkowo system dokladal
je do tresci obok. Powod: `Modifier.navigationBarsPadding()` bierze
wszystkie cztery strony, a na tym telefonie pas nawigacji jest raportowany
razem z obszarem wyciecia - czyli z lewej strony dokladal 39 dp.

Co zmienione
------------
1. Wciecie paska nawigacji liczymy sami i bierzemy z niego WYLACZNIE
   koniec i dol (`calculateEndPadding` / `calculateBottomPadding`) zamiast
   gotowego `navigationBarsPadding()`. Lewa strona nalezy do szyny.
2. Szyna 80 dp -> 68 dp (padding w niej 8 dp -> 6 dp, wiec na etykiety
   zostaje 56 dp, a najdluzsza - "Ustawienia" przy 9 sp - ma ~44 dp).
   Razem z wycieciem kamery: 107 dp zamiast 119 dp.
3. Obramowanie szyny i paska gornego rysowane PRZED wcieciem paska
   statusu. Wczesniej bylo po wcieciu, wiec pod paskiem statusu zostawala
   pozioma linia (ten "szew", ktory widac bylo na gorze).

Efekt: tresc zaczyna sie od razu przy szynie, bez pustego pasa.
Pulpit dostaje 79 px (50 dp) wiecej szerokosci - ok. 8 % ekranu.

Co sprawdzic na telefonie
-------------------------
1. Brak pustego pasa miedzy szyna a paskiem "STEROWNIK CO".
2. Szyna wezsza, ale etykiety ("Dashboard", "Ustawienia")Sie mieszcza -
   zadna nie jest ucieta ani zamieniona w wielokropek.
3. Brak poziomej linii pod paskiem statusu (na szynie i na pasku gornym).
4. Ikony w szynie nadal poza obiektywem kamery (nic pod obiektywem).
5. Dolne przyciski systemowe (wstecz/dom/recent) nie zaslaniaja tresci.

Jak cofnac
----------
W MainActivity.kt zmienic jedna linie na:
       WindowCompat.setDecorFitsSystemWindows(window, true)
To wraca do zachowania 0.31.71 (z pasem 37 dp).

Wersja
------
versionCode 31075 / versionName 0.31.75  (app/build.gradle.kts)
Wersje widac w: Ustawienia -> "Aktualizacja firmware (OTA & GitHub)"
-> wiersz "APLIKACJA WEB PRO".

Weryfikacja bez telefonu
------------------------
Brama statyczna (narzedzia/sprawdz_uklad.py): 51 OK / 0 BŁĄD.
Ta sama brama na 0.31.74: 4 BŁĄD - m.in. wlasnie ten bledny import
(brama nauczona tego przypadku), a na 0.31.73: 10 BŁĄD.
Kompilacji NIE sprawdzono - w srodowisku, w ktorym powstal pakiet, nie ma
Javy ani Gradle. Pierwsza komenda Gradle w skrypcie PowerShell zrobi to
na Twoim komputerze i zatrzyma sie, jesli cokolwiek sie nie skompiluje.

Pliki zmienione (4)
-------------------
app/build.gradle.kts
app/src/main/java/com/sterownikco/pro/ui/SterownikApp.kt
app/src/main/java/com/sterownikco/pro/ui/components/Chrome.kt
app/src/main/java/com/sterownikco/pro/ui/theme/Theme.kt
