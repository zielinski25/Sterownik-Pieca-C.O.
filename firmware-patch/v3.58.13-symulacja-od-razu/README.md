# Firmware centrali 3.58.13 — symulacja widoczna od razu; suwak nie wraca po „Zastosuj”

Data: 2026-11-26 · następna po **3.58.12** (`firmware-patch/v3.58.12-suwaki-symulacji/`)
i zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 010 linii, SHA-256 `9118629e…`).
Pliki `.h`, `secrets.h`, biblioteki i `platformio.ini` bez zmian. Patch względem 3.58.12:
**10 hunków, +88/−4** — jedna nowa funkcja C++ (9 linii) + dwa jednolinijkowe wywołania,
JavaScript strony WWW (`symulacjaSekcjaHtml()`), nagłówek, historia, numer wersji.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Skąd ta wersja

Zgłoszenie 23.11 (po 3.58.12, panel WWW „Bojler C.W.U.”): suwak da się już przesunąć i
nacisnąć „Zastosuj”, ale **po naciśnięciu wartość wraca suwakiem do tego, co było**.

Przyczyna: komenda `symuluj` ustawiała tylko `symulowanaTemperatura*` i flagę. Do zmiennej
**odczytu** (`temperaturaBojlera` itd. — to, co widać w statusie, na kafelku, LCD, w Telegramie)
wartość trafiała dopiero przy kolejnym odczycie czujnika z harmonogramu `dsPlanInterval()`:
bojler/panel co **60 s**, zewn/pokój co **120 s** (DHT 120 s, BMP280 180 s). Po ACK panel
czytał status: „symulacja aktywna” + **stara** temperatura → 3.58.12 zwalniało blokadę suwaka
(`symEdycja`) i `live$` ustawiało go na starą wartość; po 1–2 min suwak i kafelek skakały na
zadaną.

## Co zmienia 3.58.13

* **C++ `symulacjaUstawOdczytOdRazu(pole, wartosc)`** — start symulacji (komenda `symuluj`
  z Firebase/LAN i `POST /api/symulacja-czujnik` ze starej strony) od razu przepisuje wartość
  do zmiennej odczytu (9 pól; `dym` ma własną pętlę i jest pominięty). Status, kafelki, LCD,
  Telegram i telemetria widzą symulację natychmiast. Kolejny odczyt z harmonogramu zwróci tę
  samą wartość (`odczytajTemperature()` przy symulacji nie dotyka filtra) — logika regulacji
  i alarmów bez zmian. Zapis `float` jest atomowy; zadanie czujników i tak nadpisuje te zmienne.
* **JS `symZadana`** — po potwierdzonym „Zastosuj” suwaki trzymają zadane parametry, dopóki
  sterownik nie pokaże zadanej wartości (±0,06) albo odczyt nie zmieni się od chwili ACK (np.
  Telegram) — najwyżej 3 min. Na 3.58.13 blokada zwalnia się natychmiast po ACK (status już
  niesie zadaną wartość), więc „Czas trwania” odlicza bez przerwy; na starszym firmware przez
  Firebase (`Piec.html`) chroni przed cofnięciem. „Wyłącz teraz” i koniec symulacji zdejmują
  blokadę. Ta sama zmiana w `Piec.html` w repozytorium.

## Sprawdzenie na PC (`WYNIKI/testy-pc.txt`)

`tests/uruchom_testy.sh`: A — `symulacjaSekcjaHtml()` z HTML pliku w jsdom, 18 sprawdzeń
(kontrolnie 3.58.12: 1 BŁĄD — wada odtworzona); B — `symulacjaUstawOdczytOdRazu()` wycięta
z pliku, `g++ -Wall -Wextra -Werror`, 11 sprawdzeń; C — plik = 3.58.12 + patch (SHA),
wywołania helpera, `node --check`; D — DYM-SYM 3.58.11 21/21. Wynik: **WSZYSTKO OK**.

## Pliki

| plik | co |
|---|---|
| `src/main_centrala.cpp` | pełny plik 3.58.13 |
| `main_centrala-v3.58.12-do-v3.58.13.patch` | `patch -p1` z katalogu projektu na plik 3.58.12 |
| `SHA256SUMS.txt` | sumy kontrolne |
| `CZYTAJ-NAJPIERW.txt` | podmiana, kompilacja, test na płytce |
| `WYNIKI/zakres-zmian.txt` | zmienione miejsca z numerami linii |
| `WYNIKI/testy-pc.txt` | wyniki testów |
| `tests/` | `uruchom_testy.sh`, `test_suwaki_symulacji.js`, `test_sym_od_razu.cpp`, `wytnij_sym_od_razu.py`, `README.txt` |
