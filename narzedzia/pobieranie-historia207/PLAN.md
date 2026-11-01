# 207 — plan przed kodem: pobieranie historii podczas dopisywania i rotacji

NIE WGRYWAĆ. Offline, bez zmian centrali 3.55.8, SDK i urządzenia.

Etap 199 zostawił otwarte pytanie: co się dzieje z pobieranym plikiem historii, gdy w tym samym czasie
trwa dopisywanie nowych pomiarów albo rotacja (zmiana nazwy, usunięcie starego pliku). Odczyt pliku po
nazwie, przy jednoczesnej zmianie nazwy lub skróceniu, może dać klientowi zmieszaną albo urwaną treść,
a `CheckedFileBody` z 199 nie ma jak tego rozpoznać.

Zakres:
1. Nowy komponent `Snapshots`: przyjęcie pobrania w kontekście sieciowym, pod tym samym zamkiem co
   dopisywanie i rotacja. Zapamiętuje dokładną, potwierdzoną długość i otwiera plik raz, na stałej nazwie.
2. Czytanie tylko do zapamiętanej długości, porcjami o zadanym rozmiarze; rozróżnienie: dane, koniec,
   zatrzymanie, błąd, nieaktualny uchwyt. Błąd nigdy nie jest zwracany jako poprawny koniec.
3. Rotacja jest odroczona, dopóki trwa choć jedno pobranie. Podczas rotacji nowe pobrania są odrzucane.
   Stary plik nie jest usuwany ani zmieniany pod czytającym; nowe dopisywanie idzie już do nowego pliku.
4. Ograniczenie puli (2 pobrania), pokolenia uchwytów, zwolnienie po zamknięciu/rozłączeniu, termin
   ważności pobrania, żeby zawieszony klient nie blokował zapisu historii na zawsze.
5. Sprawdzenie z rzeczywistym `RuntimeFilesystem.h` (197) i rzeczywistym Arduino `FS.cpp`; backend plików
   jest modelowany, jak w 197/198/199. Dopisywanie i rotacja w teście używają tych samych prawdziwych
   wywołań open/write/seek/rename/remove/close.

Granice: brak całej biblioteki HTTP, brak AsyncTCP, FreeRTOS i ESP. Modelowany backend nie realizuje
`O_APPEND` — test jawnie ustawia pozycję na końcu przed zapisem; to ograniczenie modelu, nie dowód.
Ochrona dotyczy wyłącznie pobrań przyjętych przez ten komponent; inne miejsca otwierające plik historii
bezpośrednio nie są chronione. Nadal brak niezmienności przy zewnętrznym uszkodzeniu pliku, braku zasilania,
RAM/WDT i usuwania plików tymczasowych. Nie zmieniać zamrożonych etapów 196–206.
