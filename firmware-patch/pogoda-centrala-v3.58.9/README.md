# Centrala v3.58.9 — poprawka pobierania prognozy dla LCD

## Dlaczego
W otrzymanym logu centrala dostała odpowiedź HTTP 200, ale `deserializeJson` zgłosił `InvalidInput`, a `/api/prognoza` było puste. W kodzie v3.58.8 znaleziono rzeczywisty problem: parser korzystał z `http.getStream()`, który podaje surowe bajty po nagłówkach. Jeśli serwer wyśle `Transfer-Encoding: chunked`, zawierają one również nagłówki fragmentów. Jest to prawdopodobna przyczyna pasująca do objawów, **nie udowodniona przez sam log**, który nie pokazał nagłówków odpowiedzi.

## Zmiany
- `HTTPClient::writeToStream()` łączy fragmenty odpowiedzi, potem JSON jest parsowany z bufora w PSRAM. Maksymalny rozmiar odpowiedzi to 64 KiB; błąd odbioru i brak pamięci są wykrywane. Dla odpowiedzi bez `Content-Length` akceptowany jest `Transfer-Encoding: chunked`; bez obu tych informacji odpowiedź jest odrzucana. Klient prosi o nieskompresowane dane i odrzuca nieobsługiwaną kompresję.
- W logu Serial pojawi się `PROGNOZA zdarzenie=kod_pogody kod=... dzien=... bajtow=... transfer=...`, albo szczegół błędu. Zawartość odpowiedzi nie jest zapisywana w logu. `/api/status` i zachowanie LCD v1.4.3 pozostają bez zmian.
- Zmieniono jedynie centralę; wcześniejsze pliki LCD v1.4.3 pozostają aktualne. Centrala nadal korzysta z tej samej lokalizacji i interwału, a po nieudanym pobraniu kolejną próbę podejmuje po około 15 minutach.

## Instalacja i próba
Prywatna paczka na Dysku zawiera kompletne `centrala/src/main_centrala.cpp` i `centrala/src/InternetProbe.h` — to pliki do podmiany w istniejącym projekcie centrali, **nie kompletny projekt PlatformIO**. Zrób kopię projektu; podmień `main_centrala.cpp`, dodaj albo podmień `InternetProbe.h` (jak w v3.58.8). Zachowaj swoje `secrets.h`, ustawienia projektu i biblioteki. Porównaj źródła, jeśli masz własne późniejsze zmiany. Skompiluj i wgraj **wyłącznie na odłączoną płytkę testową**; nie podłączaj wyjść wykonawczych ani sieci/Firebase/Telegrama produkcyjnej centrali. Nie używaj `uploadfs` ani nie kasuj LittleFS — ta poprawka go nie wymaga.

W monitorze portu 115200 znajdź wpis `kod_pogody`, np. `kod=61 dzien=0`. Następnie na tej samej płytce otwórz `/api/prognoza` (dane, nie `{}`) i `/api/status` (liczba przy `pogoda_kod`, `true` lub `false` przy `pogoda_dzien`). Z LCD połączonym z tą samą centralą powinna pojawić się ikona. Adres IP sprawdź w bieżącym logu `WIFI ... polaczono`, ponieważ może się zmienić. Jeśli problem nadal występuje, przekaż tylko wpisy `tag=WEATHER` (bez haseł i danych sieciowych).

## Weryfikacja
Hostowy test korzysta z rzeczywistych metod `writeToStream` i `writeToStreamDataBlock` z Arduino-ESP32 3.3.8 w atrapach sieci, testuje fragmentowaną i zwykłą odpowiedź, dane obcięte, niepoprawny JSON i przekroczenie bufora. Sprawdza też, że stary surowy chunked nie jest JSON-em. Uruchomienie: `python3 tests/run_host_test.py SCIEZKA_DO_MAIN_CENTRALA.cpp SCIEZKA_DO_HTTPClient.cpp SCIEZKA_DO_ArduinoJson/src`. To nie jest rzeczywisty test sieci, ESP ani Open-Meteo. **Nie skompilowano firmware ESP32 i nie przetestowano na sprzęcie.** Przyczyna problemu wymaga potwierdzenia na płytce.
