# 206 — plan przed kodem

NIE WGRYWAĆ. Offline. Bez zmian głównego programu, SDK i urządzenia. Bez nowych zmian biblioteki ponad już zaproponowane w 205.

Etap 205 sprawdził body, ale nagłówki, linia żądania, granica formularza, długość, metoda, URL i autoryzacja były wstrzykiwane przez test. Etap 206 ma podłączyć do tego **prawdziwe, przypięte metody parsowania nagłówków** i prawdziwy moment przypisania handlera, a dopiero potem parser body z 205 i pula 204.

Zakres:
1. Wyodrębnić bez zmian: `_parseReqHead`, `_addGetParams`, `urlDecode`, `_parseReqHeader`, `_parseLine`, `AsyncWebHeader::parse`, `stringToMethod`, enum metod oraz przypięte literały. Udowodnić, że kandydat 205 nie zmienił tych funkcji (bajtowa równość z przypiętym oryginałem).
2. Odtworzyć kolejność: komplet nagłówków → wybór trasy/handlera (model dopasowania dokładnej ścieżki/metody) → przyjęcie do puli i włączenie trybu 205 → body → zakończenie i odpowiedź. Modelowany pozostaje wybór trasy, nie samo parsowanie.
3. Zbadać na prawdziwych nagłówkach: podział wejścia w każdym miejscu, wielkość liter, OWS, granice cytowane/ucięte/puste/za długie, brak granicy, duplikaty Content-Type i Content-Length, `X-Expected-Entity-Length`, długość ujemna/niepoprawna/zbyt duża, `Expect: 100-continue`, chunked, metodę i URL, parametry zapytania oraz Authorization (brak/zły/dobry token).
4. Sprawdzić, że odmowa na etapie wyboru trasy nie rezerwuje miejsca, a przyjęcie następuje przed pierwszym bajtem body; rozłączenie w trakcie zwalnia miejsce; 50 kolejnych żądań korzysta z tej samej puli.
5. Testy native/ASan/UBSan z LSan, mutacje logiki przyjęcia (autoryzacja, trasa, długość, duplikaty, przerwanie), czysta aplikacja patcha 205 do przypiętych źródeł.

Granice: modelowany String/Stream i klient TCP; wybór trasy to dopasowanie dokładne, nie AsyncURIMatcher; brak pełnego serwera, FreeRTOS, całej biblioteki i ESP. String na ESP, RAM, WDT, zaniki zasilania i usuwanie częściowego pliku pozostają niekwalifikowane. Nie zmieniać zamrożonych etapów 199–205.
