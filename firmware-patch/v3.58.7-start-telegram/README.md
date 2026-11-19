# Firmware centrali 3.58.7 — wiadomość startowa na Telegram; TAGCHECK mówi, co naprawdę poszło nie tak

Data: 2026-11-19 · następna po **3.58.6** (`firmware-patch/v3.58.6-panel-ota/`)
i zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h`, `secrets.h`,
biblioteki i `platformio.ini` bez zmian. Patch jest względem **3.58.6**; plik
jest kompletny, więc można go położyć także na 3.58.3–3.58.5 (wejdą wtedy też
zmiany 3.58.4–3.58.6).

Do kompletu — ale niezależnie — jest firmware panelu **v1.4.6**
(`LCD-Centrali/`), który przy starcie pokazuje wersję panelu i centrali.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Co zostało sprawdzone, a co nie: rozdział na końcu. Instrukcja wgrania
> i powrotu: `CZYTAJ-NAJPIERW.txt`.

---

## Skąd ta wersja

Log Telegrama z 19.11 (08:48–09:00), po `/update` na płytce z 3.58.6:

```
08:48  Aktualizacja pobrana OK. Restart za chwile...
       (11 minut ciszy)
08:59  UWAGA: aktualizacja "powiodla sie" (flash + restart bez bledu), ale
       dziala wersja v3.58.6, a mial byc zainstalowany 3.58.5. To zwykle
       znaczy, ze Release 3.58.5 na GitHub zostal opublikowany z binarka
       skompilowana PRZED podniesieniem FIRMWARE_VERSION w kodzie...
```

Dwie rzeczy były nie tak:

1. **Po restarcie centrala milczała.** Na Telegramie odzywała się po starcie
   tylko w trzech przypadkach: włączenie zasilania (`powerOnNotice`, z
   wersją), crash (raport crashu), restart z nadzoru zadań
   (`wyslijRaportNadzoru`). Po `ESP_RST_SW` — czyli po własnym OTA,
   `/restart`, ArduinoOTA, aktualizacji z przeglądarki — po przycisku RESET
   (`EXT`) i po `UNKNOWN`: nic. Czy nowy program w ogóle wstał i jaka to
   wersja, można było sprawdzić tylko ręcznie (`/wersja`, WWW).
2. **Ostrzeżenie TAGCHECK mówiło nieprawdę.** Tekst zakładał jeden przypadek
   (binarka *starsza* niż tag — ktoś nie podbił `FIRMWARE_VERSION`), a 19.11
   zaszedł odwrotny: binarka **nowsza** niż tag (Release 3.58.5 z plikiem
   v3.58.6). Kod był dobry, zły był numer Release — a tekst wysyłał szukać
   błędu w kodzie i straszył pętlą `/update`, której w tym przypadku nie ma
   (centrala odmawia downgrade'u).

Do tego jedyne ostrzeżenie kompilacji: `'beginResponse_P' is deprecated`
(ESPAsyncWebServer 3.12.1), linia ~31000.

## Co zmienia 3.58.7

### (a) Wiadomość startowa — `wyslijWiadomoscStartowa()`

Raz po każdym starcie, którego **nie opisuje inny raport**, gdy WiFi i
Telegram już działają:

```
Centrala wystartowala: v3.58.7 (boot=124). Powod restartu: programowy (SW)
- po aktualizacji OTA (tag 3.58.7). Nowy program czeka na potwierdzenie:
10 min stabilnej pracy z WiFi; potem przyjdzie "Aktualizacja zakonczona
pomyslnie" albo ostrzezenie.
```

| powód restartu (`esp_reset_reason()`) | co przychodzi | decyzja w logu `tag=START-TG` |
|---|---|---|
| `SW` (OTA, `/restart`, WWW, ArduinoOTA), `EXT` (RESET), `UNKNOWN`, `DEEPSLEEP`, `SDIO` | **wiadomość startowa** (nowa) | `wynik=wyslij` |
| `POWERON` | jak dotąd: „Centrala włączona po zaniku zasilania…” (`powerOnNotice`) | `wynik=poweron_osobno` |
| `PANIC`, `INT_WDT`, `TASK_WDT`, `WDT`, `BROWNOUT` (`bylCrash()`) | jak dotąd: raport crashu | `wynik=crash_osobno` |
| restart z nadzoru zadań (znacznik `nadzorRestart` w NVS) | jak dotąd: raport nadzoru | `wynik=nadzor_osobno` |

Treść: wersja, licznik startów `boot=`, powód po polsku z kodem ESP w
nawiasie; „po aktualizacji OTA (tag …)” gdy w NVS jest znacznik własnego
`/update` (`otaSelfTrwa` — tylko odczyt, sprząta go jak dotąd
`potwierdzPartycjeJesliCzeka()`); dopisek o potwierdzeniu, gdy partycja jest
w stanie `PENDING_VERIFY` (czyli tylko po OTA, nie po wgraniu kablem).
`START_TG_POTW_MIN` (10) jest związane `static_assert` z
`OTA_CONFIRM_UPTIME_MS`, żeby dopisek nie rozjechał się z realnym czasem.

Zachowanie: decyzja zapada raz (`g_startTgRozstrzygnieta`); nieudana wysyłka
(sieć jeszcze nie stoi) → ponowienie co 60 s, jak `powerOnNotice`. Teksty
(400 B) na stercie, nie na stosie zadania Telegrama (hwm_tg ≈ 2,2 kB); brak
pamięci = ponowienie za 60 s. Wołana z `tgTaskFn` zaraz po
`wyslijRaportNadzoru()`, bo ta ustawia `g_startTgNadzor`. **Nie pisze do
NVS.** Pętla restartów SW nie zasypie Telegrama: każda wiadomość wymaga
WiFi + Telegrama (kilkanaście sekund po starcie), a crash-loop idzie inną
ścieżką (streak w raporcie crashu).

### (b) TAGCHECK — dwa różne błędy, dwa różne teksty

`potwierdzPartycjeJesliCzeka()` po 10 min porównuje `FIRMWARE_VERSION` z
tagiem zapisanym przed `/update`:

| `porownajWersje(FIRMWARE_VERSION, tag)` | rodzaj | treść (skrót) | pętla `/update`? |
|---|---|---|---|
| `== 0` | `TAGCHECK_ZGODNA` | „Aktualizacja zakonczona pomyslnie. Dziala wersja: …” (bez zmian, co do znaku) | — |
| `< 0` binarka **starsza** niż tag | `TAGCHECK_BINARKA_STARSZA` | „UWAGA: … Binarka w Release X jest STARSZA niz jego tag: skompilowana PRZED podniesieniem FIRMWARE_VERSION. Popraw kod (FIRMWARE_VERSION = vX), skompiluj od nowa i podmien firmware.bin w Release X. Do tego czasu /update bedzie w kolko probowal to samo.” | **tak** |
| `> 0` binarka **nowsza** niż tag (19.11) | `TAGCHECK_BINARKA_NOWSZA` | „Aktualizacja wgrana i dziala wersja vY - NOWSZA niz tag Release (X)… Kod jest dobry; za maly numer ma Release na GitHub… Popraw tag i nazwe Release na numer dzialajacej wersji (vY, bez "v"…). Petli nie bedzie: /update odmowi (tag starszy niz dzialajaca wersja), ale do poprawki nie zobaczy tez zadnej nowszej wersji.” | nie |

W logu `tag=OTA-TAGCHECK … rodzaj=binarka_starsza|binarka_nowsza`. Tekst
(≤ 470 znaków) na stercie; przy odmowie sterty krótka wersja.

### (c) `beginResponse_P` → `beginResponse`

Handler lokalnego panelu WWW (`WIFI_LOCAL_PANEL_HTML`). Ta sama funkcja —
na ESP32 PROGMEM to zwykły flash; stara nazwa jest tylko aliasem oznaczonym
jako przestarzały. Kompilacja powinna mieć **0 ostrzeżeń**.

## Gdzie to jest w pliku (v3.58.7, 50 272 linie)

| linie | co |
|---|---|
| 1–47 | wpis changeloga |
| 6264 | `FIRMWARE_VERSION = "v3.58.7"` |
| 23069 | prototyp `bylCrash()` (definicja jest niżej, 23850) |
| 23071–23152 | **`[v3.58.7 START-TG-BEGIN]…[-END]`** — czyste funkcje: `startTgDecyzja()`, `startTgPowodOpis()`, `startTgTresc()`, `startTgDopisek()`, `tagcheckRodzaj()`, `tagcheckTresc()`; bez `String`/Arduino, testowane na PC |
| 23154–23214 | kod klejący: zmienne `g_startTg*`, `wyslijWiadomoscStartowa()` |
| 31199 | `beginResponse` zamiast `beginResponse_P` |
| 47025 | `wyslijRaportNadzoru()`: `g_startTgNadzor = true` |
| 47089 | `tgTaskFn`: wywołanie `wyslijWiadomoscStartowa()` |
| 48246–48266 | TAGCHECK w `potwierdzPartycjeJesliCzeka()` |
| 48308 | `static_assert` przy `OTA_CONFIRM_UPTIME_MS` |

Diff względem 3.58.6: 8 hunków, +212 / −11 linii
(`main_centrala-v3.58.6-do-v3.58.7.patch`). Pełna lista:
`WYNIKI/zakres-zmian.txt`.

## Co zostało sprawdzone, a co nie

**Sprawdzone (PC, `tests/`, wyniki w `WYNIKI/`):**

- `test_start_telegram.cpp` — **40 sprawdzeń** na bloku wyciętym z pliku
  centrali: decyzja dla każdego powodu restartu i kombinacji flag; treść co do
  znaku dla OTA z tagiem / bez tagu / `nullptr`, zwykłego SW, EXT, UNKNOWN;
  dopisek obecny tylko przy `PENDING_VERIFY`; obcinanie przy za małym buforze
  bez przepełnienia (`n=0` też); najdłuższy realny przypadek < 200 B; TAGCHECK
  dla przypadku z 19.11 (3.58.5 / v3.58.6), klasycznego (3.58.7 / v3.58.6)
  i zgodnego; długie tagi mieszczą się w 560 B. Kompilacja `-Wall -Wextra
  -Wshadow -Wconversion -Wformat=2 -Werror`, także z ASan + UBSan.
- Testy z paczki 3.58.6 uruchomione na tym pliku: **166** (OTA panelu) +
  **2697** (wykresy) — zielone; blok `[v3.58.6 PANEL-OTA]` jest **bajt w bajt**
  taki sam jak kopia, z którą rozmawiają testy panelu LCD
  (`LCD-Centrali/testy_host/kod_centrali/panel_ota_blok.inc`).
- `sprawdz_naglowki_esp32.sh` — blok + kod klejący + fragment TAGCHECK
  z **prawdziwymi nagłówkami** `Arduino.h`, `Preferences.h`, `esp_system.h`,
  `esp_ota_ops.h` (arduino-esp32 **3.0.7** i **3.3.8**, ESP-IDF 5.1): bez
  błędów, **0 ostrzeżeń** clang. Deklaracje otoczenia (`config`, `tgEnabled`,
  `logBootCounter`, `zapiszLog`, `sendTelegramMessage`, `porownajWersje`,
  `bylCrash`, `resetReasonNaTekst`) skrypt sprawdza po tekście w pliku
  centrali, więc zmiana sygnatury w centrali zatrzyma test.
- Patch nakłada się na 3.58.6 bez offsetów i daje plik o sumie z
  `SHA256SUMS.txt`.

**Nie sprawdzone:**

- kompilacja całego pliku w PlatformIO dla ESP32-S3 i uruchomienie na
  płytce; realny czas od startu do wiadomości (zależy od WiFi);
- `esp_reset_reason()` po resecie z esptool (SW czy EXT — oba dają
  wiadomość, różni się tylko opis);
- zapas stosu zadania Telegrama na płytce (nowy kod trzyma teksty poza
  stosem; `hwm_tg` w `tag=HEALTH-A` powinno zostać jak w 3.58.6:
  2256–2708 B).

## Zawartość paczki

```
CZYTAJ-NAJPIERW.txt                      3 kroki, sprawdzenie, porządek na GitHub
README.md                                ten plik
SHA256SUMS.txt
main_centrala-v3.58.6-do-v3.58.7.patch   diff -u (8 hunków)
src/main_centrala.cpp                    plik kompletny, v3.58.7
tests/uruchom_testy.sh                   C (40) + A (166) + B (2697) + porównanie bloku OTA
tests/test_start_telegram.cpp
tests/sprawdz_naglowki_esp32.sh          prawdziwe nagłówki ESP32 (pełne repozytorium)
tests/wytnij_start_tg.py
tests/README.txt
WYNIKI/testy-pc.txt, testy-pc-sanitizer.txt, naglowki-esp32.txt, zakres-zmian.txt
```
