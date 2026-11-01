# 203 — plan przed kodem

Brakujący tor odpowiedzi uploadu: wynik 202 odbierany wyłącznie w kontekście
sieciowym, nie przez pracownika zapisującego pliki. Nie podmieniać onPoll AsyncClient.
Propozycja jawnego, pojedynczego hooka wewnątrz istniejącego request::_onPoll,
przed canSend; z mocną referencją request i ochroną przed reentrancją.

Hook disconnect ma anulować token kolejki bez I/O i zachować istniejący callback
użytkownika. Przetestować synchroniczne rozłączenie w środku callbacku i rzeczywisty
destruktor request, a nie tylko licznik close. Nie używać _tempObject.

Odpowiedź tylko dla wstrzymanego, niewysłanego żądania bez istniejącej odpowiedzi.
Deadline oczekiwania sprawdzany także gdy canSend=false, lecz wyłącznie po nadejściu
zdarzenia poll — bez obietnicy twardego czasu przy zatrzymanej pętli sieciowej.
Nie przenosić tego na anulowanie pobierania: request.abort nie działa po _sent=true.

Własność tworzonej odpowiedzi do momentu send chroniona RAII. Sprawdzić brak
pamięci, obcą odpowiedź i rozłączenie podczas fabryki. Faktyczna fabryka odpowiedzi
ESP i jej alokacje pozostają do kwalifikacji. C++11, ASan/UBSan, mutacje, clean replay.
Bez main/SDK/urządzenia; etapy 195–202 zamrożone. Patch 203 łączony z propozycją 199.
