# Uzgodniony zakres — 2026-11-13
Prace w toku, lista jest planem, nie deklaracją wykonania. Stara grafika pozostaje; nowe elementy w starym stylu. Bez Firebase zdalnego, trybu demo, zmian terminala i jego Remote/DLOG oraz nowego dziennika strony. Istniejące funkcje pozostają. Zachować zapis wyboru strony z v3.58.10.

## Wykresy i historia
Zakresy stare oraz 6h/24h/7/30 dni, dostępne próbki, jawne braki; wybór serii, osie wspólna/normalizowana, opcjonalny filtr anomalii, opcje linii i wypełnienia, pełny ekran. Wykresy serw, pompy, ciśnienia i wilgotności, a po zgodnym rozszerzeniu historii od wdrożenia: trociny i dym. Zapisywać i wyróżniać symulację od wdrożenia; dla dawnych próbek pochodzenie nieznane. Różnica dwóch wybranych czujników A−B, wspólna z panelem solarnym. Analiza stanów/zmian pompy na podstawie próbek, bez uzurpowania dokładności. Ustawienia wykresów w przeglądarce. Eksport CSV krótkiej i długiej historii. Bez analizy cykli kotła, statystyk min/max/średniej/zmiany, linii LOLO/LO/HI/HIHI.

## Pogoda i solary
Rozszerzona prognoza w przeglądarce dla 51.066389,21.509167 (wymaga internetu klienta); zachować istniejącą z centrali i jej dane rolnicze. Aktualne wartości (wilgotność, wiatr/porywy, ciśnienie, chmury, opad i prawdopodobieństwo, UV), okresy 24h/3/7/14 dni, szczegóły godzin, 7-dniowe min/max, wschód/zachód/długość dnia, pozycja słońca, promieniowanie, GHI dziś i 7 dni. Archiwum centrali: prognoza, zmiana temperatury zbiornika, próbki/pokrycie, ekwiwalent 100l jawnie jako szacunek; skrót w kafelku panelu. Brak licznika poboru CWU i eksportu solarnego CSV.

## Sterowanie i serwis
Opis strategii pompy; potwierdzenie lub błąd ustawień/poleceń, nie mylić ACK z fizycznym skutkiem. Czytelność brakujących/nieświeżych danych serw i alarmów; stan i pozostały czas wspólnego rozpalania; czas ręcznych wymuszeń, jeśli dostępny; status czujników i porównanie brakujących funkcji symulacji, bez zmiany zabezpieczeń. Pokazywać czas wygasania symulacji, nie dodawać osobnego potwierdzenia. Bez ręcznego przycisku RTC. Pokazać stan połączenia/źródło/IP/wersję/nieświeże dane. Dodać włączanie zapisu historii i jawnie potwierdzane czyszczenie obu plików, sprawdzanie GitHub Releases oraz polecenia OTA centrali i panelu z rozróżnieniem ACK od wyniku. Zachować dostępne istniejące OTA/terminal/logi.

## Bezpieczeństwo i ograniczenia
Nie zmieniać zachowania sterowania ani interlocków; testować na odłączonej płytce bez wyjść i usług produkcyjnych. Stara historia musi pozostać czytelna bez kasowania. Sprawdzić obciążenie pamięci i synchronizację AsyncWebServer, wpływ zapisu i ograniczenia liczby próbek. Historyczne próbki nie uzyskują nowych danych wstecz. Brak kompilacji ESP32 lub prób na sprzęcie nie może być przedstawiany jako sukces.
