# Centrala v3.58.15 — większy etap: pogoda i prognoza solarna

Etap na podstawie v3.58.14, **nie całość zadania**. [Uzgodniony zakres](../stara-strona-v3.58.11/ZAKRES.md).

## Co dodano

- Na starej stronie nad niezmienioną prognozą centrali osobna rozszerzona prognoza z Open-Meteo dla 51.066389, 21.509167. Żądanie wykonuje **przeglądarka użytkownika**, więc jej urządzenie potrzebuje internetu, obsługi HTTPS/CORS i prawidłowego zegara. Dane centrali są nadal dostępne oddzielnie, także wykresy rolnicze.
- Warunki modelowe: temperatura, wilgotność, wiatr i porywy, ciśnienie powierzchniowe, chmury, opad, prawdopodobieństwo opadu w godzinie, UV i GHI. Zakresy 24 h, 3, 7 i 14 dni, wybór godziny i jej szczegóły; dzienne minima/maksima, wschód/zachód i długość dnia. Słońce: przybliżona wysokość geometryczna i azymut liczony od północy zgodnie z ruchem zegara, bez refrakcji atmosferycznej.
- Zakładka Solary: od północy do ostatniej zakończonej godziny suma **modelowego** GHI w kWh/m² z liczbą dostępnych/oczekiwanych godzin i oznaczeniem niepełności oraz profil godzinowy na 7 dni. Wartość `shortwave_radiation` jest średnią z przedziału poprzedzającego podaną godzinę (T−1 h, T]. Godziny są liczone po znacznikach UTC, opisywane w Europe/Warsaw — również dni 23/25-godzinne. Te wartości nie są pomiarem nasłonecznienia ani uzysku kolektora. Osobno pozostaje zapisana prognoza GHI z archiwum centrali.
- Błędy sieci, brak pól i brak przedziałów są jawne; wynik częściowy nie jest podawany za kompletny, a puste pole nie jest zamieniane na zero. Dane po godzinie są oznaczane jako potencjalnie nieaktualne. Odświeżenie usuwa poprzedni obraz i ignoruje spóźnione odpowiedzi. Dane pozostają w pamięci przeglądarki, nie są dopisywane do archiwum centrali. Używana jest strefa Europe/Warsaw niezależnie od strefy urządzenia, ale jego zegar musi być poprawny.

## Weryfikacja i ograniczenia

Testy hostowe na danych syntetycznych obejmują zerowe/brakujące wartości, częściowe GHI, granice dnia, zmianę czasu 23/25 h, przybliżenie pozycji słońca, formularze, stare archiwum, wykresy oraz błędy/wyścigi sieci. Sprawdzono składnię JS i fragmentu C++ generującego HTML oraz nakładanie patcha. **Nie przetestowano połączenia z rzeczywistym Open-Meteo, przeglądarki, kompilacji ESP32 ani sprzętu.** Format/usługi i dostęp do internetu użytkownika należy zweryfikować. Modelowe GHI nie zastępuje obiecanego rzeczywistego pomiaru, którego centrala nie ma w tym etapie.

Pozostały m.in. skrót solarny w kafelku, zapis w historii trocin/dymu/pochodzenia symulacji, przełącznik zapisu i bezpieczne czyszczenie, diagnostyka, OTA i inne punkty planu. Czyszczenie historii serwera nadal zablokowane HTTP 409. Nie zmieniono historii, zabezpieczeń ani sterowania.

Prywatna paczka zawiera pełny `main_centrala.cpp` oraz `InternetProbe.h` i `WyborStrony.h`, ale nie cały projekt PlatformIO. Zrób kopię, podmień pliki z `centrala/src`, sam skompiluj i testuj drugą odłączoną płytkę, bez wyjść wykonawczych i usług produkcyjnych. Nie wykonuj `uploadfs` i nie kasuj NVS/LittleFS. Zweryfikuj dostęp do Open-Meteo w telefonie/komputerze, zakresy, daty, DST, luki, rozdzielenie danych centrali i modelu oraz wykresy.
