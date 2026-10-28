# Warunki podłączenia 197 — nadal niespełnione

## Zamrożona baza i ustalenia z kodu

Plik referencyjny: `firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp`.
SHA256: `3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31`.
Nie modyfikujemy go w 197. Numery poniżej odnoszą się do tej wersji.

- 5525: globalny File używany przy wysyłaniu pliku.
- 22692: historiaBladIO zgłasza awarię mechanizmowi startu historii, nie nowemu Owner.
- 45286–45288: SystemCalls/Engine historii korzystają z osobnego dostępu do plików.
- 28856: `if (!fsUploadFile) return;` opuszcza obsługę porcji bez ustawienia błędu.
- 28800: końcowa odpowiedź wysyłania może być HTTP 200, jeśli flaga błędu nie została ustawiona.
  Sama podmiana File na chroniony uchwyt nie wystarcza: zatrzymanie między porcjami
  powodowałoby fałszywy File i pomijanie porcji, a niekoniecznie sygnał błędu wysyłania.
  To wniosek z analizy przepływu; nie przeprowadzono testu całego serwera HTTP.
- 28555, 28694, 28730: odpowiedzi używające FS wymagają kontroli rzeczywistego przerwania transportu.

## Rzeczywisty znacznik błędu odczytu

Arduino FS.cpp zwraca `size_t(-1)` przy odczycie nieważnego File do bufora.
Przypięte WebResponses.cpp, metoda AsyncFileResponse::_fillBuffer, przekazuje wynik bez zmiany.
ESPAsyncWebServer.h definiuje RESPONSE_TRY_AGAIN jako 0xFFFFFFFF.

Na 32-bitowym ESP są to te same wartości. Zwykła odpowiedź bez szablonu może
potraktować zatrzymanie jako prośbę o ponowienie, nie zakończenie błędem. Nie wolno
przenosić obserwacji z 64-bitowego hosta na ESP i twierdzić, że każda odpowiedź
wysyła wtedy ogromny blok. Test wyodrębnionej metody sprawdza wartość znacznika,
nie realizację anulowania połączenia.

Osobno: ścieżka szablonów `_readDataFromCacheOrContent` dodaje odczytaną liczbę
bajtów do długości pamięci podręcznej, a dalsze przetwarzanie używa jej do działań
na wskaźnikach i przeszukiwania pamięci. Wymaga odrębnej walidacji. Nie jest objęta
wykonanym testem pełnego transportu. Sam CheckedRead nie zmienia tej biblioteki.

## Kontrakt właściciela

- Jeden Owner i jedna blokada obejmują wszystkie chronione File/FS i przyszłe
  transakcje historii wykonywane przez publiczne deskryptory plików.
- Stan Cold -> Ready -> Stopped; brak ponownego włączenia po zatrzymaniu.
- trip nie czeka. Operacja wpuszczona wcześniej może wykonać skutki uboczne.
  drained czeka na zakończenie takiej operacji; nie wolno wywoływać go z jej wnętrza.
- Lease jest dla transakcji używającej bezpośrednio chronionego backendu/deskryptorów.
  Nie wolno wewnątrz Lease wywołać proxy File/FS: blokada nie jest rekurencyjna.
- Proponowana kolejność: logMutex -> blokada Owner. Trzeba sprawdzić każdy callback
  i ścieżkę błędu; nie jest to jeszcze udowodniona własność programu.
- Po zatrzymaniu backendy pozostają w kwarantannie, bez close/destrukcji/retry.
  Nie wolno rekonstruować Owner w tym samym magazynie ani niszczyć backendu/blokady.
  Permanent pomaga skonstruować obiekt, ale sam typ nie dowodzi statycznego czasu
  życia jego magazynu. Zapewnienie tego należy do integracji.
- close/flush Arduino są void. Nie dowodzą sukcesu. Konieczne jest wiarygodne źródło
  błędu, m.in. sprawdzany most historii. Krótki zapis i niemożliwy rozmiar odczytu są
  wykrywane, ale nie wyczerpują wszystkich błędów nośnika.
- Brak miejsca w puli nie otwiera backendu. Bloki kontrolne oczekujące na weak_ptr
  także zajmują ograniczoną pulę. Nie obiecujemy braku alokacji wewnątrz backendu.

## Następny pakiet musi zamknąć, nie ominąć

1. Wspólne wpuszczanie transakcji historii i File/FS; publikacja awarii do Owner.
2. Faktyczne anulowanie pobierania w odpowiednim kontekście sieciowym, bez niszczenia
   obiektów w niewłaściwym wątku i bez podmiany callbacków należących do biblioteki.
3. Błąd wysyłania oraz uczciwy wynik operacji w pozostałych odbiorcach File/FS.
4. Pełna lista dróg omijających kontrolę, inicjalizacja przy starcie i statyczne życie.
5. Port FreeRTOS: dziedziczenie priorytetu, czas przetrzymywania blokad, WDT i RAM.
6. Dopiero potem kompilacja ESP i osobno autoryzowane próby urządzenia. Bez formatowania,
   automatycznego remount, kasowania ustawień albo automatycznego usuwania nowej historii.

To lista blokad wdrożenia, nie zapewnienie, że jeden kolejny etap wystarczy.
