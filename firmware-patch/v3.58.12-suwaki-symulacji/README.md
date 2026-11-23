# Firmware centrali 3.58.12 — suwaki symulacji w WWW nie cofają się pod palcem

Data: 2026-11-26 · następna po **3.58.11** (`firmware-patch/v3.58.11-alarm-dymu-po-symulacji/`)
i zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (50 925 linii, SHA-256 `fe6f0b20…`).
Pliki `.h`, `secrets.h`, biblioteki i `platformio.ini` bez zmian. Patch względem 3.58.11:
**7 hunków, +56/−4** — wyłącznie JavaScript strony WWW wbudowanej w plik
(`WIFI_LOCAL_PANEL_HTML`, funkcja `symulacjaSekcjaHtml()`) + nagłówek, historia, numer wersji.
**Kod C++ sterownika nie zmienił się ani o linię.**

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Skąd ta wersja

Zgłoszenie 23.11 11:07 (telefon, strona z adresu centrali, arkusz „Bojler C.W.U.”, symulacja
włączona): po przesunięciu suwaka „Wartość” nie da się zdążyć z „Zastosuj” — suwak sam
wraca do poprzedniej wartości symulacji.

Przyczyna: arkusz jest odświeżany co 1 s (`refreshSheet()` → `live$(wrap)`). Przy aktywnej
symulacji ta funkcja ustawia suwak na bieżącą wartość symulowaną, **chyba że suwak ma fokus**
(`document.activeElement`). Na telefonie `<input type="range">` po dotknięciu fokusu nie
dostaje — na PC z myszą dostaje, stąd różnica.

## Co zmienia 3.58.12

* `symEdycja = {wartosc, czas}` — flaga „operator rusza tym suwakiem”, ustawiana na
  `pointerdown` / `touchstart` / `mousedown` / `keydown` / `input`.
* `live$(wrap)` nie nadpisuje suwaka z flagą; drugi suwak dalej podąża za sterownikiem
  („Czas trwania” odlicza minuty).
* Flagi gasną po **Zastosuj** i **Wyłącz teraz** oraz gdy symulacja przestaje być aktywna —
  wtedy suwaki znów pokazują stan sterownika (zmiana z Telegrama/aplikacji jest widoczna).
* Ten sam błąd był w `Piec.html` w repozytorium — poprawiony tą samą zmianą. Stary interfejs
  („STARA WERSJA”, `symZaladujStan()`) ładuje suwak tylko przy otwarciu — wady nie miał.
  Aplikacja Android trzyma suwak w `remember{}` — też nie.

## Sprawdzenie na PC (`WYNIKI/testy-pc.txt`)

`tests/test_suwaki_symulacji.js` wycina `symulacjaSekcjaHtml()` z HTML w pliku centrali i
uruchamia ją w jsdom: 13 sprawdzeń (suwak bez fokusu + 5 odświeżeń arkusza, „symuluj bojler
45.0 30”, powrót do śledzenia sterownika po Zastosuj/Wyłącz). To samo na `Piec.html`; kontrolnie
na 3.58.11 — 5 z 13 sprawdzeń BŁĄD (wada odtworzona). Plik = 3.58.11 + patch (SHA); `node --check`
obu bloków `<script>`; testy DYM-SYM z 3.58.11 (21 sprawdzeń) na tym pliku — bez zmian.

## Po wgraniu

Odśwież stronę na telefonie (Chrome potrafi trzymać starą w pamięci podręcznej). Symulacja →
przesuń suwak → odczekaj 5 s: ma zostać. Zastosuj → kafelek pokazuje nową wartość.
