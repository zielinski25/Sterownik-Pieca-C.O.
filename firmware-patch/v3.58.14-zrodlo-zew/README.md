# Firmware centrali 3.58.14 — wybór źródła temperatury zewnętrznej (Zew 1 / Zew 2) wspólny dla WWW, aplikacji i LCD

Data: 2026-11-26 · następna po **3.58.13** (`firmware-patch/v3.58.13-symulacja-od-razu/`)
i zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 142 linie, SHA-256 `eb763b1b…`).
Pliki `.h`, `secrets.h`, biblioteki i `platformio.ini` bez zmian. Patch względem 3.58.13:
**21 hunków, +134/−4** — C++: 1 zmienna, 1 gałąź ustawień, 1 linia NVS, 3 linie JSON,
1 linia logu; reszta to JavaScript/CSS strony WWW wbudowanej w plik, nagłówek, historia, wersja.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Skąd ta wersja

Życzenie 23.11: w arkuszu „Temperatura zewnętrzna” na stronie ma być wybór źródła
(**Zew 1 = DS18B20**, **Zew 2 = temperatura z czujnika wilgotności DHT22**) — i ma być
**współzależny** z aplikacją: wybór w aplikacji zmienia kafelek na stronie i odwrotnie.

Żeby tak było, wybór **nie może** mieszkać w przeglądarce ani w telefonie. Mieszka w centrali.

## Co zmienia 3.58.14

* **`byte zewnZrodlo`** (1 = DS18B20, 2 = DHT22), NVS `"zewnZrodlo"`, domyślnie 1 = dotychczasowe
  zachowanie.
* **`ustawParametrPieca("zewnZrodlo", 1|2)`** — ta sama ścieżka co każde ustawienie: komenda
  `ustaw zewnZrodlo N` z Firebase `/piec/cmd` (strona, aplikacja), z LAN `/api/lan/cmd` i ze
  starej strony `/api/ustawienia-set`. Zero nowych tras.
* **Pole `"zewnZrodlo"` w każdym statusie**: `zbudujJsonStatusPelny()` (Firebase `/piec/status`
  + `/api/lan/status`), `wifiZbudujJsonStatus()` (`/api/status` — LCD, stara strona),
  `/api/ustawienia-get`; linia logu ustawień.
* **Strona WWW** (w pliku i `Piec.html`): `applyIncomingData()` czyta `t_zewn_2` (−40..80,
  `null` = brak) i `zewnZrodlo`; surowy DS18B20 zostaje w `S.t_zewn_ds`, a `S.t_zewn`
  (kafelek ZEWNĘTRZNA, arkusz, delta panel−zewn, podgląd Telegram) to **wybrane** źródło.
  Zew 2 bez świeżego DHT22 = BRAK DANYCH (bez cichego podstawiania DS18B20). Symulacja „zewn”
  ma pierwszeństwo. W arkuszu sekcja **„Źródło temperatury zewnętrznej”** — dwa przyciski z
  bieżącymi odczytami obu czujników, wysyłają `ustaw zewnZrodlo N`; podświetlenie i kafelek
  zmieniają się dopiero po statusie z centrali (bez stanu optymistycznego — jak reszta ustawień).
  Nagłówek karty pokazuje magistralę wybranego źródła. Wykresy: obie serie bez zmian.

**Czego centrala celowo nie robi:** nie podmienia `temperaturaNaZewnatrz`. `t_zewn` w telemetrii
i historii to nadal DS18B20, `t_zewn_2` nadal DHT22 — wykresy „Zew 1 / Zew 2” w aplikacji dalej
porównują oba czujniki, korekta wilgotności i automatyka bez zmian.

**Aplikacja Android i LCD** już czytają `t_zewn_2`, nieznane pole statusu pomijają — nic się nie
psuje. Żeby ich kafelek też podążał za ustawieniem i miały przełącznik, muszą uwzględnić
`zewnZrodlo`: aplikacja — potrzebne źródła bieżącej wersji (`sterownik-co-app\app\src` jako ZIP);
LCD — osobna wersja panelu 1.5.13.

## Sprawdzenie na PC (`WYNIKI/testy-pc.txt`)

`tests/uruchom_testy.sh`: A — `applyIncomingData()` + sekcja wyboru z HTML pliku w jsdom,
18 sprawdzeń (to samo na `Piec.html`; 3.58.13 nie ma sekcji); B — gałąź C++ + NVS + JSON wycięte
z pliku, `g++ -Wall -Wextra -Werror`, 12 sprawdzeń; C — plik = 3.58.13 + patch (SHA), wersja,
spójność, `node --check`; D — testy 3.58.13 (18+11) i DYM-SYM 3.58.11 (21). Wynik: **WSZYSTKO OK**.

## Pliki

| plik | co |
|---|---|
| `src/main_centrala.cpp` | pełny plik 3.58.14 |
| `main_centrala-v3.58.13-do-v3.58.14.patch` | `patch -p1` z katalogu projektu na plik 3.58.13 |
| `SHA256SUMS.txt` | sumy kontrolne |
| `CZYTAJ-NAJPIERW.txt` | podmiana, kompilacja, test na płytce |
| `WYNIKI/zakres-zmian.txt` | zmienione miejsca z numerami linii |
| `WYNIKI/testy-pc.txt` | wyniki testów |
| `tests/` | `uruchom_testy.sh`, `test_zrodlo_zew.js`, `test_zrodlo_zew.cpp`, `wytnij_zrodlo_zew.py`, `README.txt` |
