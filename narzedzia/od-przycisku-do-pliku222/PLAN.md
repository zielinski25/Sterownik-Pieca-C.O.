# Etap 222 - plan

## Luka, ktora etap zamyka
218 sprawdzil panel na MODELU serwera, 221 - prawdziwy serwer, ale zadaniami
podanymi wprost do parsera. Nikt nie sprawdzil PRAWDZIWEGO gniazda i
PRAWDZIWEGO skryptu panelu (fetch + przekierowanie) naraz.

## Kroki
1. Mostek Most.cpp: gniazdo POSIX <-> AsyncClient biblioteki; kazde polaczenie
   to jedno zadanie (biblioteka konczy odpowiedz naglowkiem Connection: close).
2. panel222.mjs: prawdziwy skrypt panelu w V8; fetch i location.href robia
   PRAWDZIWE zadania HTTP do mostka; pobrany plik jest zapisywany na dysk.
3. run.py: dwa profile (token / bez tokenu) + 6 mutantow + wynik.json.
4. Pakiet i replay.

## Czego etap nie obejmuje
- przegladarki i CSS (DOM jest modelem dokladnie tej czesci, ktorej panel uzywa),
- prawdziwego lwIP (gniazdo jest systemowe, ale po stronie biblioteki stoi
  emulator z etapu 221),
- kompilacji PlatformIO na komputerze uzytkownika.
