# Ikony łączności — LCD v1.4.1 i centrala v3.58.7

Kod bazuje na ZIP-ach LCD-Centrali-v1.4.0.zip i Centrala-v3.58.6-panel-ota.zip użytkownika, **nie** na poprzedniej paczce dla 1.1.0/3.54.0. W poprawionym kodzie stałe wersji podniesiono do `v1.4.1` i `v3.58.7`, a w obu plikach źródłowych dodano wpisy do historii zmian. To zmodyfikowane źródła, nie oficjalne wydania. Nie ma tu gotowych binariów.

## Najprościej: paczka do pobrania

[Pobierz ZIP z poprawką](pobierz/Poprawka-ikon-LCD-1.4.1-Centrala-3.58.7.zip). Rozpakuj go. Zawiera pliki LCD i patch dla centrali. Równoważne pliki są także w tym katalogu repozytorium.

1. **Zrób kopię zapasową** obu projektów. Zacznij od oryginalnego panelu v1.4.0 i centrali v3.58.6. Innych wersji nie nadpisuj. Jeżeli zastosowano poprzednią paczkę ikon z niepodniesionymi numerami, przywróć oryginalny `main_centrala.cpp` v3.58.6 przed zastosowaniem nowego patcha — zawiera on również te wcześniejsze zmiany.
2. **LCD v1.4.1**: skopiuj cztery pliki z `lcd/src` paczki do `src` projektu LCD v1.4.0, zastępując trzy istniejące pliki i dodając `SiecIkony.h`. Reszty projektu ani konfiguracji nie zmieniaj. Jeżeli chcesz uruchamiać dołączone testy hostowe, zastosuj do katalogu projektu LCD `lcd-testy.patch` — aktualizuje oczekiwany numer wersji w symulatorze. Zachowaj lokalne `src/secrets.h` z hasłami (lub utwórz je z `secrets.h.example`).
3. **Centrala v3.58.7**: ZIP 3.58.6 z Dysku zawiera aktualny `src/main_centrala.cpp`, ale nie cały projekt PlatformIO. Na jego podstawie przygotuj projekt centrali 3.58.6 zgodnie z własną dotychczasową konfiguracją, zachowując wszystkie pozostałe nagłówki, biblioteki, platformio.ini i `src/secrets.h`. Następnie na tym projekcie zastosuj `centrala.patch` i skopiuj `centrala/InternetProbe.h` do `src/`. Uwaga: patch jest przeznaczony dla dokładnego `main_centrala.cpp` z tego ZIP-a; jeśli plik się różni, może nie pasować — nie wymuszaj go.

Przykład w **PowerShell**, z zainstalowanym Git for Windows. Ustaw dwie ścieżki na swoje katalogi; projekt nie musi być repozytorium Git:

```powershell
$poprawka = 'C:\SCIEZKA\DO\ROZPAKOWANEJ-POPRAWKI'
$projekt = 'C:\SCIEZKA\DO\PROJEKTU-CENTRALI'
Set-Location $projekt
git apply --check "$poprawka\centrala.patch"
# Tylko jeśli poprzednie polecenie nie zgłosiło błędu:
git apply "$poprawka\centrala.patch"
# Tylko jeśli git apply nie zgłosiło błędu:
Copy-Item "$poprawka\centrala\InternetProbe.h" 'src\InternetProbe.h'
```

Oryginalnych pełnych źródeł centrali celowo nie powielamy w publicznym repozytorium; patch stosuje się lokalnie do Twojego pliku, bez publikowania jego konfiguracji. Zachowaj istniejące lokalne sekrety. Można też użyć `lcd.patch` i `lcd/SiecIkony.h` zamiast podmieniać pliki LCD; nie stosuj obu metod naraz.

4. Jeżeli korzystasz z OTA przez GitHub, dopasuj tag wydania do nowej wersji firmware centrali (`v3.58.7`); ta paczka nie tworzy wydania ani nie publikuje binariów. Zbuduj projekty w swoim PlatformIO. **Kompilacja dla ESP32 ani próba na płytce nie zostały wykonane**. Przed uruchomieniem na instalacji grzewczej sprawdź działanie na bezpiecznym stanowisku i zachowaj możliwość powrotu do poprzedniego firmware. Nie zmieniaj automatycznie konfiguracji pinów ani bibliotek.

## Co pokazują ikony

- Kreski: lokalny `WiFi.RSSI()` panelu w połączeniu z AP centrali, nie sygnał centrali do routera. X oznacza rozłączenie panelu z WiFi, `?` brak wiarygodnego pomiaru.
- Globus: centrala niezależnie od Firebase wysyła `internet_wan`. Dwa nieblokujące zapytania DNS do 1.1.1.1 i 8.8.8.8 sprawdzają osiągalność publicznych resolverów. Sukces = globus, niepowodzenie obu = X, brak aktualnej informacji lub kontaktu = `?`. Częstotliwość ok. 15 s po zakończeniu testu; czas oczekiwania po 2 s na serwer. Zablokowany DNS może dać fałszywy brak internetu; to nie gwarancja dostępu do stron, Firebase czy wszystkich usług.
- Stare pole `internet` pozostaje bez zmian dla pozostałych klientów. Stara centrala bez `internet_wan` daje `?`, a nie mylący stan Firebase. Zachowano oryginalne mechanizmy świeżości danych i OTA.

## Testy hostowe

Nowe testy ikon i DNS przeszły na hostowym g++ na podstawie oryginalnych ZIP-ów (w tym rzeczywisty fragment rysujący ikony). Po podniesieniu wersji ponownie uruchomiono testy centrali: OTA 166/166, historia 2697/2697; komplet testów hostowych LCD, w tym symulator i OTA, przeszedł z oczekiwaną wersją v1.4.1. To nie jest test radia, lwIP, sprzętu ani kompilacji dla ESP32.

Aby odtworzyć test poprawki, potrzebne są Python 3, g++, `patch`, oryginalne ZIP-y i nagłówki ArduinoJson 7.2.1:

```sh
ARDUINOJSON_SRC=/sciezka/ArduinoJson-7.2.1/src python3 tests/run_tests.py /sciezka/LCD-Centrali-v1.4.0.zip /sciezka/Centrala-v3.58.6-panel-ota.zip
```

Oryginały ZIP: SHA-256 LCD `d9f3d3b294c23822e84f3300081939e69dc0cb418c905b76ede03d9d0230c33f`; centrala `1c421240c7a25638d6f8f3d4ecc1dd2523ab78e1f78b480eea39a87c89fa7f02`.
