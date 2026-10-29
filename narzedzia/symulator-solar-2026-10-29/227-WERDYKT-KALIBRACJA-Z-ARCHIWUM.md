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

## 5b. Co zostało wykluczone, a co nie

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

Nie wykluczone, od najbardziej prawdopodobnego:
1. **wiatr** — zbiornik stoi na dachu, a wiatr potrafi zwiększyć straty
   kilkukrotnie. Aplikacja **już pobiera** prędkość wiatru z prognozy
   (`wind_speed_10m`), ale model cieplny tej liczby w ogóle nie używa.
2. **cyrkulacja wody użytkowej** — jeśli jest, to ciągły mały przepływ,
   którego nie widać jako skok.
3. **cyrkulacja wsteczna pętli solarnej** — literatura mówi „nearly fourfold",
   zmierzone 4,4× pasuje, ale nie tłumaczy nocy 25/26.10 (8,8×) przy zbiorniku
   o temperaturze zaledwie 20 °C, czyli bez ciągu termicznego.


## 5c. Czujnik „panel" mierzy zbiornik, nie kolektor

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

## 5d. Jedno pytanie do użytkownika

Gdzie fizycznie przykręcony jest czujnik z kanału `BUS_3` — na zbiorniku czy na
kolektorze na dachu? Od tego zależy, czy:
- etykieta „Panel solarny" w aplikacji jest tylko myląca (czujnik na zbiorniku), czy
- mamy prawdziwy pomiar kolektora i cyrkulację wsteczną można policzyć z danych.

## 5e. Wariant „z detektorem" — też sprawdzony, też odpada

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

**Kalibracja z archiwum nie wchodzi do kodu.** Model zostaje taki, jaki jest
w 0.31.51. To nie ostrożność, to wynik pomiaru — i teraz wiadomo dokładniej,
dlaczego: skoro efektywna strata zbiornika zmienia się dziesięciokrotnie między
nocami, to **żadna stała jej nie opisze**, ani 1,4 z katalogu, ani nauczona
z archiwum.

Czego to nie znaczy: że model jest do wyrzucenia. Znaczy, że **model nie zna
jednej rzeczy, której nie ma w danych** — co w nocy dzieje się na dachu.

Dwa tropy, oba tanie, w kolejności:

1. **Ustalić, gdzie wisi czujnik z `BUS_3`** (punkt 5d). Jeśli na zbiorniku,
   to model nie ma żadnego pomiaru z dachu i żadna kalibracja tego nie naprawi —
   trzeba jednego czujnika na kolektorze. Firmware ma wolne wejście na tej
   samej magistrali, więc to kwestia podłączenia, nie przepisywania.
2. **Wiatr** — aplikacja już go pobiera (`wind_speed_10m`), a model cieplny nie
   używa; da się sprawdzić na istniejącej historii, bez nowych czujników.

Dopiero po jednym z tych dwóch będzie wiadomo, czy w ogóle jest co poprawiać
w równaniu.

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
- `narzedzia/symulator-solar-2026-10-29/kalibracja/WYNIK-POROWNANIE.txt`
- `narzedzia/symulator-solar-2026-10-29/RAPORT-KALIBRACJI-NOCNEJ.md` §„WERDYKT"
- `MODEL_SOLARNY_TRZEMCHA_2026-10-23.md` §19
