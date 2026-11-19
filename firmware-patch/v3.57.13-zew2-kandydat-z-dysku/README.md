# Centrala 3.57.13 — kopia referencyjna z Dysku Google (paczka KANDYDAT, 17.11)

To **nie jest nowa paczka do wgrania** — to ostatnia wersja, która chodziła na
płytce przed 3.58.8 i jedyna z linii 3.57.10–3.57.13, jaką mam w ręku. Linii
3.57.1x nigdy nie było w repozytorium (ani w git, ani na dysku) — tylko na Dysku
Google. Wkładam ją tu, żeby:

* patch `firmware-patch/v3.58.9-zew2-scalenie/main_centrala-v3.57.13-do-v3.58.9.patch`
  miał swoją podstawę w repo (test C2 paczki 3.58.9 nakłada go na ten plik),
* było widać, co dokładnie wróciło w 3.58.9 (`t_zewn_2`, harmonogram DS, 12 bitów…),
* dało się do niej wrócić przez kabel (OTA nie cofa wersji), gdyby 3.58.9 zawiodła
  — wtedy bez panelu LCD i OTA panelu.

Pliki:

* `src/main_centrala.cpp` — dokładnie plik z Dysku, końce linii LF;
  SHA-256 `a5f77e491cfb5ece66801ee8bbff91b73649c2c307a644323ad707b9370c590e`
  (zgadza się z `$expectedModified` w `buduj-3.57.13.ps1` tej paczki).
* `README-oryginalny-3.57.13.md` — README z paczki, bez zmian.
* `centrala-3.57.12-do-3.57.13.patch` — oryginalny patch z paczki
  (`sensors1.begin(9)` → `begin(12)`), na plik 3.57.12 (SHA `ea6256be…`).

Pominięte: `buduj-3.57.13.ps1` (skrypt Windows — kompilował w Twoim katalogu
PlatformIO; nie ma czego sprawdzać na PC) i pliki pośrednie zipa.
Wcześniejsze kandydaty 3.57.10–3.57.12 są tylko na Dysku Google.
