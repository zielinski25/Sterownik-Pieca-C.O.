# Co dalej pozostaje przed połączeniem z centralą

1. Bezpieczny kontekst sieciowy i ograniczony czas przerwania przy canSend=false.
   Nie wolno zastąpić callbacku onPoll należącego do biblioteki ani zamykać klienta
   z callbacku historii, gdy trzymana jest Lease właściciela plików.
2. Własność i życie żądania w prawdziwym _onDisconnect. Testowy close tylko zapisuje
   zdarzenie, nie wywołuje destruktora żądania; _this w teście nie symuluje pełnej
   grafowej własności serwera. Nie jest to dowód braku use-after-free w AsyncTCP.
3. Spójność pobieranego pliku przy dopisywaniu i rotacji: stała długość nie oznacza
   stałych bajtów. Nie blokować Owner przez cały transfer sieciowy. Potrzebny
   osobny protokół stabilnego źródła, zgodny z zapisem nowych pomiarów.
4. Uczciwy wynik uploadu: brak pliku, brak końcowej porcji, współbieżne żądania,
   brak blokady przy finalizacji, niepewny close. Obecny globalny fsUploadFile
   nie jest zastąpiony przez nowy mechanizm. Nie wolno twierdzić, że 199 naprawia upload.
5. Połączenie etapów startu 196, właściciela 197 i transakcji 198 w jednym programie;
   wszystkie zwykłe drogi FS mają otrzymywać proxy, nie surowy backend.
6. Port blokad FreeRTOS, kolejność logMutex -> Owner, WDT, pamięć, pełna kompilacja
   i kwalifikacja zależności. Nie dotykano SDK ani urządzenia w tym pakiecie.

Nie ma nowej wersji firmware ani zmiany ustawień, 8 s/60 s, SERWO%, sterowania,
dostępu zdalnego, zgody A czy zasad ochrony nowych pomiarów. Brak remount/restartu,
formatowania i automatycznego usuwania danych. Kolejne działania pozostają offline.
