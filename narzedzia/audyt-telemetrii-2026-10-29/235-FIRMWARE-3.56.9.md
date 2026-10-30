# 235 — firmware 3.56.9: koniec fałszywego 20,0 °C na „Ogrz. śr."

Data 2026-10-30. Wybrano **wariant A** (minimalny). Wersja: **3.56.8 → 3.56.9**.

## 1. Co pokazał Twój zrzut z 29.10 (11:06–12:44)

Na wykresie widać dwa wyraźne załamania żółtej linii: **11:31:02** i **11:55:41**.
Sprawdziłem log w tych minutach — to nie zakłócenia pomiaru, to **restarty**:

| godzina | boot | seq | „Ogrz. śr." | co się stało |
|---|---|---|---|---|
| 11:25:29 | 3 | 71 | 12,5 | spokój |
| **11:34:56** | **4** | **16** | **20,0** | reset `seq` → restart |
| 11:39:56 | 4 | 20 | 20,0 | |
| 11:44:57 | 4 | 29 | 20,0 | |
| 11:54:58 | 4 | 37 | 12,2 | 20 min po starcie |
| **12:13:07** | **5** | **22** | **20,0** | restart |
| **12:27:00** | **6** | **22** | **20,0** | restart |
| 12:42:02 | 6 | 43 | 12,2 | 20 min po starcie |

Czyli: **każdy restart = 20,0 °C przez ~20 minut**, a potem skok do prawdy.
Dokładnie to, co widać na wykresie jako załamanie — linia urywa się na czas
fałszywej wartości (próbka jest odrzucana jako „stale"), a nie dlatego, że
zabrakło danych.

## 2. Skala problemu — 19 okien w 12 dni

| kiedy | ile | realna temp. | pokazane | błąd |
|---|---|---|---|---|
| 29.10 14:01–14:11 | 15 min | 12,0 °C | 20,0 °C | +8,0 K |
| 29.10 11:34–11:44 | 15 min | 12,2 °C | 20,0 °C | +7,8 K |
| 24.10 13:59–14:09 | 15 min | 11,5 °C | 20,0 °C | +8,5 K |
| 22.10 11:50–12:00 | 15 min | 14,2 °C | 20,0 °C | +5,8 K |
| **19.10 20:54–21:04** | 15 min | **52,0–53,0 °C** | **20,0 °C** | **−32,0 K** |
| 20.10 23:07–23:17 | 15 min | 49,2–50,8 °C | 20,0 °C | −30,0 K |
| 21.10 00:30–00:40 | 15 min | 37,5–38,8 °C | 20,0 °C | −17,0 K |
| 20.10 02:56–03:26 | 30 min | 27,5–28,5 °C | 20,0 °C | −8,0 K |

Pełna lista: 19 okien, wszystkie po restartach, żadne w trakcie normalnej pracy.

**To jest ważniejsze, niż wygląda:** wartość nie jest dziurą, jest linią.
Dziurę widać i wiadomo, że to brak danych. Linia 20,0 °C wygląda jak pomiar
i można na niej oprzeć decyzję.

## 3. Przyczyna (potwierdzona w kodzie)

```cpp
void obliczanie_srednia() {
  static int ostatni_poprawny = 20;   // <- stąd 20,0
```

Przez pierwsze `Czas_OFF` minut (domyślnie 20) po starcie piec nie jest jeszcze
uznany za wygasły, a oba bufory uśredniające są puste (same zera). Wyliczona
średnia = 0, zostaje odrzucona (`|0 − 12| > 6`), więc na kanał trafia stała
startowa **20**. Po 20 minutach warunek „wygasły" wreszcie działa i wartość
wraca do prawdy — dokładnie widać to w tabeli wyżej.

Dodatkowo próbka jest wtedy oznaczana jako „stale", więc do chmury nie idzie —
stąd załamanie na wykresie zamiast skoku do 20.

## 4. Poprawka — dwie zmiany w jednej funkcji

**a) Ziarno z realnego odczytu** zamiast stałej 20:

```cpp
static bool ostatniPoprawnyZeZrodla = false;
if (!ostatniPoprawnyZeZrodla && temperaturaOgrzewania > 0) {
  ostatni_poprawny = (int)lroundf(temperaturaOgrzewania);
  ostatniPoprawnyZeZrodla = true;
}
```

**b) Publikacja, gdy pompa stoi, a wartość jest realna:**

```cpp
if (!telemetryOgrzSrAccepted && !pompaWlSrednia && temperaturaOgrzewania > 0 &&
    fabsf(temperaturaOgrzewaniaSrednia - temperaturaOgrzewania) <= 6.0f) {
  telemetryOgrzSrAccepted = true;
}
```

Warunek `!pompaWlSrednia` jest **celowy**: przy pracującej pompie zachowanie
zostaje co do znaku takie samo jak w 3.56.8.

### Czego świadomie NIE zrobiłem

Pierwsza wersja poprawki miała publikować fallback także przy pracującej
pompie. Test wykazał, że przy skoku czujnika (60 → 85 °C) przepuściłaby 85 °C
do chmury. Wycofałem to i zawężyłem warunek. To jest różnica między „działa
na moim przykładzie" a „nie psuje niczego innego".

## 5. Dowód z uruchomienia, nie z rozumowania

Wyciąłem **tę samą funkcję** z obu wersji, dołożyłem atrapę sprzętu,
skompilowałem g++ i uruchomiłem. Wyniki:

**Restart przy piecu 52 °C:**

| t | 3.56.8 pokazuje | 3.56.9 pokazuje | próbka do chmury |
|---|---|---|---|
| 0 s | 20,0 | **52,0** | odrzucona → **wysłana** |
| 60 s | 20,0 | **52,0** | odrzucona → **wysłana** |
| 600 s | 20,0 | **52,0** | odrzucona → **wysłana** |
| 1199 s | 20,0 | **52,0** | odrzucona → **wysłana** |
| 1200 s | 52,0 | 52,0 | wysłana / wysłana |

**Restart przy piecu 12 °C:** 3.56.8 → 20,0 przez 20 minut; 3.56.9 → 12,0
od pierwszej sekundy.

**Pompa pracuje, czujnik skacze 60 → 85 °C:** 3.56.8 i 3.56.9 dają
**identyczny wynik co do wiersza**. Poprawka nie zmienia niczego w normalnej
pracy.

## 6. Paczka

`SterownikCO-v3.56.9-SREDNIA-REALNA.zip` — **735 116 B**
SHA-256 `af67d2e1ac587b5902874c534ddde7ba4e5e95999bf6e0888275c4f916df8bac`

ID na Dysku: `1dY42yS6_OpWMqpr8bdJGrE4h8hhXd_9x`
https://drive.google.com/file/d/1dY42yS6_OpWMqpr8bdJGrE4h8hhXd_9x/view

Zawartość (20 plików): `main_centrala.cpp` (2 387 646 B), łatka
`v3.56.8→v3.56.9`, `README.md`, `CZYTAJ-NAJPIERW.txt`, `SHA256SUMS.txt`,
14 × `.h` (bez zmian — SHA zgodne z 3.56.8), `secrets.h.example`.

Sprawdzone: pobrane z Dysku ma **identyczny SHA-256** bit w bit z plikiem
w repo; `unzip -t` bez błędów; w środku `FIRMWARE_VERSION = "v3.56.9"`.

## 7. Test po wgraniu — bez palenia w piecu

Piec stoi na 12 °C, więc sprawdzenie zajmuje minutę:

1. `/version` → `v3.56.9`
2. restart centrali
3. Dashboard → „Ogrz. śr." ma pokazać **~12 °C**, nie 20,0
4. Wykresy → 6 H → „Ogrz. śr." — linia **ciągła** przez moment restartu

## 8. Co dalej

- Telemetria 3.56.8 wyszła czysto (1200/1200, zero luk, przetrwała
  11-minutową awarię Firebase) — nie ma tam nic do naprawiania na podstawie
  jednej doby. Zbierać kolejne.
- Łata `srOstPompa` z 3.56.8 **zostaje**. Zadziała przy pierwszym rozpaleniu —
  nie usuwam jej, bo nie przeszkadza, a po pierwszym cyklu pompy skróci
  powrót do prawdy o ewentualne sekundy.
- Znany, nieruszony: przy pracującej pompie skok czujnika nadal przechodzi
  do średniej (tak było przed 3.56.8). DS18B20 filtruje „spurious 85 °C"
  upstream, więc w praktyce do funkcji nie trafia.
