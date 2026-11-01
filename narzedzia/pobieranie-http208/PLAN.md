# 208 — plan przed kodem: pobranie przez HTTP, nie tylko odczyt

NIE WGRYWAĆ. Offline, bez zmian centrali 3.55.8, SDK i urządzenia. Bez nowych zmian biblioteki ponad 199/203/205/206.

207 daje bezpieczny, przypięty prefiks pliku historii, ale nikt go jeszcze nie wysyła. 199 daje
`CheckedFileBody` i poprawioną pętlę wysyłki, ale nie ma właściciela żądania ani czasu życia. 203 daje
hooki poll/rozłączenie. 208 ma je złożyć: **przyjęcie pobrania → zapamiętanie długości → odpowiedź
o stałej długości → wysyłka prawdziwą pętlą 199 → zwolnienie pinu przy zakończeniu, rozłączeniu, terminie
albo błędzie magazynu**.

Zakres:
1. `Adapter`: stała pula 2 pobrań, uchwyty slot+pokolenie, powiązanie z konkretnym żądaniem, deadline od
   przyjęcia, fabryka odpowiedzi zwracająca `nullptr` przy braku pamięci (wtedy pin jest zwalniany).
2. Odpowiedź czyta wyłącznie przez uchwyt pinu 207; przy zatrzymaniu magazynu źródło staje się nieważne —
   wysyłka jest przerywana, nigdy nie kończy się cichym krótszym plikiem.
3. Pin zwalniany przy: zakończeniu odpowiedzi, rozłączeniu, terminie, błędzie odczytu, nieudanej fabryce.
   Slot wraca do puli dopiero po rozłączeniu i wygaśnięciu słabego wskaźnika żądania, żeby stara odpowiedź
   nie czytała nowego pinu.
4. Testy z prawdziwą pętlą 199 (`Abstract.inc`, wyodrębniona `write_send_buffs`), prawdziwymi metodami 203,
   prawdziwym właścicielem 197 i `FS.cpp`, komponentem 207 i modelowanym backendem/klientem.
5. Sprawdzenie: dopisywanie w trakcie wysyłki, rotacja odroczona i dopiero po zamknięciu, dwa równoległe
   pobrania, brak miejsca, rozłączenie w połowie, zatrzymanie magazynu, termin, brak pamięci, 50 kolejnych
   pobrań, ponowne użycie slotu, obcy/nieaktualny uchwyt.

Granice: brak całej biblioteki HTTP, AsyncTCP, FreeRTOS i ESP; modelowany klient i bajty nagłówków (marker);
`Range`, kompresja, szablony i Content-Disposition poza zakresem; nagłówki i routing nie są tu badane
(206). Nadal brak RAM/WDT, zaniku zasilania i niezmienności przy zewnętrznym uszkodzeniu pliku.
Nie zmieniać etapów 196–207 poza odczytem ich plików.
