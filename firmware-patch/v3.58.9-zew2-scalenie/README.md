# Firmware centrali 3.58.9 — powrót „Zew 2” (DHT22): scalenie linii 3.57.13 i 3.58.8 + filtr przekłamanych ramek

Data: 2026-11-19 · następna po **3.58.8** (`firmware-patch/v3.58.8-fw-w-statusie/`)
**i po 3.57.13** (paczka KANDYDAT z Dysku Google, kopia w
`firmware-patch/v3.57.13-zew2-kandydat-z-dysku/`). Zawiera wszystko z obu linii.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (50 711 linii, SHA-256
`2b02197f…`). Pliki `.h`, `secrets.h`, biblioteki i `platformio.ini` bez zmian.
Dwa patche (`-p1`): względem **3.58.8** (25 hunków, +474/−62) i względem
**3.57.13** (60 hunków, +2699/−144) — oba dają bajt w bajt ten sam plik
(sprawdzane w `tests/uruchom_testy.sh`, część C2).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Instrukcja krok po kroku: `CZYTAJ-NAJPIERW.txt`.

## Skąd ta wersja — odpowiedź na pytanie z 19.11

Pytanie: *czy moje zmiany w centrali (3.58.7/3.58.8) zepsuły temperaturę „Zew 2”,
która zniknęła z wykresu ok. 10:40, skoro na 3.57.13 działała — i czy dzisiejsze
zmiany były robione na innej/starszej wersji?*

**Tak, i tak.** Szczegóły:

1. W repozytorium (git i dysk) **nie ma** żadnej wersji 3.57.10–3.57.13. Ostatnia
   wersja ze wspólnego pnia to **3.57.9** (1.11). Po niej powstały dwie linie:
   * **3.58.0 → 3.58.8** (10–19.11, repozytorium): panel LAN, OTA panelu, wykresy
     dla panelu, START-TG, pole `fw` w `/api/status`;
   * **3.57.10 → 3.57.13** (15–17.11, paczki KANDYDAT na Dysku Google, poza
     repozytorium): **Zew 2** = temperatura z DHT22 jako `"t_zewn_2"` w
     `/api/status`, `/piec/status` i próbkach minutowych Firebase, nowy
     harmonogram DS18B20, 12 bitów na GPIO4, DHT co 120 s, progi świeżości
     (DHT 135 s, BMP 195 s), `FB_JSON_CAP 4096`.
2. Paczki 3.58.7 i 3.58.8 robiłem na 3.58.6 z repozytorium — **nie** na 3.57.13
   z płytki. Nie zawierały więc niczego z 3.57.1x. W 3.58.8 nie ma ani jednego
   wystąpienia `t_zewn_2` (`grep` w `WYNIKI/zakres-zmian.txt`).
3. Wgranie 3.58.8 o ≈10:40 zdjęło z płytki całą linię 3.57.1x. Aplikacja
   Android (0.31.94) czyta klucz `t_zewn_2` — gdy go nie ma, rysuje „Zew 2: —”.
   Kod nie był „zepsuty”; po prostu zniknął. Zew 1 (DS18B20, klucz `t_zewn`)
   jest w obu liniach, dlatego działa dalej.

Jak to sprawdziłem: pobrałem z Dysku paczki 3.57.9 i 3.57.13 (SHA 3.57.13
`a5f77e49…` zgadza się z `$expectedModified` w jej `buduj-3.57.13.ps1`),
porównałem `diff -u` 3.57.9→3.57.13 (24 hunki) i 3.57.9→3.58.8 (54 hunki):
zbiory zmian są rozłączne poza nagłówkiem i numerem wersji.

## Co zawiera 3.58.9

| | skąd | co |
|---|---|---|
| **A** | 3.57.10–3.57.13 | `t_zewn_2` w trzech JSON-ach, `TelemetrySample.tDhtDiagX10` (bajty 76..79, dotąd padding; znacznik `TEMP_DIAG_MARK` w `qualityBits`, ABI 80 B bez zmian), harmonogram DS18B20 (`dsPlan*`, CO/powrót 2,8/15/60 s wg pompy i temperatur, reszta 60–120 s, odczyt po potwierdzonej konwersji), `zastosujFiltrWiarygodnosci(f, odczyt, odczytPoprawny)` dla ujemnych temperatur DS, `sensors1.begin(12)`, DHT co 120 s z odstępem ≥ 2 s, progi świeżości, `telemetryQuality` nie zgłasza „świeży” po błędnej próbie, `FB_JSON_CAP 4096`, DHT w `setup()` ponawiany co 2 s |
| **B** | 3.58.0–3.58.8 | panel LAN (`/panel`, `/api/*`, token komend), `/api/history/panel`, OTA panelu przez centralę (`/update_panel`), START-TG (wiadomość startowa na Telegram, TAGCHECK, `beginResponse`), `"fw"` w `/api/status` |
| **C** | nowe | **filtr Zew 2** (`// --- ZEW2-FILTR-POCZATEK/KONIEC ---`, linie 6206–6279, i `wilgotnosc()`, linie 34002–34084) |

Scalenie: `git merge-file` (3-way: 3.57.13 / wspólna podstawa 3.57.9 / 3.58.8).
2 konflikty — nagłówek pliku i `FIRMWARE_VERSION` — rozwiązane ręcznie; pozostałe
78 hunków nałożyło się bez konfliktu. Numer **3.58.9**, nie 3.57.14: OTA centrali
(`porownajWersje`) odmawia downgrade'u, więc 3.57.14 nie wgrałaby się po OTA na
płytkę z 3.58.8. Funkcjonalnie płytka zachowuje się jak na 3.57.13 plus wszystko
z 3.58.x. Numery linii wszystkich zmian: `WYNIKI/zakres-zmian.txt`.

## Filtr Zew 2 — dlaczego i jak

Na wykresie z 3.57.13 Zew 2 miała co ≈ godzinę pojedyncze piki do ≈ −2 °C przy
rzeczywistych ≈ +2 °C. **Lustrzana wartość** to obraz przekłamanego bitu znaku:
DHT22 koduje temperaturę jako znak (bit 15) + moduł (nie U2), a jego „suma
kontrolna” to zwykła 8-bitowa suma czterech bajtów — dwa jednakowe błędy o tej
samej wadze w różnych bajtach (128 + 128 ≡ 0) przechodzą niezauważone. Biblioteka
`dhtESP32-rmt` (ta w projekcie) sprawdza tylko tę sumę. Ten sam obraz
(pojedyncze −9,9 °C / −12 °C z poprawną sumą) opisują inni użytkownicy DHT22
(wątki na forum Home Assistant i Arduino) i leczą go filtrem wartości odstających.
Wybrałem wariant z **powtórką odczytu** (wg Twojej decyzji „która najbardziej
odpowiednia”) — nie zgaduje, tylko pyta czujnik drugi raz:

* **a)** kontrola **całej ramki**: `DHT_OK` i temperatura −40…80 °C i wilgotność
  1…100 % — oba pola przychodzą w jednej ramce, więc nieprawdopodobne jedno
  dyskwalifikuje też drugie (dotąd wilgotność > 100 % przechodziła, a tylko
  ≤ 0,1 % była odrzucana);
* **b)** odczyt **podejrzany** = różni się od ostatniej przyjętej wartości o > 3 °C
  **albo** ma przeciwny znak przy |t| ≥ 0,5 °C → nie trafia na wykres;
  `wilgotnosc()` powtarza odczyt po ≈ 3 s (zamiast po 120 s, odstęp ≥ 2 s zachowany,
  cykl 120 s nieprzesunięty). Dwa zgodne odczyty (≤ 0,5 °C) → **potwierdzony**
  (prawdziwa zmiana, opóźniona o 3 s). Powtórka zgodna z ostatnią przyjętą →
  przyjęta (to był pik). Niezgodna z niczym → **odrzucony**, czeka na następny cykl.
  Powtórka, która nie doszła do skutku w 15 s, przepada;
* **c)** pierwszy odczyt po starcie przyjęty bez weryfikacji; jeśli był przekłamany,
  następny jest „podejrzany”, powtórzony i potwierdzony — filtr sam się naprawia
  (znana pułapka filtra `outlier` w Home Assistant, który blokuje się na złym
  pierwszym odczycie, tu nie występuje);
* **d)** odrzucona lub zła ramka **nie kasuje** Zew 2 (dotąd `zapiszTempDiag(NAN)` —
  seria znikała do następnego odczytu); zostaje poprzednia wartość, którą
  `czytajTempDiag()` wygasza po 135 s jak dotąd. Symulacja wilgotności nadal
  zeruje Zew 2 i kasuje oczekującą powtórkę.

Progi w jednym miejscu (linie 6214–6217): `ZEW2_PROG_SKOKU_C 3.0`,
`ZEW2_PROG_ZNAKU_C 0.5`, `ZEW2_TOLERANCJA_POWTORKI_C 0.5`, `ZEW2_WERYFIKACJA_MAX_MS 15000`.
Logi: `DHT22 Zew2 zdarzenie=podejrzany|potwierdzony|odrzucony|zla_ramka …`,
liczniki `zew2_przyjete/potwierdzone/odrzucone` w linii `DHT22 diagnostyka` (co
odczyt, INFO). Sterowanie piecem, alarmy, Telegram, OTA, format telemetrii i
Firebase — bez zmian.

## Co było sprawdzone (`WYNIKI/`)

`tests/uruchom_testy.sh` (także z `SANITIZER=1`) → `WYNIKI/testy-pc.txt`:

| część | co | wynik |
|---|---|---|
| A | blok ZEW2-FILTR wycięty z pliku, 12 scenariuszy (pik lustrzany, prawdziwy skok, dwa niezgodne, zła ramka, zły pierwszy odczyt, przejścia przez 0 °C, przepełnienie `millis()`, dwa piki z rzędu…) | 57 sprawdzeń, 0 błędów (g++ `-Werror`, ASan+UBSan) |
| B | **prawdziwa `wilgotnosc()`** z pliku + atrapy (`millis`, kolejka `read_dht`, telemetria, log): start, pik → powtórka po 3 s, skok → potwierdzony bez przesunięcia cyklu 120 s, zła ramka (RH 3366 %), zanik po 135 s, powrót po przerwie, symulacja, przepełnienie `millis()` | 33 sprawdzenia, 0 błędów, 17 odczytów DHT; test mutacyjny: usunięcie powtórki → 11 błędów, surowy zapis zamiast `zapiszTempDiag` → 3 |
| C | znaczniki obu linii w pliku (27 wzorców, brak znaczników konfliktu), oba patche → ten sam SHA, zgodność z `SHA256SUMS.txt` | OK |
| D | `sprawdz_pola_statusu.py`: 81 pól `/api/status` vs 47 czytanych przez panel LCD | zgodne |
| E | testy paczek 3.58.7 (START-TG 40) i 3.58.6 (OTA panelu 166, wykresy 2697) na tym pliku | OK |

`tests/sprawdz_naglowki_esp32.sh` (`RDZEN=3.3.8 OSTRZEZENIA=1`) → `WYNIKI/naglowki-esp32.txt`:
wycinki Zew 2 + `wilgotnosc()`, START-TG, OTA panelu i wykresów skompilowane
clangiem 22 z **prawdziwymi nagłówkami arduino-esp32 3.3.8** — bez błędów,
0 ostrzeżeń.

**Nie było:** pełnego buildu PlatformIO, uruchomienia na płytce, testu na
prawdziwym DHT22 (progi 3 °C / 0,5 °C dobrane z wykresu; do korekty po logach).
Zastrzeżenia z README 3.57.13 (12 bitów po power-cycle DS nie potwierdzone na
sprzęcie) nadal obowiązują.

## Pliki

```
src/main_centrala.cpp                     v3.58.9, kompletny (tylko ten plik podmieniasz)
main_centrala-v3.58.8-do-v3.58.9.patch    -p1, na plik 3.58.8 (SHA e6034d4e…)
main_centrala-v3.57.13-do-v3.58.9.patch   -p1, na plik 3.57.13 (SHA a5f77e49…)
SHA256SUMS.txt                            sumy: wynik, patche, obie podstawy
CZYTAJ-NAJPIERW.txt                       3 kroki: kopia, Build/Upload, sprawdzenie
WYNIKI/zakres-zmian.txt                   co i gdzie (numery linii) względem 3.58.8 i 3.57.13
WYNIKI/testy-pc.txt, naglowki-esp32.txt   wydruki testów
tests/                                    skrypty (tests/README.txt)
```

Powrót: kopia `.bak-przed-3.58.9` (3.58.8, bez Zew 2) albo 3.57.13 z
`firmware-patch/v3.57.13-zew2-kandydat-z-dysku/` (bez panelu LCD; przez kabel, bo
OTA nie cofa wersji).
