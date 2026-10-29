# Nocne chłodzenie zbiornika — research i kalibracja, 29.10.2026

Pytanie: karta „DZIŚ" pokazała **−1,0 °C** przy „ZYSK DZIŚ +0,2". Czy to błąd
wyświetlania, czy model źle liczy noc? Odpowiedź: **oba**, ale to dwie różne rzeczy.

## 1. Co mówi literatura

| źródło | co podaje | do czego użyte |
|---|---|---|
| Ong i in., *Int. J. Low-Carbon Tech.* 10(4) 2015, `10/4/430` | kolektor heat-pipe, termosyfon: **nocny spadek 2–10 °C**; większy, gdy noc zimniejsza; **przepływ wsteczny mniejszy niż w płaskim**, bo kondensator rurki cieplnej jest izolowany | potwierdza rząd wielkości |
| OSTI `biblio/116242` | 100 l + płaski 1,8×0,8 m; przepływ nocny ~0,005 kg/s; **straty rosną „nearly fourfold"** | skąd 0,004 kg/s w modelu |
| Michaelides, EuroSun 2010 | straty nocne liniowo od temperatury otoczenia, r = 0,93–0,95; **cyrkulacja wsteczna „minimalna"** | sprzeczne z OSTI |

Wniosek: literatura sama nie rozstrzyga. Dlatego model dwuwęzłowy, a nie mnożnik.

## 2. Co mówią Twoje własne dane

Z `MODEL_SOLARNY_TRZEMCHA_2026-10-23.md` §18.2 — policzone z archiwum centrali:

| noc | spadek | godzin | °C/h | **efektywne UA** |
|---|---|---|---|---|
| 21.10 wieczór (po pochmurnym) | 4,0 K | 8,8 | 0,45 | **2,3 W/K** |
| 22.10 noc (po pochmurnym) | 3,4 K | 8,3 | 0,41 | **2,3 W/K** |
| 23.10 wieczór (po **słonecznym**) | 11,4 K | 8,9 | **1,28** | **4,6 W/K** |
| 24.10 noc (po **słonecznym**) | 8,8 K | 9,3 | 0,95 | **4,3 W/K** |
| 29.10 14:30–17:20 (dziś) | 1,6 K | 2,8 | 0,57 | — |

**Efektywne UA zależy od tego, jak słoneczny był poprzedni dzień.** 2,3 W/K po
pochmurnym, 4,3–4,6 W/K po słonecznym. To nie jest szum — to cyrkulacja wsteczna:
po słonecznym dniu pętla jest gorąca i nocą silniej odbiera ciepło.

## 3. Co robi model

Uruchomiony na prawdziwym kodzie (`kalibracja/WYNIK-KALIBRACJI.txt`):

| stan | model | pomiar | ile razy za wolno |
|---|---|---|---|
| woda 41 °C, powietrze 8 °C | −0,60 °C/h | −1,28 °C/h (23.10) | **2,1×** |
| woda 35 °C, powietrze 8 °C | −0,49 °C/h | −0,95 °C/h (24.10) | 1,9× |
| woda 19,5 °C, powietrze 11 °C | −0,15 °C/h | −0,41 °C/h (22.10) | 2,7× |

Model ma **efektywne UA ≈ 2,0 W/K** (tankUa 1,4 + wkład pętli 0,6). Pomiar: 2,3–4,6 W/K.

## 4. Dlaczego nie da się tego naprawić jednym parametrem

Przemiatałem trzy parametry osobno. Kluczowe wyniki:

### `reverseFlowKgS` jest w modelu **bezwładny**

| przepływ wsteczny | noc 41 °C |
|---|---|
| 0,002 kg/s | −0,60 °C/h |
| 0,004 kg/s (teraz) | −0,60 °C/h |
| 0,050 kg/s (12× więcej) | −0,62 °C/h |

Zmiana 25× daje 0,02 °C/h. Powód: w stanie ustalonym ciepło z zbiornika do pętli
jest ograniczone **stratami pętli do otoczenia** (`loopUaWk`), nie samym przepływem.
Pętla ma 4 kg wody i wychładza się do temperatury powietrza w kilkanaście minut.

To ważne: parametr `reverseFlowKgS = 0.004` **nie robi w praktyce nic**.
Wartość z OSTI jest w modelu, ale nie wpływa na wynik.

### `loopUaWk` działa, ale psuje dzień

| loopUa | noc 41 °C | dzień 900 W/m² |
|---|---|---|
| 1,0 (teraz) | −0,60 | **33,3 °C** |
| 3,0 | −0,97 | 29,6 |
| 5,0 | **−1,26** ✅ | 26,9 ❌ |

Żeby dopasować noc, trzeba stracić **19% dziennego zysku**.

### Dwoma parametrami naraz — też nie

Żeby odzyskać dzień, trzeba podnieść aperturę optyczną:

| loopUa | aperture | noc 41 °C | dzień 900 | η₀ = aperture / 0,95 |
|---|---|---|---|---|
| 5,0 | 0,678 | −1,26 ✅ | 26,9 | 0,71 |
| 5,0 | 0,800 | −1,26 ✅ | 32,1 | 0,84 |
| 5,0 | 0,850 | −1,26 ✅ | **34,2** ✅ | **0,89** ❌ |

**η₀ = 0,89 jest fizycznie niemożliwe** dla kolektora próżniowego (realnie 0,70–0,78).
Czyli dopasowanie idzie w złą stronę — model nie ma dość swobody.

## 5. Wniosek

Jednowarstwowy zbiornik nie potrafi być jednocześnie dokładny w dzień i w nocy.
Prawdziwy zbiornik 100 l jest **uwarstwiony**: góra cieplejsza, dół chłodniejszy.
Nocą cyrkulacja wsteczna zabiera ciepło z **górnej** warstwy, a czujnik w połowie
wysokości widzi szybki spadek. W dzień kolektor grzeje **dół**, który potem się
miesza — i czujnik widzi wolniejszy wzrost.

To samo zauważył `MODEL_SOLARNY` §18.6: „godzina szczytu jest ~2 h za późno,
to konsekwencja traktowania zbiornika jako jednej bryły".

**Naprawa to model warstwowy, nie zmiana liczby.** Świadomie nie ruszam
parametrów: każda zmiana psuje dzień, który jest już sprawdzony (błąd 1,3 °C
na 4 dniach).

## 6. Co za to naprawiono — błąd wyświetlania

To była osobna sprawa i **rzeczywisty błąd**. Karta DZIŚ liczy całą dobę:
zmierzone + reszta dnia + noc do północy (`dailyForecast7`, wiersze 421–425).
Kafelek „DO KOŃCA DNIA" po zachodzie pokazywał „—", więc użytkownik widział
**+0,2** i **−1,0** i nie miał jak odtworzyć różnicy.

Poprawka 0.31.51: po zachodzie kafelek pokazuje modelowane nocne stygnięcie
do północy, z podpisem „bez słońca · woda stygnie do północy".

Weryfikacja (test G, 1000 dób):

| wersja | dni nieodtwarzalne z ekranu |
|---|---|
| 0.31.50 | 539 / 1000 = **53,9%** |
| **0.31.51** | **0 / 1000 = 0,0%** |

## 7. Czego to nie naprawia

Model nadal **zaniża nocne chłodzenie 2×** po słonecznym dniu. Prognoza „całej doby"
będzie więc o **1–2 °C za optymistyczna** w takie noce. Po pochmurnym dniu błąd
jest mały.

To nie jest ukryte: karta jest podpisana „szacunek, nie pomiar".

## 8. Kiedy wrócić do tematu

Po 2–3 tygodniach archiwum będzie dość danych, żeby dopasować model warstwowy
(3–5 warstw zbiornika). Wtedy:
- godzina szczytu przestanie być 2 h za późno,
- nocne chłodzenie wyjdzie z fizyki, nie z parametru,
- `reverseFlowKgS` zacznie wreszcie cokolwiek robić.

Do tego czasu **nie ruszamy parametrów**.

---

# KOREKTA 29.10.2026, godz. 17:40 — instalacja jest BEZPOŚREDNIA

Użytkownik doprecyzował budowę instalacji: woda z sieci przychodzi **pod
ciśnieniem sieci**, drugą rurą wychodzi do kranów, **innej rury nie ma** —
brak wężownicy, brak oddzielnego obiegu. Zbiornik 100 l i kolektory stoją
razem na dachu, woda w kolektorach to **ta sama woda**, którą bierze się
z kranu.

## Dlaczego to przewraca wniosek z §5

W układzie bezpośrednim **każde odkręcenie kranu** wpuszcza do zbiornika
od dołu zimną wodę z sieci (~10 °C), a gorącą wypycha do kranu. Czujnik
w połowie wysokości widzi to jako spadek temperatury — **identyczny
z pozorem straty ciepła**.

Policzone od nowa (skrypt: `kalibracja/`):

| odcinek | spadek | moc | **UA, które by to tłumaczyło** | × katalog 1,4 W/K |
|---|---|---|---|---|
| 21.10 wieczór | 4,0 K / 8,8 h | 53 W | 6,6 W/K | 4,7× |
| 22.10 noc | 3,4 K / 8,3 h | 48 W | 6,0 W/K | 4,3× |
| 23.10 wieczór | 11,4 K / 8,9 h | 149 W | **5,0 W/K** | 3,5× |
| 24.10 noc | 8,8 K / 9,3 h | 110 W | **4,6 W/K** | 3,3× |
| 29.10 16:13–17:20 | 0,8 K / 1,1 h | 83 W | **20,3 W/K** | 14,5× |

**UA zmienia się 4,4× między ciepłą i zimną wodą.** Przy przewodzeniu ciepła
UA jest stałe. Czyli to **nie są straty cieplne zbiornika**.

## Co pasuje zamiast tego

Zwykły, stały przepływ wody przez zbiornik:

| odcinek | ΔT woda−sieć | potrzebny przepływ |
|---|---|---|
| 21.10 wieczór | 6,0 K | 7,6 l/h |
| 22.10 noc | 5,0 K | 8,2 l/h |
| 23.10 wieczór | 31,5 K | **4,1 l/h** |
| 24.10 noc | 17,0 K | **5,6 l/h** |
| 29.10 16:13–17:20 | 7,6 K | **9,4 l/h** |

**4–9 litrów na godzinę** tłumaczy wszystkie pomiary naraz — od zimnej wody
po gorącą. To dużo bardziej spójne niż UA zmieniające się czterokrotnie.

## Dlaczego aplikacja tego nigdy nie pokazała jako „pobrania"

Detektor pobrań (`recomputeDraws`, `DRAW_STEP_C = 0.5`, `DRAW_WINDOW_MS = 5 min`)
wymaga spadku **0,5 °C w 5 minut**. Przy 5–9 l/h woda schodzi o ~0,02 °C
na 5 minut. **Ponad 20× pod progiem.** Aplikacja pokazuje „Pobrania: brak",
bo z definicji nie widzi poboru rozłożonego w czasie.

## Wniosek

**Wycofuję zdanie „model zaniża nocne chłodzenie 2×".** Nie jest udowodnione.
Zmierzone spadki można w całości wyjaśnić zużyciem wody, a nie stratami.

Model fizyczny mógł cały czas być poprawny — nie da się tego rozstrzygnąć
bez pomiaru zużycia wody.

## Test rozstrzygający (bez ryzyka, bez wchodzenia na dach)

Odczyt licznika wody wieczorem i rano, w noc, gdy nikt nie używa wody.
- **Licznik stoi** → cała woda poszła wieczorem na normalne zużycie,
  model jest w porządku, temat zamknięty.
- **Licznik się kręci** (5–10 l/h) → gdzieś jest stały przepływ:
  cieknący kran, zawór spłuczki, nieszczelny zawór zwrotny.
  Wtedy to nie jest problem aplikacji, tylko hydrauliki.

---

# TEST NA 11 PRAWDZIWYCH NOCACH (29.10.2026, 17:40)

Zamiast zgadywać parametry — uruchomiłem model na **każdej z 11 nocy** z logu
centrali. Dane wejściowe: rzeczywista temperatura powietrza co godzinę
(z logu, `STATUS`), rzeczywista temperatura wody o 19:00. Wyjście: temperatura
o 06:00. Porównanie z tym, co naprawdę pokazał czujnik.

Plik: `kalibracja/WYNIK-NOCE.txt`, dane: `kalibracja/noce.csv`.

| noc | T 19:00 | zmierzone | model | błąd |
|---|---|---|---|---|
| 18.10 | 28,0 | −2,0 | −3,4 | **−1,4** model za szybko |
| 19.10 | 24,2 | −1,4 | −2,5 | **−1,1** model za szybko |
| 20.10 | 23,2 | −1,7 | −1,7 | 0,0 OK |
| 21.10 | 36,5 | −2,5 | −3,9 | **−1,4** model za szybko |
| 22.10 | 30,8 | −4,6 | −3,7 | +0,9 |
| **23.10** | **42,8** | **−14,3** | **−6,2** | **+8,1** ⚠ |
| 24.10 | 21,8 | −4,3 | −2,4 | +1,9 |
| 25.10 | 20,0 | −4,5 | −1,3 | +3,2 |
| 26.10 | 14,8 | −3,0 | −1,6 | +1,4 |
| 27.10 | 22,5 | −5,5 | −2,6 | +2,9 |
| 28.10 | 17,5 | −1,0 | −0,7 | +0,3 OK |

**Średni błąd bezwzględny: 2,07 °C na noc. W granicach 0,5 °C: 2 z 11 nocy.**

## Co z tego wynika

### 1. Wycofuję OBA wcześniejsze twierdzenia

- „model zaniża nocne chłodzenie 2×" — **nieprawda**, bo na 3 nocach (18, 19, 21.10)
  model chłodzi **za szybko** o 1,1–1,4 °C.
- „wszystko wyjaśniają pobory wody" — **też nieprawda**, bo pobory mogą tylko
  dodawać chłodzenia. Nie wyjaśnią nocy, na których model chłodzi ZA SZYBKO.

Model myli się **w obie strony**, średnio 2 °C. To gorsze niż jedna systematyczna pomyłka
— znaczy, że brakuje zjawiska, nie współczynnika.

### 2. Noc 23.10 to nie „błąd modelu", to inny tryb pracy

−14,3 °C w jedenaście godzin przy starcie z 42,8 °C. Model daje −6,2 °C.
Tego typu noc zdarzyła się **raz w jedenastu**. To ta sama noc, którą
`MODEL_SOLARNY` §18.2 opisał jako UA_eff = 4,6 W/K (po bardzo słonecznym dniu,
szczyt 47,2 °C).

### 3. Bazowe straty zbiornika są POPRAWNE

Na nocach z dużą różnicą wodа−powietrze (18, 19, 21.10, ΔT = 12–19 K)
pozorne UA wynosi **1,2–1,4 W/K**, czyli dokładnie tyle, ile w katalogu.
**`tankUa = 1.4` jest prawidłowe i nie wolno go ruszać.**

Rozrzut pozornego UA (1,2–12,2 W/K) bierze się z tego, że przy małej różnicy
temperatur (3–9 K) spadek jest na granicy rozdzielczości czujnika 0,1 °C.

### 4. Czego model nie ma

Model nie zna:
- **uwarstwienia** zbiornika — 100 l to nie jedna bryła; czujnik w połowie
  widzi inną temperaturę niż średnia,
- **stanu pętli kolektora o zachodzie** — po słonecznym dniu pętla jest gorąca
  i nocą oddaje ciepło inaczej niż po pochmurnym,
- **poboru wody** — w instalacji bezpośredniej każde odkręcenie kranu wpuszcza
  zimną wodę z sieci do zbiornika.

Dlatego dopasowanie jednego współczynnika jest skazane na porażkę — i dlatego
go nie robię.

## Wniosek praktyczny

Prognoza „całej doby" ma **błąd rzędu 2 °C**, czasem 8 °C. Karta jest podpisana
„szacunek, nie pomiar" i to jest uczciwe. Ale **nie da się tego naprawić
parametrem** — potrzebny jest model warstwowy z jawnym węzłem pętli i z
odetnieniem poborów.

## Propozycja: kalibracja z własnego archiwum

Aplikacja ma już 7–14 dni własnych nocy. Zamiast stałej `tankUa = 1.4` mogłaby
**mierzyć** rzeczywiste tempo stygnięcia z własnych danych (regresja
spadek ↔ różnica wodа−powietrze, tylko na nocach bez poborów) i używać
zmierzonej wartości.

Zalety: brak nowych czujników, brak wchodzenia na dach, liczy się samo.
Wady: nowy kod, ryzyko, i trzeba go przetestować.

**Nie wprowadzam tego bez Twojej zgody.**

---

## WERDYKT NA PROPOZYCJĘ „KALIBRACJA Z ARCHIWUM" (2026-10-29)

Dostałem zgodę na wdrożenie tego pomysłu. **Najpierw go sprawdziłem — i pomysł
nie działa.** Poniżej dowód, żeby nie było wątpliwości, że to nie „nie chciało
mi się", tylko że dane mówią „nie".

Skrypt: `kalibracja/porownanie.py`, wynik: `kalibracja/WYNIK-POROWNANIE.txt`,
dane: log centrali 18–29.10, 11 nocy z pełnym pokryciem (19:00 → 06:00).

### 1. Pięć metod, jeden komplet nocy, leave-one-out

Każda metoda uczy się na 10 nocach i jest sprawdzana na tej jedenastej.

| metoda | co robi | średni błąd |
|---|---|---|
| **A — model fizyczny (stan teraz, 0.31.51)** | równanie zbiornika + pętla | **2,05 °C** |
| B — uczony współczynnik `k` | zastępuje model: `spadek = k · ΔT · t` | 2,89 °C |
| C — persystencja | tempo z pierwszych 2 h nocy, przeskalowane ΔT | 1,95 °C |
| D — model × mnożnik | mnożnik 0,5–2,0 uczony na pozostałych nocach | 2,28 °C |
| E — model + stała poprawka | stała −1…+1 K uczona na pozostałych nocach | 2,10 °C |

**Żadna forma uczenia nie bije modelu fizycznego.** Jedyna, która wyszła o 0,1 °C
lepiej (C), przegrywa na teście praktycznym — patrz punkt 3.

### 2. Dlaczego nie ma czego nauczyć

Błąd modelu na 11 nocach: −1,4 · −1,1 · 0,0 · −1,4 · +0,9 · **+8,1** · +1,9 ·
+3,2 · +1,4 · +2,9 · +0,3.

Korelacja błędu z tym, co da się zmierzyć:

| z czym porównane | współczynnik korelacji |
|---|---|
| temperatura wody o 19:00 | +0,33 |
| temperatura powietrza | −0,31 |
| różnica woda − powietrze | +0,24 |

Wszystkie trzy w granicach szumu. **Skoro błąd nie zależy od niczego mierzalnego,
to nie ma z czego zrobić poprawki** — jedyne, co uczenie może zrobić, to
wpisać na stałe średnią pomyłkę i psuć noce, które były dobre.

### 3. Test praktyczny: „ile zostanie do 24:00" (66 przypadków)

Sprawdzone o każdej pełnej godzinie 17:00–22:00, dla dni 18–28.10:

| metoda | średni błąd |
|---|---|
| persystencja (tempo z ostatnich 2 h) | 2,00 °C |
| model | **1,29 °C** |

Persystencja przegrywa, bo o 17:00–18:00 **jeszcze grzeje słońce**. 26.10 o 17:00
przewidywała spadek −12,6 °C, a woda straciła 2,6 °C. Model fizyczny wie o
zachodzie słońca, persystencja nie.

### 4. Największy błąd to jedna noc — ale nie jedyna podejrzana

Mediana błędu bezwzględnego to **1,40 °C**. Po odjęciu nocy 23/24.10 średni błąd
spada z 2,05 do **1,45 °C**. Ta jedna noc to **36% całego błędu**.

Co tam się stało: zbiornik szedł 42,5 → 27,8 °C, równiutko, bez skoków, bez
poboru, ~1,5 °C/h przy ΔT ≈ 30 K. To daje **UA_eff ≈ 6,1 W/K**, czyli 4,4× katalog.

**WYCOFANE (zgłoszenie użytkownika: „nic nie kapie").** Pierwotnie napisałem, że
to odpowiada stałemu dopływowi 4,4 l/h, czyli że cieknie kran. Błędne, i to z
dwóch powodów: użytkownik sprawdził instalację, a poza tym wniosek opierał się
na założeniu, że **tylko ta noc** odstaje od katalogu. Po policzeniu UA_eff
regresją osobno dla każdej nocy (skrypt `kalibracja/ua_noc.py`) okazało się,
że odstaje **7 nocy z 11**:

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

Rozrzut **10,2×**. Tylko 4 noce zgadzają się z katalogiem 1,4 W/K. To znaczy,
że zdanie „`tankUa = 1.4` potwierdzone pomiarem" też było za mocne — ono jest
najlepszą **pojedynczą** liczbą, ale nie jest prawdą o każdej nocy.

Sprawdzone i wykluczone jako przyczyna:
- **wyciek** — użytkownik: „nic nie kapie",
- **pojedynczy pobór wody** — na 23/24.10 spadek gładki, −0,2…−0,3 °C co 5 min;
  40 l pobrane naraz dałoby skok ~4 K w jednej próbce, a takiego nie ma,
- **pompa** — 0/132 próbek ON przez całą noc 23/24.10,
- **błąd czujnika** — w całym logu jedna próbka −1,0 °C (24.10 13:47, po restarcie),
  poza oknami nocnymi.

Nie wykluczone, w kolejności prawdopodobieństwa: **wiatr** (zbiornik na dachu;
aplikacja już pobiera `wind_speed_10m`, ale model cieplny tej liczby nie używa),
**cyrkulacja wody użytkowej**, **cyrkulacja wsteczna pętli** (literatura: „nearly
fourfold", zmierzone 4,4× — ale nie tłumaczy nocy 25/26.10 z UA 8,8× przy
zbiorniku 20 °C).

### 5. Wariant „z detektorem" — też sprawdzony, też odpada

Pomyślałem, że skoro uczenie globalne nie działa, to może działa **detektor
nietypowej straty**: jedź na modelu, ale gdy zmierzone tempo stygnięcia jest
wyraźnie większe od modelowego, przełącz się na zmierzone. To by złapało
właśnie taką noc jak 23/24.10.

Sprawdzone dla prognozy robionej o 21:00, 22:00 i 23:00 (do 06:00), próg 1,8×:

| okno | model | model + detektor | ile razy zadziałał |
|---|---|---|---|
| 21:00 | 1,63 °C | 1,62 °C | 7/11 |
| 22:00 | **1,29 °C** | 1,74 °C | 8/11 |
| 23:00 | 1,20 °C | 1,10 °C | 7/11 |

Średnia raz lepsza, raz gorsza — ale to nie jest prawdziwy problem. Problem
jest w ogonie:

- 22:00, noc 20/21.10: model **0,45 °C** → z detektorem **4,23 °C**
- 21:00, noc 27/28.10: model **0,98 °C** → z detektorem **4,61 °C**

Detektor myli **przemijający ruch wody wieczorem** (pętla oddaje ciepło po
słonecznym dniu) z trwałą stratą. Efekt: zamienia bardzo dobrą prognozę w bardzo
złą. Na 11 nocach średnia tego nie pokaże, ale użytkownik trafi na taki dzień
i zobaczy bzdurę.

**Przycisk „przełącz na zmierzone" jest loterią. Nie wchodzi do kodu.**

### 6. Decyzja

**Nie wdrażam kalibracji z archiwum.** Model zostaje taki, jaki jest w 0.31.51.
Powód nie jest filozoficzny: cztery niezależne sposoby uczenia na 11 nocach
wypadły gorzej niż to, co już jest w aplikacji.
