Sterownik CO — aplikacja Android 0.31.80 (pełny projekt)
========================================================

Baza: 0.31.75 (wersja z telefonu) + poprawki 0.31.77, 0.31.78, 0.31.79, 0.31.80.
Zmiana dotyczy TYLKO aplikacji. Piec zostaje na 3.57.9.
versionCode 31080 / versionName "0.31.80" (app/build.gradle.kts).

Trzy poprawki w 0.31.80
-----------------------
1. Kafelek „DO KOŃCA DNIA" nie pokaże już zysku szczytu po zachodzie słońca
   (przy nieświeżej prognozie, np. telefon offline od popołudnia).
2. Opis pod liczbą jest krótki i mieści się w kafelku oraz w arkuszu Czujniki.
3. Arkusz Czujniki: liczba i opis w osobnych liniach, więc opis nie kończy
   się wielokropkiem.

Do tego ERRATA-0.31.78.txt w folderze 0.31.78 prostuje nieprawdę o nagłówku.


1) Warunek „jest jeszcze słońce" — jedno miejsce w kodzie
----------------------------------------------------------
Zgłoszenie: restOfDayDeltaC() nie sprawdzało, czy słońce jeszcze jest.

Reguła „czy do końca doby jest jeszcze słońce" (> 20 W/m² w płaszczyźnie
kolektora) stała do 0.31.79 w pliku ekranu (Weather.kt, funkcja
solarSunLeft). Prognoza 7-dniowa w SolarAnalytics nie mogła jej użyć, więc
brała zysk szczytu bez patrzenia na zegar. Po przeniesieniu reguły do
core/Weather.sunLeftToday korzystają z niej wszystkie trzy miejsca:

  • karta „DO KOŃCA DNIA"          (ui/screens/Weather.kt)
  • arkusz Czujniki                (ui/screens/sheets/Sensors.kt)
  • prognoza 7-dniowa, dzień DZIŚ  (core/SolarAnalytics.dailyForecast7)

Sama funkcja ma teraz obowiązkowy parametr:

  fun restOfDayDeltaC(sunLeft: Boolean): Double

Zysk szczytu jest zwracany TYLKO gdy sunLeft == true. W przeciwnym razie
zwracana jest zmiana „do północy" z modelu (forecastEndDeltaC). Wywołanie
bez argumentu nie skompiluje się — to celowe, żeby ten błąd nie wrócił.

Symulacja (prognoza pobrana o 14:00, telefon offline, sprawdzenie o 22:00;
model policzył wtedy szczyt +1,5 °C na 15:00 i zmianę do północy -0,2 °C):

  godzina  prognoza     0.31.79 pokazywało   0.31.80 pokazuje
  ---------------------------------------------------------------------
  16:00    świeża       +0,9 °C  (szczyt)    +0,9 °C  (szczyt)   bez zmian
  22:00    świeża       -0,3 °C               -0,3 °C             bez zmian
  22:00    z 14:00      +1,5 °C  BŁĄD         -0,2 °C             poprawione

Efekt uboczny, o którym trzeba wiedzieć: przy nieświeżej prognozie liczba
-0,2 °C też jest z 14:00 (taka sama jak w 0.31.79), ale jest to właściwy
rodzaj liczby — zmiana do północy, a nie szczyt z południa. Aplikacja nie
zapisuje godziny pobrania prognozy, więc nie umie oznaczyć „dane z 14:00".
To osobna sprawa, do decyzji.


2) Opisy „do końca dnia" — zmierzone, nie na oko
------------------------------------------------
Wiersz arkusza ma maxLines = 1, więc długi tekst nie zawija się w trzy
linie — kończy się wielokropkiem. Pomiar (DejaVu Sans, górna granica;
Roboto jest ~7 % węższy; ekran 360 dp):

  gdzie            miejsce na tekst   tekst                              wynik
  ---------------------------------------------------------------------------
  kafelek          85,3 dp            „…woda się jeszcze ociepla do północy"  190 dp
  wiersz arkusza   308 dp             „+0,1°C · …ociepla do północy"         459 dp

Dlatego opisy są teraz krótkie (limit 26 znaków pilnuje brama):

  woda stygnie          -> "bez słońca · stygnie"
  woda się ociepla      -> "bez słońca · ociepla się"
  bez zmian             -> "bez słońca · bez zmian"
  brak prognozy         -> "bez słońca"

Między „ociepla" a „się" jest twarda spacja (\u00A0), żeby w kafelku nie
zostało w drugiej linii samo „się". W kafelku mieszczą się teraz 1-2 linie
zamiast 3, a w wierszu arkusza opis mieści się w całości.


3) Arkusz Czujniki — liczba i opis w osobnych liniach
-----------------------------------------------------
Przed: SolarMetricRow("Do końca dnia · szacunek", "$v · $sub")
        wartość doklejona do opisu, jedna linia, wielokropek na końcu.

Teraz:  SolarMetricRow("Do końca dnia · szacunek", v, sub)
        liczba sama (taka jak w kafelku, np. "+0,1°C"),
        opis w drugiej linii pod spodem, cała szerokość (308 dp).


Co było wcześniej (bez zmian w 0.31.80)
---------------------------------------
0.31.77 — usunięte wykrywanie pobrań wody (zgadywanie ze spadku temperatury).
0.31.78 — tytuł karty zawija się do 2 linii z wielokropkiem zamiast cięcia
          w połowie słowa; nota w Czujnikach napisana od nowa.
          UWAGA: podział nagłówka prognozy na 3 linie z 0.31.78 był pomyłką
          (nagłówek nie był ucięty) i został cofnięty w 0.31.79.
          Szczegóły: android-sync/v0.31.78-tytuly-bez-ucinania/ERRATA-0.31.78.txt
0.31.79 — „DO KOŃCA DNIA" z jednego źródła: kafelek, jego podpis, karta DZIŚ
          i wiersz w Czujnikach liczą z SolarAnalytics.restOfDayDeltaC(),
          a opis zależy od znaku liczby, nie od pory dnia.


Brama (bez Gradle/JDK)
----------------------
  python3 narzedzia/sprawdz_uklad.py
  76 OK / 0 BŁĄD dla 0.31.80
  kontrola: 0.31.79 -> 11 BŁĄD, 0.31.78 -> 12 BŁĄD (brama łapie te błędy)


Instalacja
----------
APK z CI ma podpis debug, inny niż poprzednie wydania:
  1. Odinstaluj starą wersję Sterownik CO.
  2. Zainstaluj SterownikCO-0.31.80-debug.apk.
  3. Sprawdź w Ustawieniach numer wersji: 0.31.80.
Bez odinstalowania instalator zgłosi „aplikacja nie została zainstalowana".


Pliki
-----
  SterownikCO-Android-v0.31.80-PELNY-PROJEKT.zip   pełne źródła (to drzewo)
  ZMIANY-0.31.79-do-0.31.80.diff                  różnica wobec 0.31.79
  SHA256SUMS.txt                                  sumy kontrolne
  sprawdz_uklad.py                                brama (76 sprawdzeń)

Gotowy plik do telefonu: android-sync/apk/SterownikCO-0.31.80-debug.apk
(po zbudowaniu w CI; podpis debug — patrz „Instalacja").
