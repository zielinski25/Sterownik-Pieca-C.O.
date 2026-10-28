# 201 — plan przed kodem

Powiązać token 200 z wewnętrzną tożsamością żądania. Raz attach, raz pierwszy plik;
nie resetować stanu przy kolejnym index==0. Pula i generacje ograniczone.
Disconnect tylko oznacza stan; close/release wykonuje osobny kontekst roboczy.
Nie trzymać blokady rejestru podczas Pool200/Owner/operacji nośnika.
W trakcie operacji slot nie może być użyty ponownie, ale disconnect ma móc wejść.
Brak miejsca/stare callbacki/brak pliku/dodatkowy plik/autoryzacja odmowna muszą
mieć jednoznaczne wyniki. Busy nie oznacza przyjęcia ani odrzucenia porcji.

Bez _tempObject: przypięty destruktor request wywołuje free, nie destruktor C++.
Bez zmiany onPoll i bez zamykania klienta z callbacku pamięci. Testy z prawdziwym
Pool200, Owner197, FS.cpp i modelem backendu; próby wyścigu disconnect oraz błędu
close. Nie uruchamiamy pełnego parsera HTTP/AsyncTCP/ESP. Nie dodajemy kolejki
kopiującej bufory callbacku ani finalnego mostu odpowiedzi do sieci — nadal blokady.
Centrala i etapy 195–200 zamrożone. Bez SDK, urządzenia, nadpisania i kasowania.
