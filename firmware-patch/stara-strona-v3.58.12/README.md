# Centrala v3.58.12 — kolejny częściowy etap starej strony

Zmiany względem v3.58.11 (plan całego zadania: [ZAKRES](../stara-strona-v3.58.11/ZAKRES.md)):

- Wybór serii temperatur, skala wspólna lub normalizacja każdej widocznej serii do 0–100%; wartości stałe są na 50%. Normalizacja jest obliczana na widocznym fragmencie po ewentualnym filtrze; inspektor pokazuje dane surowe.
- Opcjonalny filtr Hampela: okno do 7 próbek, próg 3 × 1,4826 × MAD, co najmniej 2°C; nie używa sąsiadów przez brak czasu ani długą przerwę. Może wygładzić prawdziwy skok — jest domyślnie wyłączony, wpływa jedynie na obraz, a nie na CSV ani zapis. Wygładzenie, schodki i wypełnienie dotyczą wykresów temperatur; wypełnienie jest obszarem do dołu skali, nie miarą energii. Wygładzona linia może nie przechodzić przez każdy punkt; inspektor odczytuje próbki.
- Pełny ekran poszczególnych kart wykresów (jeśli przeglądarka obsługuje Fullscreen API), z przyciskiem zamknięcia. Ustawienia prezentacji są lokalne dla przeglądarki.
- Aktualne porównanie A−B, z wyborem dwóch czujników, także trocin, na podstawie ostatniego statusu centrali. Przeterminowany/brakujący status lub wartość są oznaczone jako brak. Sama świeżość statusu nie potwierdza sprawności pomiaru; źródło może zawierać zastępczą wartość błędu. Nie używać tego odczytu do sterowania ani decyzji bezpieczeństwa.
- Nie zmieniono zapisu historii, API, sterowania ani obu stron jako takich. Stare dane i wcześniejsze zakresy pozostają; czyszczenie historii w bazie nadal zablokowane HTTP 409.

**Niewykonane:** wykres historycznego ΔT i podsumowanie solarne ze wspólnym wyborem, historia trocin/dymu/pochodzenia symulacji, ustawienie flash i bezpieczne czyszczenie, rozbudowana pogoda, solary, diagnostyka, OTA i pozostałe punkty planu. Aktualne porównanie A−B nie jest jeszcze wykresem ΔT. Pełny ekran i zmiany wyglądu wymagają sprawdzenia w przeglądarce.

## Testy i instalacja

Patch `etap-wykresy.patch` odnosi się wyłącznie do v3.58.11. Przetestowano jego zastosowanie i identyczność z pełnym plikiem, testy Node parserów i rzeczywistego fragmentu renderera, filtr, ustawienia, brak/starość statusu, przełączanie strony, składnię JavaScript i wygenerowane kontrolki HTML, a także składnię C++ fragmentu składającego HTML. To nie jest kompilacja firmware ESP32 ani test przeglądarki lub sprzętu.

Prywatny ZIP zawiera pełny `main_centrala.cpp` i oba nagłówki, **nie** cały projekt PlatformIO. W kopii projektu podmień pliki z `centrala/src`, skompiluj samodzielnie i testuj wyłącznie drugą, odłączoną płytkę bez wyjść wykonawczych i produkcyjnych usług. Nie wykonuj `uploadfs` i nie kasuj NVS ani LittleFS. Sprawdź wykresy, ich legendę/inspektor, zapamiętanie preferencji, pełny ekran i powrót oraz wyboru strony i brak zmian w historii. Nie instaluj na działającym piecu przed weryfikacją.
