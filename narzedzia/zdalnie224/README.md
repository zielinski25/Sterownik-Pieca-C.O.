# Zdalnie, bez kabla — co już jest w firmware, a czego nie ma

Ustalenia z 2026-10-28, kiedy okazało się, że użytkownik jest 100 km od pieca.

## 1. Pobranie historii CSV **już działa** — komendą Telegram `/historia`

To najważniejsze ustalenie tego etapu. Komenda nie jest nowa: jest
w `main_centrala.cpp` od wersji v3.54.0 i **jest w firmware v3.55.1, który
pracuje na piecu** (`firmware-patch/v3.55.1-bezpieczny-start/main_centrala.cpp`):

| miejsce | co robi |
|---|---|
| linia 39192 | `} else if (cmd == "/historia") { tgKolejkujAlboOdpowiedzBledem(TG_HIST); }` |
| linia 44154 | `case TG_HIST:` → wysyła trzy dokumenty |
| linia 33753 / 33871 | `sendTelegramDocumentHistKrotka()` / `...Dluga()` |

Wysyła jako dokumenty: `hist_short.csv`, `hist_long.csv` oraz `log_ustawien.txt`.
Pliki to `/hist_short.csv` i `/hist_long.csv` na LittleFS — **te same**, które
serwuje nowa trasa `/history` z etapów 209–223.

Skutek: żeby pobrać historię z drugiego końca Polski, **nie trzeba wgrywać
żadnego nowego firmware**. Wystarczy wiadomość na Telegramie.

Czym różni się to od nowej trasy `/history`:
* `/historia` wysyła **cały plik** (może kończyć się niedokończonym wierszem
  CSV, jeśli w chwili wysyłki trwał zapis),
* `/history` wysyła **tylko potwierdzony prefiks** (`g_histShortSize`) przez
  zapinkę `historia211::Pin` i dodaje `Content-Disposition: attachment`
  z nazwą pliku — czyli pobranie „jak z przeglądarki".

Dane są te same, różni się sposób podania.

## 2. Zdalna aktualizacja przez Telegram `/update` — istnieje, ale wymaga Release

`case TG_UPDATE` (v3.56.5: linia 44647) robi dokładnie to:

1. `sprawdzNajnowszaWersjeGithub()` → `https://api.github.com/repos/zielinski25/Sterownik-Pieca-C.O./releases?per_page=1`
2. porównuje tag z `FIRMWARE_VERSION` (`porownajWersje`) — **odmawia downgrade'u**,
3. `wykonajAktualizacjeOTA(tag)` → `https://github.com/zielinski25/Sterownik-Pieca-C.O./releases/download/<tag>/firmware.bin`
4. zapisuje tag do NVS (`otaSelfTag`), restartuje,
5. po starcie `potwierdzPartycjeJesliCzeka()` potwierdza partycję i melduje na Telegramie.

Wymagania po stronie GitHuba:

* Release **opublikowany** (draft jest niewidoczny dla API bez logowania),
* tag w formie `v3.56.5` (liczby, porównywane przez `porownajWersje`),
* asset nazwany **dokładnie** `firmware.bin`,
* plik z `.pio\build\esp32-s3-n16r8\firmware.bin` po `pio run` z prawdziwym `secrets.h`.

## 3. Bezpiecznik

`sdkconfig.defaults`: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`,
`CONFIG_APP_ROLLBACK_ENABLE=y`; partycje dual-OTA (`partitions.csv`, app0/app1).
Firmware nadpisuje `verifyRollbackLater()` (zwraca `true`), więc nowy obraz
zostaje w stanie `PENDING_VERIFY` aż do `potwierdzPartycjeJesliCzeka()` w `setup()`.
**Jeśli nowy firmware nie wstanie, bootloader wraca do poprzedniego.**

Czego to nie chroni: firmware, który wstaje, ale działa źle. Dlatego skok
v3.55.1 → v3.56.5 (siedem wersji nigdy nieuruchomionych na sprzęcie) to
decyzja użytkownika, nie agenta. Awaryjnie: ojciec podłącza ESP pod USB.

## 4. Czego Telegram nie zastąpi

`/historia` wysyła plik **na czat**. Panel WWW, przyciski „Krótka (24 h)" /
„Długa (archiwum)" i trasa `/history` działają wyłącznie w sieci domowej —
do sprawdzenia dopiero na miejscu (albo przez VPN, jeśli kiedyś powstanie).
