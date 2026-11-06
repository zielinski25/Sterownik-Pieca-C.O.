# Strona do nauki "to be" — katalog publikowany

Ten katalog zawiera jedyną część repozytorium przeznaczoną do udostępniania
innym osobom (np. przez Telegram):

- `index.html` — wygenerowany, samodzielny plik z `nauka-to-be/to-be-kartkowka.html`
  (wersja 3: 4 949 zadań, 16 ćwiczeń, Laboratorium zdań, Miks, samoocena 🚦,
  oceny 1–6, powtórki z błędów, tryb ciemny)
- `sciaga-to-be.png` — ściąga strona 1/2: odmiana + zasady + duży kod QR
  (także miniaturka Open Graph)
- `sciaga-to-be-2.png` — ściąga strona 2/2: błędy, pytania, mini-kartkówka, plan
- `.nojekyll` — żeby GitHub Pages nie pominął plików z kropką

Po zmianach w źródłach:

```
cd nauka-to-be
python3 build.py
cp to-be-kartkowka.html ../docs/index.html
PYTHONPATH=/tmp/pylibs python3 make_sciaga.py   # odświeża obie ściągi w ../docs/
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
