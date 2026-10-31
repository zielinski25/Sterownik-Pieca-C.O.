# Etap 221 - plan i granice

## Problem, ktory ten etap zamyka
Etap 220 pokazal, ze nasz kod jest zgodny z prawdziwymi naglowkami biblioteki,
ale dowodzil tego analiza statyczna (parser deklaracji). Nie bylo dowodu, ze
ten kod SIE KOMPILUJE i ZACHOWUJE SIE poprawnie wobec prawdziwego kodu
biblioteki. Etap 221 to robi: kompiluje prawdziwe .cpp biblioteki i nasz kod
jednym kompilatorem i przepuszcza przez nie prawdziwe zadania HTTP.

## Kroki
1. Wciagnac przypiete zrodla: ESPAsyncWebServer (commit c47917d), ArduinoCore-API,
   FS.h z etapu 196, cbuf.h z rdzenia ESP32, libb64 (zaleznosc Basic auth).
2. Zbudowac warstwe platformy: Arduino.h, WiFi.h, FreeRTOS.h, sdkconfig.h,
   gniazdo (AsyncTCP.*), dysk (FS.cpp), cbuf, zaslepki authenticate(), itoa/dtostrf.
   Warstwa jest CELOWO mala i widoczna - README ma tabele "co prawdziwe, co nasze".
3. Wyciagnac z v3.56.4 (odbudowanego z v3.55.8 + patch 218, z kontrola SHA)
   bramke historii i trzy rejestracje tras. Zero przepisywania recznego.
4. Napisac Test.cpp: surowe bajty zadania -> prawdziwy parser -> prawdziwy
   dyspozytor -> nasza trasa -> prawdziwe odpowiedzi -> surowe bajty odpowiedzi.
5. Dwa profile (token / bez tokenu) i 12 mutantow.
6. Brama run.py + pakiet.

## Czego etap nie obejmuje (swiadomie)
- kompilacji PlatformIO/xtensa na komputerze uzytkownika (to robi uzytkownik),
- prawdziwego lwIP/WiFi/LittleFS (emulacja opisana w README),
- wygladu panelu na telefonie,
- obciazenia i rownoleglych polaczen z sieci.
