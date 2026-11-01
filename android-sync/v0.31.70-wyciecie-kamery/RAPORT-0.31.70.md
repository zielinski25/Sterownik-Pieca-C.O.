# 0.31.70 — czarny pas przy wycięciu kamery (lewa strona w poziomie)

Data: 2026-11-01 · baza: 0.31.69 · zakres: **tylko ten pas, nic więcej**

---

## 1. Zgłoszenie

Po wgraniu 0.31.69 (aplikacja zajmuje całe okno) po **lewej stronie w poziomie**
został czarny pas.

## 2. Pomiar — co to dokładnie jest

Zrzut 2400×1080 px, gęstość 2,94 px/dp:

| obszar | piksele | dp | kolor |
|---|---|---|---|
| lewy pas | 0–109 (szer. **110 px**) | **37 dp** | **#000000** (czysta czerń) |
| aplikacja (0.31.69) | 110–2267 | 734 dp | tło Pal.Bg #060D18 |
| pasek nawigacji Androida | 2268–2399 | 45 dp | #081422 = Pal.Nav #081421 |

**Dowód, że to nie nasz układ:** żadne tło aplikacji nie ma koloru #000000
(`Pal.Bg` #060D18, `Pal.Nav` #081421, `Pal.Top` #071421, `@color/bg` #060D18).
Gdyby pas należał do okna aplikacji, miałby kolor `Pal.Bg`. Jest czarny, więc
leży **poza oknem** — rezerwuje go system.

**Ten sam pas był w 0.31.68** — te same 110 px. Nie było go widać, bo obok
stała dużo większa czarna ramka (948 px). Wniosek: 0.31.69 tego nie zrobiła,
0.31.69 tylko odsłoniła istniejący problem.

## 3. Przyczyna

Motyw `Theme.SterownikCo` nie ustawiał `windowLayoutInDisplayCutoutMode`, więc
działał tryb domyślny (`LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT`): pod wycięciem
ekranu (kamera w ekranie / punch-hole) **wolno rysować tylko w pionie**.
W poziomie system omija wycięcie i zamalowuje jego obszar na czarno
(letterbox). Dotyczy każdej aplikacji, która tego nie zadeklaruje.

Ten sam objaw, niezależnie opisany w źródłach:

- `SansNope/UnleashedRecomp-Android` PR #153: „a window is allowed under the
  cutout in portrait, but avoids it in landscape, leaving a black bar over the
  cutout area" — naprawa: `LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES`,
  **potwierdzona na fizycznym urządzeniu** (OnePlus 13): „the black bar is gone".
- `matasarei/hcboard` PR #49: „Android defaulted to
  LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT, causing WindowManager to place a
  **black letterbox mask over the cutout area**".
- `a3ryk/classtrack` v2.0.0-alpha.7: „**Zero Punch Hole Letterboxing** …
  Eliminates the black rectangular letterbox bar across camera cutouts".
- Stack Overflow (m.in. 56353716, 77445032, reddit r/androiddev „Why does my
  Game have a black bar on the side?" — dokładnie ten sam przypadek: pas po
  stronie dziurki na kamerę).

## 4. Decyzje

1. **`shortEdges` w motywie** (nowy `res/values-v27/themes.xml`, bo atrybut
   istnieje od API 27): pozwala rysować pod wycięciem i w pionie, i w poziomie.
2. **To samo w kodzie, w `MainActivity.onCreate`**, przed pierwszym rysowaniem:
   część urządzeń (m.in. Samsung, wg wątku SO) ignoruje sam atrybut z motywu.
   Warunek `SDK_INT >= P` jest obowiązkowy — stała `SHORT_EDGES` istnieje od
   API 28, a `minSdk` aplikacji to **26**.
3. **`displayCutoutPadding()` na głównym `Row`**: skoro obszar wycięcia jest już
   częścią okna, interfejs odsuwa się od samej dziurki — żadna ikona ani napis
   nie wchodzi pod obiektyw. W pionie padding = 0 (wycięcie jest nad oknem),
   więc układ w pionie się nie zmienia.
4. **Tło pod wycięciem = `Pal.Bg`** (już było po 0.31.69) — pas ma wyglądać jak
   tło aplikacji, nie jak czarna dziura.
5. **Nie ruszam:** paska statusu (37 dp u góry) ani paska nawigacji Androida
   (45 dp po prawej) — to elementy systemu, nie aplikacji. Nie ruszam też
   układu z 0.31.69 ani blokady orientacji (nie ma jej i nie będzie).

Czego **nie** zrobiłem i dlaczego: nie „dorysowałem” tła w miejsce pasa innym
kolorem ani nie przesuwałem całego okna — pas jest poza oknem, więc malowanie
po nim byłoby niemożliwe. Jedyna droga to zgoda na rysowanie pod wycięciem.

## 5. Zmiany (4 pliki)

| plik | zmiana |
|---|---|
| `res/values-v27/themes.xml` **(nowy)** | ten sam motyw + `windowLayoutInDisplayCutoutMode = shortEdges` |
| `MainActivity.kt` | `window.attributes.layoutInDisplayCutoutMode = SHORT_EDGES` (API ≥ 28) |
| `ui/SterownikApp.kt` | `Row(Modifier.fillMaxSize().displayCutoutPadding())` + import |
| `app/build.gradle.kts` | 0.31.70 / 31070 |
| `README-WGRANIE.txt` | opis przyczyny, dowód z kolorów, instrukcja sprawdzenia |

## 6. Weryfikacja

| sprawdzenie | wynik |
|---|---|
| brama `narzedzia/sprawdz_uklad.py` na 0.31.70 | **32 OK / 0 BŁĄD** |
| ta sama brama na 0.31.69 (kontrola negatywna) | **8 BŁĄD** (m.in. brak motywu, brak insetu, zła wersja) |
| ZIP rozpakowany = źródła | bit w bit, 198 pozycji (+2: `values-v27/`) |
| balans nawiasów w zmienionych plikach | OK |
| **kompilacja** | **nie sprawdzona** — brak Javy/Gradle w środowisku |

Ryzyko kompilacji: jedno nowe API Compose (`displayCutoutPadding`, dostępne od
foundation 1.4 — projekt ma BOM 2024.12.01) i jedno API platformy
(`layoutInDisplayCutoutMode`, od API 28, za warunkiem wersji). Oba użyte
zgodnie z podpisem.

## 7. Czego się spodziewać po wgraniu

- **W poziomie:** pas po lewej ma kolor tła aplikacji (ciemny granat #060D18),
  nie czerń; szyna nawigacji przesuwa się o 37 dp w prawo, ale pozostaje tej
  samej szerokości (80 dp); nic nie wchodzi pod kamerę.
- **W pionie:** bez zmian (padding wycięcia = 0).
- **Jeśli pas zostanie czarny:** to nie wycięcie — wtedy potrzebny zrzut
  w pionie i wersja Androida, żeby nie zgadywać.

## 8. Plik

`SterownikCO-Android-v0.31.70-PELNY-PROJEKT.zip`, 1 196 952 B,
SHA-256 `1677b87d6b90b04f780c1cebd50469d1dc819b37b8def433d1e466e9576d9e01`.
