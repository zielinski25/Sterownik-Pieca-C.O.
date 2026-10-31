# 243 — 0.31.60: naprawa kompilacji (0.31.59 się nie zbudowało)

Data: 2026-10-30 · Dotyczy: aplikacja Android · Firmware bez zmian (3.57.1)

## Co się stało

Budowanie paczki **0.31.59** padło u Ciebie na:

```
e: file:///C:/Users/Rafcio/Documents/SterownikCO-0.31.59/app/src/main/java/com/sterownikco/pro/core/AppModel.kt:1023:40
   Unresolved reference 'statusStampBefore'.
FAILURE: Build failed with an exception.
BUILD FAILED in 23s
```

To mój błąd. W poprawce serwa (0.31.59) użyłem zmiennej `statusStampBefore`
do sprawdzenia „czy odczyt statusu jest świeży", ale **jej nie zadeklarowałem**.
Kotlin nie znał takiego symbolu → cały moduł się nie skompilował → żadnej APK
z 0.31.59 nie było. Poprzednie paczki (0.31.58 i starsze) kompilowały się.

## Poprawka (0.31.60 / versionCode 31060)

W `AppModel.kt`, w funkcji `setServoMode`, na początku bloku po wysłaniu komendy:

```kotlin
scope.launch {
    var waitToastId = 0L
    // [0.31.60] Punkt odniesienia dla „świeżości": znacznik ostatniego udanego
    // odczytu statusu SPRZED wysłania komendy.
    val statusStampBefore = lastFetchTs
    servoModeCommandMutex.lock()
    ...
```

Jedna linia deklaracji. Cała reszta logiki z 0.31.59 zostaje bez zmian:

- czekamy na **świeży odczyt** (`lastFetchTs != statusStampBefore`) albo na żądany tryb,
- maks. **10 s** (`SERVO_MODE_STATUS_WAIT_MS`),
- **każde** wyjście zdejmuje toast „wait" (`clearToast(waitToastId)`),
- komunikaty: „Status centrali potwierdził tryb: X", „Centrala raportuje tryb X, nie Y",
  „Świeży status nie zawiera trybu serwa", „Brak świeżego statusu po 10 s".

## Weryfikacja (zrobiona w sandboxie)

| Co | Wynik |
|---|---|
| `versionCode` / `versionName` w ZIP | 31060 / "0.31.60" |
| `statusStampBefore` w ZIP | jest — linia 970 (`val statusStampBefore = lastFetchTs`) |
| `lastFetchTs != statusStampBefore` w ZIP | jest — linia 1027 |
| `unzip -tq` | bez błędów, 189 plików |
| SHA256 ZIP | `adf3dfdcd2a5c92167d1efbdf1ce995c117c9c387800c7954e186e310c4ecb55` |
| Pobranie z Dysku i porównanie SHA | zgodne (1 171 084 B) |
| Zmienne użyte w `setServoMode` | kompilator zgłosił **jeden** błąd (tę jedną zmienną) — reszta się rozwiązywała |

Czego sandbox **nie** zrobi: nie zbuduje APK (brak Android SDK) — dlatego build
zostaje po Twojej stronie, jednym poleceniem.

## Paczka

- ZIP: `SterownikCO-Android-v0.31.60-PELNY-PROJEKT.zip` — 1 171 084 B,
  ID na Dysku `1TQ4znvhej71i9G9Ei_k2o070xIInCPPM`,
  SHA256 `adf3dfdcd2a5c92167d1efbdf1ce995c117c9c387800c7954e186e310c4ecb55`
- 0.31.59 → **NIE BUDOWAĆ** (błąd kompilacji, opisany wyżej)

## Firmware

Bez zmian: **3.57.1** (`1xVg04lM8nHaHHYEAJW4LyjwnDKbr4n8B`, SHA
`a4923803f221d6e27d69a4c842ee9abd47848da71df76dff3decd09433589c57`).
Plik masz już podmieniony w `src` (kopia `.bak-20261030-1243`) — zostaje
tylko **Upload** w PlatformIO.
