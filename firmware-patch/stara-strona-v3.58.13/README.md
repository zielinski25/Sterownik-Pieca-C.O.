# Centrala v3.58.13 — historyczny wykres różnicy A−B

**Częściowy etap**, oparty na v3.58.12. Cały uzgodniony plan pozostaje w [ZAKRES.md](../stara-strona-v3.58.11/ZAKRES.md).

## Zmiany

- Karta wykresu ΔT = A−B korzysta z tego samego wyboru A i B w zakładce wykresów co bieżące porównanie ze statusu. Po zmianie wyboru wykres jest przeliczany. Zaznacza liczbę dostępnych par i działa z istniejącymi zakresami, powiększaniem, inspektorem i pełnym ekranem. Ustawienia linii, filtra i skali z v3.58.12 dotyczą także wykresu ΔT; na skali normalizowanej pionowa oś jest w procentach, inspektor nadal podaje surowe ΔT w °C.
- Różnica obliczana jest tylko z dwóch liczbowych wartości w tym samym wierszu. Brak któregokolwiek odczytu to luka, nie zero. Stara historia nie zawiera trocin: po wybraniu trocin wykres będzie pusty i wyświetli odpowiednią informację. W historycznej długiej serii starsze rekordy mogą zawierać średnie, więc ich różnica nie musi być chwilowym ΔT. Stary format nie pozwala ustalić pochodzenia symulacji ani potwierdzić sprawności czujników — możliwe są też wartości zastępcze błędów.
- Przy przełączeniu zakresu i odświeżeniu wyświetlane są puste wykresy podczas pobierania; spóźnione odpowiedzi wcześniejszych żądań nie zastępują obecnego widoku. Błąd pobierania nie pozostawia starego wykresu jako aktualnego.
- Bez modyfikacji plików historii, sterowania lub API. Czyszczenie historii nadal pozostaje zablokowane na serwerze (HTTP 409).

**Pozostało m.in.** podsumowanie solarne wykorzystujące ten sam wybór, rozszerzenie zapisu historii o trociny/dym/pochodzenie symulacji, przełącznik zapisu i bezpieczne czyszczenie, pogoda, solary, diagnostyka, OTA oraz pozostałe punkty planu. Nie używać wyników wykresów do decyzji dotyczących bezpieczeństwa czy sterowania.

## Weryfikacja i instalacja

Przeszły hostowe testy krótkiego i starego długiego CSV, braków, ujemnych i zerowych różnic, wyboru trocin, zmiany pary, zakresu i opóźnionych odpowiedzi, błędów pobierania, starszych opcji i renderera, składni JS i wycinka C++ generującego HTML, a także test przełączania strony. Sprawdzono zastosowanie patcha do v3.58.12 i zgodność z pełnym plikiem. **Nie przeprowadzono kompilacji ESP32, testu w prawdziwej przeglądarce ani na płytce.**

Prywatny ZIP zawiera kompletne `main_centrala.cpp`, `InternetProbe.h` i `WyborStrony.h`, ale nie cały projekt PlatformIO. Zrób kopię projektu, podmień te pliki, skompiluj i testuj na drugiej odłączonej płytce bez wyjść wykonawczych i usług produkcyjnych. Nie wykonuj `uploadfs` i nie kasuj NVS ani LittleFS. Sprawdź różne pary, brak trocin, zakresy, pełny ekran, odświeżanie oraz odczyt starych danych, zanim rozważysz użycie na instalacji.
