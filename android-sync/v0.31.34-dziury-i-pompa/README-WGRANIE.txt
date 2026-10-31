Sterownik CO — aplikacja Android v0.31.34 „DZIURY I POMPA"
=========================================================

CO TA WERSJA ZMIENIA (w porównaniu z v0.31.33)
----------------------------------------------
1. Pompa CO nie rysuje już pionowych zielonych „ścian" na całą wysokość.
   W trybie mieszanym (temperatury + pompa) pompa zajmuje niski pasek
   przy dolnej krawędzi wykresu. W trybie „tylko pompa" — bez zmian.
2. Przerwa w danych jest widoczna i PODPISANA: dwie przerywane pionowe
   kreski + napis „luka 1 h 12 min" nad przerwą.
3. Podpis pod wykresem mówi, ile jest przerw i jak długa jest najdłuższa.
4. Pobieranie historii jest odporne na błędy sieci:
   - nieudana strona nie kasuje już całego dnia (zostaje to, co przyszło),
   - po pobraniu aplikacja sama sprawdza ciągłość i dopytuje każdą lukę
     dłuższą niż 3 minuty. Jeśli w chmurze te minuty są — dziura się zasypie.
     Jeśli ich nie ma — zostanie i będzie podpisana.

JAK WGRAC (4 kroki)
-------------------
1. Rozpakuj ten ZIP. Jest w nim 5 plików .kt (reszta projektu bez zmian):
   AppModel.kt, ChartEngine.kt, RtdbClient.kt, Charts.kt, Telemetry.kt.
   Zmienione są tylko trzy pierwsze — pozostałe są identyczne z v0.31.33.
2. W projekcie Android Studio podmień pliki o tych samych nazwach
   (AppModel.kt, ChartEngine.kt, RtdbClient.kt).
3. Zwiększ versionName na "0.31.34" i versionCode o 1 (Build > Edit Build Types
   albo app/build.gradle). Jeśli nie chcesz — zostaw, aplikacja się zbuduje,
   tylko w telefonie będzie ten sam numer wersji.
4. Build > Generate Signed Bundle / APK > APK, podpisz tym samym kluczem
   co poprzednio, zainstaluj na telefonie (nadinstalowanie, dane zostają).

JAK SPRAWDZIĆ, ŻE DZIAŁA
------------------------
- Otwórz wykres „TEMPERATURY + POMPA C.O.", zakres 7 DNI, przybliż okolice
  24.10 godz. 12–14. Powinna być widoczna przerwa z napisem „luka … min"
  (ta przerwa JEST prawdziwa — w chmurze nie ma tych minut).
- Przewiń na noc z 24 na 25.10. Wcześniej była tam wielogodzinna dziura.
  Teraz powinna zniknąć (dane w chmurze są, aplikacja ma je dociągnąć).
- Nad wykresem, w podpisie, pojawi się „· luk: N (najdłuższa X h Y min)".

CZEGO TA WERSJA NIE ROBI
------------------------
- Nie zmienia progu 6 minut (przerwa krótsza niż 6 min nadal nie jest luką).
- Nie rusza archiwum centrali (/api/history) ani plików hist_short/hist_long.
- Nie zmienia niczego w firmware.

JEŚLI COŚ NIE ZADZIAŁA
----------------------
Wróć do plików z katalogu v0.31.33-os-czasu-i-archiwum-esp (są nietknięte).

Plik ZMIANY-0.31.33-do-0.31.34.diff zawiera dokładnie wszystkie zmiany
(linia po linii) — do wglądu, nie trzeba go nigdzie wgrywać.
SHA256SUMS.txt — sumy kontrolne plików .kt z tego ZIP-a.
