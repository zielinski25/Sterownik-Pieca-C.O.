Sterownik CO - aplikacja Android 0.31.69 (pelny projekt)
=========================================================

Co nowego w 0.31.69
-------------------
Aplikacja nie rozciagala sie po obrocie telefonu na poziomo.

PRZYCZYNA (ta sama regula co w Piec.html, ale zle przeniesiona do Androida):
w ui/SterownikApp.kt bylo

    val wide = maxWidth > 560.dp
    Column(if (wide) Modifier.width(m.frameWidth.dp).fillMaxHeight(0.98f) else Modifier.fillMaxSize())

czyli decyzje podejmowala SAMA SZEROKOSC okna. Telefon w pionie ma ~411 dp
(ramka sie nie wlaczala), ale po obrocie te same 411 dp staje sie ~890 dp
szerokosci - i wtedy aplikacja zostawala wpisana w sztywna ramke 412 dp,
przyklejona do lewego gornego rogu. Reszte okna wypelnialo tlo sceny.

Na telefonie uzytkownika (735 x 331 dp w poziomie) ramka zajmowala 56 %
szerokosci, czyli ok. 45 % powierzchni okna bylo martwe, a zakladka
"Panele" byla ucieta.

CO ZMIENIONE
------------
1. ui/SterownikApp.kt - aplikacja zajmuje CALA szerokosc okna zawsze.
   Usunieta ramka 412 dp, obramowanie i fillMaxHeight(0.98f).

2. core/ScreenLayout.kt (nowy plik) - decyzja o nawigacji zalezna WYLACZNIE
   od rozmiaru okna w dp, nigdy od orientacji urzadzenia:

       useNavRail():  wysokosc < 480 dp  I  szerokosc >= 600 dp

   To sa granice klas rozmiaru okna Androida: telefon w poziomie to
   "Medium/Expanded szerokosc x Compact wysokosc (< 480 dp)".

3. ui/components/Chrome.kt - nowa funkcja NavRail: te same 5 zakladek,
   ale w kolumnie po lewej (80 dp). Wlaczana w niskim i szerokim oknie.

   Po co: przy dolnym pasku na 331 dp wysokosci zostawalo 195 dp tresci,
   a wykres ma stala wysokosc 220 dp - nie miescil sie i trzeba bylo
   przewijac. Z szyna zostaje 269 dp, wiec wykres miesci sie w calosci.

4. Usuniete martwe po ramce: AppModel.frameWidth, Dimens.frameW, Dimens.frameH.
   Nic innego z nich nie korzystalo.

5. Test app/src/test/.../core/ScreenLayoutTest.kt - 6 testow na realnych
   wymiarach okien (331x735 pion, 735x331 poziom, 915x412, 851x400,
   1280x800, 500x400, granice 600x479 i 600x480).

Numer wersji: versionCode 31069 / versionName 0.31.69.
Reszta bez zmian wzgledem 0.31.68.

CZEGO SWIADOMIE NIE ZMIENIONO
-----------------------------
- Tablet 1280x800 dp (duzo pionu) zostaje przy dolnym pasku: 800 - 62 - 74
  = 664 dp tresci, wykres sie miesci. Szyna jest odpowiedzia na BRAK PIONU,
  a nie na sama szerokosc.
- Brak blokady orientacji. Od Androida 16 (targetSdk 36) system i tak ignoruje
  screenOrientation na duzych ekranach, a w Androidzie 17 furtka znika.
- Bardzo szerokie okna (np. desktop 1440 dp): tresc nadal zajmuje cala
  szerokosc. Ograniczenie szerokosci tresci (odpowiednik max-width z
  Piec.html) to osobny krok, nie ta poprawka.

UWAGA: NIE SPRAWDZONO KOMPILACJI
--------------------------------
W srodowisku, w ktorym powstal ten pakiet, nie ma Javy ani Gradle, wiec
testow NIE uruchomiono. Sprawdzono tylko:
  - brame statyczna narzedzia/sprawdz_uklad.py (23 OK / 0 BLAD),
  - spojnosc testu z kodem przez model w Pythonie (9/9 przypadkow),
  - balans nawiasow w zmienionych plikach.
Pierwsze prawdziwe uruchomienie testow nastepuje przy budowaniu APK (punkt 2).

Jak zbudowac (Windows, PowerShell)
----------------------------------
1. Rozpakuj ZIP, np. do %USERPROFILE%\SterownikCO-0.31.69
2. W tym folderze:
     .\gradlew.bat :app:testDebugUnitTest :app:assembleDebug --console=plain
   Gdy Gradle zglosi "SDK location not found", utworz plik local.properties:
     sdk.dir=C:/Users/<user>/AppData/Local/Android/Sdk
3. APK: app\build\outputs\apk\debug\app-debug.apk
4. Wersja w Ustawieniach aplikacji: v0.31.69-debug (pakiet com.sterownikco.pro.debug)

Sprawdzenie po wgraniu
----------------------
1. Telefon w pionie: wyglad bez zmian.
2. Obroc na poziom: aplikacja na calym ekranie, nawigacja po lewej,
   wykres widoczny bez przewijania.
3. Obroc w trakcie pracy (na Pulpicie i na Wykresach): uklad przelicza sie
   bez restartu aplikacji i bez utraty stanu.
4. Split-screen (aplikacja na polowie ekranu): bez martwego pola.
