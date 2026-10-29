# 0.31.53 — „zbiornik", nie „panel"

Paczka nakładkowa: **13 plików**. Data 2026-10-29.
**Zawiera całą 0.31.52** — jeśli jej nie wgrałeś, nie musisz. Ta jedna wystarcza.

## Dwie rzeczy naraz

**1. Model z niebem (to samo, co 0.31.52).** Człon `skyRadWk = 8,20 W/K`
przy niezmienionym `tankUa = 1,4`, `reverseFlowKgS` wyzerowany.
Test na prawdziwym kodzie, 11 nocy 18–28.10: błąd **2,07 → 0,88 °C (58%)**.

**2. Nazwy.** Czujnik `t_panel` mierzy **wodę w zbiorniku 100 l na dachu**,
a ekran nazywał go „kolektorem glikolowym". Glikolu w instalacji nie ma,
drugiego obiegu też nie. Poprawione w 12 plikach — szczegóły w
`README-WGRANIE.txt` i w temacie 228.

## Dowód, że to zbiornik, a nie kolektor

Z logu centrali, 10 dni, pole `panel=`:

| dzień | zbiornik min/max | powietrze |
|---|---|---|
| 21.10 | 19,8 / **39,2** | 10,5 / 20,5 |
| 23.10 | 25,8 / **47,2** | 8,5 / 18,0 |
| 29.10 | 16,5 / 18,5 | 11,5 / 14,5 |

Rura próżniowa bez odbioru ciepła ma 80–100 °C, nie 47. I nie stygnie przez
11 godzin w tempie 1,3 K/h jak woda w 100 litrach (noc 23/24.10).

## Znaleziony przy okazji błąd

`ui/screens/Dashboard.kt` — przy alarmie przegrzania panelu baner pokazywał
temperaturę **bojlera pieca** (`t_bojler`) zamiast zbiornika. Poprawione.

## Czego zmiana NIE rusza

- **Klucze w bazie** (`panel`, `t_panel`) — bez zmian, historia i wykresy zostają.
- **Firmware** — bez zmian, 3.56.7 zostaje, 3.56.8 czeka.
- **Zakresy walidacji** czujnika w modelu — bez zmian (tylko pasek kafelka 0–100 °C).
- **Reszta aplikacji** — bez zmian.

## Sprawdzenie paczki

- ZIP: **96 295 B**
- SHA-256: `d06b163449d994ab1c59bf4e5e8ba77a1a1fc00838c0d8afa002a30fe24d81cd`
- 13 plików, `unzip -t` bez błędów
- 13/13 plików w ZIP ma **identyczny** SHA-256 jak pliki w repozytorium
- bilans nawiasów w każdym zmienionym pliku identyczny jak przed zmianą
  (zmieniano tylko literały tekstowe i jedną linię `val`)

## Jak sprawdzić po wgraniu

1. Ustawienia → na dole „v0.31.53".
2. Kafelek na pulpicie nazywa się **Zbiornik** (był „Panel słon.").
3. Arkusz po kliknięciu: **„Zbiornik solarny 100 l (na dachu)"**.
4. Wykresy: legenda **Zbiornik**.
5. Widget: **ZB. SOLARNY**, skrót **SOLAR**.
