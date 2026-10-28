# 198 — plan przed kodem

Cel: połączyć rzeczywisty algorytm historii 193 z właścicielem plików 197,
bez modyfikacji programu 3.55.8 i bez działań na urządzeniu.

1. Osobna fasada append/trim trzyma jedną Lease przez całą transakcję. Nie
   blokujemy osobno każdego wywołania w sposób pozwalający wtrącić zapis File.
2. Bramka publicznych deskryptorów rezerwuje ograniczone miejsce przed open.
   Zachowuje deskryptor uzyskany także przy dodatnim open z błędem errno.
3. Błąd operacji oraz błąd semantyczny silnika zatrzymują wspólnego Owner.
   Silnik 193 próbuje close przy sprzątaniu po błędzie. Nowa bramka NIE może
   przepuścić tego do backendu po zatrzymaniu. Nie wolno ponawiać niepewnego close.
4. Do obserwatora błędu potrzebny jest kontekst obiektu, nie globalny wskaźnik
   ani thread_local. Wyodrębnić Engine z zamrożonego źródła, zmienić wyłącznie
   sygnaturę/wywołanie obserwatora i konstruktor; potwierdzić mechanicznie.
5. Zachować rozróżnienie potwierdzonego zapisu od późniejszej awarii/rotacji.
   Nie cofać potwierdzenia już sprawdzonego zapisu; nie ponawiać niepewnego.
6. Testować prawdziwy FS.cpp, ten sam backend modelowy dla File i deskryptorów,
   awarie każdego wywołania, błędy semantyczne, współbieżność i brak close po stop.
7. Testy z sanitizerami, mutacje, manifest i odtworzenie samodzielnej paczki.

Nie obejmuje: podłączenia centrali, HTTP, FreeRTOS, WDT, montowania, A1,
formatowania, wznowienia, kasowania nowej historii, SDK, wgrywania i OTA.
Lease jest nierekurencyjna. Obserwator błędu i service nie mogą wołać File/FS,
append/trim ani drained. Owner, backend, blokada i fasada wymagają życia do resetu.
