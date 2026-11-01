# Potwierdzenie: historia z pieca dotarła przez Telegram (2026-10-28)

## Co zrobił użytkownik

1. `/version` → **`Obecna wersja: v3.55.1`** — potwierdzone, co pracuje na piecu.
2. `/historia` → piec wysłał trzy dokumenty na Telegram.
3. Pliki wgrane na Dysk Google:
   * `hist_short (7).csv` — **107 130 B**, zmodyfikowany 2026-10-28 19:23
   * `hist_long (7).csv` — **233 391 B**, zmodyfikowany 2026-10-28 19:22

To jest pierwszy raz, kiedy dane historii pochodzą z prawdziwego pieca
i przeszły całą drogę: czujniki → bufor RAM → LittleFS → Telegram.

## Format `hist_short.csv` (potwierdzony z pliku)

Bez wiersza nagłówka, 14 kolumn, separator `,`, jedna próbka na minutę:

```
19:00,8.5,48.5,58.7,63.0,51.2,22.5,17.5,0,1022.1, 0,180,90,1793124003
```

| # | kolumna | wartość w pierwszym wierszu | ocena |
|---|---|---|---|
| 1 | etykieta `HH:MM` | `19:00` | czasu lokalnego |
| 2 | t_zewn | 8.5 °C | październik, wieczór — sensowne |
| 3 | t_bojler | 48.5 °C | sensowne |
| 4 | t_ogrz | 58.7 °C | sensowne |
| 5 | t_ogrz_sr | 63.0 °C | sensowne |
| 6 | t_powrot | 51.2 °C | sensowne |
| 7 | t_panel | 22.5 °C | wieczór, solar wystudzony — sensowne |
| 8 | t_pokoj | 17.5 °C | sensowne |
| 9 | pompa | 0 | wyłączona |
| 10 | ciśnienie | 1022.1 hPa | sensowne |
| 11 | wilgotnosc | ` 0` (drugi wiersz: 94) | patrz niżej |
| 12 | klapa_proc | 180 | w stopniach (nazwa historyczna) |
| 13 | syberko_proc | 90 | w stopniach (nazwa historyczna) |
| 14 | czas_unix | 1793124003 | = 2026-10-27 18:00:03 UTC = **19:00:03 czasu lokalnego** |

Zgodność etykiety `19:00` z kolumną 14 potwierdza, że znacznik czasu jest
spójny i lokalny (CET, UTC+1) — to była jedna z rzeczy, których nie dało się
sprawdzić offline.

Zakres: pierwszy wiersz **27.10 19:00**, ostatnie odczytane wiersze ~19:42;
plik ~107 KB przy ~70 B na wiersz to ~1440–1500 próbek, czyli **doba**.
Bufor krótki ma `HISTORIA_MAX_PROBEK = 1440`.

## Format `hist_long.csv`

35 kolumn: 11 kanałów × (średnia, min, max) + etykieta + czas_unix + flaga.
Etykieta ma pełną datę: `24.10 19:06`. Pierwszy `czas_unix` = 1792861618
(= 2026-10-24 17:06:58 UTC = 19:06:58 lokalnie).

Zapis **zdarzeniowy**: kolejne wiersze 19:06, 19:07, 19:08, 19:09, 19:10,
19:11, 19:13, 19:16, 19:18, 19:20 — odstępy rosną, gdy wartości się ustalają
(progi `HIST_PROG_*` + heartbeat). To zachowanie zgodne z projektem
`[HISTORIA-DLUGA-ZDARZENIOWA]`, nie usterka.

Archiwum sięga **24.10 19:06**. Poprzedni plik na Dysku (`hist_long (5).csv`,
1 464 529 B, z 24.10 09:14) jest większy, ale pochodzi sprzed zmiany formatu
i sprzed resetu — zgodne z decyzją użytkownika A („stara historia może
przepasc, nowa ma być zapisywana i pobieralna").

## Drobiazgi znalezione w danych

1. **Brak wiersza nagłówka** w obu plikach. Tak samo zbuduje CSV trasa
   `/history` z v3.56.5 (`historiaZbudujCsv()` nie dopisuje nagłówka).
   Kolejność kolumn zna kod panelu, nie plik. Do rozważenia: dopisać
   nagłówek tylko w warstwie pobrania (`/history`), nie w pliku.
2. **Spacja w kolumnie 11** gdy wilgotność wynosi 0 (`1022.1, 0,180,90`).
   To formatowanie szerokością pola w `historiaLiniaCsv()`, nie błąd danych.
   Kosmetyka — Excel i parser CSV to przyjmą.

## Co to udowadnia, a czego nie

**Udowadnia:** próbkowanie co 60 s, zapis do LittleFS, bufor długi
zdarzeniowy, kolumna `czas_unix`, wysyłka dokumentów przez Telegram
(ten sam mechanizm multipart, na którym opiera się `sendTelegramDocument()`).

**Nie udowadnia:** trasy HTTP `/history` i `/history/long` z v3.56.5 —
tej nie da się sprawdzić spoza sieci domowej. Zostaje do zrobienia na miejscu,
przy okazji wgrania v3.56.5 (albo przez VPN, jeśli kiedyś powstanie).
