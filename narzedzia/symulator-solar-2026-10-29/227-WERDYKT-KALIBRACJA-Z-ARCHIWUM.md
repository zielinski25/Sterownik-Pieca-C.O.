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

## 5. Cały błąd siedzi w jednej nocy — ale nie tylko ona odstaje

- mediana błędu: **1,40 °C**
- bez nocy 23/24.10: średnia spada z 2,05 do **1,45 °C**
- ta jedna noc to **36% całego błędu**

Co się tam stało: zbiornik szedł 42,5 → 27,8 °C, równiutko, bez skoków,
bez wykrytego poboru, ~1,5 °C/h przy różnicy do powietrza ok. 30 K.
To daje **UA_eff ≈ 6,1 W/K**, czyli 4,4× katalog.

## 5a. Pomyłka, którą sam zrobiłem — i którą trzeba wycofać

Napisałem najpierw, że ten ubytek odpowiada stałemu dopływowi 4,4 l/h, czyli
że **gdzieś cieknie kran**. Powiedziałeś: „nic nie kapie". Sprawdziłem to
jeszcze raz, inaczej — licząc UA osobno dla każdej nocy regresją, a nie
z dwóch punktów:

| noc | UA_eff [W/K] | × katalog |
|---|---|---|
| 18/19.10 | 1,33 | 0,95 |
| 19/20.10 | 1,20 | 0,86 |
| 20/21.10 | 1,53 | 1,09 |
| 21/22.10 | 1,38 | 0,99 |
| 22/23.10 | 2,88 | 2,06 |
| 23/24.10 | 6,14 | 4,38 |
| 24/25.10 | 4,63 | 3,31 |
| 25/26.10 | 12,27 | 8,77 |
| 26/27.10 | 4,88 | 3,48 |
| 27/28.10 | 7,19 | 5,13 |
| 28/29.10 | 3,52 | 2,51 |

**Miałem złe założenie.** Zakładałem, że tylko ta jedna noc odstaje, a resztę
trzyma katalog. W rzeczywistości z katalogiem zgadzają się **4 noce z 11**,
a rozrzut jest dziesięciokrotny (1,2 → 12,3 W/K). Skoro tak, to „ubytek
odpowiada 4,4 l/h" było jednym z wielu możliwych wyjaśnień, a nie wynikiem.
Zdanie o kapiącym kranie wycofuję.

## 5b. Co zostało wykluczone

Wykluczone:
- **wyciek** — sprawdziłeś, nic nie kapie,
- **pojedynczy pobór wody** — na 23/24.10 spadek był gładki, −0,2…−0,3 °C co
  5 minut; 40 l pobrane naraz dałoby skok ~4 K w jednej próbce, a takiego nie ma,
- **pompa obiegu pieca** — pole `pompa=` w logu to `pinPompa`, pompa CO pieca;
  0 z 132 próbek ON przez całą noc. **Uwaga: w instalacji solarnej NIE MA
  pompy** — w firmware nie ma ani jednego odwołania do pompy solarnej, a obieg
  działa grawitacyjnie (termosyfon). Ten punkt wyklucza więc tylko podbieranie
  ciepła przez obieg pieca, a nie ruch wody w solarze.
- **błąd czujnika** — w całym logu jest jedna próbka −1,0 °C (24.10 13:47,
  zaraz po restarcie) i nie trafiła w żadne okno nocne.

## 5c. ZNALEZIONE: zbiornik patrzy w niebo

Opisałeś instalację: **woda z wodociągów pod ciśnieniem idzie prosto na dach
do zbiornika**. Zbiornik stoi więc **na zewnątrz**, a nie w kotłowni. To była
brakująca informacja — i od razu dała odpowiedź.

Zbiornik na dachu nie tylko traci ciepło przez ściankę do powietrza: on
**patrzy w niebo**. Przy czystej nocy niebo ma temperaturę efektywną o
kilkadziesiąt stopni niższą od powietrza. Płyta kolektora wychładza się wtedy
**poniżej temperatury powietrza** i grawitacyjnie podciąga ciepło ze zbiornika.
Przy chmurach niebo „oddaje” to promieniowanie z powrotem i kanał znika.

Sprawdziłem to na prawdziwej pogodzie z Open-Meteo dla Twoich współrzędnych:

| z czym porównana strata | korelacja |
|---|---|
| **zachmurzenie** | **−0,53** |
| wiatr średni | 0,00 |
| wiatr maksymalny | +0,05 |
| temperatura powietrza | −0,20 |

**Wiatr odpada definitywnie** — korelacja zerowa. Dobrze, że to sprawdziłem,
zanim zaproponowałem dopisywanie go do modelu.

Model z dwoma członami: `tempo = −(a·ΔT + b·ΔT·(1−zachmurzenie))/C·3600`.
Sprawdzone **leave-one-out** (uczy się na 10 nocach, sprawdzany na jedenastej):

| | średni błąd tempa |
|---|---|
| model teraz (`tankUa = 1,4`) | 0,237 K/h |
| model z niebem | **0,106 K/h** |
| **poprawa** | **55%** |

Parametry: **a = 0,65 W/K, b = 9,90 W/K** — czyli strata rośnie od 0,65 W/K przy
pełnym zachmurzeniu do 10,55 W/K przy niebie czystym, **siedmiokrotnie**.
To jest dokładnie ten dziesięciokrotny rozrzut, którego nie umiałem nazwać.

W każdym z 11 podziałów parametry wychodzą podobne (a 0,4–1,1; b 9,2–10,4).
Model nauczony na 10 nocach trafia w jedenastą — to nie jest dopasowanie
do jednej nocy.

**Do wdrożenia nie trzeba żadnego nowego czujnika.** Zachmurzenie **już jest**
w prognozie godzinowej, którą aplikacja pobiera (`Weather.kt`, pole
`cloud_cover`). Model cieplny tej liczby po prostu nie używa.


## 5c-2. Zdjęcie z dachu wyjaśnia resztę

Przysłałeś zdjęcie instalacji. To **zestaw kompaktowy**: zbiornik leży poziomo
bezpośrednio na rzędzie **rur próżniowych heat-pipe** (ok. 10 szt., 58/1800),
na tej samej ramie, na dachu. Zgadza się z parametrami w kodzie — apertura
0,95 m², komentarz w `SolarThermalModel.kt` mówi wprost „10 rur 58/1800".

Dwie rzeczy z tego wynikają:

**1. Zbiornik jest najwyżej i patrzy w niebo.** Potwierdza to człon „niebo".

**2. Cyrkulacji wstecznej nie ma i nie może być.** Rura próżniowa heat-pipe jest
**termiczną diodą** — czynnik paruje w rurze i skrapla się w zbiorniku; gdy rura
jest zimniejsza od zbiornika, parowania nie ma i ciepło **nie płynie w dół**.
Do tego woda użytkowa w ogóle nie krąży przez rury: ona siedzi w zbiorniku,
a rury to zamknięte heat-pipy.

Czyli parametry `reverseFlowKgS = 0,004` i `loopUaWk = 1,0` w modelu opisują
mechanizm, **którego w tej instalacji nie ma**. To wyjaśnia, dlaczego zabawa
nimi nie działała: `loopUaWk = 5,0` poprawiał noc o 1,26 °C, ale psuł dzień
o 19% — bo model dokładał straty pętli także w dzień, kiedy ich nie ma.

**Kontrola rzędu wielkości:** współczynnik promieniowania do zimnego nieba to
ok. 5,5 W/(m²·K). Zmierzone `b = 9,9 W/K` odpowiada ok. **1,8 m² patrzących
w niebo**. Zbiornik 100 l jako poziomy walec Ø 0,45 × 1,0 m ma **1,73 m²**.
Zgadza się co do 5%.

## 5c-3. Za darmo: okryć zbiornik

Przy czystym niebie zbiornik traci do **9,9 W/K**. Przy różnicy 30 K to
**~300 W** — więcej niż grzałka, tylko w drugą stronę. Przez 10 godzin nocy
około **3 kWh**, czyli **~2,5 °C na 100 l wody**.

Najtańsza naprawa nie jest w kodzie: **okryć zbiornik od góry** — koc izolacyjny,
mata alufoil, ekran odbijający promieniowanie. Nic nie trzeba rozbierać.

**Jak sprawdzisz, że zadziałało:** po okryciu powtórz `kalibracja/niebo.py` na
nowym logu. Jeśli `b` spadnie z ~9,9 do ~2–3 W/K, okrycie działa. Pomiar, nie wiara.

## 5d. Czujnik „panel" mierzy zbiornik, nie kolektor

Sprawdziłem przy okazji, **co naprawdę mierzy** kanał, na którym stoi model.
W firmware to `temperaturaPaneluSlonecznego`, czujnik `czujnik_temp_Panel_Plus`
na `BUS_3`; log i aplikacja pokazują go jako „Panel solarny".

Trzy liczby z 11 dób:

| co | kolektor płaski na dachu | zmierzone |
|---|---|---|
| najwyższa temperatura w 11 dni | 80–120 °C | **47,2 °C** |
| największa zmiana w 5 min | 5–10 °C | **1,0 °C** |
| noc, 11 godzin | wyrównuje do powietrza w minuty | **stoi 20 K nad powietrzem** |

Goła płyta tak się nie zachowuje. Tak zachowuje się **zbiornik 100 l**.
Czyli czujnik podpisany „panel solarny" siedzi najprawdopodobniej na zbiorniku,
a model **nigdy nie widzi temperatury dachu**.

To ma bezpośredni skutek: cyrkulacja wsteczna (jedyny mechanizm pasujący do nocy
z UA 4–12 W/K) zależy właśnie od różnicy zbiornik–kolektor, a tej drugiej liczby
w danych nie ma.

**Istotne przy tym:** w instalacji **nie ma pompy** — użytkownik potwierdził,
a w firmware nie ma ani jednego odwołania do pompy solarnej; obieg jest
grawitacyjny. Pole `pompa=` w logu to pompa CO pieca (`pinPompa`), nie solar.
Skoro nie ma pompy, to **nic nie stoi na przeszkodzie, żeby obieg nocą kręcił się
wstecz** — grawitacja działa w obie strony.

## 5e. Jedno pytanie, które zostało

Gdzie fizycznie przykręcony jest czujnik z kanału `BUS_3` — na zbiorniku czy na
którejś z rur? Na zdjęciu widać, że zbiornik jest jedynym sensownym miejscem,
a dane to potwierdzają (max 47,2 °C w 11 dni, max 1,0 °C na 5 min — płyta
kolektora tak się nie zachowuje). Ale to Twoje słowo jest tu rozstrzygające,
nie moje domysły.

## 5f. Wariant „z detektorem" — też sprawdzony, też odpada

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

## 6. Wniosek

**Kalibracja z archiwum nie wchodzi do kodu.** Ale wyszło przy tym coś lepszego:
**brakujący człon fizyczny** — zbiornik na dachu oddaje ciepło do nieba, a model
zna tylko temperaturę powietrza. Poprawa 55%, zmierzone leave-one-out, bez
ani jednego nowego czujnika (zachmurzenie już jest w prognozie).

Różnica między jednym a drugim jest zasadnicza, więc żeby nie było nieporozumienia:

| | uczenie z archiwum (Twoja opcja b) | człon „niebo" |
|---|---|---|
| czego się uczy | jednej stałej dla całej instalacji | dwóch własności fizycznych |
| wynik | 4 metody **gorsze** od modelu | **55% lepiej** niż model |
| walidacja | leave-one-out | leave-one-out, parametry stabilne |
| nowe dane | żadne | żadne |

**Do wdrożenia potrzebna jest Twoja zgoda** — zmienia `SolarThermalModel.kt`
i wymaga podbicia wersji. Nie ruszam bez pytania.

W tej samej zmianie wypada **wyłączyć cyrkulację wsteczną** (`reverseFlowKgS`
i nocne straty pętli `loopUaWk`), bo — jak pokazało zdjęcie — w instalacji
heat-pipe ten mechanizm nie istnieje. Czyli dwie zmiany w jednym pliku:

1. dodać człon nieba do zbiornika (`a`, `b`),
2. wyzerować nocny przepływ wsteczny pętli.

Zastrzeżenie, które trzeba powiedzieć wprost: 11 nocy to jeden sezon, a
zachmurzenie biorę z modelu Open-Meteo, nie z czujnika. Przed wgraniem warto
powtórzyć na 3–4 tygodniach archiwum — kod do tego jest gotowy
(`kalibracja/niebo.py`, wystarczy podmienić plik z logiem).

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

- `narzedzia/symulator-solar-2026-10-29/kalibracja/porownanie.py` (5 metod uczenia)
- `narzedzia/symulator-solar-2026-10-29/kalibracja/ua_noc.py` (UA osobno dla każdej nocy)
- `narzedzia/symulator-solar-2026-10-29/kalibracja/niebo.py` (człon „niebo", walidacja leave-one-out)
- `narzedzia/symulator-solar-2026-10-29/kalibracja/WYNIK-POROWNANIE.txt`
- `narzedzia/symulator-solar-2026-10-29/RAPORT-KALIBRACJI-NOCNEJ.md` §„WERDYKT"
- `MODEL_SOLARNY_TRZEMCHA_2026-10-23.md` §19
