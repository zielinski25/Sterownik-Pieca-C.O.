# Warunki podłączenia 200 — nadal niespełnione

## Adapter żądania

Każde żądanie musi mieć serwerowy kontekst z jednym wynikiem begin. Nie wolno
uruchamiać begin ponownie i podmieniać tokenu przy drugim pliku/index==0 w tym
samym multipart. Kolejne porcje prowadzą przez chunk; kompletne żądanie przez
complete; przerwanie przez release w bezpiecznym kontekście. Test drugiego pliku
używa tego samego tokenu — nie jest dowodem poprawności nieistniejącego adaptera.

Brak slotu daje nieważny token. Adapter musi zachować swój błąd 503 dla danego
żądania i nie wywoływać domyślnego sukcesu. complete dla nieznanego/starego tokenu
zwraca emit=false. complete emituje wynik co najwyżej raz; żadne sprzątanie nie
może ponownie wysłać odpowiedzi. Oddzielnie trzeba kwalifikować życie request,
onDisconnect i kolejność zdarzeń biblioteki. Nie należy wstawiać obiektu C++ do
pola biblioteki, którego zwalnianie jest nieznane lub używa innej metody alokacji.

## Autoryzacja, nazwa i rozmiar

Token nie jest tajemnicą ani uprawnieniem. Autoryzacja oraz akceptacja rozmiaru
pliku muszą poprzedzać begin. Wdrożenie wymaga uzgodnionego pola z długością
samego pliku, a nie długością całego żądania. Domyślny limit 4096 B to limit
prototypu, nie potwierdzony budżet pamięci urządzenia. Potrzebna kontrola wolnego
miejsca i wspólnej rezerwy dla historii/logów. Ta pula jej nie zapewnia.

O_EXCL wyklucza nadpisanie istniejących plików, także częściowego pliku z poprzedniej
próby. Nie ma automatycznego usunięcia takiej przeszkody. Polityka nadpisywania,
staging/rename, sprzątania i odzyskiwania nie jest implementowana w 200.
Nie wykorzystujemy zgody A do usuwania nowej historii ani ustawień.

## Blokady, życie obiektów, błędy

Pool, Owner, Calls, mutex i magazyny Permanent mają trwać do resetu.
Destruktor Pool jest usunięty, ale nie gwarantuje to czasu życia magazynu.
Nie rekonstruować obiektów, nie zwalniać slotu z niepewnym deskryptorem.
Wpis CloseAttempted nie oznacza, że backend nadal ma otwarty plik: close mógł
zużyć FD i jednocześnie zwrócić błąd. Nie wolno go ponawiać.

Każda metoda Pool zakłada własną Lease. Nie wołać z posiadanej Lease. Jeżeli
operacja ma zewnętrzny logMutex, obowiązuje logMutex -> Owner. Bez kodu sieciowego
wewnątrz Lease. release może wykonać synchroniczne close, dlatego nie można go
bez kwalifikacji wywołać z dowolnego kontekstu callbacku disconnect.

Docelowo trzeba ograniczyć czas pracy w callbacku odbioru, ustalić własność
buforów przy przekazywaniu do zadania i obsłużyć backpressure. Nie wolno zachować
wskaźnika data callbacku na później bez prawidłowej własnej kopii. Port FreeRTOS,
WDT, czas oczekiwania na Owner i priorytety nie zostały sprawdzone.

## Pozostała kolejka

- Wiązanie kontekstu uploadu z rzeczywistym parserem i życiem żądania.
- Bezpieczne, ograniczone czasowo przerwanie pobierania z 199 przy canSend=false.
- Spójne źródło pobierania podczas dopisywania i rotacji historii.
- Połączenie startu 196, Owner197, historii198 oraz transportów w całym programie.
- Pełny audyt dróg omijających Owner, kompilacja SDK i późniejsza kwalifikacja ESP.

To blokady wdrożenia, a nie zapewnienie zakończenia w jednym kolejnym etapie.
