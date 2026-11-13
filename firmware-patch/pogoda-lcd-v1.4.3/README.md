# Pogoda na LCD — centrala v3.58.8 i panel v1.4.3

## Zmiany

- Centrala przy istniejącym pobieraniu prognozy Open-Meteo pobiera też `weather_code` i `is_day`, wysyła je lokalnie w `/api/status` jako `pogoda_kod` i `pogoda_dzien` (bool). Pierwsza próba po uzyskaniu sieci, kolejne co 15 minut; gdy nie uda się jej odnowić przez godzinę lub pola są błędne, zwracane jest `null`. Para pól jest wspólnym, chronionym snapshotem.
- LCD wybiera ikonę z kodu WMO: słońce/księżyc, częściowe zachmurzenie dzień/noc, chmury, mgła, deszcz, śnieg i burza. Ikony są jednokolorowymi bitmapami, zmieniają się automatycznie, nie są animowane. Nie zależą od samej temperatury. Gdy brak danych lub przy starszej centrali jest znak zapytania; po utracie łącza też. Zachowano dotychczasowe tekstowe odliczanie deszczu, które może zastępować temperaturę, również gdy ikona pokazuje bieżącą pogodę. Odliczanie dotyczy prognozy opadu, ikona — bieżącego kodu meteo.
- Strona WWW pobiera dane Open-Meteo osobno w przeglądarce, a centrala dla LCD ze swoich zapisanych w kodzie współrzędnych **51.066389, 21.509167**; przeglądarka może mieć inne ustawienia lokalizacji i odświeżać się w innym czasie. Zatem warianty są zgodne rodzajem, ale chwilowe różnice są możliwe. To prognoza modelowa, nie fizyczny czujnik pogody. Temperatura na kafelku pozostaje odczytem z centrali.

## Pliki i instalacja

1. Zrób kopię obydwu istniejących projektów. W prywatnej paczce pełnych plików (Dysk użytkownika) w katalogu `centrala/src` są kompletne `main_centrala.cpp` i `InternetProbe.h`, a w `lcd/src` sześć plików panelu. Użyj projektów, na których oparto poprzednią paczkę centrali v3.58.7 i LCD v1.4.2; to **nie** są kompletne projekty PlatformIO.
2. W projekcie centrali podmień `src/main_centrala.cpp` oraz dodaj/podmień `src/InternetProbe.h` (jeżeli był już z poprzedniej paczki). Nie nadpisuj własnego `secrets.h`, konfiguracji i bibliotek. Skompiluj i wgraj centralę v3.58.8.
3. W projekcie LCD podmień `src/main.cpp`, `src/dashboard_lvgl_poc.cpp`, `src/dashboard_lvgl_poc.h`, `src/SiecIkony.h` oraz dodaj `src/PogodaIkony.h` i `src/PogodaWybor.h`. Nie nadpisuj innych plików. Skompiluj i wgraj panel v1.4.3. Alternatywnie pliki panelu są też w publicznym ZIP-ie w `pobierz/`.
4. Sprawdź na odłączonej płytce testowej i panelu: dostęp do internetu i kod/dzień w `/api/status`, zmianę ikony i znak zapytania bez danych. Nie wgrywaj historii testowej do właściwej centrali. Pamiętaj, że wynik wcześniejszego wgrywania LittleFS na COM7 nie został potwierdzony.

Jeśli masz własne nowsze zmiany, porównaj pliki przed podmianą. Dla osób chcących nanieść tylko zmianę jest tu również `centrala-v3.58.7-do-v3.58.8.patch`, na bazę v3.58.7; nie stosuj patcha i pełnego pliku jednocześnie.

## Testy i ograniczenia

Test wyboru wariantów `g++ -std=c++17 tests/test_wybor.cpp -o /tmp/pogoda && /tmp/pogoda`. Cały zestaw symulatora hostowego LCD przeszedł, w tym nowy scenariusz: status słoneczny, noc, burza, brak pól/null/błędny typ. Do odtworzenia zastosuj `tests/symulator.patch` do projektu z oryginalnego ZIP-a LCD v1.4.0, skopiuj sześć nowych plików i uruchom `testy_host/uruchom_testy.sh`. Hostowo sprawdzono także wycinki centrali z ArduinoJson (null, typ, przedawnienie, przepełnienie licznika czasu). **Nie wykonano kompilacji ESP32 ani testów na sprzęcie.** Przed pracą z prawdziwą instalacją przeprowadź bezpieczne testy i zachowaj poprzednie firmware.
