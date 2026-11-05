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

> **Wersja 2 · mega-baza:** 4 949 unikalnych zadań, system ocen 1–6, powtórki
> rozłożone w czasie z tego, czego dziecko nie umie, oraz 8 dodatków.

**🧠 Bardzo duża baza i zero powtórek.** `data.js` generuje deterministycznie (stałe ziarno
`20261110`) 4 949 zadań z 41 podmiotów i 88 okoliczników; każde ma stały `id` i przypisany
obszar umiejętności. Silnik `deal()` zapisuje w `localStorage` indeksy już widzianych zadań
osobno dla każdego ćwiczenia — **każde wejście daje całkiem nowy zestaw**, także po
zamknięciu karty, odświeżeniu strony i następnego dnia. Gdy pula się wyczerpie, zaczyna się
nowy cykl (numer widać w nagłówku ćwiczenia). Baza: 546 pytań wyboru, 460 luk,
385 rozsypanek, 376 przekształceń, 314 krótkich odpowiedzi, 252 zdań do poprawy,
436 zdań do sortowania, 312 zadań słuchowych, 224 dyktand, 151 zadań pisowni,
116 tłumaczeń, 27 tekstów, 1 044 pytania kartkówkowe, 84 fiszki, 54 zestawy memory/matching.
Wszystkie treści są unikalne — `validate.js` sprawdza brak duplikatów.

**🎓 System oceniania.** Po każdym ćwiczeniu, szybkiej rundzie, powtórce i próbnej kartkówce
pojawia się ocena w skali szkolnej: `95%+ → 6 celujący`, `88%+ → 5 bardzo dobry`,
`72%+ → 4 dobry`, `56%+ → 3 dostateczny`, `36%+ → 2 dopuszczający`, poniżej → `1 jeszcze ćwiczę`.
W **Postępach** jest dzienniczek z wykresem ostatnich 12 wyników, średnią i **ocenami
cząstkowymi według umiejętności** (odmiana, przeczenia, pytania, krótkie odpowiedzi,
czytanie, słuchanie, pisownia, tłumaczenie). Raport dla rodzica uwzględnia oceny i powtórki.

**🔁 Powtórka tego, czego dziecko nie umie.** Każda błędna odpowiedź trafia do zakładki
**Powtórki** z tematem i wyjaśnieniem. Zadanie wraca po 1, 2, 4, 7, 14 i 30 dniach
(pudełka Leitnera); poprawna odpowiedź przesuwa je dalej, błąd resetuje na jutro,
a po 6 pudełku jest „opanowane". Są statystyki, lista trudnych zadań, przycisk
**„Ćwicz najsłabszy temat"** i powtórka zakończona oceną.

**🎁 Inne dodatki:** 3 nowe odznaki (Pracuś 100 zadań, Naprawiacz 8 błędów, Szóstkowicz),
+5 XP przy serii 5 odpowiedzi, **pytanie dnia** (stałe dla danego dnia), **tryb ciemny**,
**wyciszanie głosu** (oba zapamiętane), **wybór liczby zadań kartkówki** (10/15/20/30),
**losowa kartkówka do druku z kluczem dla rodzica**, licznik zestawów i rozmiaru bazy w hubie.


**📘 Nauka** — 7 zasad z przykładami czytanymi na głos, pełna tabela odmiany (długa/krótka forma,
przeczenie, pytanie, krótka odpowiedź), 8 pytań z podręcznika, 12 najczęstszych pułapek,
słowniczek (kraje/narodowości, przymiotniki, miejsca) oraz **ściąga do druku** (A4).

**🎮 15 ćwiczeń różnego typu** (każde bierze świeży, niepowtarzalny zestaw z mega-bazy):

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

**🗂️ Fiszki** — 84 karty z prostą powtórką rozłożoną w czasie („Umiem" odkłada kartę,
„Jeszcze ćwiczę" wraca szybciej).

**📝 Kartkówka próbna** — wybór 10, 15, 20 lub 30 zadań z 1 044-elementowej puli
(każde podejście to inny zestaw) w dwóch trybach: *tryb nauki* (wyjaśnienie od razu)
i *tryb kartkówki* (bez podpowiedzi, z czasem, oceną, omówieniem odpowiedzi na końcu
i przyciskiem przejścia do powtórki błędów). Dodatkowo **🖨️ losowa kartkówka do druku
z kluczem odpowiedzi** dla rodzica.

**📊 Postępy** — punkty XP, serie poprawnych odpowiedzi, skuteczność, 15 odznak,
dzienniczek ocen z wykresem, oceny cząstkowe według umiejętności, wykresy dla każdego
ćwiczenia, raport dla rodzica/nauczyciela (także do druku), 5-dniowy plan nauki
z checkboxami. Wszystko zapisuje się wyłącznie w `localStorage` tej przeglądarce
(klucz `tobe-kartkowka-v1`, migracja ze starszej wersji bez utraty postępów).

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
styles.css              wygląd, responsywność, tryb ciemny, tryb druku
data.js                 4 949 zadań: generator mega-bazy, reguły, słownictwo, stałe id
app.js                  silnik bez powtórek, oceny, powtórki, kartkówka, postępy, głos
to-be-kartkowka.html    WYGENEROWANY jeden plik (CSS + JS w środku) — główny plik do użytku
build.py                scala powyższe w to-be-kartkowka.html:  python3 build.py
validate.js             spójność bazy, unikalność id i treści:   node validate.js
make_sciaga.py          generuje sciaga-to-be.png (wymaga pillow + qrcode)
sciaga-to-be.png        ściąga do wysłania jako zdjęcie na Telegramie
UDSTEPNIJ-TELEGRAM.txt  gotowa wiadomość, linki i instrukcja udostępniania
README.md               ten plik
../docs/                wersja publikowana (index.html + sciaga-to-be.png)
```

Po każdej zmianie w `index.html` / `styles.css` / `data.js` / `app.js` uruchom `python3 build.py`,
żeby zaktualizować wersję jednoplikową.

## Udostępnianie przez Telegram 📨

Telegram nie renderuje HTML-u w czacie, dlatego przygotowano trzy drogi:

1. **Link (polecane)** — krótki adres **`https://is.gd/gAbph1`** prowadzi przez
   `htmlpreview.github.io` do `docs/index.html` z tej gałęzi. Wklejony do
   wiadomości otwiera się od razu w przeglądarce Telegrama jako gotowa strona.
   Pełny adres zapasowy i wyjaśnienie w `UDSTEPNIJ-TELEGRAM.txt`.
   ⚠️ Nie linkuj bezpośrednio do `cdn.jsdelivr.net/.../index.html` ani
   `raw.githubusercontent.com/.../index.html` — serwują HTML jako tekst,
   więc przeglądarka pokaże kod zamiast strony (do PNG i do załącznika są OK).
   Strona ma tagi Open Graph (podgląd z miniaturką ściągi) oraz przycisk
   **„🔗 Udostępnij"** (`navigator.share`, fallback = kopiowanie linku).
2. **Plik** — `to-be-kartkowka.html` wysłany jako dokument działa u odbiorcy offline.
3. **Zdjęcie** — `sciaga-to-be.png` (tabela + zasady + kod QR do strony) wysłane jako
   zdjęcie pokazuje się bezpośrednio na czacie.

Gotową wiadomość, instrukcję krok po kroku, linki do Dysku oraz tekstową
„kartkówkę na czacie" (plan B) zawiera **`UDSTEPNIJ-TELEGRAM.txt`**.
Ściągę generuje `make_sciaga.py` (`pip install pillow qrcode`).

Katalog `docs/` (strona + ściąga + `.nojekyll`) jest jedyną częścią repozytorium
przeznaczoną do publikacji; w `docs/README.md` opisano, jak go aktualizować.
Włączenie GitHub Pages wymaga ręcznej zmiany w ustawieniach repozytorium — token
aplikacji GitHub w tej sesji ma dostęp do kodu, ale nie do ustawień (HTTP 403,
tak samo zakończyła się próba włączenia Pages przez workflow Actions).
Po ręcznym włączeniu strona dostanie własny adres
`https://zielinski25.github.io/Sterownik-Pieca-C.O./`.

## Sprawdzenie jakości

```
node --check data.js && node --check app.js && node validate.js && python3 build.py
```

Dodatkowo strona została uruchomiona w jsdom i przećwiczona end-to-end: otwarcie wszystkich
15 ćwiczeń, odpowiedzi w reprezentatywnych typach zadań, fiszki, oba tryby kartkówki,
raport, druk i reset postępów — bez błędów JS. Osobny test (`smoke-extras`) ze współdzielonym
`localStorage` potwierdza, że trzy kolejne wizyty dają rozłączne zestawy zadań, że po
wyczerpaniu bazy startuje cykl 2, że oceny i błędy zapisują się, że powtórka kończy się
oceną oraz że tryb ciemny, wyciszanie i wydruk kartkówki działają.
