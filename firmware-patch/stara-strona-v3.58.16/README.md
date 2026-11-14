# Centrala v3.58.16 — historia i diagnostyka starej strony

Kolejny etap na bazie v3.58.15, **nie cały uzgodniony zakres**: [plan](../stara-strona-v3.58.11/ZAKRES.md). Dotychczasowa strona i jej wygląd pozostają.

## Changelog

- W zakładce Wykresy dodano karty diagnostyki i włączania/wyłączania zapisu historii na flash. Wyłączenie nie usuwa plików, a po ponownym włączeniu istniejąca kolejka RAM może zostać zapisana. Nie gwarantuje to zapisu każdej próbki.
- Komendy są wysyłane istniejącą lokalną kolejką LAN. Samo przyjęcie do kolejki nie oznacza wykonania: strona czeka na wynik. Dotąd komendy historii zawsze zgłaszały sukces nawet po błędzie NVS; teraz zgłaszają błąd, jeśli zapis ustawienia nie powiódł się. Zmiana w RAM następuje przed zapisem NVS — przy błędzie albo timeout może być aktywna do restartu mimo braku potwierdzenia. Sprawdź status i po restarcie. Po timeout komenda może jeszcze zostać wykonana; nie naciskaj wielokrotnie w ciemno. Inny klient może w każdej chwili zmienić ustawienie.
- Diagnostyka pokazuje strategię i stan pompy, serwa, alarmy, rozpalanie, czujnik dymu, symulacje oraz czasy wymuszeń dostępne w `/api/status`; wyświetla także połączenie, IP, wersję, status WAN i osobno gotowość Firebase. Dane statusu sterownika nie dowodzą fizycznego przepływu ani rzeczywistej pozycji serw. Nie wszystkie czujniki mają niezależny status sprawności. Wskazania WAN są wynikiem sondy, nie gwarancją dostępności wszystkich usług.
- Status starszy niż 6,5 s jest oznaczany jako nieświeży; spóźnione odpowiedzi i błędy starszych zapytań nie nadpisują nowszych. Brak pozycji serwa nie jest interpretowany jako zero. Komunikaty przy poleceniach pompy, mieszadła i rozpalania mówią o przyjęciu polecenia, nie o fizycznym skutku.
- Przycisk czyszczenia nadal nie działa: istniejący backend celowo zwraca 409 z powodu wyścigów; obok dodano ostrzeżenie. Bezpieczne, osobne czyszczenie obu historii nie zostało jeszcze wdrożone.

## Testy i ograniczenia

Przeprowadzono testy hostowe na danych i odpowiedziach syntetycznych: diagnostyka, nieznane/zerowe wartości, nieświeżość, powodzenie/odmowa/ACK kolejki, błąd NVS, brak sieci, kolejność odpowiedzi statusu oraz HTML. Testy regresyjne z v3.58.13 i v3.58.15, wybrane z v3.58.14, składnia JS i fragmentu C++ HTML oraz porównanie patcha przeszły. Starsze testy v3.58.11–12 i test solarów v3.58.14 są zależne od granic wycinanych fragmentów kolejnych wersji i nie są uniwersalnymi testami tego pliku; aktualne testy zastępcze v3.58.13 i 15 przeszły. **Nie wykonano pełnej kompilacji ESP32 ani testów w przeglądarce i na sprzęcie.**

Prywatny ZIP zawiera pełny `main_centrala.cpp`, `InternetProbe.h` i `WyborStrony.h`, ale nie cały projekt PlatformIO. Zrób kopię i podmień pliki w `centrala/src`; wymagane są pozostałe pliki projektu, biblioteki i własny `secrets.h`. Testuj najpierw na drugiej odłączonej płytce, bez elementów wykonawczych i usług produkcyjnych. Nie wykonuj `uploadfs` i nie kasuj NVS ani LittleFS. Sprawdź obie strony, zmianę zapisu, restart i zachowanie przy odłączeniu sieci oraz błędzie zapisu.

Pozostały m.in. zgodne rozszerzenie historii o trociny/dym/pochodzenie symulacji i ich wykresy, osobne bezpieczne czyszczenie, skrót solarny w kafelku i pełny zakres OTA/Release. Nie zmieniono formatu historii ani algorytmów sterowania. Wymagane są również testy poprzednich etapów na sprzęcie.
