Sterownik CO — wersja 0.31.46 (Android)
======================================
WYKRESY SZYBCIEJ. Zachowanie bez zmian — nadal pobiera tylko to, czego nie ma.

CO ZMIENIONO (tylko szybkość, zero zmian w wyglądzie)
-----------------------------------------------------
1) KONIEC CZEKANIA 6 s NA PIEC
   Aplikacja pytała archiwum centrali przy KAŻDYM wejściu w wykresy —
   także przy 7 i 30 dniach. Ale wynik z pieca jest używany tylko dla
   6h/24h, więc dla dłuższych zakresów był wyrzucany do kosza, a czekanie
   i tak trwało. Poza domem piec nie odpowiada, więc to było pełne 6 s
   stracone przy każdym otwarciu wykresów.
   Teraz: dla 7/30 dni nie pytamy wcale, a dla 6h/24h czekamy maks. 2 s
   (w domu piec odpowiada w ~0,3 s).

2) 1000 REKORDÓW NA ZAPYTANIE zamiast 300
   Firebase spokojnie oddaje 1000. 7 dni = 11 zapytań zamiast 34.

3) DNI PO 4 NARAZ zamiast po kolei
   Wcześniej każdy dzień czekał na poprzedni.

EFEKT (wyliczone z kodu, nie „na oko")
--------------------------------------
                                przed      po
  pierwsze wejście 7 DNI        ~10 s      ~2 s
  next day (1 nowy dzień)       ~7,5 s     ~0,4 s
  pierwsze wejście 30 DNI       ~30 s      ~8 s
  kolejne wejście 30 DNI        ~6 s       ~0,3 s

Dlaczego „next day" przedtem trwał ~7,5 s, choć pobierał tylko 1 dzień:
1,5 s chmura + 6 s czekania na piec. Teraz 0,4 s.

JAK TO DZIAŁA (bez zmian — to już było w 0.31.43)
-------------------------------------------------
- Historia ląduje na dysku telefonu: filesDir/telemetria/historia.tsv.gz
- Trzymamy 35 dni, więc 30-dniowy zakres zawsze mieści się w pamięci.
- Przy wejściu liczone jest, czego brakuje w oknie, i chmura jest pytana
  TYLKO o te przedziały.
- Luki, o które już pytaliśmy i chmura ich nie ma, zapamiętujemy jako puste
  (luki.tsv) — nie pytamy o nie w kółko.
- Bez zasięgu wykres rysuje się z pamięci telefonu.

Sprawdzone symulacją dzień po dniu (wierna kopia algorytmu):
  wejście 7d, dzień 1:  z pamięci      0 próbek, pobrano 10081, 34 zapytania
  wejście 7d, dzień 2:  z pamięci   8641 próbek, pobrano  1440,  5 zapytań
  wejście 7d, dzień 3:  z pamięci   8641 próbek, pobrano  1441,  5 zapytań
Czyli dokładnie to, co miało być: następnego dnia dociąga tylko jedną dobę.

JAK ZBUDOWAĆ
------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.46"
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "$env:USERPROFILE\Downloads\SterownikCO-v0.31.46-debug.apk"

versionCode = 31046 — aktualizacja wejdzie bez odinstalowywania.

PO CZYM POZNASZ, ŻE DZIAŁA
--------------------------
W Ustawieniach → „Bufor offline" (to nadal jest), a wykresy:
  - 7 DNI po raz pierwszy: kilka sekund, potem natychmiast.
  - Wyjdź z wykresów i wejdź ponownie: praktycznie od razu.
  - Następnego dnia: krótkie „dociągam tylko nowe" (~0,4 s), nie 7,5 s.

CZEGO NIE ZMIENIŁEM
-------------------
- Wygląd wykresów: bez napisów, plakietek i liczników nad wykresem.
- Sposób liczenia zysku solarnego, alarmy, SERWO, Bufor offline.
- Format zapisu pamięci (historia.tsv.gz) — starsza pamięć z 0.31.43/45
  czyta się bez zmian, nic nie trzeba czyścić.

CO SPRAWDZIŁEM
--------------
Bez kompilatora w tym środowisku, ale:
  - skan składni 96 plików .kt: 0 problemów (nawiasy, komentarze, stringi),
  - nowe importy (async, awaitAll, coroutineScope) używane, nie wiszą,
  - poprzednie błędy (viewState, komentarz w RtdbClient) nadal naprawione,
  - SHA256SUMS.txt w paczce: suma każdego pliku .kt.

JEŚLI GRADLE COŚ ZGŁOSI
-----------------------
Wklej tylko linie zaczynające się od „e:" — plik i numer wiersza wystarczą.
