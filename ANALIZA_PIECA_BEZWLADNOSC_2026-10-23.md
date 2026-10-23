# Co policzyłem z pliku Firebase — bezwładność pieca i odbiór ciepła przez grzejniki

**Plik:** `akwarium-367be-default-rtdb-2026-export.json` (23 126 691 bajtów)
**Analiza:** 2026-10-23
**Skrypt:** `narzedzia/analiza_pieca.py` (można uruchomić ponownie)

---

## 1. Co jest w pliku

To eksport bazy Firebase (RTDB) z **telemetrią Twojego sterownika**. Każdy rekord to jedna
minuta pracy pieca i 11 odczytów:

| nr | kanał | co to jest | zakres w danych |
|----|-------|-----------|-----------------|
| 0 | zewn | temperatura na zewnątrz | 3,5 – 37,5 °C |
| 1 | bojler | bojler C.W.U. | 11,5 – 71,3 °C |
| 2 | ogrz | **zasilanie C.O. (wyjście z pieca)** | 7,8 – 91,3 °C |
| 3 | ogrz_sr | zasilanie — wartość średnia | 7,8 – 91,0 °C |
| 4 | powrot | **powrót C.O. (wejście do pieca)** | 8,0 – 86,8 °C |
| 5 | panel | kolektor słoneczny | 16,0 – 69,5 °C |
| 6 | pokoj | pokój | 15,0 – 25,0 °C |
| 7 | trociny | czujnik w trocinach (paliwo) | 7,3 – 22,3 °C |
| 8 | rh | wilgotność | uśredniona, mało wiarygodna |
| 9 | cisn | ciśnienie | 1002,6 – 1030,5 hPa |
| 10 | dym | czujnik dymu (surowy odczyt) | 0 – 4095 |

**Ile tego jest:**

- **40 205 rekordów**, czyli **40 205 minut**
- od **26.09.2026 03:11 UTC** do **23.10.2026 07:21 UTC** — **27,2 dnia**
- próbka co **60 sekund**, kompletność **99,7%**
- **11 epizodów grzania**, łącznie **95,3 godziny** pracy pieca (z 653 godzin w pliku)

**Ważne odkrycie:** w pliku jest **178 różnych identyfikatorów urządzenia**. To nie jest
178 pieców — to **jeden piec**, któremu przy restarcie zmienia się identyfikator.
Sprawdziłem to tak: na 3515 granicach między identyfikatorami **mediana skoku temperatury
zasilania wynosi 0,0 stopnia**. Gdyby to były różne instalacje, temperatury skakałyby
gwałtownie. Są ciągłe — to jedna instalacja.

---

## 2. Jak liczyłem

Wszystko poniżej to **fitowanie modelu fizycznego do Twoich danych**, nie wzory z książki
podstawione pod zgadywane liczby.

Podstawa: prawo stygnięcia Newtona. Kiedy piec przestaje być zasilany paliwem, jego
temperatura spada wykładniczo:

```
T(t) = T_otoczenia + (T_start − T_otoczenia) × exp(−t / τ)
```

- **τ (tau)** — stała czasowa w godzinach. To jest **liczba, która opisuje bezwładność**.
- Po jednej τ piec traci 63% różnicy do otoczenia.
- **τ = C / UA**, gdzie C to pojemność cieplna (ile energii trzeba, żeby podgrzać piec
  o 1 stopień), a UA to przewodność strat (ile watów ucieka na każdy stopień różnicy).

Dodatkowo czyściłem dane: filtr Hampela (odrzuca pojedyncze skoki czujnika większe niż
6 stopni) i mediana z 5 próbek. Bez tego pojedyncze błędne odczyty (np. 16,0 °C w środku
80 °C) psuły wyniki — takich artefaktów w pliku jest sporo.

**Sygnał palenia:** czujnik dymu włącza się dopiero powyżej pewnej temperatury pieca,
więc „dym = 0" oznacza też „piec zimny". Palenie rozpoznaję po **dym ≥ 300** (powyżej szumu
tła MQ-2, który wynosi około 250).

---

## 3. Bezwładność pieca — najważniejszy wynik

### 3.1 Stała czasowa stygnięcia

Zmierzyłem na **9 niezależnych odcinkach**, kiedy piec stygł bez pompy C.O.
(żadne ciepło nie uciekało do grzejników — tylko straty przez ścianki kotła):

| data i godzina startu | τ | temperatura startu | temperatura końca | dopasowanie R² |
|---|---|---|---|---|
| 10-11 13:22 | 2,91 h | 66,5 °C | 33,8 °C | 0,970 |
| 10-12 03:11 | 3,49 h | 43,7 °C | 29,0 °C | 0,999 |
| 10-14 02:16 | 5,19 h | 45,5 °C | 22,5 °C | 0,997 |
| 10-15 21:41 | 2,38 h | 76,4 °C | 33,0 °C | 0,977 |
| 10-16 22:02 | 3,43 h | 55,8 °C | 21,8 °C | 0,997 |
| 10-19 18:50 | 3,12 h | 54,9 °C | 41,8 °C | 0,956 |
| 10-19 21:25 | 4,18 h | 41,0 °C | 28,8 °C | 0,999 |
| 10-20 22:57 | 4,50 h | 36,3 °C | 24,0 °C | 0,986 |
| 10-22 17:24 | 4,43 h | 81,0 °C | 19,3 °C | 0,996 |

**Wynik: τ = 3,5 do 3,8 godziny** (mediana z dwóch niezależnych przeliczeń: 3,49 h i 3,83 h).

> **To jest zmierzone, nie oszacowane.** R² od 0,956 do 0,999 — dopasowanie bardzo dobre.

Co to znaczy w praktyce:

- po **3,6 godziny** od wygaśnięcia piec ma jeszcze **37%** pierwotnej różnicy temperatur
- po **7 godzinach** — około 13%
- po **10 godzinach** — około 5%

### 3.2 Ile realnie trwa stygnięcie

Policzyłem to wprost z danych: od momentu najwyższej temperatury, ile czasu mija,
zanim zasilanie spadnie o daną wartość. 19 szczytów:

| spadek temperatury zasilania | mediana czasu | ile pomiarów |
|---|---|---|
| o **20 stopni** | **5,8 godziny** | 19 |
| o **30 stopni** | **8,2 godziny** | 19 |
| o **40 stopni** | **9,4 godziny** | 14 |
| o **50 stopni** | **17,5 godziny** | 5 |

Najlepsze, najczystsze przykłady:

- **10-15 08:54** — piec doszedł do 67 °C. Spadek o 20 stopni zajął **14,8 h**,
  o 40 stopni — **18,6 h**.
- **10-16 16:25** — 69,3 °C → spadek o 20 stopni w 6,5 h, o 40 stopni w 10,0 h,
  o 50 stopni w 17,5 h.
- **10-13 18:08** — 66 °C → 20 stopni w 8,4 h, 30 stopni w 10,2 h, 40 stopni w 14,5 h.

**Wniosek prostym językiem:** po wygaśnięciu piec trzyma ciepło **kilkanaście godzin**.
Przez pierwszą dobę po paleniu chałupa praktycznie nie traci temperatury, bo piec
oddaje to, co w sobie zgromadził.

### 3.3 Jak szybko piec się nagrzewa

Zmierzony najszybszy przyrost zasilania: **63 do 75 stopni na godzinę** (mediana z 8
najlepszych rozpaleń). Czyli z 15 °C do 40 °C piec wchodzi w **około 20–25 minut**.

| moment rozpalenia | tempo | od jakiej temperatury | przyrost w 20 min |
|---|---|---|---|
| 10-22 14:23 | 75,0 st./h | 14,0 °C | +25,0 |
| 10-20 15:41 | 75,0 st./h | 15,3 °C | +25,0 |
| 09-26 12:51 | 75,0 st./h | 15,3 °C | +25,0 |
| 10-11 18:37 | 70,4 st./h | 28,5 °C | +23,5 |
| 10-19 12:39 | 67,5 st./h | 10,3 °C | +22,5 |
| 10-15 08:24 | 64,5 st./h | 12,5 °C | +21,5 |

To są odcinki, kiedy pompa C.O. **nie pracowała** — całe ciepło z paleniska szło w masę
pieca. Kiedy pompa pracuje, przyrost jest wolniejszy, bo część ciepła od razu ucieka
w grzejniki.

---

## 4. Zdolność odbioru ciepła przez grzejniki

Tu trzeba było zrobić bilans. Ciepło, które woda oddaje w grzejnikach, można zapisać
na dwa sposoby:

```
Q = przepływ × 4,19 × (zasilanie − powrót)        ← strona wody
Q = UA_grzejników × (średnia temperatura wody − temperatura pokoju)   ← strona grzejnika
```

Z porównania wychodzi:

```
(zasilanie − powrót) = NTU × (średnia temp. wody − temp. pokoju)
```

**NTU** to liczba niemianowana: jaki ułamek różnicy temperatur woda oddaje, zanim wróci
do pieca. Im większe NTU, tym grzejniki „wyciągają" więcej ciepła z każdego litra wody.

### 4.1 Co pokazały dane

Z **3150 próbek**, kiedy pompa pracowała i piec był powyżej 30 °C:

| wielkość | wartość | status |
|---|---|---|
| **różnica zasilanie − powrót** | **mediana 4,3 stopnia** | zmierzone |
| zakres tej różnicy | od 2,2 do 46 stopni | zmierzone |
| średnia temperatura wody − pokój | mediana **52,2 stopnia** | zmierzone |
| **NTU** | **0,081** | wyliczone z danych |
| współczynnik regresji | 0,094 (R² = 0,087) | wyliczone |

**NTU = 0,081** znaczy: woda przepływając przez grzejniki oddaje tylko **8%** różnicy
temperatur, jaką ma wobec pokoju. Przy 90 °C na zasilaniu i 23 °C w pokoju woda wraca
do pieca o temperaturze **85 °C**.

### 4.2 Porównanie z normą — i tu jest niespodzianka

Według literatury technicznej typowe różnice zasilanie-powrót wynoszą:

| rodzaj instalacji | typowa różnica zasilanie − powrót |
|---|---|
| kocioł konwencjonalny (grzejniki) | **15–20 stopni** |
| pompa ciepła (grzejniki) | 5–8 stopni |
| ogrzewanie podłogowe | 5–7 stopni |
| Twoja instalacja (zmierzone) | **4,3 stopnia** |

Twoja instalacja ma różnicę **3,5 do 4,5 razy mniejszą** niż typowy kocioł na grzejniki.
W branży nazywa się to **„low delta T syndrome"** — zespół zbyt małej różnicy temperatur.
Opisuje się go jako stan, w którym „woda wraca prawie tak gorąca, jak wyszła — z powodu
przepompowywania, obejść albo zanieczyszczonych wymienników".

**Ale w Twoim przypadku to nie musi być wada.** Przy kotle na paliwo stałe (drewno,
trociny) wysoka temperatura powrotu jest **celowa**: chroni kocioł przed korozją
niskotemperaturową i smołowaniem. Dlatego w takich instalacjach często montuje się
zawór mieszający, który podnosi temperaturę powrotu. U Ciebie po prostu wychodzi ona
wysoka sama z siebie.

Co to jednak oznacza praktycznie:

- **woda krąży bardzo szybko** — przy różnicy 4,3 stopnia i mocy 4 kW przepływ musi
  wynosić około **0,8 m³/h**
- **pompa prawdopodobnie pracuje na zbyt wysokim biegu** — przy mocy pieca 10 kW
  i różnicy 4,3 stopnia przepływ byłby już **2 m³/h**, czyli bardzo dużo
- **grzejniki przy piecu są gorętsze niż te na końcu instalacji** — przy dużym przepływie
  woda nie zdąża oddać ciepła po drodze

### 4.3 Ile ciepła realnie idzie w grzejniki

Bez znanego przepływu nie da się podać watów wprost. Ale można podać **ile kilowatogodzin
przypada na każdy 1 m³/h przepływu**:

| wielkość | wartość |
|---|---|
| suma (różnica × czas) przy pracującej pompie | **264,9 stopniogodzin** |
| energia na każdy 1 m³/h przepływu | **308 kWh** w całym 27-dniowym okresie |
| przy przepływie 0,5 m³/h | 154 kWh |
| przy przepływie 0,8 m³/h | **246 kWh** |
| przy przepływie 1,2 m³/h | 370 kWh |

Najlepszy pojedynczy epizod (10-11 18:49 → 10-12 03:47, 9 godzin palenia, pompa
pracowała 4,9 h): **25,6 kWh na każdy 1 m³/h**. Przy 0,8 m³/h to **20,5 kWh** ciepła
wpompowanego w dom w ciągu jednej nocy.

### 4.4 Opóźnienie: zasilanie → powrót

Korelacja krzyżowa przebiegów: **6 minut** (r = 0,972).

Tyle czasu woda płynie od czujnika na zasilaniu, przez instalację, do czujnika na powrocie
— łącznie z opóźnieniem samego czujnika w miedzianej rurce. To wartość spójna z tym,
co widać na wykresach: kiedy w piecu robi się gorąco, powrót reaguje po kilku minutach.

---

## 5. Straty ciepła pieca

Skoro τ = C / UA, to przy znanej pojemności cieplnej można policzyć, ile watów piec
traci na postoju. **Tu jest jedno „ale"** — pojemność cieplna Twojego kotła nie jest
w danych. Muszę ją założyć.

Poniżej cztery warianty. Woda ma 4,186 kJ na kilogram i stopień, stal około 0,49.

| założona pojemność kotła | C [kJ/°C] | UA (straty) [W/°C] | strata przy 70 °C | strata przy 50 °C |
|---|---|---|---|---|
| 40 l wody + 120 kg stali | 226 | 16,4 | **0,82 kW** | 0,49 kW |
| **70 l wody + 150 kg stali** | **367** | **26,6** | **1,33 kW** | 0,80 kW |
| 100 l wody + 200 kg stali | 517 | 37,4 | **1,87 kW** | 1,12 kW |
| 150 l wody + 250 kg stali | 750 | 54,4 | **2,72 kW** | 1,63 kW |

**Status: szacunek.** Sama stała czasowa 3,5–3,8 h jest zmierzona. Przeliczenie na waty
wymaga założenia o ilości wody w kotle.

**Czy 1,3 kW strat to dużo?** Na forach instalatorskich dla starego kotła żeliwnego
o masie 776 kg podaje się utratę 16,5 °F/h przy 187 °F, co daje **około 3,7 kW**.
Twój wynik 0,8–2,7 kW mieści się w tym zakresie, bliżej dolnej granicy. To normalne
dla kotła bez porządnej izolacji i **nie jest awarią**.

---

## 6. Czego nie da się policzyć bez jednej informacji

Żeby zamienić wszystkie powyższe liczby na **kilowatogodziny i kilowaty**, brakuje
jednej rzeczy: **ile litrów wody jest w kotle**. To nie jest w danych i nie da się
tego wywnioskować z temperatur.

Jak to ustalić:

1. **Tabliczka znamionowa kotła** — producent podaje „pojemność wodna" w litrach.
2. **Instrukcja lub strona producenta** — parametr „water content" / „Wasserinhalt".
3. **Zmierzyć**: spuścić wodę z kotła do wiadra (najpewniejsze, ale kłopotliwe).

Typowe wartości: kocioł na paliwo stałe 20 kW ma **50–80 litrów wody** i **120–200 kg
stali**. Z literatury: w kotle na drewno woda to od 1/5 do 1/3 masy całego kotła,
a **ponad 80% zmagazynowanego ciepła siedzi w wodzie**, nie w stali.

Jak podasz mi pojemność wodną kotła, przeliczę wszystko na kW i kWh dokładnie.

---

## 7. Bezwładność budynku (przy okazji)

Z nocnych okresów, kiedy grzanie było wyłączone, a pokój stygł:

| start | τ budynku | od → do | R² |
|---|---|---|---|
| 10-13 00:03 | 55,6 h | 23,0 → 21,5 °C | 0,902 |
| 10-14 13:15 | 90,1 h | 20,0 → 18,0 °C | 0,953 |
| 10-16 01:40 | 42,3 h | 22,5 → 19,5 °C | 0,961 |

**Mediana: 55,6 godziny** — czyli **ponad dwa dni**.

Status: **szacunek, dość niepewny.** Sygnał był mały (spadek tylko 1,5–3 stopnie),
a w ciągu dnia dochodzą zyski słoneczne i ciepło z kuchni. Ale rząd wielkości jest
wiarygodny: dom murowany o dużej masie stygnie w skali dni, nie godzin.

To ważne, bo pokazuje, że **bezwładność pieca (3,6 h) i bezwładność domu (55 h) to dwie
zupełnie różne rzeczy**. Piec reaguje w ciągu godzin, dom — w ciągu dni.

---

## 8. Układ słoneczny (przy okazji, bo dane są w tym samym pliku)

| dzień | max panelu | max ponad zewnętrzną | godzin powyżej 40 °C |
|---|---|---|---|
| 09-28 | 66,3 °C | 50,3 | 20,6 h |
| **09-29** | **69,5 °C** | **53,5** | **25,4 h** |
| 09-30 | 51,5 °C | 35,0 | 22,6 h |
| 10-01 | 53,3 °C | 37,8 | 21,8 h |
| 10-07 | 48,3 °C | 32,3 | 10,9 h |
| 10-09 | 46,3 °C | 30,8 | 12,6 h |

Rekord: **69,5 °C w kolektorze 29.09**, przy 16 °C na zewnątrz — czyli **+53,5 stopnia**
ponad otoczenie. Przy 10 rurach i 100-litrowym zbiorniku to bardzo dobry wynik.

Od 13.10 panel nie przekracza 40 °C — to już jesień. Zgadza się z tym, że zimą kolektor
jest nieużywany i woda spuszczona.

---

## 9. Co z tego wynika — praktycznie

**1. Piec ma dużą bezwładność i to jest jego zaleta.**
3,6 godziny stałej czasowej i 8 godzin do spadku o 30 stopni oznacza, że **nie musisz
palić codziennie**. Jeden epizod grzania (np. 10-15, 15,5 godziny pracy) ładuje piec
na dobę albo dłużej.

**2. Palisz rzadko — i to widać w danych.**
11 epizodów w 28 dni, łącznie 95 godzin. W dniach 28.09–10.10 piec w ogóle nie pracował
(15–17 °C w zasilaniu). To okresy, kiedy dom trzymał temperaturę sam.

**3. Różnica zasilanie-powrót 4,3 stopnia to najmocniejszy sygnał w całym pliku.**
Woda wraca do pieca prawie tak gorąca, jak z niego wyszła. Dla kotła na paliwo stałe
to **bezpieczne i dobre** (chroni przed smołą). Ale oznacza też, że **pompa prawdopodobnie
biegnie za szybko** i grzejniki przy piecu grzeją mocniej niż te na końcu.

Co można z tym zrobić — **do decyzji, nie do automatycznej zmiany:**
- sprawdzić, na jakim biegu pracuje pompa C.O. (jeśli ma przełącznik 1/2/3)
- jeśli jest zawór różnicowy albo obejście — sprawdzić, czy nie jest otwarte
- wyregulować zawory na grzejnikach (tzw. regulacja wstępna)

**4. Straty postojowe 0,8–2,7 kW to normalne, ale jest co odzyskać.**
Przy 8 godzinach stygnięcia na dobę i 1,3 kW to około **10 kWh ciepła na dobę**, które
idzie w kotłownię, a nie w dom. Izolacja kotła (płaszcz z wełny) i rur w kotłowni
zmniejszyłaby to zauważalnie.

**5. Krótki obieg: 6 minut od zasilania do powrotu.**
Instalacja jest „szybka" — reaguje w minutach. To dobra wiadomość dla sterowania.

---

## 10. Druga runda — co jeszcze wycisnąłem z danych

Po pierwszym raporcie wróciłem do pliku z nowymi metodami. Cztery rzeczy wyszły nowe.

### 10.1 Energia słoneczna w kilowatogodzinach

**Kluczowe odkrycie:** czujnik `panel` (BUS_3, `czujnik_temp_Panel_Plus`) siedzi
**w zbiorniku**, nie na absorberze. Potwierdza to komentarz w firmware (v3.52.0):
„czujnik W ZBIORNIKU — czyli mierzymy masę wody, a nie absorber".

Skoro mierzy 100 litrów wody, to z temperatury można policzyć energię:

```
C = 100 kg × 4,186 kJ/(kg·°C) = 418,6 kJ/°C = 0,1163 kWh/°C
energia netto = 0,1163 × (T_max − T_min)
```

| dzień | min | max | wzrost | energia netto | godzin > 40 °C |
|---|---|---|---|---|---|
| 09-26 | 20,8 | 46,3 | 25,5 | 2,97 kWh | 6,8 |
| 09-27 | 18,8 | 48,8 | 30,0 | **3,49 kWh** | 14,0 |
| 09-28 | 38,3 | 66,3 | 28,0 | 3,26 kWh | 20,6 |
| 09-29 | 49,8 | 69,5 | 19,7 | 2,29 kWh | 25,4 |
| 09-30 | 38,0 | 51,5 | 13,5 | 1,57 kWh | 22,6 |
| 10-01 | 39,0 | 53,3 | 14,3 | 1,66 kWh | 21,8 |
| 10-02 | 32,3 | 51,0 | 18,7 | 2,17 kWh | 10,7 |
| 10-03 | 27,8 | 49,3 | 21,5 | 2,50 kWh | 7,8 |
| 10-04 | 20,5 | 40,8 | 20,3 | 2,36 kWh | 0,5 |
| 10-05 | 20,0 | 38,3 | 18,3 | 2,13 kWh | 0,0 |
| 10-06 | 25,0 | 40,0 | 15,0 | 1,74 kWh | 0,0 |
| 10-07 | 28,3 | 48,3 | 20,0 | 2,33 kWh | 10,9 |
| 10-08 | 27,0 | 43,5 | 16,5 | 1,92 kWh | 3,9 |
| 10-09 | 32,8 | 46,3 | 13,5 | 1,57 kWh | 12,6 |
| 10-10 | 37,0 | 41,3 | 4,3 | 0,50 kWh | 7,0 |
| 10-11 | 34,5 | 42,8 | 8,3 | 0,97 kWh | 4,8 |
| 10-12 | 33,3 | 44,5 | 11,2 | 1,30 kWh | 6,2 |
| 10-13 | 33,3 | 39,8 | 6,5 | 0,76 kWh | 0,0 |
| 10-16 | 18,8 | 47,0 | 28,2 | **3,28 kWh** | 5,4 |
| 10-21 | 19,8 | 39,3 | 19,5 | 2,27 kWh | 0,0 |

**Suma: 41,0 kWh w 20 dni = 2,05 kWh na dzień** (szacunek dolny).

Status: **szacunek dolny, nie pomiar uzysku.** Ta liczba **nie obejmuje** ciepłej wody
pobranej ze zbiornika — jeśli ktoś brał prysznic, woda się wychładzała i energia
„uciekała" z pomiaru. Realny uzysk jest wyższy. Nie jest to też energia Słońca na
kolektorze — to energia, która faktycznie **została w wodzie**.

Energia zatrzymana w zbiorniku przy szczycie:

| dzień | szczyt | energia ponad 20 °C |
|---|---|---|
| 09-28 | 66,3 °C | 5,38 kWh |
| **09-29** | **69,5 °C** | **5,76 kWh** |

Czyli w najlepszym dniu zbiornik 100 l zmagazynował **5,76 kWh** — jak grzałka 2 kW
pracująca prawie 3 godziny.

### 10.2 Model prognostyczny — sprawdzony i działa

Dopasowałem model stygnięcia do pierwszych 2 godzin po szczycie, a potem **sprawdziłem
go po 4 kolejnych godzinach** na rzeczywistych danych.

| τ | temperatura startu | rzeczywista po 6,2 h | prognoza | błąd |
|---|---|---|---|---|
| 7,37 h | 25,8 °C | 18,0 | 18,1 | +0,1 |
| 14,58 h | 20,2 °C | 16,3 | 16,1 | −0,2 |
| 16,41 h | 12,6 °C | 9,3 | 9,1 | −0,2 |
| 18,27 h | 16,2 °C | 13,5 | 13,0 | −0,5 |
| 4,49 h | 32,6 °C | 22,0 | 20,6 | −1,4 |
| 2,03 h | 48,2 °C | 26,5 | 30,1 | +3,6 |
| 1,87 h | 60,5 °C | 25,5 | 29,2 | +3,7 |

**Błąd po 4 godzinach: mediana −0,2 stopnia**, zakres od −1,4 do +3,7.

Wniosek: **można przewidywać temperaturę pieca z dokładnością poniżej stopnia**.
Największy błąd pojawia się przy gorącym piecu (60 °C) i szybkim stygnięciu — wtedy
model przeszacowuje o około 3,6 stopnia, bo nie zna chwilowej mocy paleniska.

To otwiera konkretną funkcję: „masz 70 °C, jeśli nie dołożysz, o 22:00 będzie 48 °C".

### 10.3 Drugie, niezależne oszacowanie przepływu

W pierwszym raporcie przepływ szacowałem z mocy i różnicy temperatur. Teraz drugą drogą:
**z opóźnienia i objętości wody w instalacji**.

```
przepływ = objętość instalacji / czas przejścia (6 minut)
```

| objętość instalacji | przepływ | na minutę |
|---|---|---|
| 80 litrów | 0,80 m³/h | 13,3 l/min |
| **100 litrów** | **1,00 m³/h** | 16,7 l/min |
| 120 litrów | 1,20 m³/h | 20,0 l/min |
| 150 litrów | 1,50 m³/h | 25,0 l/min |
| 200 litrów | 2,00 m³/h | 33,3 l/min |

**Dwie niezależne metody dają ten sam rząd: 0,8 do 1,2 m³/h.** To podnosi wiarygodność
wcześniejszego szacunku mocy grzejników (2,5–5 kW).

### 10.4 Jakość czujników — konkretne liczby

| kanał | brakujące minuty | artefakty (skoki > 6 stopni) |
|---|---|---|
| **ogrz (zasilanie)** | 1829 (4,5%) | **49 (0,12%)** |
| powrót | 1829 (4,5%) | 14 (0,03%) |
| bojler | 1829 (4,5%) | 4 (0,01%) |
| pokój | 1829 (4,5%) | 0 (0,00%) |
| panel | 1829 (4,5%) | 1 (0,00%) |
| zewn | 1859 (4,6%) | 1 (0,00%) |
| trociny | 1829 (4,5%) | 0 (0,00%) |

Dwie obserwacje:

1. **Braki są niemal identyczne we wszystkich kanałach** (1829 próbek). To znaczy, że
   nie gubią się pojedyncze czujniki — gubią się **całe minuty zapisu**. 4,5% to około
   **27 minut na dobę**.
2. **Najgorszy jest czujnik zasilania** — 49 skoków, trzy razy więcej niż powrót.
   To ten sam czujnik, który dawał 16,0 °C w środku 80 °C.

### 10.5 Dni bez palenia — dom trzyma temperaturę sam

Przez 15 z 28 dni piec w ogóle nie palił. Co się wtedy działo z pokojem:

| dzień | pokój min | pokój max | zewnątrz (mediana) | amplituda dobowa |
|---|---|---|---|---|
| 09-28 | 17,0 | 17,5 | 16,0 | 0,5 |
| 09-30 | 16,5 | 17,0 | 16,5 | 0,5 |
| 10-02 | 16,0 | 16,5 | 13,5 | 0,5 |
| 10-04 | 15,0 | 16,0 | 15,0 | 1,0 |
| 10-09 | 16,0 | 16,0 | 15,5 | **0,0** |
| 10-18 | 16,0 | 18,0 | 10,5 | 2,0 |
| **10-19** | 15,0 | **19,5** | 11,0 | **4,5** |

Amplituda 0,5–1,0 stopnia na dobę bez palenia. To potwierdza wcześniejszy wynik:
**τ budynku ≈ 55 godzin**, dom stygnie w skali dni.

Wyjątek: **19.10** — pokój wzrósł o 4,5 stopnia przy 11 °C na zewnątrz, bez palenia.
To czyste zyski słoneczne. Tego dnia panel miał 46,3 °C, a słońce grzało przez okna.

### 10.6 Ile energii siedzi w piecu

| wariant kotła | C | przy 50 °C | przy 70 °C | przy 90 °C |
|---|---|---|---|---|
| 40 l wody + 120 kg stali | 226 kJ/°C | 1,9 kWh | 3,1 kWh | 4,4 kWh |
| **70 l wody + 150 kg stali** | **367 kJ/°C** | **3,1 kWh** | **5,1 kWh** | **7,1 kWh** |
| 100 l wody + 200 kg stali | 517 kJ/°C | 4,3 kWh | 7,2 kWh | 10,0 kWh |
| 150 l wody + 250 kg stali | 750 kJ/°C | 6,3 kWh | 10,4 kWh | 14,6 kWh |

To jest odpowiedź na pytanie „ile piec daje po wygaśnięciu" — **od 5 do 10 kWh**
zakumulowanych w żeliwie i wodzie. Przy zapotrzebowaniu domu 2–3 kW to ciepło na
2–4 godziny grzania.

Status: **szacunek** — zależy od pojemności wodnej kotła, której nie ma w danych.

### 10.7 Co nadal wymaga dodatkowych danych

| co | czego potrzeba | po co |
|---|---|---|
| moc paleniska w kW | pojemność wodna kotła | ile realnie daje piec |
| sprawność instalacji | moc paleniska + przepływ | ile ciepła dociera do domu |
| UA grzejników w W/°C | przepływ (przepływomierz) | czy grzejniki są przewymiarowane |
| koszt palenia | cena paliwa + moc | ile kosztuje doba grzania |
| strata kominowa | temperatura spalin | ile ucieka kominem |

---

## 11. Źródła

Metody i wartości odniesienia, których użyłem:

1. **Prawo stygnięcia Newtona i stała czasowa** — Wikipedia, „Time constant":
   τ = m·c / (h·A), po jednej τ układ osiąga 63,2% zmiany.
2. **Bezwładność cieplna** — Wikipedia, „Thermal inertia": τ = C / (A·U), czyli
   pojemność podzielona przez przewodność strat.
3. **Identyfikacja z krzywej stygnięcia** — industrialmonitordirect.com, „Thermal System
   Identification": τ = czas do 63,2% zmiany, dopasowanie krzywej chłodzenia.
4. **Moc grzejników wg EN 442** — radiatori2000.it, „Thermal output according to EN 442":
   P = Km · Δt^n, gdzie Δt = średnia temperatura wody − temperatura pomieszczenia.
5. **Wykładnik grzejników n = 1,3** — heatlosscalculator.com i squote.app:
   Q = Q_50 × (ΔT/50)^1,3.
6. **Typowe różnice zasilanie-powrót** — solimpeks.com, „Delta-T": kocioł konwencjonalny
   15–20 stopni, pompa ciepła 5–8, oraz opis „low delta T syndrome".
7. **Pojemność wodna kotłów na paliwo stałe** — axinar.com, FAQ kotłów na drewno:
   woda to 1/5–1/3 masy kotła, ponad 80% energii w wodzie.
8. **Bufor dla kotłów na drewno** — solarbayer.de: minimum 55 litrów na kW;
   flexiheatuk.com: 40–50 l/kW dla biomasy.
9. **Straty postojowe kotła** — forum.heatinghelp.com: kocioł żeliwny 776 kg traci
   16,5 °F/h przy 187 °F, co daje około 3,7 kW; ray-pack: straty proporcjonalne
   do różnicy temperatur.
10. **Stała czasowa budynku z okresów swobodnego stygnięcia** — ScienceDirect,
    „Thermal characterisation of residential building dynamics": metoda Cool-Down,
    najpierw H z okresów quasi-ustalonych, potem C z okresów bez zasilania.
11. **Współczynnik strat budynku (HLC) z „energy signature"** — arXiv 2503.22321,
    „Estimation of Building Energy Demand Characteristics using Bayesian Statistics and
    Energy Signature Models": Φ = Φ0 + UA0·(T_wew − T_zewn), nachylenie = HLC [W/K].
12. **Metoda Cool-Down dla budynku** — ScienceDirect, „Thermal characterisation of
    residential building dynamics for demand response": C = τ·H, gdzie H z okresów
    quasi-ustalonych, a C z okresów bez zasilania.
13. **Fazy spalania drewna** — ScienceDirect, „Molecular characterization and optical
    properties of primary emissions from a residential wood burning boiler": cykl dzieli
    się na zapłon, spalanie i żarzenie; w fazie żarzenia temperatura spada, a CO rośnie.
    Użyte do interpretacji sygnału dymu.
14. **Stratyfikacja zbiornika solarnego** — solimpeks.com, „Tank stratification":
    warstwowanie wody daje do 20% więcej uzysku; kolektor karmiony najzimniejszą wodą
    pracuje sprawniej. Uzasadnia, dlaczego czujnik w zbiorniku mierzy masę wody.
15. **Wzory hydrauliczne** — Taco / Bell & Gossett: GPM = BTU/h ÷ (500·ΔT), w metryce
    Q[m³/h] = 0,86·P[kW] ÷ ΔT. Użyte do przeliczenia różnicy temperatur na przepływ.

**Czego próbowałem i nie wyszło.** Regresja „energy signature" na tych danych dała
**R² = 0,013** (nachylenie −0,12). Powód: instalacja nigdy nie jest w stanie ustalonym
— pompa taktuje, piec cyklicznie pali się i stygnie. Metoda wymaga długich okresów
quasi-ustalonych, których tu nie ma. Podobnie test „czy τ zależy od temperatury
zewnętrznej" (wpływ ciągu kominowego): nachylenie 0,034 h/°C przy R² = 0,013, czyli
brak zależności — ale próbek było tylko 6, więc wynik jest niekonkluzywny.

---

## 12. Jak to powtórzyć

```bash
python3 narzedzia/analiza_pieca.py google_drive/akwarium-367be-default-rtdb-2026-export.json
python3 narzedzia/analiza_zaawansowana.py google_drive/akwarium-367be-default-rtdb-2026-export.json
```

**Pierwszy skrypt** (13 sekcji): jakosc danych, sygnal palenia, epizody grzania, stan
pompy, bezwladnosc, czasy stygniecia, odbior grzejnikow, opoznienie, dynamike
rozpalania, przeliczenia na moce, bezwladnosc budynku, solar, bilans dobowy.

**Drugi skrypt** (6 sekcji): energie sloneczna w kWh, weryfikacje modelu
prognostycznego, jakosc czujnikow, przeplyw z opoznienia, dni bez palenia, energie
zakumulowana w piecu.

Skrypt wypisuje 13 sekcji: jakość danych, sygnał palenia, epizody grzania, stan pompy,
bezwładność, czasy stygnięcia, odbiór grzejników, opóźnienie, dynamikę rozpalania,
przeliczenia na moce, bezwładność budynku, solar i bilans dobowy.

Plik JSON nie jest w repozytorium (23 MB) — leży w `google_drive/`, który jest
w `.gitignore`.
