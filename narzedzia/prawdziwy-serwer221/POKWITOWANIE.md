# Etap 221 — pokwitowanie

**Data:** 2026-10-28
**Wynik:** `PASS brama221 kontrol=28 profili=2 mutantow=12 czas=20.4s`
**Commit:** `0531708` (gałąź `arena/8d4e1b71-sterownik-pieca-c-o`)
**Paczka:** `ETAP-221-PRAWDZIWY-SERWER.zip` — 211 392 B, 109 wpisów, sha256 `df95102e…94ba`

## Co zostało zrobione

Do tej pory nasz kod HTTP był sprawdzany na własnych atrapach biblioteki.
Etap 220 pokazał, że atrapy różnią się od prawdziwej biblioteki — więc atrapa
nie jest dowodem.

Etap 221 bierze **prawdziwe źródła biblioteki** (parser żądań, wybór handlera,
budowa odpowiedzi — pliki `WebRequest.cpp`, `WebServer.cpp`, `WebHandlers.cpp`,
`WebResponses.cpp` z przypiętego commita `c47917d`), **prawdziwe**
`String`/`Print`/`Stream` z ArduinoCore-API i **nasz kod wycięty dosłownie
z v3.56.4** (bramka historii + trzy trasy `/history*`), kompiluje to jednym
`g++` i podaje do parsera **surowe bajty żądania HTTP**. Odczytuje surowe bajty
odpowiedzi — takie, jakie poszłyby do telefonu.

Wycięcie jest pilnowane: skrypt odbudowuje v3.56.4 z v3.55.8 + patch 218,
sprawdza SHA pliku (`f89874f3…8e38`) i porównuje SHA wyciętych fragmentów.
Nie ma ani jednej linii przepisanej ręcznie.

## Wynik w liczbach

* 24 kontrole w profilu „z tokenem”, 4 w profilu „bez tokenu” (razem 28),
* 12 mutantów — każdy psuje jedno miejsce i **każdy** został wykryty,
* czas całej bramy: ok. 20 s.

Najważniejsze kontrole:

| Co | Wynik |
|---|---|
| `/history/long` trafia do długiej historii (błąd z 218 nie wrócił) | PASS |
| `/history` zwraca CSV bajt w bajt równy plikowi | PASS |
| `Content-Type: text/csv`, `Content-Disposition: attachment; filename="historia-krotka.csv"` | PASS |
| 401 bez tokenu, 401 ze złym tokenem, 405 na POST, 503 bez tokenu w secrets.h | PASS |
| bilet: wydany → działa raz → drugi raz 401 → po 5 min 401 → zły strumień 401 | PASS |
| drugie pobranie w trakcie pierwszego → 503; po zwolnieniu → 200 | PASS |
| potwierdzony prefiks: wysyłane jest `min(licznik, rozmiar pliku)` | PASS |
| plik ponad limit dokumentu → 413; brak potwierdzonych bajtów → 404 | PASS |

## Czego ten etap NIE udowadnia (uczciwie)

* **Nie zastępuje PlatformIO na Twoim komputerze.** Tutaj kompiluje `g++`
  na Linuksie, nie `xtensa-esp32-elf-g++` z Arduino-ESP32.
* Nie sprawdza prawdziwego lwIP/WiFi/LittleFS — gniazdo i dysk są emulowane
  (tabela „co prawdziwe, co nasze” jest w README).
* Nie sprawdza wyglądu panelu na telefonie ani zachowania pod obciążeniem.

## Co dalej (Twoje kroki, bez zmian)

1. Nałóż **skumulowaną** łatkę `main_centrala-v3.55.8-to-v3.56.4.patch`
   i zostaw `HistoriaPobranie.h` **bez zmian** (sha `b58d1a4c…`).
2. Sprawdź, że linia 5601 to `v3.56.4`, a include’y są w liniach 5242–5244.
3. Uruchom PlatformIO. Napisz „przeszło” albo pierwszy błąd.
4. Test na urządzeniu: telefon → HISTORIA CSV → „Krótka (24 h)” →
   `historia-krotka.csv`; „Długa (archiwum)” → `historia-dluga.csv`.

Plik do wgrania pozostaje ten sam co po etapie 218 (v3.56.4) — etapy 219–221
nie zmieniły firmware’u ani o linię.
