# Strona do nauki "to be" — katalog publikowany

Ten katalog zawiera jedyną część repozytorium przeznaczoną do udostępniania
innym osobom (np. przez Telegram):

- `index.html` — wygenerowany, samodzielny plik z `nauka-to-be/to-be-kartkowka.html`
  (wersja 4: 5 698 zadań / 5 266 różnych treści, 16 ćwiczeń, **jeden globalny rejestr
  bez powtórek**, 6 sprawdzianów do druku, Laboratorium zdań, Miks, samoocena 🚦,
  oceny 1–6, powtórki z błędów, tryb ciemny)
- `sprawdzian-1.html` … `sprawdzian-6.html` — 6 rozłącznych arkuszy A4 po 15 zadań,
  każdy z kluczem odpowiedzi dla rodzica na osobnej stronie
- `sprawdziany-wszystkie.html` — wszystkie 6 arkuszy + 6 kluczy w jednym pliku do druku
- **`sprawdzian-1.pdf` … `sprawdzian-6.pdf`** — 6 OSOBNYCH testów w PDF (A4, 2 strony:
  arkusz + klucz). To główna forma do druku.
- `klucz-1.pdf` … `klucz-6.pdf` — same klucze, 1 strona każdy
- `sprawdziany-wszystkie.pdf` (13 stron) i `klucze-wszystkie.pdf` (6 stron) — wydania zbiorcze
- `sciaga-to-be.png` — ściąga strona 1/2: odmiana + zasady + duży kod QR
  (także miniaturka Open Graph)
- `sciaga-to-be-2.png` — ściąga strona 2/2: błędy, pytania, mini-kartkówka, plan
- `.nojekyll` — żeby GitHub Pages nie pominął plików z kropką

Po zmianach w źródłach:

```
cd nauka-to-be
python3 build.py
cp to-be-kartkowka.html ../docs/index.html
node make_sprawdziany.js            # odświeża 7 plików HTML w ../docs/
PYTHONPATH=/tmp/pylibs python3 make_sprawdziany_pdf.py   # odświeża 14 PDF-ów w ../docs/
PYTHONPATH=/tmp/pylibs python3 make_sciaga.py   # odświeża obie ściągi w ../docs/
node validate.js
```

## Jak to otwiera odbiorca

GitHub Pages nie jest jeszcze włączony (API i workflow zwróciły 403 — trzeba
kliknąć ręcznie w `Settings → Pages`: branch `arena/f423a839-sterownik-pieca-c-o`,
folder `/docs`). Docelowy adres to wtedy
`https://zielinski25.github.io/Sterownik-Pieca-C.O./`.

Na już działa krótki link `https://is.gd/gAbph1`, który prowadzi przez
`htmlpreview.github.io` do `raw.githubusercontent.com/.../docs/index.html`.
To konieczne, bo jsDelivr i raw.githubusercontent serwują pliki `.html` jako
zwykły tekst — przeglądarka pokazałaby kod źródłowy zamiast strony.

Szczegóły, gotowa wiadomość i linki do plików na Dysku:
`../nauka-to-be/UDSTEPNIJ-TELEGRAM.txt`.
