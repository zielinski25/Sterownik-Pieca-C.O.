# Przykładowa historia wykresów — osobna płytka testowa

Te pliki zawierają **wymyślone dane**, nie pomiary pieca. Służą do porównania wykresów WWW i LCD na **osobnej, gołej płytce ESP32-S3 odłączonej od instalacji grzewczej**. Nie modyfikują kodu centrali: wgraj na tę płytkę swoje zwykłe źródła centrali, np. v3.58.7. Dane zostaną wczytane jak normalna historia, więc ekrany mogą nie oznaczać ich jako testowych.

**OSTRZEŻENIE: `uploadfs` zastępuje cały system plików LittleFS na wskazanej płytce**, a nie tylko dwa CSV. Usunie istniejące pliki w tej partycji (m.in. historię, logi, pliki użytkownika, plik aktualizacji panelu). **Nigdy nie wykonuj tej operacji na centrali pieca.** Nie uruchamiaj `erase_flash`: czyści więcej niż LittleFS. Zapisz kopie wszystkiego, co chcesz zachować, także projektu na komputerze. Operacje wykonuj wyłącznie świadomie i na osobnej płytce.

## Co jest w paczce

- `przykladowe/hist_short.csv`: 1440 próbek minutowych z ostatniej doby.
- `przykladowe/hist_long.csv`: 169 próbek godzinowych z ostatnich 7 dni.
- `generuj.py`: generator takich samych danych z aktualnym czasem komputera. Pliki generuje lokalnie, niczego nie wgrywa i nie nadpisuje istniejącego folderu.

Wygenerowane pliki znajdujące się w paczce mają datę podaną w `DATA-PACZKI.txt`; jeśli nie są już świeże, do bieżącego porównania uruchom generator. Wartości są zgodne w momentach wspólnych obu plików. Przed uruchomieniem generatora ustaw poprawną datę, godzinę i strefę czasową Polski na komputerze.

## Przygotowanie i wgranie

1. Sprawdź, że testowa płytka jest **fizycznie odłączona od pieca, przekaźników i innych elementów instalacji**, a komputer jest podłączony USB tylko do niej. Sprawdź, jaki port COM znika po jej odłączeniu i wraca po podłączeniu. Upewnij się, że drugi działający sterownik nie jest podłączony do tego portu.
2. Nie łącz testowej centrali z produkcyjną siecią, Firebase ani Telegramem: prawdziwy kod może wysyłać telemetrię lub odbierać polecenia. Dwie centrale z tym samym SSID mogą powodować, że LCD połączy się z niewłaściwą. Testuj w odizolowanym otoczeniu i upewnij się, do którego AP podłączony jest LCD. Nie wyłączaj centrali obsługującej piec tylko na potrzeby testu.
3. Wgraj i uruchom na płytce testowej właściwy firmware centrali (np. v3.58.7) z własnego projektu i konfiguracji. Nie zmieniaj haseł ani ustawień na działającej centrali.
4. Przygotuj dane. Możesz skopiować `przykladowe` z paczki albo utworzyć świeże: w PowerShellu otwartym w rozpakowanym folderze paczki uruchom:

   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\generuj.py historia_testowa
   ```

   Powstanie nowy folder `historia_testowa`. Jeśli już istnieje, program odmówi nadpisania — użyj innej nazwy. Nie uruchamiaj generatora wewnątrz produkcyjnego `data`.
5. W **kopii projektu centrali przeznaczonej tylko do testów** przygotuj folder `data` w katalogu projektu (obok `platformio.ini`). Jeżeli istnieje, najpierw zachowaj go w innym miejscu. Skopiuj do `data` tylko dwa pliki `hist_short.csv` i `hist_long.csv`, bez podfolderu. Wszystkie pliki w `data` zostaną użyte do stworzenia obrazu LittleFS.
6. W testowej kopii `platformio.ini` pod `[env:esp32-s3-n16r8]` dodaj `board_build.filesystem = littlefs`, jeśli nie ma już takiej pozycji. Pozostałych ustawień, `partitions.csv` i pliku `secrets.h` nie zmieniaj. Sprawdź, że `partitions.csv` w **tej kopii** ma właściwą partycję systemu plików `spiffs, data, spiffs` używaną przez LittleFS Twojej centrali. Jeśli ma inny układ — zatrzymaj się i prześlij go do sprawdzenia.
7. Po ponownym sprawdzeniu płytki i jej numeru COM otwórz terminal w katalogu testowego projektu i uruchom (wpisz własny port zamiast `COM7`):

   ```powershell
   pio run -e esp32-s3-n16r8 -t uploadfs --upload-port COM7
   ```

   Jeśli `pio` nie jest rozpoznawane, użyj pełnej ścieżki: `& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32-s3-n16r8 -t uploadfs --upload-port COM7`.

   **Jeszcze raz: uploadfs nadpisze całą partycję plików na płytce z wybranym portem.**
8. Po zakończeniu zresetuj testową centralę. Otwórz lokalny panel WWW i wykresy oraz wykresy na LCD. Dla porównania wybierz ten sam zakres i parametr. LCD zmniejsza dane do maksymalnie 96 punktów, więc linie, skala i wartości nie muszą być identyczne punkt po punkcie. Po pewnym czasie firmware zacznie dopisywać nowe, rzeczywiste odczyty testowej płytki.

Jeśli brakuje danych, sprawdź lokalny zegar testowej centrali i log startowy w Serial (wczytywanie historii) oraz czy komputer i LCD na pewno używają tej samej testowej centrali. Na gołej płytce bez RTC lub synchronizacji czasu data może być nieprawidłowa. Nie naprawiaj tego przez zmianę zegara centrali pieca.

## Sprawdzenie

Generator i formaty CSV sprawdzono na komputerze. Dane załadowano do hostowego testu obliczeń wykresów z kodu centrali: zakresy godzina, doba i tydzień zwróciły po 96 punktów. Nie sprawdzono wgrywania LittleFS ani wyświetlania na fizycznym ESP32/LCD, ani pełnego builda firmware. Wynik na płytce zależy od jej konfiguracji, zegara i narzędzi PlatformIO.
