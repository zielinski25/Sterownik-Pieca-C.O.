# 242 — 0.31.59: „oczekuję na świeży odczyt" już nie wisi (serwo)

Data: 2026-10-30. Zgłoszenie ze zrzutu 12:54: arkusz **Serwo**, komunikat
„Sterownik przyjął tryb — oczekuję na świeży odczyt." wisiał na ekranie.
Reszta działała (serwo się przełączało).

Aplikacja: **0.31.59** (`versionCode 31059`). Firmware: **bez zmian** (3.57.1).

## Co było nie tak (kod, nie domysły)

`AppModel.setServoMode()` czekał w pętli **do 30 s** na warunek:

```
servoModePresentation().awaitingStatus == false
```

a `awaitingStatus` znaczy dosłownie: *„tryb potwierdzony przez ACK jest różny od
trybu raportowanego przez status"* (`ModeSelectionUi.kt`). Trzy skutki:

1. **Centrala ma własne zdanie** — jeśli po komendzie raportuje inny tryb
   (np. bezpieczny, albo AUTO po wygaśnięciu override), warunek **nigdy** nie
   jest spełniony → 30 s czekania, potem komunikat ostrzegawczy.
2. **Status bez pola `tryb_serwa`** (np. chwilowo `0` poza zakresem 1–3 — parser
   w `PiecState` wtedy zeruje i usuwa pole) → `reportedMode = null` → też nigdy.
3. **Kolejne kliknięcie** zmienia `servoModeRequestId`; stara pętla wychodzi
   przez `return@launch` **nie zdejmując toastu „wait"** — a toasty `wait`
   (inaczej niż `ok`/`warn`) **nie mają automatycznego wygaszania**. Efekt:
   komunikat zostaje na ekranie na zawsze, do następnego toastu.

Do tego arkusz Serwo pokazywał własną notkę `awaitingStatus` — także na 30 s.

## Co zmienione

| element | było | jest |
|---|---|---|
| warunek wyjścia z pętli | „status podał dokładnie żądany tryb" | **„przyszedł świeży odczyt"**: tryb zmienił się względem chwili ACK **lub** równa się żądanemu |
| limit czasu | 30 s | **10 s** |
| sprzątanie toastu | tylko w 1 z 4 ścieżek | **każde wyjście** zdejmuje swój toast (`showToastTracked` + `clearToast`) |
| komunikat po czasie | jedno zdanie o porażce | zależy od prawdy: `Status centrali potwierdził tryb: X` / `Centrala raportuje tryb X, nie Y — pokazuję stan z centrali` / `Świeży status nie zawiera trybu serwa` / `Brak świeżego statusu po 10 s` |
| notka w arkuszu | 30 s „oczekuję" | znika razem z override (≤10 s) |

Nowa stała: `SERVO_MODE_STATUS_WAIT_MS = 10_000L`.

## Czego to nie zmienia

- Sama komenda i ACK: bez zmian (`ustaw trybSerwa N`).
- Szybkość potwierdzeń: bez zmian (heartbeat + firmware 3.57.1).
- Polling statusu: **8 s** (0.31.58), heartbeat: co 50 s, gaśnie po 10 min ciszy.
- Żadnych zmian w firmware.

## Jak sprawdzić

1. Ustawienia → **`v0.31.59`**.
2. Otwórz Serwo, wybierz tryb. Po ACK pojawi się krótkie „czekam na najbliższy
   odczyt statusu", a potem **w maks. 10 s** jeden z czterech komunikatów wyżej.
3. Wybierz tryb **dwa razy pod rząd** — pierwszy komunikat musi zniknąć
   (wcześniej zostawał na zawsze).
4. Gdy centrala ma inne zdanie, zobaczysz to wprost: `Centrala raportuje tryb
   AUTO, nie RĘCZNY` — to informacja, nie błąd aplikacji.

## Paczka

| co | rozmiar | SHA-256 | Dysk |
|---|---|---|---|
| Aplikacja 0.31.59 (ZIP, 144 pliki) | 1 170 729 B | `7f67d5bee0aa673e3be86bc851003ee1dfb3405a88b13108f72b4620bc595fe2` | `136-VWsV9vTNRzPTaI7L48qQ_85xflm7K` |

W repo: `android-sync/v0.31.59-serwo-bez-wiszenia/`.

## Uwaga diagnostyczna

Jeśli po tej wersji zobaczysz **`Centrala raportuje tryb X, nie Y`**, to znaczy,
że centrala **odrzuca** żądany tryb (ma własną logikę, np. przejście w bezpieczny).
Wtedy warto przysłać: tryb wybrany, tryb pokazany i log centrali z linią
`tag=FB-CMD … cmd=ustaw trybSerwa N` oraz `tag=FB lvl=INFO akcja=serwo`.
