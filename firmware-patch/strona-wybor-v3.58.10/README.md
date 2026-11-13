# Centrala v3.58.10 — etap: pamięć wyboru strony

**To nie jest pełna realizacja przeniesienia funkcji nowej strony na starą.** W tej wersji dodano tylko pamięć wyboru i przełączanie. Wygląd ani funkcje starej strony nie zostały zmienione. Nie należy traktować tej wersji jako zakończenia zadania.

## Zmiany od v3.58.9
- Wybór przechowywany jest w NVS centrali (`wyborwww/strona`), wspólnie dla wszystkich przeglądarek. Bez zapisanego wyboru domyślnie jest nowa strona.
- Po wejściu na adres główny `/` centrala otwiera zapamiętany widok. Bezpośrednie adresy to `/stara` i `/nowa`. Kliknięcie przełącznika zapisuje wybór przed przejściem; przy błędzie wyświetla komunikat. Otwarcie lub przywrócenie bezpośredniego adresu z aktywnym JavaScriptem też ustawia wybór; ostatnie skuteczne zgłoszenie z dowolnego urządzenia wygrywa.
- Zapis tylko gdy wartość się zmienia; sprawdzenie wyniku zapisu, token i mutex dla jednoczesnych żądań. Zmiana wymaga JavaScriptu, sieci i działającego NVS. Przy bezpośrednim wejściu błąd automatycznego zapisu nie jest pokazywany.
- Nie zmieniono funkcji sterowania, kodu LCD ani prognozy v3.58.9.

## Wstępne porównanie
Stara strona posiada dashboard i sterowanie, wykresy historii, pogodę z centrali, OTA, logi, ustawienia i terminal. Nowa ma m.in. odrębny rozbudowany interfejs, własną obsługę wykresów i pogody, analitykę i archiwum solarne oraz sterowanie LAN. Zakres podobnych funkcji nie oznacza zgodności zachowania: należy porównać i przenieść szczegóły, w tym wykresy, analitykę solarną i obsługę stanów/komend. Ta analiza nie jest jeszcze kompletna; żaden z tych obszarów nie został przeniesiony w tej wersji.

## Pliki i próba
Pełny `main_centrala.cpp`, `InternetProbe.h` i nowy `WyborStrony.h` są w prywatnej paczce na Dysku, ze względu na możliwe dane poufne. To nie jest cały projekt PlatformIO. Zrób kopię projektu v3.58.9, podmień `main_centrala.cpp`, dodaj `WyborStrony.h` do `src` i zachowaj `InternetProbe.h` oraz pozostałe pliki projektu. Samodzielnie skompiluj. **Próba wyłącznie na drugiej odłączonej płytce**, bez wyjść wykonawczych i produkcyjnych usług. Nie używaj uploadfs ani nie kasuj LittleFS/NVS.

Po uruchomieniu: otwórz IP centrali, wybierz starą stronę, uruchom ponownie, otwórz samo IP; powinna być stara. Kliknij NOWY PANEL, ponownie zrestartuj i otwórz samo IP — powinna być nowa. Sprawdź bezpośrednio `/stara` i `/nowa` oraz komunikat przy błędzie zapisu. Pamięć jest wspólna dla różnych urządzeń.

## Testy i ograniczenia
Test logiki wyboru: `g++ -std=c++17 -Wall -Wextra -Werror tests/test_wybor.cpp -o /tmp/test_wybor && /tmp/test_wybor`. Test skryptów obu HTML: `node tests/test_ui.js /ścieżka/do/main_centrala.cpp`. Sprawdzono również nałożenie patcha na prywatny plik v3.58.9. **Nie skompilowano firmware ESP32 ani nie przetestowano na sprzęcie, w tym NVS, tras AsyncWebServer i działania po zaniku zasilania.** Testy hostowe nie gwarantują bezpieczeństwa sterownika.
