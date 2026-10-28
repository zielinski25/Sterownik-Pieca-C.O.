# 200 — plan przed kodem

Osobny komponent uploadu, bez zmiany centrali i SDK. Nie używać globalnego stanu
fsUploadFile/fsUploadBlad dla wielu żądań. Ograniczona pula żądań, token slot+generacja,
stare callbacki nie mogą zmienić nowego żądania. Wszystkie operacje pod Owner197.

Sukces dopiero po: prawidłowej kolejności offsetów, dokładnej zadanej liczbie bajtów,
final=true, zakończeniu całego żądania i sprawdzonym sync/rozmiarze/close.
Dodatkowo zapis/odczyt porównawczy w blokach 256 B, bez wyjątków i RTTI.
Void Arduino File.close NIE jest potwierdzeniem. Użyć sprawdzanych Calls z 193.

Prototyp celowo tylko dla NOWEGO pliku (O_EXCL), bez nadpisania/usuwania/rename.
Nie jest to gotowy zamiennik obecnej trasy. Długość pliku wymaga osobnego kontraktu;
Content-Length multipart nie oznacza długości pliku. Nazwa ograniczona do płaskiego
ASCII i chronione nazwy historii/A1/logów. Autoryzacja, quota, polityka nadpisania,
request ownership i kontekst disconnect pozostają blokadami integracji.

Błąd protokołu/disconnect: bez sukcesu, pojedynczy sprawdzony close jeśli nośnik
nadal pewny; nie usuwać częściowego nowego pliku. Błąd I/O lub zatrzymanie: zachować
FD/pamięć, nie próbować kolejnego close. Brak restartu/remount/formatowania.

Testy: przeplatane żądania, stare tokeny, wyczerpanie generacji/puli, brak final,
dodatkowy plik, błąd każdej operacji, errno przy sukcesie, krótki zapis, kolizja
nazw, prawdziwy FS.cpp, sanitizery, mutacje, czyste odtworzenie ZIP.
