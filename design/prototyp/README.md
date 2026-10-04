# Prototyp HTML aplikacji Sterownik CO

Makieta pulpitu, kafelków, ilustracji i paneli ustawień — do dopracowania designu
**przed** przeniesieniem do aplikacji Android (kompilacja Kotlina jest wolna, HTML odświeża się natychmiast).

## Jak uruchomić
* Otwórz `index.html` w przeglądarce (działa z pliku, bez serwera, bez internetu),
  albo `python3 -m http.server 8080 --directory design/prototyp` i wejdź na `http://localhost:8080/`.
* Na telefonie: najlepiej przez serwer (adres komputera w sieci lokalnej) — układ jest responsywny (360–768 px).

## Co jest w środku
| Plik | Zawartość |
|---|---|
| `index.html` | szkielet ekranu: pasek górny, pulpit (hero, karty szybkie, kafelki), Ustawienia, Więcej, arkusz dolny, toast, dialog, nawigacja |
| `style.css` | paleta = `object C` z `MainActivity.kt`, układ kafelków = `buildDashboard()`, stany `.warn/.err/.ok/.dis/.stale/.sim` i animacje ilustracji przeniesione z ESP (`main_centrala.cpp`) |
| `icons.js` | `window.ILU` — ilustracje kafelków (SVG wielowarstwowe, animowane, zależne od stanu) + ikony nawigacji |
| `app.js` | makieta statusu Firebase (`S`), render pulpitu 1:1 z `renderDashboard()`/`odswiezKafelki()`, panele ustawień 1:1 z `dpOtworz*()` ESP, polecenia → potwierdzenie → toast ACK, szuflada DEMO |

## Szuflada DEMO (zakładka na prawej krawędzi)
Przełącza stany makiety: online/offline, noc, pompa/mieszadło/rozpalanie, tryb serwa, alarmy, nieświeży pomiar dymu,
symulacja bojlera, suwaki odczytów oraz szerokość ekranu (360 / 412 / 480 / tablet).

Po akceptacji designu wszystko z tego katalogu przenosimy do `TileArt.kt`, `DashboardTileView.kt` i `MainActivity.kt`.
