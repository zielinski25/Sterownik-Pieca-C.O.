# Wgranie v3.56.5 zdalnie przez Telegram — procedura krok po kroku

Stan wyjściowy potwierdzony w kodzie i w repozytorium (2026-10-28):

* piec pracuje na **v3.55.1** (`/version`),
* repo `zielinski25/Sterownik-Pieca-C.O.` jest **publiczne** — `/update` pobierze plik bez logowania,
* najnowsze wydanie to **`3.55.0`** z załącznikiem `firmware.bin` (2 436 032 B) — ta droga była już użyta,
* tagi wydań są **bez literki `v`** (`3.55.0`, `3.54.0`, …). Nowy tag: **`3.56.5`**,
* `porownajWersje("3.56.5", "v3.55.1")` = **+1** (56 > 55) → firmware uzna to za aktualizację,
* oba pliki CSV kończą się `\n` → kontrola startu `Reason::Boundary` przejdzie,
* partycje: `app0` 3 MB, `app1` 3 MB — nowy obraz (~2,4 MB) się mieści.

## Krok 0 — warunek wstępny

`secrets.h` musi mieć prawdziwe wartości (nie `ZMIEN-MNIE-*`), inaczej po wgraniu
aplikacja straci sterowanie.

```powershell
powershell -ExecutionPolicy Bypass -File "$env:TEMP\v3565\SPRAWDZ.ps1"
```

Oczekiwane: `WYNIK: wszystko gotowe`. Jeżeli folder `%TEMP%\v3565` zniknął —
rozpakuj ponownie `PROJEKT-v3.56.5-POPRAWIONY.zip` z Pobranych.

## Krok 1 — build (około 2,5 minuty)

```powershell
cd "C:\Users\Rafcio\Documents\PlatformIO\sterownik-co-esp32"
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```

## Krok 2 — sprawdzenie pliku wynikowego

```powershell
$bin = "C:\Users\Rafcio\Documents\PlatformIO\sterownik-co-esp32\.pio\build\esp32-s3-n16r8\firmware.bin"
if (-not (Test-Path $bin)) {
  Write-Host "BRAK firmware.bin - build sie nie udal, nie rob nic dalej" -ForegroundColor Red
} else {
  $f = Get-Item $bin
  Write-Host ("rozmiar : {0} B ({1:N0} KB)" -f $f.Length, ($f.Length / 1KB)) -ForegroundColor Green
  Write-Host ("zapisany: {0}" -f $f.LastWriteTime)
  Write-Host ("sha256  : {0}" -f (Get-FileHash $bin -Algorithm SHA256).Hash)
  if ($f.Length -gt 3145728) { Write-Host "ZA DUZY na partycje app0 (3 MB)" -ForegroundColor Red }
}
```

Orientacyjnie: poprzednie wydanie miało 2 436 032 B. Około 2,4–2,6 MB = dobrze.

## Krok 3 — wydanie na GitHubie

**Nazwa załącznika musi zostać dokładnie `firmware.bin`.** Firmware buduje adres
`.../releases/download/<tag>/firmware.bin` — każda inna nazwa to 404.

### Wariant A — agent publikuje (wygodniejszy)

1. Wejdź na https://drive.google.com i wgraj `firmware.bin` (nie zmieniaj nazwy).
2. Napisz w czacie: „wgrałem, publikuj".
3. Agent tworzy wydanie `3.56.5` z załącznikiem i podaje link do sprawdzenia.

### Wariant B — sam, w przeglądarce

1. Otwórz https://github.com/zielinski25/Sterownik-Pieca-C.O./releases/new
2. **Choose a tag** → wpisz `3.56.5` → kliknij **Create new tag: 3.56.5 on publish**
3. **Release title**: `3.56.5`
4. Przeciągnij `firmware.bin` w pole „Attach binaries"
5. **Nie** zaznaczaj „Set as a pre-release" ani „Save as draft"
6. Kliknij **Publish release**

### Wariant C — masz zainstalowane `gh`

```powershell
gh release create 3.56.5 "$env:USERPROFILE\Documents\PlatformIO\sterownik-co-esp32\.pio\build\esp32-s3-n16r8\firmware.bin" --repo zielinski25/Sterownik-Pieca-C.O. --title "3.56.5" --notes "Panel: przyciski pobrania historii CSV"
```

## Krok 4 — Telegram

Wyślij do bota pieca:

```
/update
```

Spodziewana sekwencja wiadomości:

1. `Sprawdzam najnowsza wersje na GitHub...`
2. `Nowa wersja: 3.56.5 (obecna: v3.55.1). Pobieram i flashuje...`
3. `Aktualizacja pobrana OK. Restart za chwile...`
4. po restarcie: komunikat potwierdzenia partycji

Cały proces: 1–3 minuty (pobranie 2,4 MB po WiFi + zapis flash).

**Warunki, w których firmware odmówi** (i dobrze): aktywny alarm
(przegrzanie/dym/pożar) albo inna aktualizacja w trakcie. Wtedy przyjdzie
stosowny komunikat — odczekać i powtórzyć.

## Krok 5 — weryfikacja

```
/version
```

Musi być **`Obecna wersja: v3.56.5`**.

```
/historia
```

Muszą przyjść 3 dokumenty: `hist_short.csv`, `hist_long.csv`, `log_ustawien.txt`.
Jeśli historia przeżyła wgranie, pliki będą zawierać dane sprzed aktualizacji
plus nowsze wiersze.

## Krok 6 — jeśli coś pójdzie nie tak

| objaw | znaczenie | co robić |
|---|---|---|
| `Aktualizacja nieudana: Blad OTA (...)` | pobranie się nie udało, **flash się nie zaczął** | piec dalej na v3.55.1, sprawdzić czy wydanie jest opublikowane i czy asset nazywa się `firmware.bin` |
| brak jakiejkolwiek wiadomości po 5 min | mógł zadziałać rollback | `/version` — jeśli `v3.55.1`, wrócił do starej wersji, nic nie zginęło |
| `/version` pokazuje `v3.55.1` po restarcie | rollback zadziałał | piec żyje, historia na miejscu; wgrać przy następnej wizycie |
| piec nie odpowiada wcale dłużej niż 15 min | wymaga kogoś przy urządzeniu | plan B: ojciec podłącza USB, `pio run -t upload --upload-port COM5` z v3.56.5 albo z powrotem v3.55.1 |

**Bezpiecznik:** `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` + `verifyRollbackLater()`
— nowy obraz pozostaje w stanie `PENDING_VERIFY` do potwierdzenia w `setup()`.
Jeśli nie wstanie, bootloader wraca do starego. Chroni przed cegłą, **nie chroni**
przed firmware, który wstaje i działa źle.

## Czego ta procedura nie sprawdzi

Trasa `/history` i przyciski „Krótka (24 h)" / „Długa (archiwum)" w panelu
działają wyłącznie w sieci domowej. Zdalnie **nie da się** ich przetestować —
zostają do sprawdzenia na miejscu.
