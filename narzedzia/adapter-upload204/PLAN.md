# Plan 204 — przed kodem

Offline, NIE WGRYWAĆ. Bez zmian centrali, SDK, konfiguracji i danych.

Cel: stała pula kontekstów od przyjęcia żądania do rozłączenia i rzeczywistego zniszczenia obiektu; API begin/chunk/end używające kolejki 202 i hooków biblioteki 203. Brak alokowania kontekstów na każde połączenie. Numer generacji adaptera oddzielny od tokenu kolejki. Odmowa instalacji hooka musi anulować token już przyjęty przez kolejkę. Nie pauzować żądania przed końcem całego requestu. Rozłączenie w trakcie odbioru musi anulować metadane bez I/O.

Testy: dwa aktywne żądania, przeciążenie, opóźnione zniszczenie, ponowne użycie, stare/obce uchwyty, odmowa hooka, niezależność final pliku i końca requestu, błędy kolejki/zapisu, timeout przed/po end, fabryka odpowiedzi/rozłączenie/reentrancja, wątek roboczy. Native i ASan/UBSan z kontrolą wycieków; celowe mutacje. Regresja 203/202. Archiwum z zależnościami i sprawdzeniem sum.

Granica: to adapter komponentowy, nie podpięcie rzeczywistego parsera multipart, nie produkcyjny dispatcher FreeRTOS ani pełny build ESP. Przyjęcie wymaga wiarygodnej długości pliku (nie multipart Content-Length) i uprzedniej autoryzacji. Hooki wymagają zmodyfikowanej, w całości przebudowanej biblioteki 203. Timeout zależy od zdarzeń poll. Obsługa zapisuje wyłącznie nowe pliki; nie rozwiązuje niezmienności pobieranej historii.
