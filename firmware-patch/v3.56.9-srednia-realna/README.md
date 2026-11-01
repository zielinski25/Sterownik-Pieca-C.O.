# Firmware v3.56.9 — „Ogrz. śr." przestaje kłamać po restarcie

Data: 2026-10-30
Baza: v3.56.8
Plik do podmiany: **`main_centrala.cpp`** (jeden plik)

---

## 1. Co to naprawia

Po **każdym** restarcie centrali kanał **„Ogrz. śr."** pokazywał **dokładnie
20,0 °C** przez 15–30 minut, niezależnie od realnej temperatury pieca.

Zmierzone na logu z 12 dni — **19 takich okien**:

| kiedy | ile trwało | realna temp. pieca | pokazane | błąd |
|---|---|---|---|---|
| 29.10 14:01–14:11 | 15 min | 12,0 °C | 20,0 °C | +8 K |
| 29.10 11:34–11:44 | 15 min | 12,2 °C | 20,0 °C | +7,8 K |
| 24.10 13:59–14:09 | 15 min | 11,5 °C | 20,0 °C | +8,5 K |
| **19.10 20:54–21:04** | 15 min | **52,0–53,0 °C** | **20,0 °C** | **−32 K** |
| 20.10 23:07–23:17 | 15 min | 49,2–50,8 °C | 20,0 °C | −30 K |
| 21.10 00:30–00:40 | 15 min | 37,5–38,8 °C | 20,0 °C | −17 K |
| 20.10 02:56–03:26 | 30 min | 27,5–28,5 °C | 20,0 °C | −8 K |

W sezonie grzewczym, przy piecu 52 °C, wykres pokazywał 20 °C przez kwadrans.
I to **wyglądało jak pomiar**, bo nie było dziurą — było linią.

Dodatkowo: próbka w tym czasie była odrzucana jako „stale", więc **linia
urywała się dokładnie w chwili restartu** — czyli tam, gdzie najbardziej to
widać. Potwierdzone na Twoim zrzucie z 29.10: załamania o **11:31:02**
i **11:55:41** to dwa restarty (`boot=3→4` i `4→5`), a nie zakłócenia pomiaru.

## 2. Dlaczego tak było

```cpp
void obliczanie_srednia() {
  static int ostatni_poprawny = 20;   // <- stąd to 20,0
```

Przez pierwsze `Czas_OFF` minut (domyślnie 20) po starcie piec **nie jest
jeszcze uznany za wygasły**, a oba bufory uśredniające są puste — same zera.
Wyliczona średnia wychodzi 0, zostaje odrzucona (bo różni się od odczytu
o więcej niż 6 K), więc na kanał trafia właśnie ta stała startowa: **20**.

## 3. Co zmieniłem (dwie rzeczy, obie małe)

**1) Ziarno z realnego odczytu.** Zamiast zaczynać od 20, przy pierwszym
poprawnym odczycie czujnika podstawiamy jego wartość:

```cpp
static bool ostatniPoprawnyZeZrodla = false;
if (!ostatniPoprawnyZeZrodla && temperaturaOgrzewania > 0) {
  ostatni_poprawny = (int)lroundf(temperaturaOgrzewania);
  ostatniPoprawnyZeZrodla = true;
}
```

**2) Publikacja, gdy pompa stoi, a wartość jest realna.** Warunek „pompa stoi"
jest celowy — przy pracującej pompie **wszystko zostaje po staremu**:

```cpp
if (!telemetryOgrzSrAccepted && !pompaWlSrednia && temperaturaOgrzewania > 0 &&
    fabsf(temperaturaOgrzewaniaSrednia - temperaturaOgrzewania) <= 6.0f) {
  telemetryOgrzSrAccepted = true;
}
```

## 4. Dowód, że działa — nie z teorii, z uruchomienia

Wyciąłem **tę samą funkcję** z 3.56.8 i 3.56.9, dołożyłem atrapę sprzętu
i uruchomiłem na komputerze. Scenariusze:

**Restart, piec 52 °C** (ten, który dawał −32 K):

| t | 3.56.8 pokazuje | 3.56.9 pokazuje | próbka do chmury |
|---|---|---|---|
| 0 s | 20,0 | **52,0** | 3.56.8: odrzucona → 3.56.9: **wysłana** |
| 60 s | 20,0 | **52,0** | odrzucona → **wysłana** |
| 600 s | 20,0 | **52,0** | odrzucona → **wysłana** |
| 1200 s | 52,0 | 52,0 | wysłana / wysłana |

**Restart, piec 12 °C** (Twój dzisiejszy stan): 3.56.8 → 20,0 przez 20 minut;
3.56.9 → 12,0 od pierwszej sekundy.

**Pompa pracuje, czujnik skacze 60 → 85 °C:** wynik **identyczny w obu
wersjach**, co do wiersza. Czyli poprawka niczego nie zmienia w normalnej
pracy — czego pilnowałem.

## 5. Wersja podbita?

**TAK.** `v3.56.8` → `v3.56.9`. Bez tego OTA odrzuci plik jako starszy.

## 6. Czego ta wersja NIE zmienia

- Progi, wygładzanie, tryby Wybor 1/2/3, bufory — bez zmian
- Zapis `tempSrednia` przy zboczu OFF→ON — bez zmian
- Telemetria, detektor luk, format JSON, trasy HTTP — bez zmian
- 14 plików `.h` — **bez zmian** (nie ruszaj ich)
- Żadnej nowej logiki czasowej — świadomie, żeby nie trzeba było testować
  kolejnej doby

Ryzyko: **małe**. Dwie zmiany, obie w jednej funkcji.

## 7. Uwaga o znanym, NIEnaprawionym

W tej samej funkcji, przy **pracującej pompie**, odczyt skokowy (np. 85 °C
z DS18B20) nadal przechodzi do średniej — tak było przed 3.56.8 i tak zostało.
Nie ruszałem tego, bo to inna sprawa, a Ty prosiłeś o wariant minimalny.
W praktyce DS18B20 ma własny filtr „spurious 85 °C" (widać go w logu:
`odczyt odrzucony jako power-on-reset`), więc do funkcji raczej nie trafia.

## 8. Jak sprawdzić po wgraniu — bez palenia w piecu

Piec stoi na 12 °C, więc test jest natychmiastowy i nic nie kosztuje:

1. `/version` → musi być `v3.56.9`
2. Restart centrali (albo poczekaj na najbliższy)
3. W aplikacji: Dashboard → **„Ogrz. śr."** — ma pokazać **~12 °C**,
   a nie 20,0
4. Wykresy → 6 H → „Ogrz. śr." — linia ma być **ciągła przez moment restartu**,
   bez załamania

## 9. Zawartość paczki

```
main_centrala.cpp                              <- TEN plik podmieniasz
main_centrala-v3.56.8-to-v3.56.9.patch         <- to samo jako łatka
README.md                                      <- ten plik
CZYTAJ-NAJPIERW.txt                            <- instrukcja krok po kroku
SHA256SUMS.txt                                 <- sumy kontrolne
*.h (14 plików)                                <- BEZ ZMIAN wobec 3.56.8
secrets.h.example                              <- wzór, NIE nadpisuj swojego
```
