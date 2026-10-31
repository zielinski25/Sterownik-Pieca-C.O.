# 247 — Firmware 3.57.2: raport nadzoru mówi, gdzie stanęło

Data: 2026-10-31
Paczka: `firmware-patch/v3.57.2-raport-zawieszenia/`
ZIP: **`1WT-ElCmVsuGM8O-95a98wi4YYWsB3EbP`** · 736 681 B ·
SHA-256 `f2c38efa329e584fa3f7eb3f49b99c4d7d0c8cc43b3bf1cae3732a1790f59881`

---

## 1. Zdarzenie, które to wywołało

**31.10.2026, 04:14** — Telegram:

> Wróciłem po automatycznym restarcie.
> Zadanie "tgTask (Telegram i Firebase)" nie dało znaku życia przez 300 s.
> Restart zrobił nadzór w firmware — nie było potrzebne wyciąganie wtyczki.
> To już restart numer 1 w ciągu 30 minut.

To **nie awaria pieca**. Zadziałał nadzór dodany w v3.55.2 (po zdarzeniu
z 24.10, gdy centrala zawisła i wymagała wizyty z kablem).

## 2. Co nadzór robi (bez zmian w 3.57.2)

| parametr | wartość |
|---|---|
| próg ciszy | 300 s (`NADZOR_PROG_MS`) |
| rozbieg po starcie | 180 s (`NADZOR_ROZBIEG_MS`) |
| maks. restartów w oknie | 3 (`NADZOR_MAX_RESTARTOW`) |
| okno ważności licznika | 30 min (`NADZOR_LICZNIK_WAZNY_MS`) |
| blokada podczas OTA / testów mega | tak |

Po 3 restartach z rzędu nadzór się wycofuje i tylko loguje
(`zdarzenie=limit_restartow`).

## 3. Czego brakowało

Raport mówił **kto** milczał, ale nie **na czym stanął**.

Dane istniały: `diagCheckpoint()` od v3.18.0 zapisuje w pamięci RTC
(`rtcCrashDiag`) czwórkę na zadanie — `op`, `stage`, `uptimeMs`, `freeHeap`.
Ale `diagInicjalizujBoota()` w `setup()` **zeruje** ten stan przy każdym
starcie. Nadzór restartował → informacja przepadała.

## 4. Zmiana w 3.57.2

| element | opis |
|---|---|
| `struct NadzorKrok` + `nadzorOdczytajKrok()` | nowa, czysta funkcja: odczyt znacznika z `rtcCrashDiag` dla `DIAG_TASK_TG` / `DIAG_TASK_TGPOLL` |
| `nadzorZadanSieciowych()`, gałąź `NADZOR_RESTART` | odczyt **przed** `ESP.restart()`, 5 kluczy do NVS, rozszerzony log |
| `wyslijRaportNadzoru()` | doklejona linia „Ostatni znacznik: …" |
| `FIRMWARE_VERSION` | `v3.57.1` → `v3.57.2` |

**Kolejność jest krytyczna:** odczyt musi być przed `ESP.restart()`, bo po
restarcie `setup()` już wyzerował `rtcCrashDiag`.

### Nowa treść komunikatu

```
Wrocilem po automatycznym restarcie.
Zadanie "tgTask (Telegram i Firebase)" nie dalam znaku zycia przez 300 s.
Ostatni znacznik: FB_SEND / BEFORE (po 11520 s pracy, wolna pamiec 94 kB)
Restart zrobil nadzor w firmware — nie bylo potrzebne wyciaganie wtyczki.
To juz restart numer 1 w ciagu 30 minut.
```

Gdy `seq == 0` (zadanie nie zapisało ani jednego znacznika od startu):

```
Ostatni znacznik: brak — zadanie nie zapisalo ani jednego znacznika od startu.
```

### Nazwy operacji (`diagOpName`, bez zmian)

`FB_SEND` · `FB_CMD` · `FB_INIT` · `FB_LOOP` · `TG_POST` · `TG_CONNECT` ·
`TG_POLL` · `TG_POLL_WAIT` · `TG_ITER` · `TG_COMMAND` · `WIFI_DNS` ·
`HIST_FS` · `LOG_FS` · `DESZCZ` · `PROGNOZA` · `WS_CLEANUP`

`BEFORE` = przed wejściem w czynność, `AFTER` = po wyjściu.

## 5. Pokrycie znacznikami (sprawdzone w kodzie)

| miejsce blokady | checkpoint |
|---|---|
| wysyłka statusu do Firebase | :45202 `FB_SEND/BEFORE` → :45204 `AFTER` |
| odbiór komend z Firebase | :45223 `FB_CMD/BEFORE` → :45225 `AFTER` |
| wysyłka na Telegram | :33298 `TG_POST/BEFORE`, :33314 (body) |
| połączenie TLS z Telegramem | :33254 `TG_CONNECT/BEFORE` |
| DNS | :32970–33018 `WIFI_DNS/*` |
| początek obiegu pętli | `TG_ITER/START` w każdej iteracji |

Czyli raport wskaże konkretną funkcję, nie „coś się zawiesiło".

## 6. Czego to NIE robi

- **Nie naprawia przyczyny zawieszenia.** To diagnoza, nie lek.
- Nie rusza `nadzorDecyzja()`, progów, testu hostowego `NadzorHostTest.cpp`.
- Nie rusza telemetrii, Firebase, Telegrama, OTA, historii, archiwum solarnego.
- Nie zmienia formatu `/piec/status` ani `/piec/cmd`, rytmu 15 s / 7 s / 2 s.
- **Aplikacja 0.31.63 bez zmian.**

## 7. Kontrola przed spakowaniem

| kontrola | wynik |
|---|---|
| bilans nawiasów (po usunięciu komentarzy i napisów) | delta identyczna jak 3.57.1 (-2/+6/+2 = artefakt regexa) |
| przyrost moich zmian | +3/+3 klamry, +28/+28 okrągłe, +1/+1 kwadratowy |
| deklaracja przed użyciem | `NadzorKrok` i `nadzorOdczytajKrok()` przed `nadzorZadanSieciowych()` |
| inicjalizacja agregatem `uint8_t` | zamieniona na pole-po-polu (narrowing) |
| rozmiar ZIP pobranego z Dysku | 736 681 B — zgodny z lokalnym |

## 8. Wgranie

Patrz `CZYTAJ-NAJPIERW.txt` w paczce. Skrót:

```
$u="https://drive.google.com/uc?export=download&id=1WT-ElCmVsuGM8O-95a98wi4YYWsB3EbP"
$z="$env:TEMP\v3.57.2.zip"; $d="$env:USERPROFILE\Documents\SterownikCO-3.57.2"
$src="$env:USERPROFILE\Documents\PlatformIO\sterownik-co-esp32\src"
Invoke-WebRequest -Uri $u -OutFile $z -UseBasicParsing
$h=(Get-FileHash -LiteralPath $z -Algorithm SHA256).Hash
if($h -ne "F2C38EFA329E584FA3F7EB3F49B99C4D7D0C8CC43B3BF1CAE3732A1790F59881"){ Write-Host "UWAGA suma inna: $h" } else { Write-Host "SUMA OK" }
if(-not (Test-Path -LiteralPath "$d\main_centrala.cpp")){ Expand-Archive -LiteralPath $z -DestinationPath $d -Force }
Copy-Item -LiteralPath "$src\main_centrala.cpp" -Destination "$src\main_centrala.cpp.bak-3.57.1" -Force
Copy-Item -LiteralPath "$d\main_centrala.cpp" -Destination "$src\main_centrala.cpp" -Force
Write-Host "Podmieniono. Teraz PlatformIO -> Upload."
```

## 9. Otwarte

1. **Log z nocy** — `piec_log (31).txt` (3,5 MB) jest na Dysku. Za duży, żeby
   wczytać cały; potrzebny ogon wokół ostatniego `zdarzenie=start`
   (okno 04:09–04:14 czasu lokalnego).
2. Pola zegara RTC w `/piec/status` — użytkownik **odłożył** (punkt A
   z wcześniejszej propozycji). Arkusz „Data i czas" nadal pokaże
   „oczekiwanie na odczyt", dopóki firmware nie zacznie wysyłać
   `rtc_ok`/`dzien`/`miesiac`/`rok` w `zbudujJsonStatusPelny()`.
3. Czy 0.31.63 zbudowało się i czy panel nie wraca na pulpit.
