# TO BE · Trening przed kartkówką 🇬🇧

Interaktywna strona do nauki czasownika **„to be"** dla ucznia klasy 4. Przygotowana na podstawie
13 zdjęć z Dysku Google (zrzut zapowiedzi w e-dzienniku + fotografie stron podręcznika i zeszytu
ćwiczeń) oraz plików PDF `Together_2026_kl4_SB/WB/ZP_01_wm.pdf`.

## Zakres kartkówki (z e-dziennika)

- **Przedmiot:** język angielski
- **Typ:** kartkówka · **termin:** 10 listopada 2026
- **Opis:** „czasownik 'to be' (być): odmiana przez osoby, twierdzenia, pytania i przeczenia"

Materiał źródłowy: **Together 2026, klasa 4** — Unit 1 (s. 10–11: *Be affirmative, personal
pronouns*; s. 13: *a, an, zero article*; s. 14–16: pytania o dane osobowe, formularz),
Unit 2 (s. 18–21: *Be negative*, pytania i krótkie odpowiedzi), powtórka / Catch-up,
„Przygotowanie do sprawdzianu", „My grammar", „On-the-Go Practice".
W zadaniach występują postacie i słowa z książki: Mickey, Ana, Lily, Robbie, Mr i Mrs Brown,
Lena, Harry, Emile, Ms Collins, Tomek, Filip, Leanne, Bianca; kraje i narodowości,
przymiotniki (*amazing, cool, clever, shy…*), miejsca (*at school, in the park…*), ubrania.

## Jak uruchomić

| Sposób | Co zrobić |
| --- | --- |
| **Najprostszy** | Otwórz **`to-be-kartkowka.html`** w przeglądarce (podwójne kliknięcie). To jeden samodzielny plik — można go wysłać mailem, wrzucić na tablet, otworzyć bez internetu. |
| **Wersja do edycji** | Otwórz **`index.html`** — muszą obok niego być `styles.css`, `data.js`, `app.js`. |
| **Podgląd / serwer** | `python3 -m http.server 8000 --bind 0.0.0.0` w tym folderze. |

Nie ma żadnych zewnętrznych bibliotek, fontów, CDN-ów ani zapytań sieciowych. Działa z `file://`.
Głos Robbiego używa wbudowanego `speechSynthesis` (angielski) — jeśli przeglądarka go nie ma,
ćwiczenia słuchowe mają przycisk „👀 Nie mam dźwięku" pokazujący tekst.

## Co jest w środku

**📘 Nauka** — 7 zasad z przykładami czytanymi na głos, pełna tabela odmiany (długa/krótka forma,
przeczenie, pytanie, krótka odpowiedź), 8 pytań z podręcznika, 12 najczęstszych pułapek,
słowniczek (kraje/narodowości, przymiotniki, miejsca) oraz **ściąga do druku** (A4).

**🎮 15 ćwiczeń różnego typu** (każde losuje pytania, więc da się je powtarzać):

| # | Ćwiczenie | Typ zadania / umiejętność |
| --- | --- | --- |
| 1 | Wybierz formę | multiple choice: am/is/are, isn't/aren't, pytania |
| 2 | Wpisz formę | gap-fill z wpisywaniem, akceptuje formy pełne i krótkie |
| 3 | Memory | gra pamięciowa: zaimek ↔ forma, długa ↔ krótka, przeczenia |
| 4 | Połącz w pary | matching: pytanie ↔ odpowiedź, twierdzenie ↔ przeczenie |
| 5 | Rozsypanka | sentence reordering z klocków, z jednym niepotrzebnym wyrazem |
| 6 | Przekształcenia | twierdzenie → przeczenie / pytanie (pisanie) |
| 7 | Krótkie odpowiedzi | Yes, I am. / No, he isn't. z pułapkami typu „Yes, I'm." |
| 8 | Detektyw błędów | wskaż błędne słowo, potem wybierz poprawne zdanie (error correction) |
| 9 | Sortownik zdań | klasyfikacja: twierdzenie / pytanie / przeczenie |
| 10 | Prawda czy fałsz? | czytanie krótkich tekstów o postaciach z podręcznika (T/F) |
| 11 | Słuchaj i wybierz | listening z `speechSynthesis`, tempo normalne i wolne |
| 12 | Dyktando | pisanie ze słuchu — apostrofy w isn't / aren't / I'm / we're |
| 13 | Tłumacz z polskiego | budowanie angielskiego zdania z klocków z pułapkami |
| 14 | Literuj formę | spelling game (wisielec) z baterią Robbiego |
| 15 | Szybka runda | 60 sekund, ile poprawnych form — trening automatyzacji |

**🗂️ Fiszki** — 16 kart z prostą powtórką rozłożoną w czasie („Umiem" odkłada kartę,
„Jeszcze ćwiczę" wraca szybciej).

**📝 Kartkówka próbna** — 15 losowych zadań z 42-elementowej puli w dwóch trybach:
*tryb nauki* (wyjaśnienie od razu) i *tryb kartkówki* (bez podpowiedzi, z czasem, omówienie
wszystkich odpowiedzi na końcu, lista słabszych obszarów).

**📊 Postępy** — punkty XP, serie poprawnych odpowiedzi, skuteczność, 12 odznak, wykresy dla
każdego ćwiczenia, raport dla rodzica/nauczyciela (także do druku), 5-dniowy plan nauki
z checkboxami. Wszystko zapisuje się wyłącznie w `localStorage` tej przeglądarki.

## Inspiracje z sieci (typy zadań)

- gap-fill, true/false, error correction, „Find someone who", pytania i krótkie odpowiedzi —
  Teach-This, *Verb To Be ESL Games, Activities and Worksheets*:
  https://www.teach-this.com/grammar/verb-to-be oraz arkusz
  https://www.teach-this.com/images/resources/verb-to-be-practice.pdf
- multiple choice, sentence re-ordering, question & answer matching — ESL Lounge:
  https://www.esl-lounge.com/level1aworksheets.php
- budowanie zdań, przeczenia z użyciem kontrakcji, mixed tasks dla dzieci A1 — Twee:
  https://twee.com/esl-grammar-worksheets/negative-and-affirmative-sentences-with-the-verb-to-be-for-kids
- informacja zwrotna natychmiast, gry na czas, drag & drop, spaced repetition, odznaki —
  5minuteenglish: https://5minuteenglish.com/how-to-use-interactive-exercises-to-master-english-grammar/ ,
  English Bright ESL: https://englishbrightesl.com/english-grammar-practice/ ,
  ESL Games Plus: https://www.eslgamesplus.com/fun-games/ ,
  grade-university: https://grade-university.com/blog/tools-to-help-teach-grammar
- dodatkowo formy ćwiczeń wprost z „Przygotowania do sprawdzianu" i „Language revision"
  w zeszycie ćwiczeń: uzupełnianie luk, dobieranie pytań do odpowiedzi, zdania z jednym
  niepotrzebnym wyrazem, rozumienie tekstu T/F.

## Pliki

```
index.html              struktura (odwołuje się do css/js)
styles.css              wygląd, responsywność, tryb druku
data.js                 cała baza treści: reguły, słownictwo, 15 banków zadań
app.js                  silnik ćwiczeń, fiszki, kartkówka, postępy, głos
to-be-kartkowka.html    WYGENEROWANY jeden plik (CSS + JS w środku) — główny plik do użytku
build.py                scala powyższe w to-be-kartkowka.html:  python3 build.py
validate.js             sprawdza spójność bazy zadań:            node validate.js
README.md               ten plik
```

Po każdej zmianie w `index.html` / `styles.css` / `data.js` / `app.js` uruchom `python3 build.py`,
żeby zaktualizować wersję jednoplikową.

## Sprawdzenie jakości

```
node --check data.js && node --check app.js && node validate.js && python3 build.py
```

Dodatkowo strona została uruchomiona w jsdom i przećwiczona end-to-end: otwarcie wszystkich
15 ćwiczeń, odpowiedzi w reprezentatywnych typach zadań, fiszki, oba tryby kartkówki,
raport, druk i reset postępów — bez błędów JS.
