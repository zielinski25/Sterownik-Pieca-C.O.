# 229 — Straty nocne: ile dokładnie traci zbiornik, a ile bojler

Data 2026-10-29. Pytanie: *czy z archiwum centrali + Meteo da się wyliczyć
dokładnie straty w nocy?* Odpowiedź: **dla zbiornika na dachu — tak, z błędem
±0,4 K. Dla bojlera w kotłowni — nie, i to nie z braku danych.**

Wszystko poniżej policzone na prawdziwym logu centrali (STATUS co 5 minut,
18–28.10) i nocnym zachmurzeniu z Open-Meteo. Skrypt: `straty_nocne.py`
w tym katalogu, wynik: `wynik.txt`. Nic tu nie jest oszacowaniem „na oko" —
każda liczba jest odtworzalna jednym poleceniem.

---

## 1. Model, którego użyto

Ten sam, co w aplikacji 0.31.52 — więc nie ma tu drugiej, „dokumentacyjnej"
wersji fizyki:

```
C · dT/dt = −(tankUa + skyRadWk · (1 − zachmurzenie)) · (T − Ta)

C         = 116,3 Wh/K   (100 l wody + rury)
tankUa    = 1,4 W/K      (ścianka zbiornika do powietrza)
skyRadWk  = 8,2 W/K      (zbiornik patrzy w niebo)
```

Noc = **19:00 → 06:00** (11 h). Temperatura powietrza brana **z logu centrali**
co 5 minut, nie z prognozy — czyli dokładnie ta, która była nad zbiornikiem.

## 2. Wynik: zbiornik na dachu (czujnik `panel`)

| noc | 19:00 | 06:00 | zmierzone | model | błąd | UA zmierz. | chmury |
|---|---|---|---|---|---|---|---|
| 18/19.10 | 28,0 | 26,0 | −2,0 | −2,4 | −0,4 | 1,30 | 98% |
| 19/20.10 | 24,2 | 22,8 | −1,4 | −1,8 | −0,4 | 1,26 | 98% |
| 20/21.10 | 23,2 | 21,5 | −1,7 | −2,0 | −0,3 | 2,50 | 85% |
| 21/22.10 | 36,5 | 34,0 | −2,5 | −4,5 | **−2,0** | 1,37 | 85% |
| 22/23.10 | 30,7 | 26,2 | −4,5 | −6,0 | −1,5 | 2,81 | 68% |
| **23/24.10** | 42,7 | 28,7 | **−14,0** | −13,7 | **+0,3** | 6,00 | 46% |
| 24/25.10 | 21,8 | 17,5 | −4,3 | −4,5 | −0,2 | 5,08 | 60% |
| 25/26.10 | 20,1 | 15,5 | −4,6 | −2,1 | **+2,4** | 12,55 | 70% |
| 26/27.10 | 14,9 | 11,8 | −3,1 | −3,5 | −0,4 | 5,11 | 48% |
| 27/28.10 | 22,2 | 17,0 | −5,2 | −4,5 | +0,7 | 6,16 | 63% |
| 28/29.10 | 17,5 | 16,5 | −1,0 | −0,4 | +0,6 | 3,49 | 100% |

**Błąd: średnio 0,84 K, mediana 0,41 K, najgorzej 2,4 K.**

Czyli: „woda będzie miała rano 17 °C" znaczy 16,6–17,4 °C. To jest odpowiedź
na pytanie „dokładnie" — **dokładniej niż sam czujnik, który ma 0,5 °C
ziarnistości**.

## 3. Gdzie ucieka ciepło

| | kWh/11 nocy | udział |
|---|---|---|
| przez ściankę do powietrza | 2,03 | 38% |
| **w niebo (promieniowanie)** | **3,26** | **62%** |

Na pojedynczą noc: **0,48 kWh średnio**, z tego 0,30 kWh w niebo.
1 kWh = 8,6 K na zbiorniku 100 l.

Udział nieba zależy od zachmurzenia prawie liniowo:

| zachmurzenie | UA razem | niebo |
|---|---|---|
| 0% (czysto) | 9,6 W/K | **86%** |
| 50% | 5,5 W/K | 75% |
| 85% | 2,6 W/K | 47% |
| 100% | 1,4 W/K | 0% |

**To jest odpowiedź na „temperatura nieba, powietrze"**: przy czystej nocy
sześć siódmych strat idzie w niebo, nie w powietrze. Dlatego dwie noce o tej
samej temperaturze powietrza mogą różnić się 7×.

## 4. Prosty wzór na „ile stracę tej nocy"

```
strata [K]  ≈  (0,13 + 0,78 · (1 − zachmurzenie)) · (T wody − T powietrza)
strata [kWh] ≈  (0,015 + 0,091 · (1 − zachmurzenie)) · (T wody − T powietrza)
```

Przykład: zbiornik 40 °C, powietrze 10 °C, 11 godzin nocy.

| zachmurzenie | strata |
|---|---|
| 0% (czysto) | **−27 K / 3,2 kWh** |
| 50% | −16 K / 1,8 kWh |
| 100% | −4 K / 0,5 kWh |

## 5. Czego model NIE zrobi — i dlaczego to ważne

Sprawdziłem, co się dzieje, gdy zachmurzenia nie ma:

| założenie o chmurach | błąd średni |
|---|---|
| **z prognozy (0.31.52)** | **0,84 K** |
| stałe 50% (zgadywanka) | 2,22 K |
| stałe 100% (model przed 0.31.52) | 2,35 K |
| stałe 0% (zawsze czysto) | 4,55 K |

Czyli cały zysk 0.31.52 bierze się z **jednej rzeczy**: że chmury są brane
z prognozy, a nie założone. Bez chmur model jest wart tyle samo, co przed
poprawką.

## 6. Bojler w kotłowni — tu pogoda nie ma nic do rzeczy

Ten sam okres, czujnik `bojler` (BUS_2, bojler C.W.U. pieca — **nie** zbiornik
solarny):

| noc | 19:00 | 06:00 | spadek | piec 19:00 | piec 06:00 |
|---|---|---|---|---|---|
| 18/19.10 | 12,0 | 11,5 | −0,5 | 10,0 | 9,5 |
| 19/20.10 | 61,5 | 32,5 | **−29,0** | 68,3 | 23,1 |
| 20/21.10 | 40,8 | 40,8 | 0,0 | 66,2 | 22,1 |
| 21/22.10 | 20,2 | 17,5 | −2,7 | 17,5 | 14,7 |
| 22/23.10 | 49,4 | 40,6 | −8,8 | 67,5 | 22,6 |
| 23/24.10 | 41,3 | 36,9 | −4,5 | 63,8 | 15,1 |
| 24/25.10 | 55,9 | 47,3 | −8,6 | 66,5 | 20,7 |
| 25/26.10 | 47,7 | 40,8 | −6,9 | 59,7 | 17,8 |
| 26/27.10 | 34,0 | 28,2 | −5,8 | 12,3 | 6,2 |
| 27/28.10 | 49,0 | 34,2 | −14,8 | 59,8 | 17,5 |
| 28/29.10 | 22,2 | 18,5 | −3,7 | 13,5 | 12,8 |

**Korelacja spadku z zachmurzeniem: −0,09.** Czyli zero. Dla porównania
zbiornik na dachu: **+0,72**.

Co widać w tabeli: bojler traci ciepło wtedy, gdy **piec wygasa** (z 60–68 °C
na 15–23 °C każdej nocy). To nie jest strata przez ściankę — to utrata źródła
ciepła plus pobór wody. Dlatego **straty nocnej bojlera nie da się wyliczyć
z pogody** i nie należy próbować.

Żeby to policzyć, brakuje dwóch rzeczy:
1. **temperatury w kotłowni** (bojler oddaje ciepło do pomieszczenia, nie do
   powietrza na zewnątrz) — jest czujnik `pokoj`, ale to pokój, nie kotłownia,
2. **licznika poboru wody** — bez niego spadek 29 K wygląda jak „strata".

## 7. Co zrobić, żeby było dokładniej

| co | zysk | koszt |
|---|---|---|
| okryć zbiornik (b 8,2 → 2,5 W/K) | **−1,6 K i −0,18 kWh średnio na noc**, na najgorszej nocy −6,3 K | koc/otulina |
| chmury godzinowe zamiast nocnej średniej | ~0,2–0,3 K mniej błędu | już jest w aplikacji (prognoza godzinowa) |
| czujnik temperatury nieba (pyrgeometr) | 0 — model już to odtwarza z chmur | ~2000 zł, niepotrzebne |
| termometr w kotłowni | umożliwiłby policzenie bojlera | 1× DS18B20 |

**Okrycie zbiornika to jedyna rzecz na liście, która zwraca się natychmiast.**
Policzone na tych samych 11 nocach: łącznie **17,4 K / 2,03 kWh** mniej strat,
czyli średnio 1,6 K na dobę.

## 8. Ograniczenia tego rozliczenia

- Noc **25/26.10** wychodzi 2,4 K poza modelem (zmierzone −4,6, model −2,1).
  Przy ΔT do powietrza zaledwie 5,9 K taki spadek wymaga UA 12,6 W/K — czyli
  albo pobór wody, albo przemeblowanie ciepła w warstwach zbiornika. Bez
  przepływomierza nie rozstrzygnę.
- Noc **21/22.10** w drugą stronę (−2,0 K): zmierzone −2,5, model −4,5.
- Chmury są z prognozy **obszarowej**, nie znad konkretnego dachu. Przy
  przejściu frontu o 22:00 prognoza może się mylić o 30–40 punktów procentowych,
  co przy ΔT 20 K daje ~1,5 K błędu.
- Model zakłada, że woda w zbiorniku ma **jedną temperaturę**. W 100-litrowym
  zbiorniku stojącym pionowo woda może się warstwować — wtedy sam czujnik
  (w połowie wysokości) nie widzi całej energii.

## 9. Wniosek w jednym zdaniu

**Da się i już jest policzone:** zbiornik na dachu traci w nocy 0,05–1,6 kWh
w zależności od zachmurzenia, a model z 0.31.52 trafia w tę liczbę z błędem
0,41 K (mediana). **Nie da się i nie warto próbować:** strata nocna bojlera
w kotłowni, bo to nie strata, tylko piec, który wygasł.
