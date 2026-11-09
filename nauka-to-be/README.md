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

> **Wersja 4 · koniec powtórek + sprawdziany do druku:** 5 698 zadań
> (5 266 naprawdę różnych treści), **16 ćwiczeń**, **6 sprawdzianów A4 z kluczami**,
> jeden współdzielony rejestr treści, system ocen 1–6 i powtórki rozłożone w czasie.

**🧠 Jeden globalny rejestr — dziecko nie dostaje dwa razy tego samego zdania.**
Wcześniej każde ćwiczenie miało własny licznik `S.seen[key]`, a próbna kartkówka, Miks,
szybka runda i wydruk **kopiowały** pytania z „Wybierz formę" / „Wpisz formę" (1 006 z 1 044
pytań kartkówki to były kopie!). Pary w Memory i „Połącz w pary" też się powtarzały, fiszki
tasowały te same karty, a pytanie dnia było stałe. Teraz `data.js` nadaje każdemu zadaniu
kanoniczną **sygnaturę treści** (`sig()` z obszarem umiejętności), a `app.js` trzyma jeden
rejestr `S.used` dla całej strony. `deal()` bierze wyłącznie treści jeszcze nigdzie
niewidziane; gdy pula się wyczerpie, zaczyna nowy cykl od **najdawniej** widzianych
(nigdy losowo). Rejestr obejmuje ćwiczenia, próbną kartkówkę, Miks, szybką rundę, fiszki,
pytanie dnia i sprawdziany do druku. Jedyny celowy powrót do zadania to zakładka
**Powtórki** (błędy według pudełek Leitnera). Starsze zapisy `S.seen` są migrowane
przy starcie (`migrateSeenToUsed()`), więc po aktualizacji strony nic nie wraca od nowa.

**Baza jest też uczciwie policzona.** `examPool` (1 044 pytania) ma teraz **własną** pulę
kombinacji podmiot + okolicznik, której nie zużyły ćwiczenia — `validate.js` sprawdza,
że żadne pytanie kartkówki nie kopiuje zdania z „Wybierz formę" / „Wpisz formę".
Zestawy Memory i „Połącz w pary" mają rozłączne pary (215 i 162 unikalnych), a w metadanych
obok `bankTotal` jest `uniqueContent` — liczba faktycznie różnych treści, bez sztucznego
zawyżania. `data.js` generuje wszystko deterministycznie (stałe ziarno `20261110`) z 41
podmiotów i 88 okoliczników; każde zadanie ma stały `id` i obszar umiejętności.
Baza: 546 pytań wyboru, 460 luk, 385 rozsypanek, 376 przekształceń, 314 krótkich
odpowiedzi, 252 zdań do poprawy, 436 zdań do sortowania, 312 zadań słuchowych,
224 dyktand, 151 zadań pisowni, 116 tłumaczeń, 27 tekstów, 84 fiszki,
54 zestawy memory/matching, 1 044 pytania kartkówkowe i 761 pytań do sprawdzianów.

**📄 6 sprawdzianów do druku.** Zakładka **Sprawdziany** ma 6 przygotowanych,
rozłącznych arkuszy po 15 zadań (każdy: 4 × odmiana, 4 × pytania, 4 × przeczenia,
3 × pułapki), podgląd, druk pojedynczego arkusza, osobny klucz odpowiedzi dla rodzica,
druk wszystkich sześciu oraz **„Wygeneruj nowy komplet"** (90 świeżych pytań z globalnego
rejestru). Zdania są ręcznie dobrane, żeby nie było nienaturalnych kombinacji typu
„My shoes are French".

**Każdy test jest OSOBNYM plikiem PDF** (A4, 2 strony: arkusz dla dziecka + klucz):
`sprawdzian-1.pdf` … `sprawdzian-6.pdf`. Dodatkowo `klucz-1.pdf` … `klucz-6.pdf`
to same klucze na jednej stronie (wygodne, gdy drukujesz tylko dla siebie), a
`sprawdziany-wszystkie.pdf` (13 stron: okładka + 6 arkuszy + 6 kluczy) i
`klucze-wszystkie.pdf` (6 stron) to wydania zbiorcze. PDF-y powstają z tych samych danych
co strona — `make_sprawdziany_pdf.py` czyta `data.js` przez `node`, używa fontu DejaVu Sans
(polskie znaki) i `fpdf2`. Zakładka **Sprawdziany** ma przyciski „Pobierz PDF" prowadzące
na publiczny Dysk Google; druk z przeglądarki działa też bez internetu.
Wersje HTML (`sprawdzian-1.html` … `sprawdzian-6.html`, `sprawdziany-wszystkie.html`)
generuje `node make_sprawdziany.js`. Wszystkie pliki lądują też w `docs/`.

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
**losowa kartkówka do druku z kluczem dla rodzica**, **6 sprawdzianów do druku**,
licznik zestawów i rozmiaru bazy w hubie (pokazuje `uniqueContent`, nie zawyżone `bankTotal`).


**📘 Nauka** — 7 zasad z przykładami czytanymi na głos, pełna tabela odmiany (długa/krótka forma,
przeczenie, pytanie, krótka odpowiedź), 8 pytań z podręcznika, 12 najczęstszych pułapek,
słowniczek (kraje/narodowości, przymiotniki, miejsca) oraz **ściąga do druku** (A4).

**🎮 16 ćwiczeń różnego typu** (każde bierze świeży, niepowtarzalny zestaw z mega-bazy):

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
| 15 | 🧩 Miks 5 minut | 12 zadań z 5 różnych typów w jednym zestawie — **interleaving** |
| 16 | ⚡ Szybka runda | 60 sekund, ile poprawnych form — trening automatyzacji |

**🧪 Laboratorium** (nowa zakładka) — trzy moduły zamiast „kolejnego testu":

| Moduł | Co robi dziecko | Skąd pomysł |
| --- | --- | --- |
| **Maszyna zdań** | Wybiera 3 klocki: **kto** (12 podmiotów), **co robimy** (twierdzenie / przeczenie / pytanie / krótka odpowiedź) i **koniec zdania** (18 końcówek). Strona natychmiast buduje poprawne zdanie, pokazuje formę krótką i wyjaśnia regułę. Tryb „🎯 Sprawdź się" zamienia zdanie w lukę z opcjami. | Kartowa gra „subject + be + ending" (*It Must Be Correct*) oraz tabele substytucji [1] |
| **Moje zdania** | Zapisuje własne zdania z maszyny, odtwarza je głosem i drukuje w ściądze/raporcie. | Własna produkcja (output) i personalizacja — „Write and draw!" [2], [4] |
| **Znajdź kogoś, kto…** | Zadaje pytanie *Are you…?* jednej z 5 postaci i wybiera krótką odpowiedź wśród pułapek *Yes, I'm.* / *amn't*. Profile są wyraźnie oznaczone jako zmyślone. | Klasyczny „Find Someone Who" z yes/no pytaniami [1] |

**🚦 Samoocena, bilety wyjścia i poziomy** — 10 zdań „Umiem…" z trzema światełkami
(🟢 / 🟡 / 🔴) w zakładce **Powtórki**. Czerwony temat ma pierwszeństwo w przycisku
„Ćwicz najsłabszy temat". Po każdym ćwiczeniu pojawia się krótki **bilet wyjścia**
(samoocena od razu po zadaniu jest najtrafniejsza [5]), a w górnym pasku rośnie
**Poziom** (co 100 pkt). Wszystko trafia też do raportu dla rodzica i wydruku.

## Co mówi research (skąd wzięły się nowe moduły)

| Wniosek | Zastosowanie w module | Źródło |
| --- | --- | --- |
| **Retrieval practice + spacing + interleaving** — samo odpowiadanie, rozłożone w czasie i z mieszanką typów zadań, daje trwalszą pamięć niż samo czytanie | Powtórki Leitnera (1/2/4/7/14/30 dni), dzienniczek ocen, ćwiczenie **Miks 5 minut** | [MindBrained / Science of Learning](https://www.mindbrained.org/2021/05/forgetting-to-remember-three-research-based-strategies-applied-to-language-teaching/) , [spaced learning in primary EFL](https://www.researchgate.net/publication/395979915_The_Impact_of_Spaced_Learning_Strategies_on_Vocabulary_Retention_in_Primary_English_Classrooms) |
| **Interleaving**: w krótkim teście mieszaj różne obszary i typy zadań („Big Basket Quiz") | Miks losuje z 5 banków: wybór, luka, przekształcenie, krótka odpowiedź, poprawa błędu | [Teach-This — Verb to be ESL games](https://www.teach-this.com/grammar/verb-to-be) |
| **Uczenie przez własną produkcję + dual coding** (tekst, obraz, dźwięk) wzmacnia zapamiętywanie | Maszyna zdań (kolorowe klocki + 🔊 głos + wyjaśnienie), własne zdania, dwustronicowa ściąga z kolorami | [Profe Magnan — retrieval & UDL](https://profemagnan.com/the-power-of-retrieval-practice/) , [English in Action — evidence-based teaching](https://englishinaction.com/blog/book-review-an-introduction-to-evidence-based-teaching-in-the-english-language-classroom/) |
| **Samoocena „I can…" i światła drogowe** rozwijają metakontrolę; ocena tuż po zadaniu jest celniejsza niż ogólna | 10 zdań „Umiem…", bilety wyjścia, czerwony temat priorytetowy | [Assessment for Learning in Primary Language Learning and Teaching](https://dokumen.pub/assessment-for-learning-in-primary-language-learning-and-teaching-9781800410657.html) , [exit tickets](https://www.worksheetzoo.com/printable-exit-tickets) |
| **T/F, wybór formy, „Make it right!", pytania o dane osobowe i rysunek o sobie** — sprawdzony zestaw zadań A1 | 15 typów ćwiczeń + mini-kartkówka na ściądze | [British Council LearnEnglish Kids — present simple: verb 'to be'](https://learnenglishkids.britishcouncil.org/sites/kids/files/attachment/grammar-practice-present-simple-verb-to-be-worksheet.pdf) , [grammar games wersja](https://learnenglishkids.britishcouncil.org/sites/kids/files/attachment/grammar-games-present-simple-verb-to-be-worksheet.pdf) |

**📄 Ściąga PNG (2 strony)** — `sciaga-to-be.png` (odmiana, 4 zasady + pułapka,
duży kod QR) oraz `sciaga-to-be-2.png` (6 najczęstszych błędów, pytania o dane osobowe,
mini-kartkówka 10 zdań z kluczem dla rodzica, plan 3 kroków i legenda samooceny).
Obie są 1080×1560, do druku A4 i do wysłania jako zdjęcie na Telegramie.

**🗂️ Fiszki** — 84 karty z prostą powtórką rozłożoną w czasie („Umiem" odkłada kartę,
„Jeszcze ćwiczę" wraca szybciej).

**📝 Kartkówka próbna** — wybór 10, 15, 20 lub 30 zadań z 1 044-elementowej puli
(każde podejście to inny zestaw) w dwóch trybach: *tryb nauki* (wyjaśnienie od razu)
i *tryb kartkówki* (bez podpowiedzi, z czasem, oceną, omówieniem odpowiedzi na końcu
i przyciskiem przejścia do powtórki błędów). Dodatkowo **🖨️ losowa kartkówka do druku
z kluczem odpowiedzi** dla rodzica.

**📊 Postępy** — punkty XP, serie poprawnych odpowiedzi, skuteczność, 18 odznak,
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
data.js                 5 698 zadań: generator mega-bazy, sygnatury treści, 6 sprawdzianów, reguły, stałe id
app.js                  JEDEN globalny rejestr S.used, oceny, powtórki, kartkówka, sprawdziany, postępy, głos
to-be-kartkowka.html    WYGENEROWANY jeden plik (CSS + JS w środku) — główny plik do użytku
build.py                scala powyższe w to-be-kartkowka.html:  python3 build.py
validate.js             spójność bazy, unikalność id i treści, globalny rejestr:  node validate.js
make_sprawdziany.js     generuje sprawdzian-1..6.html + sprawdziany-wszystkie.html (także do ../docs/)
make_sprawdziany_pdf.py generuje 14 PDF-ow A4: 6 osobnych testow, 6 kluczy, 2 wydania zbiorcze
                        (wymaga: pip install fpdf2; font DejaVu Sans z systemu)
sprawdzian-*.pdf        OSOBNE testy do druku: arkusz + klucz na 2 stronach
klucz-*.pdf             same klucze odpowiedzi, 1 strona kazdy
sprawdziany-wszystkie.pdf / klucze-wszystkie.pdf   wydania zbiorcze (13 / 6 stron)
sprawdzian-*.html       wersje HTML tych samych arkuszy, bez zewnetrznych zasobow
make_sciaga.py          generuje obie ściągi PNG (wymaga pillow + qrcode)
sciaga-to-be.png        ściąga str. 1 (odmiana + zasady + duży QR)
sciaga-to-be-2.png      ściąga str. 2 (błędy + pytania + mini-kartkówka + plan)
UDSTEPNIJ-TELEGRAM.txt  gotowa wiadomość, linki i instrukcja udostępniania
README.md               ten plik
../docs/                wersja publikowana (index.html + obie ściągi PNG)
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
3. **Zdjęcia** — `sciaga-to-be.png` (tabela + zasady + duży kod QR do strony) oraz
   `sciaga-to-be-2.png` (błędy, pytania, mini-kartkówka z kluczem, plan) wysłane jako
   zdjęcia pokazują się bezpośrednio na czacie.

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
node --check data.js && node --check app.js && node validate.js
python3 build.py && node make_sprawdziany.js
PYTHONPATH=/tmp/pylibs python3 make_sprawdziany_pdf.py   # wymaga fpdf2 (pip install fpdf2)
```

`validate.js` sprawdza m.in. spójność `sig()` w `data.js` z `contentKey()` w `app.js`
(liczba różnych treści musi się zgadzać), brak kopii zdań z ćwiczeń w `examPool`,
rozłączność wszystkich 6 sprawdzianów, kompletność kluczy odpowiedzi i obecność
gotowych plików HTML w `docs/`.

Dodatkowo strona została uruchomiona w jsdom i przećwiczona end-to-end: otwarcie wszystkich
16 ćwiczeń, odpowiedzi w reprezentatywnych typach zadań, fiszki, oba tryby kartkówki,
raport, druk i reset postępów — bez błędów JS. Test `smoke-extras` ze współdzielonym
`localStorage` potwierdza, że trzy kolejne wizyty dają rozłączne zestawy zadań, że po
wyczerpaniu rejestru startuje cykl 2, że oceny i błędy zapisują się, że powtórka kończy się
oceną oraz że tryb ciemny, wyciszanie i wydruk kartkówki działają. Test `smoke-lab`
sprawdza maszynę zdań, własne zdania, „Find Someone Who", światła samooceny, Miks,
bilet wyjścia, poziom i trwałość nowych danych po restarcie. Test `smoke-norepeat`
sprawdza migrację starych zapisów `S.seen → S.used`, współdzielenie rejestru przez
5 różnych ćwiczeń i próbną kartkówkę, 6 kart sprawdzianów, podgląd 15 pytań,
druk arkusza + klucza / samego klucza / wszystkich 12 stron, generator nowego kompletu
(90 świeżych, rozłącznych pytań) oraz uczciwy licznik `uniqueContent` w hubie.
