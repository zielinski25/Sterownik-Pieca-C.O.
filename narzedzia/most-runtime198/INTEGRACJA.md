# 198 — warunki integracji, nie instrukcja wgrywania

## Gotowe w komponencie

- Prywatna bramka deskryptorów i prywatny silnik; publiczne są append/trim/snapshot.
- Jedna Lease dla całego dopisania/przycinania, wspólna z Owner197 zwykłych File.
- Rezerwacja przed open, zatrzymanie przed kolejnym wywołaniem po wykrytym błędzie,
  brak ponownego close i zachowanie wpisu także przy dodatnim open z errno.
- Kontekst obserwatora bez globalnego wskaźnika i bez thread_local.
- Potwierdzenie operacji odrębne od bieżącej gotowości i późniejszej rotacji.

## Wciąż niepodłączone miejsca w main 3.55.8

Audyt trzech miejsc transakcji: `INTEGRACJA.json`.
- 45293: przycinanie; 45334: dopisywanie krótkiej; 45400: dopisywanie długiej.
- Każde aktualnie ma logMutex. Docelowa kolejność: logMutex -> Lease Owner.
- 45288: nadal istnieje stary Engine. Nie jest zastąpiony przez 198.
- 22692: historiaBladIO tylko publikuje zatrzymanie historii startowej.
  Przyszły obserwator 198 może wywołać taką publikację atomową, ale nie logowanie
  do pliku, kod sieciowy, ponowne append/trim/snapshot/drained ani File/FS.
- historiaIoService wywołuje obsługę WDT i vTaskDelay(1). Utrzymywanie Owner podczas
  takiego oddania procesora wymaga pomiarów i portu z dziedziczeniem priorytetów.
- historiaStartSys i historiaA1Sys pozostają surowymi drogami potrzebnymi przy
  starcie. Nie mogą być używane po publikacji gotowości runtime.

Nie wystarczy zamienić typu jednego obiektu. Stara fasada 196 po gotowym starcie
oddaje prawdziwe FS&. Trzeba zmienić to połączenie tak, aby zwykli użytkownicy
otrzymywali wyłącznie proxy 197, a prywatna droga montowania/startu pozostała
niedostępna dla zadań runtime. Inicjalizacja musi wykluczyć okno, w którym historia
jest już oznaczona gotowa, lecz wspólny Owner jeszcze nie przyjmuje transakcji.

## Wyniki i kolejki

Nie dodano niejawnej konwersji Result na bool. Wywołujący musi wybrać właściwe pole:
- confirmed: potwierdzenie konkretnej operacji, pozwala rozliczyć jej skopiowany
  fragment bez kolejnego I/O; nie zastępować przez `confirmed && ready`.
- admitted: czy operacja weszła do silnika. Cold/Stopped odmawiają bez raw I/O.
- ready: diagnostyczna obserwacja, nie trwały token dopuszczenia.
- fault/error: pierwszy błąd silnika; brak dopuszczenia przez awarię innego
  komponentu może pozostawić Fault::None, dlatego sam ten enum nie oznacza sukcesu.
- unresolved: zachowane wpisy, nie dowód, że FD można bezpiecznie zamknąć.

Istniejące flagi oddanoDoFile i wycofywanie potwierdzonego prefiksu trzeba zachować.
198 testuje granicę API, nie uruchamia całej kolejki CPP z nową fasadą. Nie wolno
zaliczać wcześniejszych testów kolejki 195 jako wykonanych z nowym kodem 198.

## Czas życia i blokady

Owner, mutex, backend, RuntimeHistory oraz magazyny Permanent muszą trwać do resetu.
Destruktory Owner/RuntimeHistory są usunięte; nie daje to jednak automatycznej
własności wiecznego życia magazynu. Nie wolno lokalnego magazynu zniszczyć albo
rekonstruować obiektów, aby obejść zatrzymanie. Bramka nie ma odzyskiwania slotów
po awarii. Dwie rezerwacje wystarczają dla bieżącego algorytmu source+stage,
nie są dowodem całkowitego budżetu deskryptorów/memory ESP.

Nie wywoływać 198 z już posiadanej Lease ani proxy File wewnątrz Lease. Nie używać
snapshot z obserwatora błędu — to też blokująca metoda. Dalszy audyt musi wykluczyć
odwrotną kolejność owner -> logMutex na wszystkich ścieżkach całego programu.

## Następna niezakończona część

HTTP nadal ma ograniczenia opisane w 197: odczyt może zamienić awarię w oczekiwanie
na kolejne dane, a wysyłanie może błędnie potwierdzić pominięte porcje. Potrzebna
jest rzeczywista obsługa zakończenia/przerwania w kontekście serwera, nie kolejna
flaga i nie niszczenie obiektów sieciowych z dowolnego zadania. Następnie pełne
połączenie startu/runtime, audyt wszystkich odbiorców, port oraz kwalifikacja ESP.
Brak obietnicy, że jeden kolejny pakiet zamknie wszystkie te warunki.
