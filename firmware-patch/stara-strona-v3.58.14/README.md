# Centrala v3.58.14 — częściowy etap: archiwum solarne

Kolejny etap na bazie v3.58.13; nie jest to całość [uzgodnionego zakresu](../stara-strona-v3.58.11/ZAKRES.md).

## Zmiany

- Zakładka **Solary** na starej stronie odczytuje istniejący lokalny endpoint `/api/solar-archive`. Pusta data pokazuje rekordy na dziś i wczoraj według zegara centrali, a wpisanie daty — zapisany dzień. Brak pliku, błąd i spóźnione odpowiedzi nie są przedstawiane jako aktualny wynik. Nie dopisuje danych, nie kasuje ich i nie korzysta z Firebase.
- Zapisana **prognoza** GHI jest pokazana z liczbą godzin i oznaczeniem pełna/niepełna/nieznana; nie jest przedstawiana jako pomiar promieniowania. Wyświetla też datę zapisu prognozy i aktualizacji rekordu, jeśli istnieją.
- Dzienny wynik zbiornika `t_panel`: pierwsza i ostatnia temperatura, zmiana, próbki, pokrycie, stan dnia oraz ekwiwalent 100 l jako **szacunek zmiany energii wody**, nie zmierzoną produkcję energii. Przy mniej niż dwóch próbkach nie pokazujemy zmiany ani ekwiwalentu. Stare rekordy schematu 1 lub o nieznanym/innym kanale nie są prezentowane jako dane zbiornika solarnego (schemat 1 używał bojlera pieca).
- Wyboru A i B można dokonać zarówno w zakładce Solary, jak i Wykresy; zmiana synchronizuje oba formularze, aktualny odczyt i wykres ΔT. Wybór zapisuje przeglądarka. **Dzienna zmiana zbiornika z archiwum jest odrębną wielkością, nie A−B**: centrala zapisuje ją dla `t_panel`, nie dowolnej pary. Świeży status nie potwierdza sprawności czujnika, może zawierać wartość zastępczą błędu; nie wykorzystywać wyników do sterowania ani decyzji bezpieczeństwa.

## Ograniczenia i pozostałe prace

To czytnik istniejących rekordów, nie gwarancja, że centrala ma zapisane dane; dzisiejszy rekord może być niepełny lub sprzed ostatniego zapisu. Nie ma jeszcze rozszerzonej pogody w przeglądarce, GHI rzeczywistego za dziś ani 7-dniowej prognozy godzinowej, skrótu w kafelku, rozszerzenia historii o trociny/dym/symulacje, bezpiecznego czyszczenia historii, pozostałej diagnostyki i OTA. Nie dodano licznika podejrzewanych poborów CWU ani eksportu solarnego CSV. Czyszczenie historii nadal zablokowane HTTP 409.

## Testy i instalacja

Testy hostowe obejmują prawidłowe i niepełne dane, zera, brak wartości, pojedynczą próbkę, stary i inny kanał, wybór daty, błędy sieci i wyścigi odpowiedzi, synchronizację formularzy A/B, testy wykresów i wyboru strony. Sprawdzono składnię JS, wycinek generowania HTML w C++ i nakładanie patcha na v3.58.13. **Nie było kompilacji ESP32 ani testu w przeglądarce lub na sprzęcie.**

Prywatny ZIP zawiera pełny `main_centrala.cpp` i oba nagłówki, ale nie kompletny projekt PlatformIO. W kopii projektu podmień pliki z `centrala/src`, sam skompiluj i testuj tylko drugą, odłączoną płytkę, bez wyjść wykonawczych i usług produkcyjnych. Nie wykonuj `uploadfs` i nie kasuj NVS ani LittleFS. Sprawdź wejście do Solary, oba wybory A/B, daty i zgodność wskazań z rekordami z centrali. Dopiero po weryfikacji rozważ użycie w instalacji.
