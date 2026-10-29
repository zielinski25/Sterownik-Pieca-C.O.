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
