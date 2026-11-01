# Etap 216 — plan

## Po co

Etapy 212–215 sprawdzały **każdą stronę osobno**: trasę (C++), panel (tekst w pliku),
API (kompilacja wobec prawdziwej biblioteki). Żadne z nich nie patrzyło na **szew**:
adres, który panel naprawdę otwiera, podany trasie, która naprawdę go obsługuje.
Ten szew wypuścił dwa błędy i oba były niewidoczne dla wcześniejszych testów.

## Kolejność

1. Przeczytać, co panel robi (JS), i co robi trasa (C++) — bez zgadywania, z pliku
   po patchu v3.56.2.
2. Dowieść błędu, zanim cokolwiek się poprawi: bilet na „długą” + trasa `/history`.
   Oczekiwane: 401. (Dowód: `d2.cpp`, wynik w raporcie etapu.)
3. Zdecydować poprawkę: **nie zmieniać znaczenia tras** (mniej ryzyka) — panel ma
   wybierać trasę swojego strumienia. Poprawka drugiego błędu (nazwa pliku) w
   nagłówku, bo tam powstaje odpowiedź.
4. Zbudować bramę, która ten szew sprawdza **na prawdziwym kodzie po obu stronach**:
   - adresy biorą się z prawdziwego skryptu panelu w silniku JS,
   - te adresy idą do prawdziwej trasy z pliku po patchu,
   - brak zgody = brama czerwona.
5. Dołożyć **pięć kontroli negatywnych** (brama bez nich jest ozdobą).
6. Przepuścić 3 profile kompilacji + ASan/UBSan z wykrywaniem wycieków.
7. Spakować, powtórzyć wszystko z paczki (clean replay), wysłać.

## Czego ten etap NIE robi

- nie dotyka urządzenia, nie wgrywa, nie zmienia OTA,
- nie zmienia tras, biletów, zapinki 211 ani kolejki pobrań,
- nie zmienia `secrets.h` (token historii zostaje tam, gdzie był),
- nie udaje przeglądarki: DOM w teście JS jest atrapą i jest to napisane wprost,
- nie obiecuje, że cały plik się skompiluje — to sprawdza PlatformIO u użytkownika.

## Definicja sukcesu

`run.py` kończy się `PASS brama216`, a `wynik.json` ma `status: PASS`,
3 profile, 5 kontroli negatywnych — każda z nich **czerwona** na zepsutym wejściu.
Patch skumulowany cofa się dokładnie do v3.55.8. `release_ready=false`.
