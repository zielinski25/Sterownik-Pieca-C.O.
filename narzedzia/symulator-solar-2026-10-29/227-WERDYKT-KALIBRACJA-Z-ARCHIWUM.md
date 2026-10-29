# Temat 227 — kalibracja z archiwum: sprawdzona i ODRZUCONA

Data: 2026-10-29 wieczór.
Podstawa: 11 pełnych nocy 18–28.10 z logu centrali + kod aplikacji 0.31.51.

Ten dokument zastępuje w części dotyczącej modelu to, co napisano w temacie 226.
226 zostaje jako zapis historyczny — opisuje prawdziwy błąd („brak próbek
z dziś"), który został naprawiony w 0.31.35 i już nie występuje.

---

## 1. Co było do zrobienia

Po teście na 11 prawdziwych nocach model mylił się średnio o **2,05 °C**
i mylił się **w obie strony**. Zaproponowałem wtedy dwie drogi:

- (a) zostawić jak jest,
- (b) **uczyć się z własnego archiwum** — aplikacja mierzy rzeczywiste tempo
  stygnięcia z własnych danych i używa zmierzonej wartości zamiast stałej
  `tankUa = 1.4`.

Wybrałeś (b). Zanim cokolwiek wgrałem do kodu, sprawdziłem to na Twoich danych.
**Pomysł nie działa — i mam na to liczby.**

## 2. Pięć metod na tych samych 11 nocach

Każda metoda uczy się na 10 nocach i jest sprawdzana na jedenastej
(tzw. leave-one-out — inaczej „uczenie" zawsze wyjdzie idealnie).

| metoda | co robi | średni błąd |
|---|---|---|
| **A — model fizyczny (to, co masz teraz, 0.31.51)** | równanie zbiornika + pętla | **2,05 °C** |
| B — uczony współczynnik | zastępuje cały model: `spadek = k · ΔT · czas` | 2,89 °C |
| C — persystencja | bierze tempo z pierwszych 2 h nocy | 1,95 °C |
| D — model × mnożnik | mnożnik 0,5–2,0 uczony na pozostałych nocach | 2,28 °C |
| E — model + stała poprawka | stała −1…+1 °C uczona na pozostałych nocach | 2,10 °C |

**Cztery sposoby uczenia, cztery razy gorzej.** Jedyna metoda, która wyszła
o 0,1 °C lepiej (C), przegrywa w prawdziwym użyciu — punkt 3.

## 3. Test praktyczny: „ile zostanie do 24:00"

Sprawdzone o każdej pełnej godzinie 17:00–22:00, dni 18–28.10 — 66 przypadków.
Tak właśnie działa karta „Do końca dnia".

| metoda | średni błąd |
|---|---|
| persystencja (tempo z ostatnich 2 h) | 2,00 °C |
| **model** | **1,29 °C** |

Persystencja przegrywa, bo o 17:00–18:00 **jeszcze grzeje słońce**.
26.10 o 17:00 przewidywała spadek −12,6 °C, a woda straciła 2,6 °C.
Model wie o zachodzie słońca, persystencja nie.

## 4. Dlaczego z tego archiwum nie da się nic nauczyć

Błąd modelu noc po nocy:
−1,4 · −1,1 · 0,0 · −1,4 · +0,9 · **+8,1** · +1,9 · +3,2 · +1,4 · +2,9 · +0,3

Sprawdziłem, czy ten błąd zależy od czegokolwiek, co da się zmierzyć:

| z czym porównane | korelacja |
|---|---|
| temperatura wody o 19:00 | +0,33 |
| temperatura powietrza | −0,31 |
| różnica woda − powietrze | +0,24 |

Wszystko w granicach szumu. **Skoro błąd nie zależy od niczego mierzalnego,
nie ma z czego zrobić poprawki.** Uczenie w takiej sytuacji robi jedną rzecz:
wpisuje na stałe średnią pomyłkę i psuje noce, które wcześniej wychodziły dobrze.

## 5. Cały błąd siedzi w jednej nocy

- mediana błędu: **1,40 °C**
- bez nocy 23/24.10: średnia spada z 2,05 do **1,45 °C**
- ta jedna noc to **36% całego błędu**

Co się tam stało: zbiornik szedł 42,5 → 27,8 °C, równiutko, bez skoków,
bez wykrytego poboru, ~1,5 °C/h przy różnicy do powietrza ok. 30 K.
To daje **UA_eff ≈ 5,8 W/K**, czyli 4× katalog. Na pozostałych nocach
wychodzi 1,2–1,4 W/K, czyli dokładnie katalog.

Przy 100 l ubytek 1,5 °C/h odpowiada **stałemu dopływowi ok. 4,4 l/h**
zimnej wody z sieci. To ta sama liczba, którą niezależnie dała analiza
instalacji bezpośredniej: **stały przepływ 4–9 l/h**.


## 5a. Wariant „z detektorem" — też sprawdzony, też odpada

Naturalna myśl: nie ucz się globalnie, tylko **wykrywaj nietypową stratę**.
Jedź na modelu, a gdy zmierzone tempo stygnięcia jest wyraźnie większe od
modelowego — przełącz się na zmierzone. To by złapało noc 23/24.10.

Sprawdzone dla prognozy o 21:00, 22:00 i 23:00 (do 06:00), próg 1,8×:

| okno | model | model + detektor |
|---|---|---|
| 21:00 | 1,63 °C | 1,62 °C |
| 22:00 | **1,29 °C** | 1,74 °C |
| 23:00 | 1,20 °C | 1,10 °C |

Średnia raz lepsza, raz gorsza — ale nie o średnią tu chodzi:

- 22:00, noc 20/21.10: model **0,45 °C** → z detektorem **4,23 °C**
- 21:00, noc 27/28.10: model **0,98 °C** → z detektorem **4,61 °C**

Detektor myli **przemijający ruch wody wieczorem** (pętla oddaje ciepło po
słonecznym dniu) z trwałą stratą. Zamienia bardzo dobrą prognozę w bardzo złą.
Na 11 nocach średnia tego nie pokaże — ale trafisz na taki dzień i zobaczysz bzdurę.

## 6. Wniosek — i jedna rzecz do naprawy

**Kalibracja z archiwum nie wchodzi do kodu.** Model zostaje taki, jaki jest
w 0.31.51. To nie ostrożność, to wynik pomiaru.

Ale wyszło przy okazji coś ważniejszego: **gdzieś kapie 4–9 litrów na godzinę.**
Kran, mieszacz albo zawór zwrotny. To nie jest błąd w kodzie i żaden kod tego
nie naprawi. Objaw dodatkowy: detektor pobrań wymaga spadku 0,5 °C na 5 minut,
a przy 5–9 l/h spadek wynosi ~0,02 °C na 5 minut — **20× pod progiem**.
Dlatego ekran pokazuje „Pobrania: brak" i jest to poprawne działanie kodu
przy takim wycieku.

## 7. Co w temacie 226 jest już nieaktualne

| miejsce w 226 | status |
|---|---|
| §1–§4 — „brak próbek z dziś" i poprawka 0.31.35 | **aktualne historycznie**, błąd naprawiony |
| §5 — „modelu prognozy świadomie NIE zmieniam" | **nieaktualne** — model przepisano w 0.31.44–0.31.51 |
| §5 — „trzeba porównać z pełną prognozą godzinową" | **zrobione**, patrz 225 i raport kalibracji |
| §6 — wzorzec „Spadek dziś −15,8 °C" | **nieaktualne** — karta DZIŚ pokazuje teraz prognozę do północy |
| tabela w §3 (26.10 / 27.10 / 28.10) | **aktualne**, to prawdziwe liczby ze zbiornika |

## 8. Stan paczek

| element | wersja | status |
|---|---|---|
| firmware centrali | 3.56.7 | wgrane 29.10 |
| firmware — czeka | 3.56.8 (średnia + detektor luki telemetrii) | paczka gotowa, **nie wgrana** |
| aplikacja Android | 0.31.51 (karta DZIŚ po zachodzie) | paczka gotowa, **nie wgrana** |

## 9. Dowód do odtworzenia

Skrypt i wynik są w repozytorium, nie „w głowie":

- `narzedzia/symulator-solar-2026-10-29/kalibracja/porownanie.py`
- `narzedzia/symulator-solar-2026-10-29/kalibracja/WYNIK-POROWNANIE.txt`
- `narzedzia/symulator-solar-2026-10-29/RAPORT-KALIBRACJI-NOCNEJ.md` §„WERDYKT"
- `MODEL_SOLARNY_TRZEMCHA_2026-10-23.md` §19
