# 133 — właściwe źródła1.20.4 i powtórzone testy

27.10.2026. Po raporcie Windows132 analizujemy wersję wskazaną zgodnie przez dependencies.lock i dwa pliki versions.txt. Nie proszono użytkownika o kolejne uruchomienie. Nie zmieniono SDK, firmware ani zamkniętych modułów.

## Łańcuch identyfikacji

- Tag `joltwallet/esp_littlefs v1.20.4`: obiekt tagu `f838c3bea9c5a89a182332ed672c4d5cf2ddf619`, commit `92ac3c2dce8c62c4b47bdb33e8461a4670411475`.
- Gitlink `src/littlefs`: upstream `adad0fbbcf5382c20978d07f94f9c13be9041c1b`. Nagłówek podaje LFS_VERSION0x0002000b, czyli API2.11; format dyskowy0x00020001, czyli2.1. To nie numer komponentu1.20.4.
- `esp_littlefs.h` ma **6235B/SHA25602ff93d30c9840235a995a6825aae72fa6bb0eb701d702740560d9f6d1bd1617**, dokładnie jak w SDK odczytanym w131.
- Manifest pobranego komponentu deklaruje1.20.4. Wszystkie adresy, rozmiary i hashe zachowano w [ZRODLA.json](ZRODLA.json), źródła i licencje w `zrodla/`.

**Nie utożsamiać zgodnego nagłówka z dowodem identyczności wszystkich binariów.** Nie odtworzono SHA736380B biblioteki .a z raportu131 i nie odtworzono registry component_hash z lock132. Wynik wiąże wersję z metadanych SDK z przypiętym źródłem wydania i zgodnym nagłówkiem, a nie dowodzi hermetycznej kompilacji. Rozbieżność manifestu Arduino ze128 nadal pozostaje osobną niewyjaśnioną różnicą pakowania/pochodzenia; nie podajemy przyczyny bez danych.

## Analiza przepływu błędów w1.20.4

| Miejsce | Wynik analizy | Konsekwencja |
|---|---|---|
| esp_littlefs.c:1568–1602,write | Błędy lfs_file_write przekazywane jako -1/errno. Opcjonalny flush-every-write nadal ignoruje wynik dodatkowego fsync. | Jawnie kontrolować pełne write i sync. Opcja jest wyłączona w głównym sdkconfig131; nie polegać na niej. |
| esp_littlefs.c:1839–1870,fsync | Wywołuje teraz helper esp_littlefs_file_sync, a nie bezpośrednio lfs_file_sync jak w analizowanym1.10.2. Błąd helpera nadal mapowany na -1/errno. | Nie przenosić starego opisu wywołań bez aktualizacji. |
| esp_littlefs.c:2348–2358,helper | Przy MTIME i uchwycie nie-RO aktualizuje bufor czasu, potem lfs_file_sync. | MTIME jest włączone w raporcie131. Test samego rdzenia nie wykonuje tego wrappera/time(). |
| esp_littlefs.c:1751–1795,close | Przed lfs_file_close aktualizuje bufor MTIME. Błąd zamknięcia wraca przed esp_littlefs_free_fd. | Pomyślny wcześniejszy sync nie zwalnia z kontroli close. Nie ponawiać close na ślepo; sprzątanie po błędzie wymaga dalszej kwalifikacji. |
| lfs.c:3431+,sync | LFS_F_ERRED nadal daje0 bez flush. Jest też dodatkowa synchronizacja danych przed zatwierdzeniem metadanych; jej błąd wraca bez ustawienia ERRED w tym miejscu. | Takie same zewnętrzne kody wyników nie oznaczają zawsze tej samej wewnętrznej ścieżki. Pamiętać pierwszy błąd niezależnie od flag littlefs. |
| littlefs_esp_part.c | Implementację flash przeniesiono z dawnego littlefs_api.c. Błędy esp_partition_* mapowane na LFS_ERR_IO; callback sync nadal zwraca0 z komentarzem o zbędności dla IDF. | Nie jest to dodatkowe sprzętowe opróżnienie bufora. Zachowanie warstwy partycji/flash musi być osobno wykazane. |

Pierwsza próba pobrania dawnej ścieżki src/littlefs_api.c dała404; odczyt drzewa wskazał aktualne littlefs_esp_part.c. Nie użyto starego pliku pod nową etykietą.

Nie przypisujemy samych ryzyk związanych z close do zaobserwowanych usterek urządzenia. Kontrola ESP VFS/flash i pełnego stosu błędów pozostaje niewykonana.

## Wykonane testy na nowym rdzeniu

Runner [testuj.py](testuj.py) kompiluje `lfs.c/lfs_util.c` z gitlinku1.20.4, a nie kopię128. Niezmienione scenariusze i strażnik129 pozwalają porównać zachowanie bez dopasowywania oczekiwań do wyniku.

Miarodajny wynik [WYNIKI/run001/wynik.json](WYNIKI/run001/wynik.json):

| Zestaw | Native | ASan/UBSan |
|---|---:|---:|
| Scenariusze rdzenia128 na nowym upstream | 3 PASS | 3 PASS |
| Integracje strażnika129 z nowym upstream | 3 PASS | 3 PASS |
| Jednostkowe strażnika129 | 23 grupy PASS | 23 grupy PASS |

Kompilacja z -Wall/-Wextra/-Werror przeszła; logi uruchomień bez diagnostyki sanitizerów. Snapshot wszystkich użytych źródeł i ich SHA jest w run001/sources.zip i wynik.json. Nie uruchamiano ponownie starych finalizerów ani nie nadpisywano ich wyników.

Scenariusze rdzenia dały:

- normalnie: write1024, sync0, drugi sync0, close0;
- awaria programowania: write-5, sync0, drugi sync0, close0;
- awaria callback sync: write1024, pierwszy sync-5, drugi sync0, close0 po wyłączeniu awarii.

Integracja strażnika prawidłowo odmówiła zatwierdzenia po błędzie write lub sync, mimo końcowego close0. Zasada zapamiętania pierwszego błędu pozostaje potrzebna również dla nowego rdzenia.

**Granice testów:** nośnik RAM, nie fizyczna NOR; callback sync wstrzykiwany w rdzeń, nie zachowanie callbacka ESP, który jest pusty. Nie uruchomiono wrappera esp_littlefs.c, MTIME, VFS, konfiguracji pamięci referencyjnego ESP ani zaniku zasilania. Parametry fixture pozostały celowo niezmienione do porównania; nie odtwarzają cache/read/write z SDK131. Brak testu restartu i pełnego odzyskiwania w133. Nie wykonano nowych mutantów w133 — istnieją wcześniejsze dowody129/130.

## Decyzja i następna bramka

Nie trzeba zmieniać logiki strażnika129: na właściwym upstream zachowuje blokadę. Nie oznacza to gotowego backendu ESP. Do następnego zakresu pozostają: kontrolowany wrapper I/O ESP z obsługą wszystkich błędów (w tym MTIME/close), dowód rezerwy na wynik/ack, zaufany punkt odtworzenia i testy fizycznej trwałości na odrębnym stanowisku. Testy zasilania nie mogą odbywać się na pracującym piecu.

Nie powtarzać odczytów131/132, kompilacji126 ani instalacji SDK. Nie zmieniono firmware,8s/60s,SERWO%,NVS,partycji,poświadczeń ani OTA. **release_ready=false;3.55.3 NIE WGRYWAĆ.**
