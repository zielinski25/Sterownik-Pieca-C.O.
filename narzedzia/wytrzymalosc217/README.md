# Etap 217 — wytrzymałość: 300 pobrań i 200 wysyłek bez wycieku

Ten etap **nie zmienia firmware** (zostaje v3.56.3). Sprawdza rzecz, której nie
sprawdzał żaden wcześniejszy etap: **co się dzieje, gdy klikniesz pobieranie
wielokrotnie**, i czy centrala po tym dalej żyje.

`release_ready=false`, `main_changed=false`, urządzenia nie tykałem.

---

## 1. Po co

Wszystkie dotychczasowe testy robiły każdą rzecz **raz** (albo kilka razy w różnych
wariantach). Tymczasem typowa usterka firmware — „po kilkunastu pobraniach przestaje
działać, pomaga restart” — to zwykle **wyciek uchwytu pliku**: każde pobranie
otwiera plik historii i musi go zamknąć, niezależnie od tego, czy wysyłka się
udała, czy klient zerwał połączenie. LittleFS ma ograniczoną liczbę otwartych
plików, więc wyciek kończy się tym, że pobieranie przestaje działać do restartu.

Ten etap to sprawdza na prawdziwym kodzie: **300 pobrań HTTP, 200 wysyłek do
Telegramu, 50 zerwanych pobrań**, plus ścieżki awarii.

## 2. Co zostało zmierzone (wynik: PASS)

| grupa | co robi | wynik |
|---|---|---|
| **G01** | 300 × (bilet → pobranie → całe ciało = plik → zwolnienie) | 0 wycieków, 0 „zapchanych” flag |
| **G02** | 50 × zerwanie w połowie (klient znika po pierwszej porcji) | uchwyt zwolniony, flaga czysta, następne pobranie działa |
| **G03** | drugie pobranie **naraz** | 503 „Trwa inne pobranie”; bilet **nie** spalony; po pierwszym działa |
| **G04** | błąd VFS w trakcie odczytu | strumień zgłasza nieważność (`_sourceValid=false`), uchwyt zwolniony |
| **G05** | zapis historii trzyma blokadę | 503 „Zapis historii chwilowo zajęty”, zero uchwytów |
| **G06** | 200 × wysyłka `hist_short.csv` do Telegramu | ciało bajt w bajt, **`Content-Length` = długość ciała**, nazwa pliku poprawna |
| **G07** | 5 awarii wysyłki (odcięte gniazdo, brak RAM na bufor, błąd VFS, brak potwierdzonego rozmiaru, cache > plik) | każda kończy się **zerwaniem**, nigdy „udanym” uciętym plikiem |
| **G08** | `hist_long` + zapinka z limitem mniejszym niż plik | wysyłka poprawna; `TooLarge` odrzucone **bez** otwierania uchwytu |
| **G09** | bilans po całości | `otwartych == 0`, `otwarć == zamknięć`, **max 1 uchwyt naraz** |

Trzy profile kompilacji: `native`, `asan-ubsan` (z `detect_leaks=1` — wykrywanie
wycieków pamięci), `glibcxx-assertions`. **66 sprawdzeń × 3 = 198, 0 błędów.**

## 3. Trzy kontrole negatywne (brama nie jest ozdobą)

| mutant | co psujemy | co to łapie |
|---|---|---|
| **M1** | `~Odpowiedz` nie zamyka zapinki | 8 błędów, **max 300 otwartych naraz** |
| **M2** | `~Odpowiedz` nie czyści flagi zajętości | 13 błędów: drugie pobranie dostaje 503 |
| **M3** | `Pin::close()` nie zamyka uchwytu w VFS | 19 błędów, **max 300 otwartych naraz** |

Każdy mutant kompiluje się i **musi** kończyć testem czerwonym. Bez tego etapu nie
byłoby wiadomo, czy soak w ogóle patrzy na uchwyty.

## 4. Co to dowodzi, a czego NIE dowodzi

**Dowodzi:**
- powtarzalne pobieranie **nie zostawia otwartych plików** (max 1 na raz, 0 po),
- zerwane połączenie i błąd odczytu **zwalniają** zasoby i nie blokują kolejnych pobrań,
- wysyłka do Telegramu **nie kłamie**: ogłoszony `Content-Length` zawsze równa się
  długości ciała, a błąd kończy się zerwaniem, nie uciętym „sukcesem”,
- kod, który to robi, to **prawdziwy kod z pliku po patchu v3.56.3**, wyciągnięty
  automatycznie (dopasowanie nawiasów), nie przepisany.

**NIE dowodzi:**
- że LittleFS na urządzeniu zachowa się tak samo (model nie ma sektorów, opóźnień
  i zaniku zasilania — sprawdza umowę interfejsu i to, co kod z niej robi),
- że TLS/Telegram naprawdę przyjmie żądanie (gniazdo jest modelem),
- ile pobrań wytrzyma **Twój** telefon i WiFi (to jest właśnie test na urządzeniu),
- że cały `main_centrala.cpp` się skompiluje — to PlatformIO.

## 5. Pliki

| plik | co to jest |
|---|---|
| `build.py` | odtwarza plik v3.56.3 (SHA z etapu 216) i wyciąga prawdziwy kod |
| `Extracted217.inc` | wygenerowane: oba wysyłacze Telegram + trasa + zapinka |
| `Test.cpp` | 9 grup soak-a (`G01`–`G09`) |
| `mutanty.py` | trzy celowo zepsute nagłówki — muszą wywalić test |
| `run.py` | całość: build → 3 profile → mutanty |
| `WYNIKI/run001/wynik.json` | wynik ostatniego przebiegu |

## 6. Co zrobić na komputerze

**Nic nowego.** Firmware zostaje na **v3.56.3** (etap 216). Jeśli jeszcze nie
nałożyłeś patcha z 216, rób to według `od-przycisku216/README.md` §6.

## 7. Co zostaje ryzykiem

1. Zachowanie LittleFS przy pełnej partycji i przy zaniku zasilania — poza modelem.
2. Kolejka pobrań (jedno naraz) — nadal świadomie jedno; ten etap pokazuje tylko,
   że odmowa (503) jest czysta i nie psuje kolejnego pobrania.
3. Rzeczywisty czas wysyłki dużego pliku przez WiFi/TLS — poza modelem.
