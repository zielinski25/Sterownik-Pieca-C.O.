# Centrala v3.58.11 — etap wykresów starej strony

**Praca w toku:** nie jest to całość uzgodnionego zadania. Lista użytkownika jest w [ZAKRES.md](ZAKRES.md). W tej wersji wykonano tylko niżej opisany wycinek; dalsze funkcje pozostają do zrobienia.

## Co dodano
- Zakresy 6 h i 30 dni obok dotychczasowych; zakres tygodnia wybiera teraz ostatnie 7 dni wg rzeczywistego czasu próbek, nie cały bufor zdarzeniowy. Wybór według zegara przeglądarki, nieprawidłowy czas przeglądarki lub brak czasu próbki wpływa na wynik. Pokazywane są tylko dostępne próbki, a nie pełne 30 dni na żądanie.
- Wykresy rzeczywistych danych historycznych pompy (0/1), klapy i szyberka w stopniach, ciśnienia w hPa i wilgotności w %. Stan pompy to próbki, a nie dokładny czas przełączenia. Słupek „obserwowane zmiany” nie liczy przejść przez brakujące dane i długie przerwy. Dane serw z długiej historii są uśrednione.
- Przyciski pobrania obecnych buforów krótkiej i długiej historii jako CSV. Nic nie kasują. Brakujące/niepoprawne wartości nie są już rysowane jako zero.
- Bez zmian zapisu LittleFS, formatu historii, API i sterowania. Wybór strony z v3.58.10 zachowany. Funkcja czyszczenia historii w tej bazie jest celowo wstrzymana (serwer zwraca 409); nie próbowano usuwać danych ani obchodzić blokady.

## Czego NIE zrobiono
Wybór serii i osi, filtr, pełny ekran, porównanie czujników; rozszerzenie zapisu o trociny/dym/symulację; pogoda i solary; zmiany diagnostyki, OTA i pozostałe punkty planu nie zostały wdrożone. Dane z lokalnych starych próbek nie zawierają trocin, dymu i pochodzenia symulacji. Nie wprowadzono w tej części ani zmian zapisu na flash ani danych przykładowych.

## Testy i instalacja
Sprawdzono parsery obecnych i starszych plików, brak pól (w tym brak stanu pompy), zakres czasu, liczenie stanów/zmian i fragment rzeczywistego renderera SVG z lukami oraz schodkową linią: `node tests/test_wykresy.js /ścieżka/do/main_centrala.cpp`. Sprawdzono składnię JS starej strony i ponownie hostowy test wyboru strony. Patch stosuje się na v3.58.10. **Nie skompilowano na ESP32 i nie testowano na płytce ani w rzeczywistej przeglądarce.**

Prywatna paczka na Dysku zawiera pełny main i nagłówki `InternetProbe.h` i `WyborStrony.h`, nie cały projekt PlatformIO. Najpierw kopia projektu i kompilacja użytkownika; próba tylko na drugiej, odłączonej płytce bez wyjść wykonawczych i produkcyjnych usług. Nie wykonywać uploadfs, nie usuwać NVS ani LittleFS. Porównać adresy próbek z zegarem telefonu i sprawdzić, czy kreski odpowiadają danym w CSV. Nie podmieniać na czynnej instalacji przed weryfikacją.
