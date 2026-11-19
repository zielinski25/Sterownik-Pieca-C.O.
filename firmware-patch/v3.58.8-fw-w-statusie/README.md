# Firmware centrali 3.58.8 — pole `fw` w `/api/status`: panel LCD widzi wersję centrali

Data: 2026-11-19 · następna po **3.58.7** (`firmware-patch/v3.58.7-start-telegram/`)
i zawiera wszystko, co ona. Jeśli 3.58.7 nie była jeszcze kompilowana — wgraj
od razu tę.

Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h`, `secrets.h`,
biblioteki i `platformio.ini` bez zmian. Dwa patche: względem **3.58.7**
(3 hunki, +27/−1) i względem **3.58.6** (9 hunków, +238/−11). Plik jest
kompletny — pasuje na 3.58.3–3.58.7.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Skąd ta wersja

Zgłoszenie z 19.11 po wgraniu panelu v1.4.6: *„łączy się z centralą, ale nie
pokazuje wersji centrali, tylko wersję LCD”*.

Panel czyta wersję z pola `"fw"` w **`/api/status`**. W centrali ten endpoint
odpowiada funkcją `wifiZbudujJsonStatus()` (linia ~30624 w 3.58.6) — i w niej
pola `"fw"` **nigdy nie było**. `"fw"` centrala wystawia w
`zbudujJsonStatusPelny()` (linia ~45161), czyli w JSON-ie do Firebase
(`/piec/status`) i w `/api/lan/status` — nie tam, skąd czyta panel. Opis
w changelogu panelu v1.4.6 („pole `fw` istnieje od dawna”) był błędny. Testy
panelu na PC tego nie wykryły, bo atrapa centrali w symulatorze podawała `fw`
— testowały panel na centrali, której nie ma. Zachowanie panelu przy braku
pola („Centrala -”) było poprawne; na płytce ten „brak” był stanem stałym.

## Co zmienia 3.58.8

Jedna linia w `wifiZbudujJsonStatus()`, zaraz po `"rssi"` (linia 30851):

```cpp
j += F("\"fw\":\""); j += FIRMWARE_VERSION; j += F("\",");
```

Odpowiedź `/api/status` rośnie o 14 B (z ~2 kB). `j.reserve(512)` bez zmiany
(String i tak dorasta — odpowiedź jest 4× większa od rezerwy od dawna).
Panel v1.4.6 **bez zmian**: przy najbliższym statusie (≤ 2 s) pokazuje wersję
na ekranie WYŚWIETLACZ i w logu `tag=PANEL centrala_fw=`, przy następnym
zimnym starcie także na ekranie startowym.

| linie (3.58.8) | co |
|---|---|
| 1–21 | wpis changeloga `[v3.58.8 FW-W-STATUSIE]` |
| 6285 | `FIRMWARE_VERSION = "v3.58.8"` |
| 30847–30851 | komentarz + linia `"fw"` w `wifiZbudujJsonStatus()` |

## Żeby to się nie powtórzyło (po stronie testów panelu, w tym samym PR)

`LCD-Centrali/testy_host/sprawdz_pola_statusu.py` (krok `[1b]` w
`uruchom_testy.sh` panelu): wyciąga nazwy pól JSON z `wifiZbudujJsonStatus()`
i funkcji pomocniczych wołanych z niej z buforem `j` (`panelOtaKlejJson(j)`)
w **najnowszym pliku centrali w repozytorium** i sprawdza, że każde pole
z `diag::POLA_STATUSU_CZYTANE[]` + `CZUJNIKI_SYMULOWANE[]` panelu tam jest.
Jedyny wyjątek, z uzasadnieniem w skrypcie: `symulator` (wysyła tylko
centrala testowa). Na 3.58.6 i 3.58.7 skrypt zgłasza `BRAK W /api/status
CENTRALI: "fw"`, na 3.58.8 przechodzi (80 pól centrali ⊇ 47 pól panelu).
Testy tej paczki uruchamiają go na obu plikach i wymagają, żeby na 3.58.7
**zawiódł** (`[D2]`).

## Co zostało sprawdzone

- `tests/uruchom_testy.sh`: D (pola statusu: 3.58.8 zgodne, 3.58.7 brak `fw`,
  linia jedna i po `"rssi"`) + testy 3.58.7 (40) i 3.58.6 (166 + 2697) na tym
  pliku, także ASan/UBSan; blok OTA panelu bajt w bajt ten sam co kopia
  w testach panelu.
- `tests/sprawdz_naglowki_esp32.sh`: nowa linia (w szkielecie funkcji ze
  `String`, `F()`) i blok START-TG z prawdziwymi nagłówkami arduino-esp32
  3.0.7 i 3.3.8 — 0 błędów, 0 ostrzeżeń.
- Oba patche nakładają się bez offsetów i dają plik o sumie z `SHA256SUMS.txt`.
- Testy panelu LCD (55 scenariuszy) zielone z nową atrapą (`fw = "v3.58.8"`).

**Nie sprawdzone:** kompilacja PlatformIO dla ESP32-S3 i płytka.

## Zawartość paczki

```
CZYTAJ-NAJPIERW.txt
README.md
SHA256SUMS.txt
main_centrala-v3.58.7-do-v3.58.8.patch     3 hunki
main_centrala-v3.58.6-do-v3.58.8.patch     9 hunków (3.58.7 + 3.58.8 razem)
src/main_centrala.cpp                      plik kompletny, v3.58.8
tests/uruchom_testy.sh                     D + C/A/B (skrypty 3.58.7 i 3.58.6)
tests/sprawdz_naglowki_esp32.sh
tests/wytnij_fw_status.py                  wycinek nowej linii dla nagłówków ESP32
tests/wytnij_start_tg.py                   kopia z 3.58.7 (numer wersji 3.58.8)
tests/README.txt
WYNIKI/testy-pc.txt, testy-pc-sanitizer.txt, naglowki-esp32.txt, zakres-zmian.txt
```
