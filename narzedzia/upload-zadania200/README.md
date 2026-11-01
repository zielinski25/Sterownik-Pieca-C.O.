# 200 — uczciwy wynik uploadu i oddzielny stan każdego żądania

**NIE WGRYWAĆ. release_ready=false. Centrala 3.55.8 bez zmian.**
To nowy komponent i testy, nie podłączona trasa HTTP ani gotowy program ESP.

## Co zostało zrobione

`UploadPool.h` zastępuje w osobnym komponencie koncepcję jednego wspólnego
fsUploadFile/fsUploadBlad ograniczoną pulą żądań. Każde ma własny stan, deskryptor,
licznik danych oraz identyfikator slot+generacja. Wszystkie metody korzystają
z tego samego Owner197 co zwykłe File/FS i transakcje historii 198.

Odpowiedź 200 może powstać dopiero po:
1. Przyjęciu porcji we właściwej kolejności, bez luk i powtórek.
2. Odebraniu dokładnie zadanej liczby bajtów i znacznika final=true.
3. Osobnym zdarzeniu końca CAŁEGO żądania, nie tylko końca pliku multipart.
4. Sprawdzeniu zapisu, synchronizacji i odczytu porównawczego blokami do 256 B.
5. Końcowym sprawdzeniu sync, rozmiaru/typu pliku oraz wyniku close.

Nie używamy void Arduino File.close jako dowodu sukcesu. Komponent używa
sprawdzanych wywołań Calls z 193. Uwzględnia błędy errno także przy dodatnim wyniku.

Błąd protokołu nie zeruje błędu innego żądania. Stary callback po zwolnieniu slotu
nie trafia do następnego żądania. Po wyczerpaniu licznika generacji slot przestaje
być używany, zamiast zawijać licznik. Dwa żądania tej samej nazwy nie nadpisują się.

## Celowo ograniczony zakres

**Tylko nowe pliki, O_EXCL. Istniejący plik daje 409, bez nadpisania.**
Domyślnie dwa sloty, maksymalnie 4096 B na plik, nazwa płaska ASCII do 63 znaków.
Chronione są nazwy historii, plików przejścia A1, logów i panel_firmware.bin.
To bezpiecznie ograniczony prototyp; nie zastępuje jeszcze funkcjonalności
nadpisywania ani wysyłania dużych plików obecnej trasy.

Przy błędzie protokołu lub przerwaniu żądania zdrowy nowy plik jest zamykany
jednorazowo ze sprawdzeniem wyniku. **Częściowy plik pozostaje** — nie kasujemy go.
Przy błędzie pamięci/zatrzymaniu nie próbujemy kolejnego close. Slot i niepewny
FD pozostają zachowane do końca działania programu. Nie ma retry, formatowania,
remount, automatycznego czyszczenia ani resetowania blokady.

## Wyniki

Finalny raport: `WYNIKI/run003/wynik.json`.

| Próba | Wynik |
|---|---|
| 55 grup | PASS zwykłe wykonanie i ASan+UBSan |
| 48 awarii wywołań | Każda pozycja operacji: wynik ujemny, dodatni z errno, stop w trakcie |
| 15 uszkodzonych wariantów | Wszystkie kompilują się i są wykrywane |
| Prawdziwy POSIX | Binarny plik 1000 B, kolizja nazwy, zachowanie częściowego pliku |
| Dwa wątki | Rozdzielone dane dwóch uploadów; blokada wspólna z Owner |
| 255 użyć małej generacji | Brak zawinięcia i przyjęcia starego tokenu |
| Pula pełna | Odmowa przed otwarciem następnego pliku |

48 awarii jest częścią grup, nie należy dodawać tych liczb jako niezależnych
scenariuszy. Mutacje wykonywano w profilu zwykłym. Testy C++11, bez wyjątków/RTTI,
z ostrzeżeniami traktowanymi jako błędy w przebiegach podstawowych.

Test modelowy łączy rzeczywisty Arduino FS.cpp z kontrolowanym modelem Calls.
Osobny test POSIX wykonuje prawdziwy SystemCalls i operacje na plikach komputera
w nowym katalogu tymczasowym, przez jawne przetłumaczenie /littlefs na ten katalog.
**Nie jest to ESP VFS ani fizyczny nośnik ESP.** Nie uruchamiano parsera multipart,
całego serwera, FreeRTOS ani firmware.

Wczesny run002 wykrył utratę korzenia obiektu testowego przy optymalizacji przez
kompilator. Poprawiono trwałe przechowywanie wskaźnika, nie wyłączając LSan.
Finalny run003 przechodzi oba profile. Pozostałe starsze raporty to historia
rozwoju, nie dowody finalnej wersji. Obiekty wymagające życia do resetu są w testach
celowo osiągalne z globalnych korzeni; żaden destruktor nie wykonuje sprzątania
niepewnego nośnika przy końcu procesu.

## Czego nie wolno wywnioskować z wyniku

- Nie zmieniono rzeczywistego /api/fs-upload. Stare globalne zmienne nadal są w CPP.
- Wartość emit=false nie jest pozwoleniem na wysłanie HTTP 200. Adapter musi
  przestrzegać emit i przypisać jeden token dokładnie jednemu żądaniu.
- Token jest wewnętrzny dla serwera, nie służy do autoryzacji i nie może pochodzić
  od klienta. Sama pula nie wykryje błędnego przypisania prawidłowego tokenu przez adapter.
- Znana długość pliku wymaga osobnego kontraktu. Content-Length multipart obejmuje
  otoczkę formularza i nie może być użyty jako rozmiar pliku.
- Synchronizacja i odczyt porównawczy nie dowodzą odporności na utratę zasilania
  ani tego, że host/ESP nie zwróci danych z pamięci podręcznej.
- Zapis wpuszczony przed stop może jeszcze zmienić dane. Brak dalszych wywołań
  dotyczy stanu po wyjściu operacji i zamknięciu wejścia, nie cofnięcia zapisu.
- Potwierdzenie zapisu nie dowodzi dostarczenia odpowiedzi klientowi. Nie ma
  trwałych identyfikatorów ponowienia po restarcie ani automatycznego wznowienia.
- Limit 256 B dotyczy bufora, nie czasu operacji. Synchroniczne sync/readback
  w callbacku sieciowym mogłyby blokować inne zadania. Port/kolejka/WDT wymagają pracy.

## Odtworzenie

Linux, Python 3, g++, ASan/UBSan. Z katalogu głównego rozpakowanej paczki:

```sh
python3 -B narzedzia/upload-zadania200/run.py
python3 -B narzedzia/upload-zadania200/mutacje.py
```

Nie pobierają SDK, nie łączą się z urządzeniem i nie dotykają rzeczywistego /littlefs.
Dołączono zależności i licencje wcześniejszych komponentów. Potwierdzenie sumy ZIP
oraz ponownego wykonania testów po rozpakowaniu znajduje się w osobnym pliku.

Bez zmian ustawień, haseł, sterowania, 8 s/60 s, SERWO%, dostępu zdalnego i zgody A.
