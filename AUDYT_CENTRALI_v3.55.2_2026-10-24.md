# Audyt centrali v3.55.2 — 24.10.2026

## Druga runda zakończona — R15, 24.10.2026
**15/15 tematów, 150/150 obszarów analizy.** To zakończenie researchu, nie naprawy i nie odbioru urządzenia. [Wynik końcowy R15](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md) · [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [Macierz przyszłego odbioru](narzedzia/audyt-v3.55.2/research2/R15_MACIERZ_ODBIORU.md).

- Wspólne próby funkcji alarmów potwierdzają: solar→C.O. i C.O.→solar pozostawiają inne ruchy przy tych samych flagach; dym może zmieniać cel wyznaczony przez C.O. Nie zatwierdzono fizycznego priorytetu dla instalacji.
- Ręczne wymuszone OFF pozostaje mimo aktywnego C.O.; -1 jako brak pomiaru kasuje alarm. Zasilanie serw po anulowaniu ruchu może pozostać włączone. Dodatnie kontrole rozdzielenia solaru/pompy i zwykłego końca ruchu działają.
- Własny scheduler jest sekwencyjny: okres zadania nie stanowi gwarancji czasu reakcji. Syntetyczna blokada 30 s opóźnia kolejne zadania; to nie pomiar centrali.
- 13 nowych plików źródłowych R15; porównawcze HS/SMF nie są zależnościami programu. Opisano wpływ debuggera na test watchdogów i potrzebę potwierdzenia elektryki rozruchu.

**R15: 24 main + 5 SMF + 4 HS = 33 nowe sprawdzenia; 3 wcześniejsze runnery ponowione, kod 0.** [Wynik](narzedzia/audyt-v3.55.2/research2/R15_TEST_WYNIK.txt). Odtworzenie wady nie oznacza naprawy. Zachowano 105 ID i niezmieniony kod programu. **Brak rekomendacji zdalnego wgrania**, bez OTA/publikacji/push/merge. Następny etap: trzy decyzje i wariant poprawek do zatwierdzenia, nie kolejny odcinek researchu.

## Historyczny postęp po R14 — 24.10.2026
**14/15 tematów, 140/150 obszarów analizy.** [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R14: walidacja i testy](narzedzia/audyt-v3.55.2/research2/R14_WALIDACJA_TESTY.md) · [15 źródeł + parser](narzedzia/audyt-v3.55.2/research2/R14_ZRODLA.md).

Doprecyzowano #55/#79/#81/#104 i kompletność publikacji #58; zachowano 105 identyfikatorów:
- Oryginalne metody String na hoście przyjmują liczbowy prefiks; NaN przechodzi predykat zakresu. Konwersję NaN do int wykrywa kontrolowana próba UBSan. Nie ustalono libc wgranego builda.
- Brak pamięci przy parsowaniu JSON jest sygnalizowany. Przy tworzeniu dokumentu wyjścia oryginalny blok prognozy mimo OOM publikuje {}, zastępując dobrą kopię.
- Własny calloc zwraca 2 B po przepełnieniu iloczynu; oryginalny calloc_base IDF odrzuca ten przypadek przed malloc. Nie wykazano osiągalności przez sieć.
- Ponowiono testy CSV i mutacje: błędny skończony wynik jest wykrywany, lecz NaN nadal przechodzi 339 bazowych asercji. Dodatkowo NaN tolerancji ukrywa duży błąd CHECK_NEAR.

**R14: 50 nowych sprawdzeń + 1 kontrola UBSan; 2 wcześniejsze runnery ponowione.** [Wynik](narzedzia/audyt-v3.55.2/research2/R14_TEST_WYNIK.txt). Runner kod 0, oczekiwane błędy negatywnych prób zweryfikowane. Bez zmian programu, danych, Androida i OTA. **Następny: R15 — wspólne wymagania ochrony i priorytety.**

## Historyczny postęp po R13 — 24.10.2026
**13/15 tematów, 130/150 obszarów analizy.** [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R13: świeżość i tożsamość](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md) · [15 źródeł](narzedzia/audyt-v3.55.2/research2/R13_ZRODLA.md).

Doprecyzowano #36/#37/#82/#83/#86/#91/#105, zachowując 105 identyfikatorów:
- Nowy odczyt starego statusu odnawia czas świeżości; null nie usuwa stanu połączenia, a późniejsza odpowiedź starszego odczytu może zostać zastosowana na końcu.
- Historia: 601 rekordów z tym samym czasem → 300; 30001 różnych czasów → 30000 przez limit 100 stron. Są to zbiory testowe, nie pomiar utraty danych użytkownika.
- Biblioteka ma warunkowy DELETE z ETag, lecz obecne wywołanie go nie przekazuje. Odtworzono usunięcie B po ACK dla A; retry A/A działa, lecz A/B/A i restart RAM nie gwarantują pojedynczego wykonania.
- Oryginalny serializer REST SDK obsługuje parę czas/klucz; wariant bez klucza wytwarza sentinel opisany w zgłoszeniu upstream. Nie przeprowadzono próby publicznego REST ani proponowanej poprawki.
- Cache solaru nie rozróżnia prawidłowo braku i błędu. Ponowiony test RTC potwierdza kolizję ID diagnostyki, nie zwykłej telemetrii.

**R13: 39 JS/TS + 9 C++ + 1 RTC = 49 PASS, kod 0.** [Wynik](narzedzia/audyt-v3.55.2/research2/R13_TEST_WYNIK.txt). Rzeczywiste wycinki, atrapy oraz osobny model kursora; bez sprzętu, chmury Firebase i OTA. Kod programu/Androida i rytm statusu nietknięte. **Następne: R14 — walidacja i wiarygodność testów.**

## Historyczny postęp po R12 — 24.10.2026
**12/15 tematów, 120/150 obszarów analizy.** [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R12: pogoda i solar](narzedzia/audyt-v3.55.2/research2/R12_POGODA_SOLAR.md) · [14 plików + parser](narzedzia/audyt-v3.55.2/research2/R12_ZRODLA.md).

Doprecyzowania #57/#59/#60 i powiązania #92/#93; zachowano 105 identyfikatorów:
- Pełna funkcja pobrania poprawnie obsługuje średnie poprzedniej godziny i granice (start,end] dla 23/24/25 h. Nie przesuwać poprawnego profilu centrali.
- Jedna godzina daje sukces/status ok i blokuje ponowne pobranie przez warunki haveForToday/haveForNext. Duplikat może zastąpić brakującą godzinę w liczniku kompletności.
- Prawdziwy ArduinoJson 7.4.3 konwertuje tekst/obiekt do zera, true do 1; duża liczba dała Inf po konwersji do float. Brakuje walidacji typu i skończoności. Są to syntetyczne odpowiedzi, nie zaobserwowane dane użytkownika.
- GTI nie jest pomiarem uzysku; wymaga orientacji i założeń geometrii. Model zbiornika ma dodatkowe ograniczenia. Przeczytano historyczny MD i Kotlin, bez zmiany/kompilacji Androida; w ścieżce Androida rozpoznano osobny problem przypisania średniej godzinnej do czasu symulacji.

**R12: 39 C++ + 5 geometrii + 1 jednostek = 45 PASS, kod 0.** [Wynik](narzedzia/audyt-v3.55.2/research2/R12_TEST_WYNIK.txt). Atrapy HTTP/TLS, prawdziwy parser, wybrane funkcje pvlib; nie pomiar pogody ani próba urządzenia. Bez zmian firmware/Androida/sekretów, OTA i publikacji. **Następne: R13 — świeżość i tożsamość danych.**

## Historyczny postęp po R11 — 24.10.2026
**11/15 tematów, 110/150 obszarów analizy.** [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R11: czas i liczniki](narzedzia/audyt-v3.55.2/research2/R11_CZAS.md) · [13 źródeł](narzedzia/audyt-v3.55.2/research2/R11_ZRODLA.md).

Doprecyzowania #3/#4/#35/#46/#47/#59, z zachowaniem 105 identyfikatorów:
- getLocalTime potwierdza rozsądny rok, nie świeżą odpowiedź NTP; dla niepoprawnego roku limit 10 ms w badanej pętli oznaczał 20 ms.
- RTC ma pierwszeństwo i jest korygowany co 24 h. Nawet fallback systemowy przez lokalne DateTime gubi rozróżnienie dwóch chwil 02:30 w dniu 25.10.2026.
- Granice 23/24/25 h są poprawne; licznik A/B/A i mianownik raportu czasu pozostają błędne. Rzeczywiste dwie minuty UTC w powtórzonej lokalnej godzinie nie są duplikatami.
- Signed overflow archiwum w 2038 wykrył UBSan. Nie jest to limit 64-bit time_t całej platformy; konwerter main poprawnie obsłużył kontrolną datę 2039.
- Odtworzono ponowny rozbieg nadzorcy i przedwczesne zakończenie symulacji przy wrap. Dodatnie kontrole potwierdzają działanie zwykłych terminów, getLocalTime przez wrap oraz bramki czasu historii przy braku obu zegarów.

**R11: 43 + 6 + 1 = 50 sprawdzeń PASS, kod runnera 0.** [Wynik](narzedzia/audyt-v3.55.2/research2/R11_TEST_WYNIK.txt). Hostowe wycinki/archiwum, atrapy, kontrolowany błąd UBSan. Bez zmiany zegara hosta/ESP, kodu firmware, Androida, danych i OTA. **Następne: R12 — Open-Meteo i znaczenie GHI/GTI.**

## Historyczny postęp po R10 — 24.10.2026
**10/15 tematów, 100/150 obszarów analizy.** [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R10: DS18B20/DHT](narzedzia/audyt-v3.55.2/research2/R10_CZUJNIKI.md) · [14 źródeł](narzedzia/audyt-v3.55.2/research2/R10_ZRODLA.md).

Doprecyzowania #8/#9/#10/#65, bez zmiany 105 identyfikatorów:
- TEMP_ERROR badanego nettigo nie jest ponawiany ani eskalowany przez licznik power-cycle. Inna sprawna magistrala może anulować odzyskiwanie.
- Poprawne −5°C dekodowane przez bibliotekę zostaje odrzucone przez filtr centrali. Stara liczba może pozostać bez końca, chociaż telemetria oznacza jej wiek.
- Odczyt 801 ms zwraca −1 zamiast dostępnego fallbacku; wybrane decyzje nadal używają samej liczby. Mieszadło trocin jest kontrprzykładem: ma kontrolę świeżości.
- Nieudany request nie uniemożliwia oznaczenia ponownie odczytanego starego scratchpad jako świeżego. CRC nie potwierdza nowej konwersji.
- DHT w centrali: błąd po 50/60/70 daje 60; zakres wilgotności nie jest sprawdzany. Dokładna lokalna kopia DHT nieznana, więc problemy sprzątania znalezione w upstream 2.0.2 pozostają oddzielone od ustaleń o firmware.

**R10: 35 asercji centrala/DS + 4 upstream DHT = 39 PASS, kod 0.** [Wynik](narzedzia/audyt-v3.55.2/research2/R10_TEST_WYNIK.txt). Rzeczywiste wycinki/biblioteka na atrapach, nie test magistral urządzenia. Bez zmian firmware/Androida/sekretów, OTA i publikacji. **Następne: R11 — UTC i granice czasu.**

## Historyczny postęp po R09 — 24.10.2026
**9/15 tematów, 90/150 obszarów analizy.** [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R09: aktualizacje i LCD](narzedzia/audyt-v3.55.2/research2/R09_AKTUALIZACJE_LCD.md) · [17 źródeł](narzedzia/audyt-v3.55.2/research2/R09_ZRODLA.md).

Doprecyzowania #25/#70/#71 oraz powiązania #18/#19:
- Pełna funkcja pobierania panelu, z atrapami, publikuje 1 bajt E9 przy nieznanej długości także po 15 s ciszy. Nieudany rename po remove usuwa dobrą kopię stagingu.
- Odbiornik LCD z ZIP używa HTTPUpdate, które propaguje błąd końcowego end. Nie wykazano instalacji jednobajtowego obrazu na panelu.
- Slot LCD z archiwum ma 1984 KiB, a centrala przyjmuje do 2048 KiB. Możliwa jest publikacja pliku niepasującego do slotu, bez gwarancji jego instalacji.
- WDT panelu wraca po normalnym powrocie update; nie oznacza to nadzoru zawieszonego wywołania. Sam zgodny panel_fw czyści oczekiwanie, bez potwierdzenia hasha i tożsamości panelu w tym fragmencie.
- Odczyt gh z 24.10: pierwszy release 3.55.0, tylko firmware.bin; JSON był zwarty. To obserwacja metadanych, nie rekomendacja instalacji i nie pomiar zachowania klienta ESP.

**R09: 31 asercji PASS w wariancie HTTPUpdate 2.0.17 oraz te same 31 PASS w 3.3.8, kod 0.** [Wynik](narzedzia/audyt-v3.55.2/research2/R09_TEST_WYNIK.txt). Atrapy, bez flashowania, testów podpisów i sprzętowego rollbacku. Firmware/Android/sekrety bez zmian, bez pobierania binarek. **Następne: R10 — czujniki DS18B20/DHT i ważność pomiarów.**

## Historyczny postęp po R08 — 24.10.2026
**8/15 tematów, 80/150 obszarów analizy.** [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R08: Telegram i WWW](narzedzia/audyt-v3.55.2/research2/R08_TELEGRAM_WWW.md) · [17 źródeł](narzedzia/audyt-v3.55.2/research2/R08_ZRODLA.md).

Doprecyzowania #19/#23/#24/#38/#39/#64/#72/#77/#78:
- Offset getUpdates potwierdza wiadomość usłudze, nie wykonanie w ESP. Kod TDLib usuwa wcześniejsze zdarzenia niezależnie od skutku polecenia.
- Rzeczywisty ekstraktor tekstu wybiera cytowane /update zamiast własnego /status, jeśli reply_to_message poprzedza text; oficjalny serializer stosuje taką kolejność. Test jedynie zarejestrował odczytane polecenie, bez wykonania.
- Wysyłka może uznać ucięte ok:true za sukces. Nieudane alarmy nie są ponawiane przy stałym stanie; nieudane ostrzeżenie nie powstrzymuje wyłączenia odbioru Telegrama.
- Sztuczny SSID zmienia strukturę HTML dwóch list panelu. Osobne terminalEscape poprawnie zabezpiecza tekst HTML — kontrola dodatnia. Nie wykonywano skryptu w przeglądarce.
- Sprawdzenie tokena jednej ścieżki nie obejmuje pozostałych, tgChatId nie jest filtrem nadawców, a setInsecure nie uwierzytelnia serwera Telegrama.

**R08: 27 asercji C++ + 5 JS/HTMLParser = 32 PASS, kod 0.** [Wynik](narzedzia/audyt-v3.55.2/research2/R08_TEST_WYNIK.txt). Wycinki i atrapy, nie pełny HTTP/TLS/ESP/TDLib. Nie ponawiano historycznej regresji 31/0. Firmware/Android i sekrety bez zmian, bez OTA. **Następne: R09 — weryfikacja obrazu, autentyczność, panel LCD.**

## Historyczny postęp po R07 — 24.10.2026
**7/15 tematów, 70/150 obszarów analizy.** Druga runda nadal trwa. [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R07: żądania WWW i upload](narzedzia/audyt-v3.55.2/research2/R07_HTTP.md) · [22 źródła](narzedzia/audyt-v3.55.2/research2/R07_ZRODLA.md).

Doprecyzowanie #20/#22 i powiązania #19:
- Dla przykładowego JSON Telegrama wszystkie 59 podziałów wywołuje zapis dwa razy; 14 pozostawia enabled=false. To wynik wycinka z atrapą zapisu, nie pomiar częstości awarii urządzenia.
- Parser chunked wywołuje końcowy callback len=0; obecny odbiorca konfiguracji ponownie zapisuje enabled=false. `send` przygotowuje odpowiedź, niekoniecznie od razu wysyła ją w TCP.
- Przeplot dwóch uploadów miesza zawartość plików mimo blokad poszczególnych operacji. Nieudana blokada otwarcia nowego pliku może pozostawić stary uchwyt i skierować następny zapis do złego pliku.
- Kontrola uprawnień dopiero w końcowym handlerze/middleware nie chroni wcześniejszych skutków body/upload. Dokumentacja wymaga odrębnej kontroli tych callbacków.
- Gotowy AsyncJson wymaga weryfikacji rzeczywistych granic bufora przy chunked; nie jest używany przez obecne ręczne endpointy. Nie ogłoszono awarii tego helpera na centrali.

**R07: 78 asercji PASS dla 3.10.3 i te same 78 PASS dla 3.11.2**, kod 0, rzeczywiste wycinki + atrapy. [Wynik](narzedzia/audyt-v3.55.2/research2/R07_TEST_WYNIK.txt). Bez pełnego HTTP/TCP/ESP i bez ponawiania historycznej regresji 31/0. Firmware/Android bez zmian, bez OTA. **Następne: R08 — Telegram/WWW: uprawnienia, odbiór, doręczenie i kodowanie.**

## Historyczny postęp po R06 — 24.10.2026
**6/15 tematów, 60/150 obszarów analizy.** Druga runda nadal trwa. [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R06: zadania i blokady](narzedzia/audyt-v3.55.2/research2/R06_WSPOLBIEZNOSC.md) · [19 źródeł](narzedzia/audyt-v3.55.2/research2/R06_ZRODLA.md).

Doprecyzowanie #16/#30/#48/#52/#58/#95/#98:
- Dwie poprawnie obsłużone sekcje nie chronią walidacji między nimi. Kontrolowany przeplot rzeczywistych gałęzi odtwarza ON=42/OFF=58; wykonanie szeregowe odrzuca sprzeczne polecenie.
- Fragment replay pozostawia logMutex po udanym/nieudanym remove i błędzie alokacji; brak strony zwalnia go prawidłowo. Fragment czyszczenia historii zmienia String mimo niezdobytej blokady.
- Spinlock niskiego poziomu i pełna sekcja krytyczna portu mają różne zachowanie przerwań. Dziedziczenie priorytetu nie naprawia brakującego give.
- Zapis flash na Core0 może wpływać na drugi rdzeń. Flaga FREERTOS_SMP wybiera wariant jądra, nie zastępuje UNICORE. Faktyczna konfiguracja builda pozostaje do potwierdzenia.

**R06: 23 asercje PASS, kod 0**, rzeczywisty spinlock.h/wycinki portu i centrali + atrapy, bez sprzętu i pełnego RTOS. [Wynik](narzedzia/audyt-v3.55.2/research2/R06_TEST_WYNIK.txt). Historycznej regresji 31/0 nie ponawiano. Firmware/Android bez zmian, bez OTA. **Następne: R07 — serwer WWW, składanie żądań i równoległe wywołania.**

## Historyczny postęp po R05 — 24.10.2026
**5/15 tematów, 50/150 obszarów analizy.** Druga runda trwa. [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R05: Firebase i wyniki operacji](narzedzia/audyt-v3.55.2/research2/R05_FIREBASE.md) · [24 źródła](narzedzia/audyt-v3.55.2/research2/R05_ZRODLA.md).

Doprecyzowanie #49 oraz #17/#36/#37/#48:
- Test wycinka returnResult potwierdza: sam Connecting nie kopiuje się do świeżego proxy zwykłego PATCH. Nadal nie ma podstaw do diagnozy, że każdy zapis jest potwierdzany przedwcześnie.
- Odtworzono możliwość nadpisania ponownie użytego wyniku przez stary zarejestrowany slot. Nie odtworzono pełnego przebiegu sieciowego ani utraty danych centrali.
- isError konsumuje informację o błędzie; znaczniki dostępności danych wygasają. Wynik trzeba przechwycić i rozliczyć świadomie, nie wielokrotnie odpytywać bez uwzględnienia skutków.
- Timeout i stop gniazda nie są dowodem anulowania kolejki ani cofnięcia zapisu serwera. stopAsync też wymaga kontroli właściwego zadania i zakończenia obsługi.
- Istniejąca ochrona identycznej paczki i sprawdzenie końca ringu są istotne. Spóźniony sukces identycznej próby nie musi być błędem; nie może jednak potwierdzać nowej zawartości.

**R05: 27 asercji PASS, kod 0**, wycinki biblioteki + atrapy, bez HTTP/Firebase/ESP. [Wynik](narzedzia/audyt-v3.55.2/research2/R05_TEST_WYNIK.txt). Nie powtarzano historycznej regresji 31/0. Firmware/Android i dane użytkownika bez zmian. **Następne: R06 — FreeRTOS, współbieżność i blokady.**

## Historyczny postęp po R04 — 24.10.2026
**4/15 tematów, 40/150 obszarów analizy.** Druga runda nadal trwa. [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R04: NVS, nastawy i kasowanie](narzedzia/audyt-v3.55.2/research2/R04_NVS.md) · [20 źródeł](narzedzia/audyt-v3.55.2/research2/R04_ZRODLA.md).

Rozwinięcie #33/#48/#76/#95 i wymagań rollbacku:
- Arduino 3.3.8 ma dodatkową ścieżkę skasowania całej znalezionej partycji NVS przy NO_FREE_PAGES/NEW_VERSION_FOUND, przed setup centrali. Nie stwierdzono, że wykonała się u użytkownika.
- Preferences wywołuje commit; w IDF 5.5.4 zapis jest już w ścieżce set, a commit poprawnego uchwytu nie tworzy transakcji kilku kluczy.
- Ignorowany błąd zapisu może rozdzielić RAM i NVS. Istniejąca kontrola startowa progów przywraca 60/50 dla błędnej pary — zabezpieczenie działa, ale nie zachowuje wtedy wybranych nastaw i nie naprawia samego NVS.
- Spool sprawdza wynik zapisu bool i raportuje błąd; nie wszystkie ścieżki mają tę samą wadę.
- Zgłoszenia IDF z 10.10.2026 wskazują dodatkowe ryzyka odzyskiwania. Porównano wzorce z 5.5.4, bez pełnej reprodukcji i bez przypisywania ich awarii centrali.

**R04: 31 asercji PASS, kod 0**, rzeczywiste wycinki + atrapy NVS/partycji. [Wynik](narzedzia/audyt-v3.55.2/research2/R04_TEST_WYNIK.txt). To nie historyczne 31 poleceń regresji Z4 ani test sprzętu. Firmware i Android bez zmian; bez OTA, kasowania, push i merge. **Następne: R05 — FirebaseClient, potwierdzenia i ponawianie.**

## Historyczny postęp po R03 — 24.10.2026
**3/15 tematów, 30/150 obszarów analizy.** R01–R03 zakończone; cała druga runda nadal trwa. [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R03: pełny raport LittleFS](narzedzia/audyt-v3.55.2/research2/R03_LITTLEFS.md) · [21 źródeł i ograniczenia wersji](narzedzia/audyt-v3.55.2/research2/R03_ZRODLA.md).

Doprecyzowanie #17/#18/#48/#76/#98/#99:
- Bezpośrednia podmiana pliku i sekwencja remove → rename mają różne gwarancje. Usunięcie dobrej kopii przed publikacją nowej pozostawia okno utraty nazwy docelowej.
- Arduino wykonuje fflush/fsync/fclose, lecz nie przekazuje wyników przez File::flush/close. Sam niezerowy zapis nie potwierdza kompletnego, trwałego dokumentu.
- Sprawdzony port ma blokadę wewnętrzną i odrzuca rename otwartego pliku; nie chroni jednak całej sekwencji kopiowania i podmiany logu. Jego dokładne dopasowanie do firmware nadal nieustalone.
- Spool weryfikuje końcową stronę przed zdjęciem rekordów z RAM; protokół jest silniejszy niż sprawdzenie zera w zapisie archiwum solarnego.

**Nowa próba R03:** rzeczywisty kod littlefs na symulowanym nośniku; 52 scenariusze rename (36 z przerwaniem, 16 kontrolnych) zachowały starą/nową pełną treść. 8 kontroli restartu po remove przed rename wykazało brak pliku docelowego i zachowany tmp. [Wynik](narzedzia/audyt-v3.55.2/research2/R03_TEST_WYNIK.txt). Nie jest to test ESP ani powtórzenie historycznej regresji 31/0. Numeracja 1–105 zachowana, firmware i Android nietknięte, bez OTA. **Następne: R04 — NVS, nastawy i formatowanie.**

## Historyczny postęp po R02 — 24.10.2026
**2/15 tematów, 20/150 obszarów analizy.** Zakończono R01 i R02; druga runda jako całość nadal trwa. [Plan](narzedzia/audyt-v3.55.2/research2/PLAN.md) · [R02: pełna analiza sieci i watchdogów](narzedzia/audyt-v3.55.2/research2/R02_SIEC_WDT.md) · [23 źródła](narzedzia/audyt-v3.55.2/research2/R02_ZRODLA.md).

Nowe doprecyzowania do #15/#53/#63/#80 i nadzoru:
- Arduino 3.3.8 zachowuje limity handshake/socket po stop; komentarz centrali oparty na historycznym błędzie 2.0.2 jest nieaktualny.
- setTimeout(4) ustawia 4 ms w Stream, nie cztery sekundy całego TLS. TCP/connect i handshake mają odrębne pola i jednostki.
- Postęp zapisu TLS odnawia czas; HTTP może ponawiać zerowy odczyt, dopóki połączenie pozostaje otwarte. Poprawienie jednostek handshake nie daje limitu całego żądania.
- Limit DNS callera nie kończy workera. Resolver ma własne retry — nie należy nazywać każdego braku odpowiedzi DNS bezterminowym zawieszeniem.
- Karmienie WDT podczas oczekiwania na sieciowy mutex nie dowodzi wykonywania ochrony. Nadzorca w loop nie wykona się, gdy pętla stoi przed jego wywołaniem.

**Nowe próby R02: 18 asercji, kod 0**, prawdziwe wycinki bibliotek i jawne atrapy czasu/transportu. [Wynik](narzedzia/audyt-v3.55.2/research2/R02_TEST_WYNIK.txt). To nie pomiar na ESP ani powtórzenie historycznych 31 poleceń Z4. Nie ustalono przyczyny wcześniejszego WDT IDLE1. Numeracja 1–105 zachowana, firmware bez zmian, bez OTA. **Następne: R03 — LittleFS i trwałość plików.**


## Druga, pogłębiona runda researchu — w toku, 24.10.2026
Na ponowne polecenie użytkownika rozpoczęto badanie **każdego z 15 tematów w 10 jawnych obszarach**. Nie deklarujemy mierzonego mnożnika czasu 10×; nie ma pomiaru czasu pierwszej rundy. [Plan i stan](narzedzia/audyt-v3.55.2/research2/PLAN.md).

**Stan historyczny po R01: wykonano OTA i powrót bez kabla; wtedy 1/15 tematów, 10/150 obszarów. Aktualny postęp podano wyżej.** [Szczegółowa analiza R01](narzedzia/audyt-v3.55.2/research2/R01_OTA.md), [źródła](narzedzia/audyt-v3.55.2/research2/R01_ZRODLA.md).

Nowe rozstrzygnięcia do #1/#27:
- Manifest zadeklarowanej platformy wskazuje Arduino 3.3.8 i pakiet IDF 5.5.4; nie identyfikuje to wgranego bootloadera.
- **Korekta:** CONFIG_APP_ROLLBACK_ENABLE jest starą nazwą CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE, nie drugim niezależnym przełącznikiem.
- **Istotne ryzyko kolejnego OTA:** Arduino Update nie wywołuje esp_ota_begin, więc nie można przypisać mu ochrony tej funkcji przed następnym OTA podczas PENDING_VERIFY. Kolejna instalacja może naruszyć poprzedni dobry slot. Brak jawnej bramki w analizowanych wejściach centrali; scenariusz nieprzetestowany na urządzeniu.
- Update.canRollBack sprawdza tylko początek obrazu, a nie pełną poprawność poprzednika. API rollback_and_reboot restartuje tylko po powodzeniu wcześniejszej operacji.
- Powrót starego programu nie oznacza cofnięcia zapisanych nastaw i plików; wymagana zgodność danych po powrocie.

**Nowe próby hostowe R01: 28 asercji, dwie konfiguracje, kod 0**, rzeczywiste wycinki upstream i jawne atrapy flash/restartu/CRC. Nie testowano bootloadera na ESP, przerwanego zapisu ani drugiego OTA. To osobny zestaw, nie zmiana historycznej regresji Z4 31/0. [Wynik](narzedzia/audyt-v3.55.2/research2/R01_TEST_WYNIK.txt).

Firmware bez zmian, bez OTA/publikacji. Numeracja 105 ustaleń zachowana; badanie rozwija istniejące punkty. **Następne: R02 — limity sieci i watchdogi.** Zakończenie audytu źródeł i pierwszej rundy opisane niżej pozostaje historycznie prawdziwe; druga runda nadal trwa.


## Wynik końcowy — audyt źródeł zakończony
**24.10.2026: zakończono P001–P088 oraz Z1–Z4, wszystkie 92 zadania planu.** Przeczytano 46 810/46 810 linii main (w tym osadzony HTML/JavaScript), cztery dostępne lokalne nagłówki logiki, osiem bazowych testów C++ i trzy runnery. Sprawdzono powiązania modułów i jawnie zapisano nierozstrzygnięte zależności.

- **105 numerowanych ustaleń:** błędy, ryzyka warunkowe, ograniczenia testów i rozbieżności wymagań. **Nie są to 105 niezależnych, odtworzonych awarii urządzenia.** Numeracja zachowana, żeby nie zerwać odniesień do testów.
- **Firmware bez zmian. Żadnych poprawek produkcyjnych, kompilacji całego obrazu, publikacji wydania ani OTA w ramach audytu.**
- Dostępny zestaw offline: **31 poleceń kod 0**, w tym 3665 sprawdzeń kompletnych wiadomości. Część testów celowo potwierdza istnienie usterek. Szczegóły i znane słabości: Z1/Z3/#104.
- **Nie rekomenduję wgrywania tej v3.55.2 bez poprawek i sprawdzenia odzyskania zdalnego dostępu.** Nie testowano ESP32/FreeRTOS, fizycznych wyjść, rzeczywistej sieci i rollbacku bootloadera. Zakończony audyt źródeł nie jest certyfikatem bezpieczeństwa.

### Co naprawiać najpierw — propozycja do zatwierdzenia
1. **Ochrona pieca i wyjścia:** ręczne OFF podczas CO (#7), nieważne pomiary (#8–10), kolizje alarmów i przerwanie ruchu (#13/#14/#101/#103), bezpieczny start (#32), nastawy (#33/#34/#45/#95).
2. **Zdalny dostęp i aktualizacja:** uprawnienia Telegram/WWW (#19/#23/#39), kompletne polecenia/konfiguracja (#20/#24/#36/#51), nieblokująca pętla sterująca (#80), nadzór i OTA (#1–5/#15/#16/#25–27/#48/#72).
3. **Trwałość danych i wiarygodność informacji:** brak automatycznego formatowania cennej partycji (#76), archiwum solarne i spool (#17/#18/#67/#73), zapisy historii/logów, kolejki powiadomień i prezentacja starych/symulowanych wartości.

Szczegółowy indeks wszystkich numerów, grupy napraw, granice dowodów, kontrola duplikatów oraz warunki przyszłego wgrania: **[Z4 — wynik końcowy](narzedzia/audyt-v3.55.2/Z4_WYNIK_KONCOWY.md)**.

## Aktualizacja po pełnym odczycie i researchu — 24.10.2026
**Przeczytałem ponownie cały raport (928 linii przed tą aktualizacją), następnie zweryfikowałem najważniejsze pytania w dokumentacji i kodzie bibliotek.** Pobrano 33 pliki źródłowe/dokumentacyjne przez GitHub API. Prześledzono odpowiednie kontrakty i funkcje; nie jest to deklaracja pełnego audytu tych 33 plików ani test działającej centrali.

Pełna analiza: **[Research — 15 pytań i rozstrzygnięć](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md)**. Wersje, commity, trwałe URL i SHA256: **[rejestr 33 źródeł](narzedzia/audyt-v3.55.2/RESEARCH_ZRODLA_2026-10-24.md)**. Odsyłacze R01–R15 poniżej prowadzą tematycznie do tego dokumentu.

### Co potwierdzono lub doprecyzowano
| Temat | Wynik researchu | Ustalenia / rozdział |
|---|---|---|
| Powrót po nieudanym OTA | Rollback wymaga odpowiedniego bootloadera i restartu niezatwierdzonego obrazu. Samo czekanie, dwa sloty i sdkconfig.defaults nie wystarczą. Hook Arduino trzeba potwierdzić w rzeczywistym buildzie. | #1/#26/#27/#80; R01 |
| Limit TLS | W Arduino 3.3.8 argument to sekundy: 4000 = 66 min 40 s, 7000 = 116 min 40 s. To nie zmierzony czas zawieszenia ani dowód przyczyny WDT. | #15/#54/#63; R02 |
| Trwałość plików | littlefs deklaruje atomowy rename/remove, lecz dwa osobne wywołania remove→rename nie tworzą jednej transakcji. Arduino flush wykonuje fsync, ale nie przekazuje błędu przez wynik tej metody. | #18/#28/#98/#99; R03 |
| NVS i formatowanie | Preferences wykonuje commit i zwraca informację o powodzeniu; problemem są ignorowane wyniki i spójność kilku nastaw. begin(true) może formatować po ESP_FAIL montowania. | #33/#76/#95; R04 |
| Firebase ACK | Prześledzenie publikacji wyniku nie potwierdziło tezy, że sam debug przedwcześnie potwierdza zwykły PATCH. Timeout/anulowanie/ponowne użycie wyniku wymagają rzeczywistej biblioteki w teście. | #49 nadal ryzyko; R05 |
| Wspólne dane i HTTP | Dwa rdzenie ani volatile nie zastępują protokołu blokad; callback body może dostarczać fragment JSON. Nie wykonywać długiej sieci/flash pod spinlockiem. | #20/#48/#95/#98; R06–R07 |
| Uprawnienia i obraz OTA | Token bota, zalogowany panel i TLS to różne warstwy. 0xE9 nie dowodzi kompletności obrazu, ale przejście stagingu nie dowodzi instalacji go przez LCD. | #19/#23/#25/#39/#70; R08–R09 |
| Czujniki DS18B20 | W sprawdzonym nettigo available() może oznaczać sam upływ konwersji; nie potwierdza obecności i CRC. TEMP_ERROR to −273,15°C, nie wartość z innej biblioteki. Kopia urządzenia niepotwierdzona. | #8–10; R10 |
| Czas i GHI | 64-bitowy time_t IDF nie naprawia lokalnego int32_t. GHI jest średnią poprzedniej godziny; obecny filtr (start,end] jest prawidłowy i nie powinien zostać cofnięty. Domyślnego offsetu API pogody nie wolno uznać za pełną obsługę DST. | #35/#47/#59/#60; R11–R12 |
| Status, historia i liczby | Dostępna baza nie dowodzi świeżości centrali; kursor musi obsłużyć remisy, a toFloat/toInt nie są pełną walidacją. | #55/#82/#86/#105; R13–R14 |

**Najważniejsza korekta interpretacji:** nie obwiniamy automatycznie LittleFS, Firebase ani zegara ESP32. Część usterek wynika ze sposobu użycia tych mechanizmów w aplikacji. Pozostaje **105 numerów**, bez dopisywania hipotetycznych awarii. #49 nie jest dowiedzioną utratą danych, #54 nie wyjaśnia dawnego WDT, #81 nie dowodzi ataku sieciowego.

### Co trzeba rozstrzygnąć przed zmianą kodu
Research R15 sprowadza decyzje do trzech grup: **pierwszeństwo ochron i fizyczne stany wyjść; warunek odzyskania zdalnego dostępu; trwałość i zachowanie danych po błędzie**. Dokumentacja internetowa nie ustali bezpiecznej pozycji syberka przy wszystkich równoczesnych alarmach ani nie potwierdzi konfiguracji wgranego bootloadera. Zmiana firmware nadal wymaga odrębnej zgody. Rytm statusu 8 s/60 s pozostaje nietykalny; solar to zbiornik 100 l/BUS_3, nie bojler pieca.

W tej rundzie zmieniono wyłącznie Markdown. **Nie wykonano nowych testów sieci/ESP ani OTA.** Wynik 31/0 wyżej jest poprzednią pełną regresją Z4, nie nowym pomiarem urządzenia. Końcowa kontrola dokumentacji i niezmienności kodu: patrz dopisek na końcu raportu. Nowe doprecyzowania researchu mają pierwszeństwo przed szerszą interpretacją historycznych opisów; historia i numeracja pozostają zachowane.

## Zakres i granice zakończenia
Źródło: `firmware-patch/v3.55.2-nadzor-zadan/main_centrala.cpp`, SHA256 `b6c3966e06ec38be155cef0e1980fade0e4399cf999380172cb20dd4a9e00760` (46 810 linii, 46 809 znaków końca linii). Nagłówki bazowe pochodzą z paczki v3.54.0. Pełna lista 16 plików z SHA256: `narzedzia/audyt-v3.55.2/ZRODLA_SHA256.json`.

Nie objęto deklaracją pełnego odczytu całego Androida, firmware panelu LCD, wszystkich starych paczek ani wszystkich źródeł zewnętrznych bibliotek. Zgodność bibliotek i nagłówków z projektem budowanym u użytkownika oraz działający rollback pozostają niepotwierdzone. #49 pozostaje ryzykiem integracji, a nie odtworzoną utratą danych; #54 nie jest dowodem przyczyny WDT; #81 nie dowodzi ataku z sieci.

**Poniżej zachowano chronologiczną dokumentację audytu.** Dawne „następne kroki”, liczniki pokrycia i otwarte Z1–Z4 opisują stan w danej rundzie, nie aktualny status. Wynik końcowy powyżej i dokument Z4 mają pierwszeństwo. Wszystkie propozycje napraw wymagają odrębnej zgody przed zmianą kodu.

## Ustalenia — poziom dowodu i ograniczenia przy każdym punkcie

### 1. OTA może zostać zatwierdzone bez dowodu zdalnego dostępu — wysoki priorytet
Linie 45041–45051; stałe 44974–44975. Po 10 minutach wystarcza aktualne połączenie WiFi, a po 60 minutach zatwierdzenie następuje nawet bez WiFi. Nie jest wymagane udane połączenie z Telegramem ani odbiór komendy. WiFi nie dowodzi dostępu do internetu. To istniejąca polityka, ale nie spełnia wymogu zachowania zdalnego dostępu. Potwierdzenie obrazu wyłącza automatyczny rollback dla tego obrazu.
Propozycja: osobno określić warunek sprawności zdalnej i politykę przy awarii internetu; nie potwierdzać obrazu tylko na podstawie upływu czasu. Przed wdrożeniem konieczna weryfikacja rollbacku w rzeczywistym bootloaderze i konfiguracji kompilacji.

**Doprecyzowanie po researchu 24.10.2026:** Rollback następuje przy ponownym rozruchu niezatwierdzonego obrazu i odpowiednim bootloaderze; nie samoczynnie od upływu czasu. Sprawdzenie hooka i konfiguracji frameworka opisuje R01. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

### 2. Nadzór ignoruje zadanie, które nigdy nie wystartowało — wysoki priorytet
Linie 46661–46665: heartbeat równy zero jest traktowany jako brak powodu do restartu. Dwa zera usypiają nadzór bezterminowo. Linie 46302 i 46308: wynik tworzenia zadań nie jest sprawdzany. Przy nieudanym utworzeniu zadania, np. z braku pamięci, nadzór może nie przywrócić łączności.
Odtworzenie na funkcji wyciętej z rzeczywistego źródła, kompilowanej przez g++: czas 1 000 000 ms, tg=999 999, poll=0 -> BRAK; oba heartbeat=0 -> USPIONY.
Propozycja: sprawdzać wynik tworzenia zadań i osobno ograniczać czas oczekiwania na ich pierwszy sygnał życia, uwzględniając celowo wyłączone usługi.

**Druga runda R15:** Porównawcza oryginalna funkcja NASA HS dochodzi do akcji również przy nigdy nieuruchomionej aplikacji. To przykład poprawnie określonego terminu startu, nie test własnego nadzorcy centrali ani zalecenie kopiowania jego limitów resetów. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### 3. Porównania terminów zawodzą przy zawinięciu millis — wysoki priorytet
Licznik 32-bitowy zawija się po około 49,7 dnia ciągłej pracy.
- 32123–32128: ręczne wymuszenie pompy może wygasnąć za wcześnie.
- 32347 i 32360: przerwa pompy może zostać skrócona.
- 31720–31730: harmonogram impulsowego zasilania czujnika dymu może przełączać się zbyt wcześnie.
- Pozostałe wykryte miejsca do przeglądu: 26730, 29985, 29994, 30005, 32660, 40417, 42197, 42471, 42946.
Odtworzenie: uint32_t now=0xfffffff0, deadline=now+60000. Wyrażenie now<deadline daje false, a now>=deadline daje true, chociaż termin jest minutę w przyszłości.
Propozycja: porównania odporne na zawinięcie i testy wartości granicznych; zachować znaczenie zera jako nieaktywnego terminu oraz sprawdzić zakres dopuszczalnych opóźnień.

**Druga runda R11:** Rzeczywisty helper symulacji z terminem 60 s po zawinięciu wygasił ją od razu. To nie dotyczy wszystkich pomiarów odstępów: getLocalTime z uint32 i odejmowaniem przeszedł próbę wrap. Horyzont i znaczenie zera muszą być częścią poprawki. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R11_CZAS.md).

### 4. Sam nadzór zasypia na 3 minuty po zawinięciu licznika
Linia 46659: nowMs < 180000 jest uznawane za początek pracy również po zawinięciu licznika. Odtworzenie: now=100000, tg=0xfff00000, poll=99999 -> USPIONY mimo przeterminowanego tg.
Propozycja: rozbieg jako jednorazowy stan po uruchomieniu, nie warunek oparty stale na surowym millis.

**Druga runda R11:** Rzeczywiste nadzorDecyzja zwraca USPIONY przy now=100000 po wrap mimo starego heartbeat; kontrole po rozbiegu wykrywają brak życia i limit restartów. Nie uruchamiano restartu ESP. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R11_CZAS.md).

### 5. Limit restartów nie jest limitem w ruchomym oknie 30 minut
Linie 46703–46708: licznik zeruje się po 30 minutach pracy, bez sprawdzenia zdrowia zadań. Po osiągnięciu limitu i 30 minutach działania ze stale zawieszonym zadaniem nadzór ponownie może wykonać restart. Wiadomość „w ciągu 30 minut” (43751) nie opisuje dokładnie tej implementacji.
Propozycja: ustalić politykę; resetować licznik po okresie faktycznie zdrowej pracy albo prowadzić prawdziwe okno czasowe. Wcześniejsze zapewnienie, że po trzecim restarcie nadzór wycofa się na stałe, było nieprawidłowe.

### 6. Raport restartu jest kasowany przed potwierdzeniem wysyłki
Linie 43738–43747: jednorazowa flaga RAM i znacznik NVS zmieniane są przed wysłaniem, a znacznik NVS również przed sprawdzeniem tgEnabled. Brak skutecznej wysyłki może oznaczać utratę tego raportu; w tej funkcji nie ma ponowienia.
Propozycja: zachować oczekujący raport do potwierdzenia dostarczenia, z ograniczeniem częstości prób.

## Ryzyka wymagające dalszej weryfikacji — nie dowiedzione awarie
- 25646–25656: opróżnianie kolejki WebSocket bez limitu liczby wiadomości/czasu. Ciągłe dokładanie wiadomości może przedłużać pojedyncze wywołanie. Nie udowodniono, że powodowało dotychczasowy crash.
- 45000: odczyt całego pliku znacznika OTA przez readString bez limitu wielkości. Normalnie plik zawiera tylko numer wersji; ryzyko dotyczy uszkodzonego/nietypowego pliku.
- Skuteczność rollbacku zależy od bootloadera i konfiguracji urządzenia; samo istnienie wywołań API w C++ nie stanowi dowodu.

## Testy wykonane
- Istniejące testy nadzoru: 34/34.
- Istniejące testy historii: 48/48.
- Istniejące testy telemetrii: 20/20.
- Dodatkowy program hostowy z wyciętą funkcją nadzorDecyzja odtworzył punkty 2 i 4; arytmetyka uint32_t odtworzyła punkt 3.
Testy hostowe nie uruchamiają całego firmware, FreeRTOS, sieci ani bootloadera. Dotychczasowe testy akceptują zerowy heartbeat jako oczekiwane zachowanie, dlatego zielony wynik nie wykrywa luki opisanej w punkcie 2.

## Następny krok
Wymagana zgoda użytkownika przed zmianą kodu. Zalecana jedna spójna poprawka, testy regresyjne oraz weryfikacja konfiguracji budowania i rollbacku przed przygotowaniem następnego pliku do OTA. Nie wykonywano flashowania, publikacji wydania ani OTA.

# Kontynuacja audytu — przegląd 2 (bez zmian firmware)

## Nowe ustalenia; numeracja kontynuowana

### 7. Ręczne wyłączenie pompy omija alarm przegrzania — KRYTYCZNY
**Dowód:** 31332–31337 uruchamia zabezpieczenie tylko przy pierwszym wejściu w alarm; 31365 zawiesza zadanie pompy. Następnie `/api/pompa-manual` (29410–29414) oraz `wykonajKomendePanelu("pompa_wyl")` (42503–42506) bez sprawdzenia alarmu wykonują `pompaUstaw(false,true)`. Sama funkcja 21904–21929 nie sprawdza alarmu. Kolejne wywołania `temperatura_alarmowa()` nie włączą ponownie pompy, gdy flaga alarmu już jest ustawiona.
**Scenariusz:** alarm CO włącza pompę → przychodzi ręczne OFF → pompa gaśnie przy nadal aktywnym alarmie.
**Zalecenie późniejszej poprawki:** jeden nadrzędny warunek bezpieczeństwa wyjścia, obowiązujący wszystkie kanały rozkazów; test kolejności alarm/komenda. Nie testować tego na gorącym piecu.

**Druga runda R15:** R15 łączy prawdziwe temperatura_alarmowa/aktywujAlarmCO/pompaUstaw/scheduler: po alarmie wymuszone OFF i ponowna kontrola pozostawiają pompę wyłączoną. GPIO to atrapa; nie wykonano polecenia na gorącym piecu. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### 8. Filtr odrzuca rzeczywiste temperatury ≤ −1°C — wysoki
30319–30323: każda wartość <= -1 jest interpretowana jak błąd, choć 30529–30539 dopuszczają temperatury ujemne (zewnętrzna, solar, pokój, powrót, trociny). Przykład: ostatnia dobra +5°C, nowy poprawny odczyt −5°C → zwrócone +5°C i brak akceptacji nowej próbki. Dotyczy zwłaszcza zimy i nieużywanego kolektora.
**Zalecenie:** oddzielić status błędu od liczbowej wartości temperatury; zachować poprawne temperatury ujemne.

**Druga runda R10:** Test drugiej rundy połączył rzeczywisty nettigo z funkcją centrali: biblioteka dekoduje −5, filtr trzyma +5; −0,5 przechodzi. Nie jest to wada dekodowania znaku w DS. Ujemny zakres testowano dla powrotu, nie bojlera pieca. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R10_CZUJNIKI.md).

### 9. Naprawa magistrali DS może zostać anulowana przez inną, zdrową magistralę — wysoki
30198–30206 łączą wyniki trzech `reset()` operatorem OR. Wystarczy odpowiedź dowolnej magistrali, aby uznać reset za skuteczny i pominąć odcięcie zasilania, choć inna pozostaje uszkodzona. Ponadto 30496–30499 zeruje wspólny licznik błędów po poprawnym odczycie dowolnego czujnika; błędy pojedynczego czujnika mogą być maskowane odczytami pozostałych.
**Zalecenie:** diagnostyka per magistrala/czujnik i potwierdzanie powrotu prawidłowych pomiarów, nie samej obecności dowolnego układu.

**Doprecyzowanie po researchu 24.10.2026:** W sprawdzonym upstream nettigo available() jest także warunkiem upływu czasu konwersji, a nie potwierdzeniem zdrowia magistrali. TEMP_ERROR=-273,15°C; dokładna lokalna kopia biblioteki nadal nieznana. R10. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

**Druga runda R10:** R10 odtworzyło jeden odczyt zamiast trzech i brak eskalacji TEMP_ERROR=−273,15 (kod eskaluje 85 lub <=−998). request=false nie unieważnia available; stare scratchpad może dostać nowy czas akceptacji. Pętla begin w setup również kończy retry po sukcesie dowolnej magistrali. Zależność liczbowa od TEMP_ERROR wymaga zgodności lokalnej biblioteki z badanym upstream. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R10_CZUJNIKI.md).

### 10. Brak ważnego pomiaru może zostać uznany za ostygnięcie pieca — KRYTYCZNE RYZYKO
30559 ustawia temperaturę CO na −1, gdy żadna magistrala nie jest gotowa; 30418/30427/30436 również zwracają −1 po przekroczeniu czasu. 31339 porównuje tę liczbę z dolnym progiem alarmu bez sprawdzania ważności; 32241/32313/32365 traktują ją jak niską temperaturę w sterowaniu pompą. 31634 może wyłączyć bramkę temperaturową nadzoru dymu po ustaniu innych wymuszeń. Z kolei zachowanie ostatniej dobrej wartości może maskować rzeczywistą zmianę temperatury.
**Status:** potwierdzona ścieżka logiczna; skutki fizyczne zależą od momentu awarii i innych alarmów. Potrzebny osobny bezpieczny stan „brak wiarygodnego pomiaru”, nie utożsamiany z zimnym piecem.

**Druga runda R15:** W połączonej próbie R15 aktywne C.O. i wartość -1 kasują alarm i wznawiają zadanie pompy. To odtworzenie dla wartości błędu; nie test awarii fizycznej sondy i nie wyznaczenie bezpiecznej reakcji sprzętu. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

**Druga runda R10:** R10: 800 ms akceptowane, 801 ms zwraca −1 mimo wcześniejszych 50°C. Fallback po błędzie utrzymał liczbę przez 60 s, ale metadane wskazywały STALE. Nie wolno rozszerzać zarzutu na wszystkich odbiorców: mieszadło trocin ma kontrolę świeżości. Odbiór fizycznych zachowań zabezpieczeń nadal nieprzeprowadzony. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R10_CZUJNIKI.md).

### 11. Zatrzask pompy jest aktualizowany mimo odrzuconej zmiany wyjścia — wysoki
21915–21924: `pompaUstaw` może odmówić zmiany wskutek minimalnego czasu stanu i zwraca void. 32313–32316 mimo odmowy wyłączenia ustawia `zatrzaskWybor2=true`, przez co kolejne próby OFF są pomijane. Podobna konstrukcja 32241–32247 ustawia blokadę niskiej temperatury, a 32344–32347 zaczyna okres OFF bez sprawdzenia rzeczywistego wyłączenia.
**Zalecenie:** potwierdzenie wykonania i ponawianie żądania po upływie blokady, bez fałszywego zapamiętywania stanu.

### 12. Piąta próbka w trybie czasowym jest nieosiągalna — średni
32275 wymaga `faza < czasWlaczenia`, a dla `probekWCyklu1 == 4` próg 32276 wynosi dokładnie `czasWlaczenia`. Warunki nigdy nie mogą być jednocześnie spełnione. Średnia jest z najwyżej czterech punktów, mimo założenia pięciu.

### 13. Polecenie zatrzymania serwa w aktualnej pozycji nie odwołuje starego ruchu — wysoki
30906–30909 zwraca z `zlecServoRuch`, gdy aktualna pozycja równa się nowemu celowi, nie kasując `r.aktywny` i starego celu. 31388–31392 celowo wywołuje tę funkcję także dla serwa otwierającego się ze zera: alarm przed pierwszym krokiem → cel 0, pozycja 0 → stary ruch otwierania pozostaje aktywny. Analogiczne zachowanie przy ręcznej zmianie celu na bieżącą pozycję.
**Zalecenie:** nowe polecenie musi odwołać poprzedni ruch także wtedy, gdy nie wymaga nowego kroku.

**Druga runda R15:** R15 wykonuje oryginalne zlecServoRuch: stare otwieranie pozostaje po zleceniu celu równego pozycji; odmienny nowy cel poprawnie zastępuje stary. Pozycja programowa nie dowodzi pozycji mechanicznej. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### 14. Alarm solarny może przerwać ruch ochronny alarmu CO — wysoki
31332–31351: alarm CO jest obsłużony wcześniej, a nowy alarm panelu wywołuje 31413–31414 zatrzymujące oba serwa. Jeśli oba alarmy wystąpią razem, zaplanowane zamykanie CO zostaje zatrzymane. Alarm CO nie zleca ruchu ponownie, dopóki pozostaje aktywny. Potrzebna jawna hierarchia jednoczesnych alarmów; nie wybierać pozycji bezpieczeństwa bez sprawdzenia wymagań instalacji.

**Druga runda R15:** R15 odtwarza połączenie rzeczywistych alarmów: solar→C.O. pozostawia zamykanie aktywne, C.O.→solar je anuluje przy tych samych flagach. Dodatkowo C.O.→dym zmienia cel na otwieranie, a późniejszy krok dymu może ponownie otworzyć po domknięciu C.O. Wycięte bloki dymu zakładają wystąpienie zdarzenia; nie testowano sensora ani fizycznego bezpieczeństwa. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### 15. Jednostki limitu TLS są błędne — wysoki, źródło biblioteki sprawdzone
W kodzie wielokrotnie `setHandshakeTimeout(4000)` opisano jako 4 s (np. 32741, 41856); 23468 ustawia 7000. W Arduino-ESP32 **3.3.8**, `NetworkClientSecure.cpp:450–451` mnoży argument przez 1000: API przyjmuje sekundy. Oznacza to 4000 s = 66 min 40 s i 7000 s = 116 min 40 s, a nie 4/7 s. `ssl_client.cpp:316–323` porównuje ten wynik z upływem millis.
**Źródła sprawdzone przez GitHub API:**
- https://github.com/espressif/arduino-esp32/blob/3.3.8/libraries/NetworkClientSecure/src/NetworkClientSecure.cpp#L450-L452
- https://github.com/espressif/arduino-esp32/blob/3.3.8/libraries/NetworkClientSecure/src/ssl_client.cpp#L316-L324
**Zastrzeżenie:** faktyczny czas blokowania może być krótszy przez błędy sieci, watchdog lub nadzór. Trzeba potwierdzić wersję/fork biblioteki w kompilacji użytkownika. Nie przypisuję temu automatycznie przyczyny wcześniejszego crashu.

### 16. Replay bufora offline pozostawia zajętą blokadę plików — KRYTYCZNY DLA DOSTĘPNOŚCI
40423 przejmuje `logMutex`. Gałąź `cursor >= recordCount` (40440–40455) wraca zarówno po udanym, jak i nieudanym usunięciu **bez `xSemaphoreGive`**. Także 40459 wraca bez zwolnienia po nieudanej alokacji bufora JSON. Inne wyjścia tej funkcji zwalniają blokadę prawidłowo.
**Osiągalność:** zakończony replay + nieudane usunięcie strony w 40521 powoduje zachowanie pełnego kursora; następna próba wchodzi do 40440. Po utracie blokady inne zadania nie mogą korzystać z operacji chronionych logMutex, a ten sam task nie przejmie ponownie zwykłego mutexu.
**Zalecenie:** gwarantowane zwolnienie na każdym wyjściu; test wstrzykniętego błędu remove i braku pamięci.

### 17. Zamknięte doby solarne nie mają ścieżki automatycznej synchronizacji — wysoki
Jedynymi wywołaniami `solarArchiveSyncOne` są 23757 i 23759: dzień bieżący i następny. O północy 23643 zamyka poprzednią dobę, po czym 23644 przełącza datę na nową. Nie ma przeglądu zaległych plików. 23370–23373 wysyła wynik dopiero po finalized. W zwykłym przebiegu zamknięty wczorajszy wynik przestaje więc być kandydatem do wysłania. Także zaległości sprzed awarii internetu nie są automatycznie przeglądane.
**Zalecenie:** trwała kolejka zaległych dni lub ograniczony skan plików do synchronizacji, niezależny od bieżącej daty.

### 18. Zapis doby solarnej nie jest odporny na przerwę między usunięciem a rename — wysoki
23085 sprawdza tylko `written != 0`, nie zgodność z pełnym rozmiarem JSON. 23091 usuwa poprzedni plik zanim 23092 podmieni go na nowy. Niepełny zapis może zastąpić dobry plik; zanik zasilania/nieudane rename może pozostawić tylko plik tymczasowy. 23613 ustawia finalized i 23614 ignoruje wynik zapisu, po czym nowa doba nadpisuje stan RAM.
**Zalecenie:** weryfikacja całego zapisu, atomowe zastąpienie zgodne z możliwościami LittleFS i odzyskiwanie pliku tymczasowego; nie porzucać wyniku po błędzie finalizacji.

**Doprecyzowanie po researchu 24.10.2026:** littlefs zapewnia atomowość pojedynczego rename/remove; nie zapewnia atomowości pary remove→rename aplikacji. Arduino flush wywołuje fsync, ale nie zwraca błędu. Nie przypisywać tej wady brakowi atomowości samego LittleFS. R03. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

### 19. Ochrona lokalnych interfejsów jest niespójna — wysoki
`/api/lan/cmd` (28344) sprawdza token, ale `/api/pompa-manual`, `/api/servo-manual`, `/api/ustawienia-set`, `/api/history/clear`, `/api/fs-upload`, `/api/telegram/save` nie mają takiej kontroli w przejrzanych handlerach. Rejestracja serwera nie ustanawia w tym miejscu globalnego uwierzytelniania. Osoba mająca dostęp sieciowy do centrali może ominąć chronioną ścieżkę poleceń. To nie oznacza, że ESP jest dostępny z publicznego internetu.
**Zalecenie:** wspólna kontrola uprawnień wszystkich operacji zmieniających stan, kompatybilna z panelem LCD i aplikacją; nie zablokować operatorowi dostępu przez jednostronną zmianę.

### 20. JSON konfiguracji jest parsowany osobno dla każdego fragmentu HTTP — wysoki
29167–29191 ignoruje `index` i `total`: fragment żądania Telegram może nie zawierać `enabled`, a wtedy `newEnabled=false` jest zapisywane. Ręczny ekstraktor wymaga postaci `"klucz":"wartosc"`, bez spacji, i nie obsługuje escaped quotes. Analogiczny schemat w konfiguracji WiFi (29050 i okolice).
**Skutek:** poprawny JSON ze spacjami lub podzielony przez transport może wyłączyć Telegram albo niepoprawnie odczytać dane WiFi. Nie zależy to wyłącznie od rozmiaru JSON — transport może dzielić dowolnie.
**R07, druga runda:** [wycinek callbacka i parser chunked](narzedzia/audyt-v3.55.2/research2/R07_HTTP.md) odtwarzają wielokrotne wywołanie zapisu i końcowe wyłączenie przez len=0. Uprawnienia muszą poprzedzać skutki uboczne callbacka. Próby hostowe, nie na centrali.

### 21. Nazwy WiFi nie są kodowane do JSON — średni
28932 i 28935 wstawiają SSID wprost w cudzysłowy. Nazwa z `"`, `\` lub znakami sterującymi psuje odpowiedź `/api/wifi/list`. Użyć serializatora/escapera; nie zmieniać prawdziwej nazwy sieci.

### 22. Upload plików używa wspólnego stanu i nadpisuje plik docelowy od razu — wysoki
28781–28845 resetuje globalne `fsUpload*`, otwiera docelowy plik w trybie `w`, bez wyłącznego właściciela żądania i bez pliku tymczasowego. Dwa uploady mogą mieszać stan/uchwyt; przerwany upload historii zostawia uciętą historię. Chronione są tylko wybrane nazwy. Limit wolnego miejsca sprawdzany jest na początku, a nie jako rezerwa przez cały upload.
**Zalecenie:** stan per żądanie albo odrzucanie równoległego uploadu, plik tymczasowy, limit rozmiaru i rezerwa dla danych systemowych.
**R07, druga runda:** [test rzeczywistego callbacka z atrapami](narzedzia/audyt-v3.55.2/research2/R07_HTTP.md) odtworzył B=B1+A2 zamiast B1+B2, zapis do starego uchwytu po nieudanym przejęciu blokady otwarcia oraz zatrzymanie A błędem żądania B. Wystarcza przeplatanie zdarzeń jednego taska AsyncTCP; nie potrzeba równoczesnego wykonania callbacków.

### 23. Telegram przyjmuje komendy bez sprawdzenia nadawcy/czatu — KRYTYCZNY
Cała funkcja `pollTelegramCommands` (38878–39363) parsuje tekst i callbacki, ale nie porównuje `message.chat.id` / `callback_query.from.id` z dozwolonym operatorem. `tgChatId` jest używane do wysyłania, nie do autoryzacji odbioru. Znajomość nazwy bota może umożliwić obcej osobie wysłanie rozkazu (restart, OTA, testy, konfiguracja Firebase), mimo że odpowiedź trafi do skonfigurowanego operatora. Wymagana lista dozwolonych nadawców i kontrola obu rodzajów aktualizacji przed wykonaniem. Nie przeprowadzano próby na rzeczywistym bocie.

### 24. Niepełna odpowiedź Telegram może bezpowrotnie zgubić komendę — wysoki
38917–38920 czyta tylko aktualnie dostępne bajty, nie pełną odpowiedź HTTP (Content-Length/chunked). 38942 przesuwa `tgLastUpdateId` przed walidacją kompletnej wiadomości. Gdy pierwsza porcja zawiera update_id, ale tekst przyjdzie później, następne getUpdates może już potwierdzić pominięcie tej wiadomości. Pozostające bajty mogą też zaburzyć następne odczyty keep-alive. Wymagane kompletne ramkowanie HTTP i parsowanie JSON, z limitem rozmiaru/czasu.
**R08, druga runda:** [test wycinka parsera](narzedzia/audyt-v3.55.2/research2/R08_TELEGRAM_WWW.md) potwierdza także odczyt text cytatu reply_to_message zamiast własnego text oraz brak odróżnienia edited_message. Kolejność cytatu przed własnym tekstem potwierdzona w serializerze oficjalnego serwera. Odczytane polecenia rejestrowano w atrapie, nie wykonywano ich.

### 25. Ochrona transmisji OTA nie weryfikuje serwera — wysoki
38555 i 38644 wywołują `setInsecure()`. HTTPS jest szyfrowane, lecz bez sprawdzenia tożsamości serwera. W przejrzanej ścieżce nie ma niezależnej weryfikacji podpisu firmware. Atakujący kontrolujący połączenie może podszyć się pod serwer aktualizacji. Faktyczne Secure Boot / podpisy w konfiguracji urządzenia pozostają do potwierdzenia. Nie utożsamiać sumy kontrolnej z autentycznością pliku.

### 26. Przerwany upload OTA może trwale uśpić nadzór — wysoki
29132 ustawia globalne `otaUploadTrwa`; sprzątanie jest w końcowej obsłudze żądania. W bloku `/ota-upload` brak obsługi zerwanego połączenia i własnego końcowego terminu uploadu. Flaga ta usypia nadzór (46711–46724). Oprócz tego `panOtaGotowa` przywraca `otaOperacjaWTrakcie=true` (46586–46595) podczas oczekiwania na panel — brak odpowiedzi panelu może pozostawić nadzór wyłączony. Wymagany odrębny nadzór stanu OTA i ograniczone czasowo oczekiwanie, nie restart w trakcie poprawnego zapisu flash.

**Druga runda R15:** Ponowiony runner przywracania panelu nadal potwierdza uśpienie nadzoru przez oczekiwanie na panel, także po dobie. Nie wykonano resetu, OTA ani testu bootloadera. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### 27. Różne drogi OTA nie mają jednakowych blokad — wysoki
Telegram sprawdza alarmy i zajętość przed aktualizacją, lecz handler `/ota-upload` 29095–29156 nie sprawdza tych warunków. Stan Update/hasła/uploadu jest globalny, nie przypisany do żądania; równoległe żądania mogą się zakłócać. Obsługa końcowa może zaplanować restart także dla POST bez pliku, jeśli Update nie zgłasza błędu — nie ma lokalnego warunku „ta konkretna sesja pomyślnie zakończyła zapis”.

### 28. Krótka historia usuwa z bufora także niezapisane bajty — wysoki
45337 wykrywa niepełny zapis, lecz 45354–45363 usuwa z `historiaPendingCsv` pełną długość `naDysk`, nie tylko poprawnie utrwalone kompletne rekordy. 45296–45306 celowo kasuje bufor przy fragmentacji pamięci. W obu wypadkach nie można obiecywać bezstratnej historii. Przepisanie pliku podczas resync (45383–45388 i 45450–45455) używa `w` bez pliku tymczasowego i bez sprawdzenia wyniku pełnego zapisu.

### 29. Długa historia nie ponawia rekordu po chwilowym błędzie plików — średni/wysoki
45511–45515 usuwa rekord z kolejki przed wywołaniem funkcji void. `historiaDlugaFlushDoFS` może wrócić przy braku pamięci, zajętej blokadzie lub błędzie open (45408–45431), bez przywrócenia rekordu. Nie jest to trwała kolejka z potwierdzeniem zapisu.

### 30. Czyszczenie historii omija niezdobytą blokadę — wysoki
24313–24317: jeśli nie uda się przejąć istniejącego `historiaPendingMux`, kod i tak zmienia wspólny String. Flaga `historiaClearPending` nie gwarantuje wyłączności z operacją już rozpoczętą. Usunięcie pliku 24320 nie przejmuje logMutex. Możliwy konflikt z zapisem i odzyskanie skasowanych wierszy przez kończącą się operację. Wymagany protokół czyszczenia zsynchronizowany z zadaniem zapisującym.

### 31. Nieudana alokacja historii wyłącza też zbieranie telemetrii i archiwum solarnego — wysoki
24138 wraca natychmiast, gdy `historiaBufor` nie istnieje, zanim zostanie zbudowana próbka telemetrii (24146) i przekazana do agregatora solarnego (24181). 22708–22711 przy nieudanej alokacji opisuje tylko „wykresy będą puste”, ale skutek jest szerszy. Rozdzielić niezależne kanały zbierania danych.

### 32. Rozruch najpierw wykonuje długie operacje, dopiero potem inicjalizuje wyjścia — wysokie ryzyko
Start czyta pliki, archiwizuje crash, stosuje backoff, inicjalizuje WiFi i Firebase, uruchamia zadania sieciowe (45938–46328), a dopiero 46511–46525 ustala GPIO przekaźników i zasilania. Oczekiwanie WiFi w setup nadal jest pętlą blokującą (29931–29938), choć pojedynczy krok używany w loop jest nieblokujący. Podczas tego rozruchu normalna automatyka jeszcze nie działa. Zadania sieciowe mogą przyjąć polecenia przed zakończeniem inicjalizacji sprzętu, a setup może później nadpisać ich skutki.
Nie ustalam bez znajomości elektryki fizycznego stanu przekaźnika w czasie resetu. Wymagane wczesne bezpieczne ustawienie wyjść i bramka gotowości rozkazów. Sam watchdog nie dowodzi nieprzerwanej ochrony pieca podczas restartu.

**Druga runda R15:** R15 zestawia kolejność setup z ostrzeżeniem IDF o możliwych zmianach pinów przez bootloader/startup. Nie wolno wnioskować o stanie elektrycznym podczas resetu z późniejszego wywołania digitalWrite. Macierz odbioru wymaga pomiaru na odizolowanym stanowisku. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### 33. Sprawdzenie konfiguracji NVS jest niepełne — średni/wysoki
**Doprecyzowanie R04:** istnieje startowa kontrola pary Temp_ON/Temp_OFF i powrót do 60/50. Problemem pozostają pozostałe niepełne kontrole, ignorowane wyniki wielu zapisów i rozdzielenie RAM/NVS. Dodatkowo Arduino może kasować NVS przy dwóch kodach błędu inicjalizacji jeszcze przed setup — [pełny zakres i ograniczenia](narzedzia/audyt-v3.55.2/research2/R04_NVS.md).
46131–46228: część nastaw jest walidowana, ale m.in. zakresy `dymCzasOn/Off/Stabilizacji`, `dymTrybPracy`, `progAlarmowyDymu`, Czas_ON/OFF i ich zależności nie są sprawdzane tak jak przy ustawianiu przez API. Ujemny `wifiConfig.count` (22523–22524) nie jest ograniczany od dołu; 22558 użyłoby ujemnego indeksu podczas dodawania sieci. To ścieżki dla nieprawidłowych/starych danych, nie dowód obecnego uszkodzenia pamięci.

### 34. Parametr tempOff=0 jest akceptowany, lecz wyłącza obsługę trybu 2 — wysoki
22147 pozwala zapisać zero, a 32307 wymaga `Temp_OFF_snap != 0`, aby w ogóle wykonać automatykę temperaturową. Zapis przyjęty jako poprawny może pozostawić pompę w dotychczasowym stanie zamiast realizować nastawę.

### 35. Przeliczanie czasu lokalnego na UTC gubi rozróżnienie powtórzonej godziny — średni/wysoki
`pobierzCzas()` 22315 preferuje zegar z lokalnym czasem nawet przy dostępności NTP. `dataLokalnaNaUnixUtc` 22323–22332 odbudowuje UTC przez mktime z tm_isdst=-1. Dwa wystąpienia 02:30 podczas jesiennej zmiany czasu w Warszawie mają identyczne pola wejściowe; nie da się z nich odtworzyć dwóch różnych chwil. Może to powodować cofnięcie/zdublowanie znaczników telemetrii i błędną kompletność doby. Najbliższy taki dzień: 25.10.2026. Korzystać z czasu systemowego UTC, kiedy jest wiarygodny; dla RTC jawnie zaprojektować zachowanie przy braku NTP.

**Druga runda R11:** R11 pokazuje utratę rozróżnienia także w fallbacku getLocalTime→DateTime, bez RTC. Sam lokalny RTC nie cofa się automatycznie o 03:00: zapis pól odbywa się co 24 h przy Wi-Fi, więc moment skoku zależy od korekty. Flaga NTP opiera się na roku, nie nowym pakiecie. Dwie chwile 02:30 dały identyczne pola i wynik konwersji na hoście; nie przesądzamy wyboru DST w Newlib. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R11_CZAS.md).

### 36. Skasowanie komendy Firebase może usunąć nowszy rozkaz — wysoki
42979 pobiera pojedynczy węzeł `/piec/cmd`, 43105 wykonuje polecenie, 43111 odsyła ACK, a 43125 bezwarunkowo usuwa cały węzeł. Jeśli w międzyczasie aplikacja zapisze tam inne ID, nowe polecenie zostanie usunięte. Szczególnie ważne, bo ACK jest wysyłane przed remove, więc aplikacja może już wysłać kolejne polecenie. Potrzebne ID jako klucz kolejki albo warunkowe usuwanie właściwego rekordu.

**Druga runda R13:** R13 uruchamia oryginalną gałąź wykonania/ACK i wywołanie remove z atrapą serwera: B podmienione w trakcie ACK A zostaje usunięte. FirebaseClient 2.2.9 ma matchingEtag i dodaje if-match dla DELETE; ta ścieżka nie przekazuje znacznika. Warunkowe usunięcie nie rozwiązuje wcześniejszego nadpisania slotu ani restartu. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md).

### 37. Deduplikacja komend Firebase nie przeżywa restartu — ryzyko zależne od komendy
43090–43111 korzysta z `lastFirebaseCmdId` w RAM. Restart po wykonaniu polecenia, ale przed jego usunięciem, może wykonać je ponownie. Rozkazy ustawienia stałej wartości często są idempotentne, ale restart, operacje plików, rozpalanie i konfiguracja wymagają osobnego rozpatrzenia. Nie wolno obiecywać wykonania „dokładnie raz”.

**Druga runda R13:** R13: A/A po błędzie ACK lub DELETE nie wykonuje ponownie; A/B/A wykonuje trzy razy, reset RAM po wykonaniu A umożliwia powtórzenie. Ten sam ID z inną treścią odsyła poprzedni wynik. Fizyczny wykonawca był atrapą; wymagany niezmienny ID i polityka ponowień zależna od skutku. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md).

### 38. Komenda konfigurująca Telegram trafia jawnie do logu Firebase — wysoki
43098 zapisuje całe `cmd`, a `wykonajKomendePanelu` obsługuje `tg_config` zawierające dane konfiguracyjne. W drodze LAN jest maskowanie (43711), lecz tutaj go brak. Logi mogą być następnie widoczne w terminalu lub wysyłane. W raporcie nie kopiowano żadnych sekretów. Wymagane centralne maskowanie przed każdym miejscem logowania.

### 39. Nazwa WiFi może zostać potraktowana przez panel jak HTML — wysoki
Aktywny panel wbudowany: 12743–12756 wstawia niekodowane `net.ssid` do `innerHTML` zarówno w tekście, jak i atrybucie; 12870–12883 robi to dla zapisanych sieci. Podobnie starsza strona w 18188–18194 i 18236–18240. Nazwa sieci jest danymi z otoczenia, nie zaufanym kodem. Konieczne tworzenie węzłów tekstowych i ustawianie atrybutów przez DOM. Nie wykonywano żadnej próby wstrzyknięcia na urządzeniu.

### 40. Wysyłka logu krytycznego nie obsługuje braku pamięci bufora — wysoki
33042–33043 próbują dwóch alokacji, ale przed 33052 nie ma sprawdzenia `chBuf != nullptr`. Dodatkowo pętla `while (fKryt.available())` nie kończy się po `read()==0`. Zależnie od biblioteki plikowej może to oznaczać błąd dostępu do pamięci albo pętlę bez postępu, trzymającą `logMutex` i `tgSendMutex`. Inne przejrzane funkcje wysyłki dokumentów sprawdzają brak bufora. Potrzebny test wymuszonej nieudanej alokacji i odczytu zerowej długości.

### 41. Bufory oczekujących logów/historii nie mają widocznego limitu wielkości — ryzyko wysokie przy awarii zapisu
25749/25751 dopisują do Stringów logów. 25817–25819 po błędzie otwarcia pozostawiają je w pamięci. 24263 dopisuje historię; błędy otwarcia/logMutex odkładają zapis. Przy długiej awarii systemu plików bufor może rosnąć do wyczerpania pamięci, zanim próba zapisu uruchomi awaryjne kasowanie przy małym bloku. Potrzebny jawny limit i policzona polityka utraty/dyskowego przechowywania, nie obietnica nieograniczonego bufora.

### 42. Eksport całej historii nadal tworzy duży String i nie jest spójnym snapshotem — ryzyko
24363–24366 i 24687–24690 blokują tylko odczyt indeksów, a pętle później czytają rekordy z bufora bez blokady. Producent może w tym czasie nadpisać rekord. Długa historia rezerwuje do 1 840 000 bajtów i powstaje bez oddawania czasu w pętli. `/api/history/long` buduje ją w callbacku AsyncTCP. Naprawa startowego czytania pliku nie naprawia tego eksportu/resync. Potrzebne porcjowanie i bezpieczne kopiowanie rekordów; nie stwierdzono, że właśnie to spowodowało wcześniejszy WDT.

### 43. Wygaśnięcie ręcznego serwa zależy od kanału polecenia — średni
WWW ustawia `serwoManualOverrideUntil` (29347); polecenia `klapa`/`syberek` wykonane przez wspólny dispatcher Firebase/LAN ustawiają TrybSerwa=2, ale nie odnawiają ani nie zerują tego terminu (42559–42572). Jeśli został stary termin z WWW, nowe polecenie może wygasnąć według niego; jeśli terminu nie było, pozostaje bez limitu. Potrzebna jedna, jawna semantyka ręcznego sterowania dla wszystkich interfejsów.

### 44. Zbyt mały krok syberka daje krok zerowy — średni
22202 dopuszcza `skokSyberka=1` (%). `procentNaKat(1,0,90)` używa całkowitoliczbowego map i daje 0°. 31266 w AUTO nie wymusza minimalnego kroku 1°, inaczej niż 31780 dla alarmu dymu. Przy Mnoznik=1 syberek nie przesuwa się mimo kolejnych zleceń. Wymagany jawny minimalny krok albo precyzyjniejszy akumulator ruchu.

### 45. Ustawienie maksymalnego progu dymu uniemożliwia przekroczenie — wysokie ryzyko konfiguracji
22214 dopuszcza próg 4095, a 31785 uruchamia alarm tylko dla `odczyt > progAlarmowyDymu`. ADC ma zakres do 4095. Taka nastawa nigdy nie wywoła alarmu. Nie rozstrzygam tutaj właściwego progu dla konkretnego czujnika — wymaga to kalibracji, ale API nie powinno udawać aktywnej ochrony przy nieosiągalnym progu.

### 46. Biblioteka archiwum zlicza ponownie odwiedzoną minutę — średni
Plik zależności odnaleziony w `firmware-patch/v3.54.0-offline-buffer-toggle/SolarDailyArchive.h`, linie 440–446: maska minut jest ustawiana, lecz `distinctMinutes` rośnie według porównania tylko z poprzednią minutą, nie według poprzedniego stanu bitu. Sekwencja A,B,A nalicza 3 minuty, choć maska ma 2. Przy cofnięciu zegara kompletność może być zawyżona. Po odtworzeniu z pliku (508–515) licznik jest obliczany z maski, więc wynik może się dodatkowo zmienić po restarcie.
**Zastrzeżenie:** paczka v3.55.2 nie zawiera tych nagłówków; to przegląd dostępnej wersji bazowej, a zgodność z lokalnym projektem użytkownika trzeba potwierdzić.

**Druga runda R11:** R11: rzeczywisty agregat dla A/B/A daje licznik 3, maskę 2 i po restoreMask licznik 2. Dwa różne UTC odpowiadające dwóm lokalnym 02:30 dają poprawnie dwa bity. Nie należy deduplikować po samym lokalnym HH:MM. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R11_CZAS.md).

### 47. Daty do 2099 są akceptowane, ale archiwum używa signed 32-bit czasu — długoterminowy
`SolarDailyArchive.h`: walidacja 130 dopuszcza lata do 2099; `utcFromFields` 179–181 mnoży int32_t, `warsawDayBounds` 215–216 zawęża time_t do int32_t. Granica 2038 nie jest obsłużona. Nie jest to obecny problem działania z datą 2026; wymaga albo zawężenia kontraktu, albo konsekwentnego użycia szerszego czasu.

**Doprecyzowanie po researchu 24.10.2026:** ESP-IDF od v5.0 ma 64-bitowe time_t. Ten punkt dotyczy jawnego int32_t archiwum, nie ogólnego ograniczenia zegara ESP32. R11. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

**Druga runda R11:** R11: UBSan wykrywa signed overflow utcFromFields dla 2038-01-19 03:14:08. Doba przecinająca granicę zostaje odrzucona; data 2039 zawęża się na hostowym GCC do ujemnego int32 i jest odrzucana przez lokalne przeliczenie. Główny konwerter uint32 ma pozytywną kontrolę daty 2039. To odrębne mechanizmy, nie ogólny limit systemowego time_t. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R11_CZAS.md).

### 48. Brak wspólnej wyłączności testów uruchamianych na centrali — potwierdzone niespójne blokady, skutki na ESP nieodtworzone
39902 sprawdza tylko uruchomienie własnego testu securityH2, a 40706–40726 nie sprawdza wszystkich innych aktywnych testów. Jedynie stability zatrzymuje tgTask na wejściu (43765); flagi innych testów wpływają na wybrane operacje, nie wszystkie. Ponadto `fbLoopWorkerZajety()` zawsze zwraca false (39564). Nie jest to dowód konkretnego crashu; trzeba sporządzić macierz właścicieli wspólnego FirebaseClient, ringu i plików dla każdego testu. Wersja produkcyjna powinna oddzielać testy niszczące lub wymagające wyłączenia ochron od zwykłych poleceń operatora.

## Odtwarzalne testy tego przeglądu
`python3 narzedzia/audyt-v3.55.2/odtworz_bledy.py` — **9/9 sprawdzeń odtworzyło błędy lub ich jawnie opisane przesłanki**. Nie są to testy zaliczenia bezpieczeństwa. Funkcje filtra, ruchu serwa, wyjścia pompy i decyzji nadzoru są wycinane z rzeczywistego źródła, sprzęt/logowanie zastąpiono atrapami. Skrypt nie wysyła poleceń, nie wgrywa firmware i nie łączy się z siecią. Host: g++ C++17. Próba C++11 wykazała niezgodność inicjalizacji istniejącego agregatu z domyślnym polem; użyto C++17 bez zmieniania firmware.

Dwa rzeczywiste skrypty JavaScript wydobyte z raw stringów HTML przechodzą `node --check` (381414 i 133216 znaków). To tylko poprawność składni, nie zachowania panelu. Pierwsze wydobycie regexem z całego C++ błędnie złapało słowo `<script>` w komentarzu; ten wynik odrzucono, użyto parsera HTML osobno dla raw stringów.

### 49. `isResult()` z FirebaseClient nie oznacza wyłącznie końcowego ACK — wysokie ryzyko integracyjne
**Druga runda R05:** 27 asercji wycinków potwierdziło m.in. bramkę publikacji świeżego wyniku oraz możliwość zapisania starego wyniku do ponownie użytego proxy. To mocniejszy dowód mechanizmu, nadal nie pełna reprodukcja utraty próbki; patrz [R05 i granice testów](narzedzia/audyt-v3.55.2/research2/R05_FIREBASE.md). Nie pomijać istniejącego retry identycznych bajtów ani utożsamiać stop SSL z anulowaniem zadania.
39577 kończy oczekiwanie przy `isResult()`. 40020, 40497, 23384 i inne miejsca uznają `isResult() && !isError()` za sukces danych. W sprawdzonym źródle FirebaseClient v2.2.9 `AsyncResult.h:318` ta metoda obejmuje także debug, event i postęp przesyłania. Dlatego sama nazwa metody nie jest gwarancją utrwalenia danych w bazie.
**Nie klasyfikuję tego jako odtworzonej utraty próbki:** trzeba sprawdzić, jakie zdarzenia biblioteka faktycznie kopiuje do konkretnego obiektu wyniku dla używanego overloadu PATCH/GET/DELETE. `SlotManager.h:233–248` ogranicza kopiowanie, więc nie wolno automatycznie zakładać, że każdy debug powoduje przedwczesny ACK. Potrzebny test z prawdziwą przypiętą biblioteką, nie tylko atrapą AsyncResult, oraz końcowy warunek sukcesu osobno dla odczytu/zapisu/usunięcia.
Źródła:
- https://github.com/mobizt/FirebaseClient/blob/v2.2.9/src/core/AsyncResult/AsyncResult.h#L303-L318
- https://github.com/mobizt/FirebaseClient/blob/v2.2.9/src/core/AsyncClient/SlotManager.h#L201-L255
- https://github.com/mobizt/FirebaseClient/blob/v2.2.9/src/database/RealtimeDatabase.h#L840-L855

**Doprecyzowanie po researchu 24.10.2026:** Ponowne prześledzenie pięciu plików FirebaseClient v2.2.9 utrzymuje status ryzyka, nie odtworzonej utraty. Zatrzymanie transportu nie jest dowodem anulowania slotów biblioteki. Nie zamieniać isResult na available w ciemno. R05. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

### 50. Restart gubi część diagnostyki doby solarnej — średni
23200–23204 zapisuje `maxGapMinutes`, `suspectedDrawCount`, `suspectedDrawDropC`. 23309–23334 odtwarza liczniki, temperatury i maskę, ale nie te trzy pola. `Aggregate.begin/reset` zeruje je. Po restarcie kolejny zapis może usunąć z rekordu informację o przerwie lub podejrzeniu poboru i zmienić `technicalCandidate`. Ponadto przy zerowej liczbie prawidłowych próbek zapis (23170–23209) nie zachowuje liczników symulacji/błędów, mimo że mogły występować.

### 51. Kolejka Telegram nie przechowuje tekstu/daty razem z poleceniem — wysoki
39056 zapisuje payload `/fbprov` do wspólnego `tgQueuedText`, a 39315 datę usuwania do `g_solarDeleteDate`; kolejka zawiera tylko rodzaj polecenia i argument liczbowy. Konsument pobiera aktualną globalną wartość w 44108 i 44269. Dwa rozkazy usunięcia dni A i B odebrane przed wykonaniem kolejki mogą oba usunąć B. Nadpisanie może nastąpić również, gdy drugiego rozkazu nie uda się już zakolejkować. String jest dodatkowo współdzielony między zadaniami bez blokady. Wymagany niezależny payload każdej pozycji kolejki.

### 52. Kasowanie logów nie synchronizuje wszystkich współdzielonych buforów — wysokie ryzyko współbieżności
`logWyczysc` 27130–27133 zmienia indeksy pierścienia bez `logRingMux` i czyści `logPendingBufCore[0]` bez ochrony właściciela tgTask. Funkcja jest dostępna nie tylko z tgTask. Mutex dla Core1 (27138) nie chroni tych operacji. Potrzebna pojedyncza ścieżka czyszczenia z uzgodnieniem wszystkich producentów.

### 53. Obsługa WWW wykonuje blokujące operacje Telegram w callbackach — ryzyko dostępności
28280 wysyła potwierdzenie panelu, a 29211 raport przez `sendTelegramMessage` bez przekazania do kolejki. Takie wysyłanie obejmuje mutex, DNS/TLS i odczyt odpowiedzi. Zajmuje zadanie sieciowe obsługujące HTTP zamiast szybko zakończyć handler. W połączeniu z błędnymi limitami TLS może blokować inne żądania i lokalną drogę ratunkową. Potrzebna kolejka pracy poza callbackami.

### 54. Ślad diagnostyczny w głównej pętli nadal wykonuje kosztowne pomiary — ryzyko wydajności
29967–29970 nadal wykonuje pełne `diagCheckpoint`, a stage BEFORE (21078–21081) mierzy stertę i minimalny zapas stosu. Scheduler robi BEFORE dla każdego uruchamianego zadania (21429); serwoTick jest częste. Dodanie sześciu lekkich śladów w v3.55.2 nie usunęło wszystkich pomiarów z każdej iteracji. Nie dowiedziono, że jest to przyczyna WDT. Potrzebny pomiar czasu bez dodatkowej lawiny logów, potem ewentualne ograniczenie częstotliwości kosztownej diagnostyki.

### 55. Ręczne parsowanie liczb dopuszcza błędne wartości jako zero/NaN — średni/wysoki
29286 i 29323 używają toInt bez sprawdzania, czy całe wejście jest liczbą. Tekst może zostać przyjęty jako 0, co dla części parametrów jest legalne. 29547–29548 używa toFloat oraz porównań zakresu bez isfinite; gdy biblioteka konwersji zwróci NaN, oba porównania zakresowe są fałszywe. Wymagane ścisłe parsowanie i odrzucanie niefinitywnych wartości przed zmianą stanu. Zachowanie konkretnego `String::toFloat` dla „nan” trzeba potwierdzić w przypiętej wersji rdzenia.

**Druga runda R14:** Oryginalne metody String z hostowym libc odtwarzają prefiksy/zero/NaN oraz przepełnienie double→float. Rzeczywisty predykat zakresu przyjmuje NaN, odrzuca Inf; oryginalne przypisanie NaN do int wykrywa UBSan. Wynik atof hosta nie jest testem libc wgranego ESP. Ścisły parser musi sprawdzić końcówkę, błąd, typ docelowy i skończoność przed zmianą stanu. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R14_WALIDACJA_TESTY.md).

### 56. Przycinać można za późno, bo licznik wolnego miejsca nie obejmuje wszystkich plików — średni/wysoki
26019/26073/26130 szacują użycie LittleFS z czterech plików (log A/B, krytyczny, firmware panelu), pomijając historię, spool, archiwum solarne, log nastaw i inne. Kontrola shadow quota 26108 ostrzega, ale nie zmienia warunku przycinania. System może raportować dużo „wolnego” w tej ścieżce, mimo że dysk zajęły dane innych modułów. Ujednolicić gospodarkę miejscem, z zachowaniem priorytetu danych.

## Uzupełnienie testów i zastrzeżeń
- Test na dostępnej bazowej bibliotece solarnej odtworzył #46: trzy próbki A,B,A → `distinctMinutes=3`, liczba bitów maski=2.
- Kontrola pozytywna: `warsawDayBounds("2026-10-25")` poprawnie zwróciło **25 godzin**. Problem #35 dotyczy wejściowych znaczników czasu, nie samych granic doby.
- Test hostowy potwierdził, że lokalne 25.10.2026 02:30 odpowiada dwóm chwilom UTC oddalonym o 3600 s przy tm_isdst=1/0.
- Przeszukanie literalnych kluczy wywołań NVS nie znalazło nazw przekraczających 15 znaków; nie obejmuje to kluczy konstruowanych dynamicznie.
- Nowe miejsca problemu czasu #3: 30791, 30798, 30930 (mieszadło, ręczne serwo, krok serwa). Regex szukający wyłącznie `millis()` nie wykrywał wcześniej porównań przez lokalne zmienne.
- Ustalono również, że `zmianaCisnienia` jest inicjalizowane na zero i później tylko odczytywane (5345,30570). Zależna od niego gałąź zmiany interwału nie będzie aktywowana przez zmianę realnego ciśnienia; dodatkowo aktualizacja interwału w wilgotnosc() nie ogranicza samego wykonania pomiaru. Niski priorytet wobec ochrony pieca.

### 57. Prognoza „za ile godzin” jest liczona od początku tablicy, nie od teraz — średni
43501–43503 zwraca indeks pierwszej temperatury poniżej zera. 43513–43514 porównuje elementy 0 i 3. 43628–43640 kopiuje pierwsze 24 elementy, bez wykorzystania `hourly.time` i bieżącej godziny. Jeśli tablica usługi zaczyna się od północy (typowy kontrakt używanego zapytania bez start_hour), po południu dane historycznej części dnia są przedstawiane jako przyszły trend/mróz za N godzin. Wymagany wybór okna według znaczników czasu, także podczas DST. Nie pobierano w tym przeglądzie rzeczywistej odpowiedzi Open-Meteo.

**Druga runda R12:** R12 uruchomiło rzeczywiste helpery z parserem 7.4.3: mróz wybiera indeks 2 bez odniesienia do teraz, a trend 0/3 wskazuje wzrost mimo spadku w późniejszym oknie 15/18. Źródła zapytań Open-Meteo potwierdzają domyślny zakres godzin oparty na dobie, nie automatycznie od momentu odczytu. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R12_POGODA_SOLAR.md).

### 58. Cache prognozy to współdzielony String bez synchronizacji — ryzyko współbieżności
43645 przypisuje `prognozaJsonCache` w zadaniu tgTask, a 28411 odczytuje go w callbacku serwera WWW. Brak mutexu lub niezmiennego snapshotu w tych miejscach. Jednoczesne kopiowanie i realokacja Stringa nie jest zabezpieczone przez sam fakt działania na tym samym rdzeniu — zadania mogą się przełączać. Należy objąć audytem również pozostałe wspólne Stringi (SSID/konfiguracja Telegram i dane panelu), a nie tylko dodać volatile.

**Druga runda R14:** Dodatkowo, niezależnie od wyścigu zadań, oryginalny blok 43568–43645 przy odmowie pamięci dokumentu wyjścia zastępuje dobrą kopię przez {}. Prawdziwy ArduinoJson 7.4.3 ustawia overflowed, ale serializacja nadal daje 2 bajty; kod nie sprawdza pełności przed publikacją. Atrapa sterty i końcowy std::string; bez HTTP i bez testu współbieżności. Błąd parsowania wejścia jest natomiast sprawdzany. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R14_WALIDACJA_TESTY.md).

## Konfiguracja i wiarygodność wcześniejszych opisów
W archiwum `Cenatrala Pieca.zip` znaleziono przypiętą platformę pioarduino 55.03.38-1 i `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` w `sdkconfig.defaults`. Jednocześnie komentarze platformio opisują rezygnację z trybu przebudowy frameworka. Sam plik defaults **nie dowodzi**, że aktualnie zainstalowany bootloader i gotowe biblioteki korzystają z tej opcji. Wymagane sprawdzenie faktycznych artefaktów budowania i próba powrotu na urządzeniu kontrolnym przed uznaniem zdalnej aktualizacji za bezpieczną.

README paczki v3.55.2 zawiera zbyt stanowczy wniosek, że loopTask „nie był tym, który się zawiesił”. Przy niedopasowanym ELF i braku pełnej symbolizacji taki wniosek nie jest uzasadniony. Potwierdzony pozostaje alarm WDT dotyczący IDLE1; nie przypisujemy winy ani niewinności konkretnej funkcji na podstawie niedopasowanego stosu. README nie zmieniono w ramach tego audytu.

# Kontynuacja — pełne odczytanie dwóch nagłówków i dalsza analiza Telegram/SD

### 59. Pokrycie czasu w raporcie solarnym jest błędne w dniach zmiany czasu — średni, odtworzony
`SolarDailyArchive.h:884–898` dzieli liczbę pokrytych minut przez `opt.nowMinutes`. Main 24024 przekazuje godzinę ścienną `hour*60+minute`, nie rzeczywisty czas od początku doby. W południe 29.03.2026 upłynęło 660 minut, ale mianownik wynosi 720: komplet pomiarów daje **91,7% zamiast 100%**. W południe 25.10.2026 upłynęło 780 minut; wynik przekracza 102% i wskaźnik czasu znika. Test wywołuje rzeczywistą funkcję budującą całą wiadomość. Granice doby są poprawne — to osobny błąd prezentacji, niezależny od #35.

**Druga runda R11:** R11 ponowiło rzeczywisty buildReport: zwykła doba 100,0%, wiosna 91,7%, jesienią wskaźnik czasu znika. Osobno pięć testów poprawnych granic dni 23/24/25 h; nie zastępować ich stałą dobą 24 h. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R11_CZAS.md).

**Druga runda R12:** R12 potwierdziło poprawny profil archiwum dla 23/24/25 godzin i (start,end]. Osobny problem wyrównania czasu rozpoznano z lektury Androida: średnie poprzedniej godziny są wybierane jako przyszłe od nowHour i tracą znaczniki w modelu. Nie wykonywano Kotlin ani zmiany aplikacji. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R12_POGODA_SOLAR.md).

### 60. Kompletność GHI sprawdza liczbę punktów, nie poprawność godzin i wartości — średni, odtworzona luka walidacji
`SolarDailyArchive.h:569–596`: `addHour` nie weryfikuje unikalności/kolejności godzin ani nieujemnego, skończonego GHI. `finish` sprawdza tylko liczbę. Test: 24 kopie jednej godziny → komplet; 24 ujemne wartości → komplet i suma 0. Main 23558–23564 odrzuca punkty poza dobą i null, ale nie uzupełnia pozostałej walidacji. **Nie stwierdzono, że Open-Meteo rzeczywiście zwróciło takie dane**; to błąd przyjęcia nieprawidłowej odpowiedzi lub rekordu jako pełnej prognozy. Wymagana unikalna siatka godzin UTC i walidacja typów/liczb.

**Doprecyzowanie po researchu 24.10.2026:** Dokumentacja Open-Meteo potwierdza średnią poprzedniej godziny. Obecny warunek main (start,end] jest poprawny; nie odwracać go podczas naprawy walidacji. Domyślne ISO/offset API nie gwarantują obsługi DST całego zakresu; autonomiczny URL już używa GMT i unixtime. R12. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

**Druga runda R12:** R12: pełne solarArchiveFetchGhi z prawdziwym ArduinoJson 7.4.3 i syntetycznym HTTP akceptuje duplikaty, ujemne, tekst/obiekt jako zero, true jako 1, a 1e100 po float daje Inf. Jedna godzina daje return true/status ok; warunki posiadania prognozy blokują uzupełnienie mimo complete=false. Poprawne kontrole odrzucają null jako brak, błędny JSON i HTTP429. Nie ustalono, że dostawca przesłał użytkownikowi uszkodzoną odpowiedź. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R12_POGODA_SOLAR.md).

### 61. Podział wiadomości nie dotrzymuje własnego limitu porcji — niski przy obecnej konfiguracji, odtworzony
`SolarDailyArchive.h:1040–1064`: limit nie odejmuje prefiksu kontynuacji; newline dokładnie na granicy może dodać jeszcze bajt. Dla maxLen=64 pierwsza porcja może mieć 65 bajtów, dalsza 77. **Obecne wywołanie 24078–24080 używa 3800, nie 4096**. Zapas nadal pokrywa prefiks i newline, więc nie zgłaszam obecnej utraty raportów z powodu tej jednej różnicy. Test pozytywny potwierdza zachowanie zapasu dla długich linii ASCII. Funkcja operuje na bajtach i w awaryjnym cięciu długiej linii może też przeciąć znak UTF-8; typowe linie raportu są znacznie krótsze.

### 62. Wysyłka dokumentów pomija część bloku po krótkim zapisie TLS — średni/wysoki, odtworzony
33233–33242 oraz kopie 33425–33434, 33878–33887, 33994–34003, 34116–34125: jeśli `write()` najpierw zapisze część bufora, a następnie zwróci 0, warunek `zapisano==0` nie przerwie operacji. Kolejny odczyt pliku pomija niewysłaną końcówkę poprzedniego bloku. Test wycina **rzeczywistą lambdę** `wyslijPlik`: plik `abcdefgh`, zapis 2 bajty, potem 0 → wysłane `abefgh`. Nie jest to dowód przyjęcia wadliwego dokumentu przez Telegram; nie zgadza się też zadeklarowana długość HTTP i wysyłka może po prostu zawieść. Potrzebne przerwanie całej transmisji albo bezpieczne dokończenie tego samego bufora. Nadawca coredumpu ma już właściwsze sprawdzenie `zapisano != n` (33584–33590); nie wszystkie kopie zostały poprawione.

### 63. Wewnętrzna pętla odbioru Telegram omija limit całej odpowiedzi — ryzyko dostępności
32925, 33076, 33267, 33455, 33605, 33904, 34020, 34142: `while (tgClient.available())` nie sprawdza czasu. Limit 4/12 sekund sprawdza dopiero zewnętrzna pętla. Gdy kolejne bajty nie pozwalają opróżnić bufora, wewnętrzna pętla może pracować dłużej niż deklarowany limit, nadal trzymając blokadę wysyłki. Limit 700 bajtów ogranicza przechowywany String, **nie ilość czytanych danych ani czas**. Nie odtwarzano tej sytuacji na prawdziwym TLS; warunek ograniczenia wynika bezpośrednio z kodu. Wymagany budżet czasu/bajtów także w wewnętrznej pętli.

### 64. Treść zewnętrzna trafia bez kodowania do wiadomości HTML Telegrama — średni
Main 34236 (SSID w /status), 38286 (raport WiFi) oraz `SolarDailyArchive.h:938–943` (treść błędu GHI) wklejają tekst bez zamiany znaków specjalnych HTML. `sendTelegramMessage` koduje JSON, ale celowo ustawia `parse_mode=HTML` (32968–32975); kodowanie JSON nie zabezpiecza HTML. SSID lub błąd serwera zawierający np. `<html>` może zmienić format lub spowodować odrzucenie wiadomości. Należy kodować wyłącznie zmienne fragmenty, pozostawiając zamierzone znaczniki raportu. Nie wysyłano prób do Telegrama w tym audycie.

### 65. Raport Telegram nie odróżnia starego/błędnego/symulowanego pomiaru od bieżącego — wysokie ryzyko mylącej informacji
34200–34294: `/status` i raport temperatur wypisują surowe globale bez jakości, wieku i znacznika symulacji. 34230 może jednocześnie zapewniać „Wszystko w normie”, ponieważ sprawdza tylko flagi alarmów, nie sprawność czujników. Przykładowo pozostawiona przez filtr stara wartość wygląda jak aktualna; -1 może być pokazane jako zwykła temperatura. Jest to osobne od #10 ryzyko informacyjne: nawet poprawne oznaczenia jakości w archiwum/telemetrii nie zabezpieczają tekstowego raportu, z którego korzysta zdalny operator.

**Druga runda R10:** R10 potwierdza rozdział liczby od metadanych: stara temperatura może nadal mieć zwyczajną wartość liczbową, a DHT po 50/60/70/błędzie wraca do 60. Poprawny znacznik INVALID/STALE w telemetrii nie poprawia sam tekstowego raportu. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R10_CZUJNIKI.md).

### 66. Automatyczna wysyłka SD kasuje również niewysłaną część dużego archiwum — wysoki
33333–33336 ogranicza wysyłkę do ostatnich **45 MiB**, a po udanej odpowiedzi 34175–34183 usuwa **cały** plik. Limit lokalnego archiwum (26840–26842) to osiem pojemności LittleFS i może przekraczać 45 MiB. Przykład: przy pojemności LittleFS 8 MiB dozwolony limit wynosi 64 MiB; plik 48 MiB nie wymaga jeszcze przycinania, ale wysyłka obejmie tylko 45 MiB i usunie niewysłane 3 MiB. To nie jest pomiar aktualnego pliku użytkownika. Należy wysyłać wszystkie części z potwierdzeniem lub zachować niewysłany zakres zamiast kasować całość.

## Dalsze sprawdzenia pozytywne i ograniczenia
- Całe dostępne `SolarDailyArchive.h` (1107 linii) i `AlarmBuzzerProfiles.h` (179 linii) odczytano w tej kontynuacji. Są to **bazowe pliki v3.54.0**, nie dowód identyczności nagłówków na komputerze użytkownika.
- `sprawdz_naglowki.py`: wywołania rzeczywistego budowania raportu, walidacja kompletności GHI, podział wiadomości, kalendarz, wszystkie identyfikatory profili buzzera 0–255. Wzorce mają dodatnie czasy kroków i bezpieczne domyślne wskazania; błędne nazwy/null są odrzucane.
- Odtwarzacz buzzera 31869–31912 ma kontrolę indeksu i porównuje upływ przez odejmowanie, odporne na zawinięcie millis. Zmiana profilu przechodzi przez krótki chroniony stan żądania resetu, nie steruje pinami z komendy sieciowej.
- Czasowe wyciszenie 31437–31480 jest chronione mutexem i liczone przez odejmowanie; komenda ogranicza zwykły czas do 3600 s. To nie jest potwierdzenie bezpieczeństwa całego toru alarmowego.
- Rozszerzenie #18: także `logTrimKeepTailSD` 26939–26941 usuwa oryginał przed rename. Przy błędzie podmiany brak automatycznego przywrócenia oryginalnej nazwy. Skopiowane dane mogą nadal istnieć w pliku tymczasowym; nie wolno utożsamiać utraty nazwy z natychmiastowym zniknięciem wszystkich bajtów.
- `fsAccountingSnapshot` 34335–34340 nie dolicza archiwum solarnego/spoola do sumy **logicznej**, natomiast `usedBytes()` uwzględnia realnie zajęte miejsce. Nie zgłaszam z tego powodu błędnego `freeBytes` w tej funkcji; #56 dotyczy innej ścieżki przycinania.
- Nowy dziennik `narzedzia/audyt-v3.55.2/PRZEGLAD_RECZNY.md` zapisuje dokładne zakresy tej kontynuacji. Nie dopisuje fikcyjnego pokrycia do wcześniejszych uciętych odczytów i nie zastępuje brakującego przeglądu reszty programu.

### 67. Skaner bufora offline dopuszcza strukturalnie nieprawidłowe strony z poprawnymi CRC — średni/wysoki, odtworzony
`spoolBatchScan` 34941–35024 sprawdza sumy kontrolne, lecz nie pełną spójność formatu. Test wycina rzeczywisty skaner, struktury danych i funkcje CRC; zastępuje tylko odczyt pliku. Odtworzone przypadki z wynikiem `footerOk=true`:
- nagłówek i stopka deklarują pierwszy numer 7, a pierwszy rekord ma numer 1;
- pierwszy rekord ma zabroniony w czytniku replay numer 0;
- pomiędzy ostatnim pełnym rekordem a stopką znajduje się dodatkowy bajt (`tailBytes=1`);
- strona zawiera 769 rekordów przy deklarowanej pojemności 768.
To **nie jest wyłącznie luka w testach**: produkcyjne `spoolRuntimeValidateFinalBySeq` 40059–40070 i skan katalogu 40154 korzystają z tego samego skanera. Z drugiej strony test nie dowodzi, że prawidłowy zapisujący generuje takie strony ani że wystąpiły one na urządzeniu. Dowodzi braku kontroli spójności po odczycie. Przypadek seq=0 ma dodatkową niespójność: strona przechodzi kontrolę dopuszczenia, lecz `spoolReplayReadRecord` 35532 odrzuca jej rekord. Wymagane reguły spójności nagłówek–pierwszy rekord–stopka, brak ogona w zamkniętej stronie, limit pojemności oraz zgodne zasady numerów próbek.

### 68. Część testów spool potwierdza własne założenia, nie działanie produkcyjnego mechanizmu — ważne ograniczenie dowodów
- 35643: `ids[0] == ids[0]` i `ids[1] == ids[1]` zawsze potwierdzają identyczność; nie wykonują drugiej próby wysłania ani ponownego zbudowania identyfikatora.
- R303 (35595–35606) podstawia `cursorAdvanced=false`; nie wywołuje produkcyjnego mechanizmu potwierdzeń.
- R304/R332 wyprowadzają „pełne ACK” z samej liczby rekordów pliku, nie z wyniku sieciowego. Są to lokalne modele i nie potwierdzają poprawności integracji #49.
- 36550–36552: `const bool after = g_telemetryRing.size()` zawęża liczebność do 0/1. Po poprawnym zdjęciu jednego rekordu z kolejki trzech lub więcej elementów test może zgłosić błąd, bo porównuje 1 z rzeczywistą oczekiwaną liczebnością ≥2.
- R334 zwraca true i stałe zapewnienia bez nowych sprawdzeń (36558–36560).
Nie należy używać sumy zielonych wyników tych lokalnych prób jako dowodu poprawnego ACK, restartu ani braku utraty danych. Wymagane osobne testy rzeczywistej ścieżki z wstrzyknięciem błędów, bez uruchamiania ich na pracującym piecu.

## Rozszerzenie przeglądu testów i ich izolacji
Przeczytano dalszy ciąg main 34841–37070. Testy R253–R334, format i kontrola CRC, publikacja .part, rzeczywisty adapter z ringu oraz R245/R246 to różne warstwy; nie traktujemy ich zamiennie.

Uzupełnienie #48: R325–R334 korzysta z **rzeczywistego ringu**, a R333 wykonuje `commitPopN`. `spoolGEnsureCommitGInput` może także dopisać kopię ostatniego snapshotu do ringu z zadania komend, czyli poza normalnym producentem. Sekcja sprzątania R263–R272 (36154–36159) usuwa ścieżki wskazane przez globalne `g_spoolGPrimaryPageSeq` i `g_spoolGSecondaryPageSeq`, nie tylko pliki własnego zestawu. Wymaga to dalszego sprawdzenia kolejności grup testów i współpracy z normalną telemetrią; nie uruchamiano tych poleceń na centrali.

Nowy test `python3 narzedzia/audyt-v3.55.2/sprawdz_spool.py` zakończył się kodem 0: cztery nieprawidłowe struktury zostały przyjęte, a poprawna strona i zwykłe uszkodzenie rekordu bez przeliczenia CRC dały oczekiwane wyniki kontrolne. Oznacza to odtworzenie luki, nie jej naprawę. Funkcje firmware pozostały nietknięte.

# Praca w ponumerowanych odcinkach — P001
Użytkownik zaakceptował podział na odpowiedzi i prosi o wyraźny sygnał, kiedy ma zlecić dalszy ciąg. Kolejka: `narzedzia/audyt-v3.55.2/PLAN_PRACY.md`. Ten podział nie oznacza zakończenia audytu ani zgody na zmiany firmware.

### 69. Test zaniku zasilania kontynuuje zapis mimo niezdobycia blokady — średni, ścieżka testowa
Main 37176 ignoruje wynik ponownego `xSemaphoreTake(logMutex, 200 ms)`. Następnie 37178 zapisuje do wcześniej otwartego pliku, a 37180/37187 oddaje blokadę, nawet jeżeli zadanie jej nie otrzymało. Została ona wcześniej zwolniona na czas wiadomości Telegram i 15-sekundowego okna testu. Przy zajętej blokadzie nie ma wyłączności dostępu; oddawanie mutexu przez zadanie niebędące właścicielem narusza kontrakt FreeRTOS i zależnie od konfiguracji może uruchomić asercję. **Nie odtworzono restartu na sprzęcie.** Dotyczy wywołania testowego R247, nie każdej normalnej próbki. Należy przerwać/zaplanować ponowienie po nieudanym zdobyciu blokady i oddawać tylko własną blokadę. W audycie nie uruchamiano `/spool247` na urządzeniu.

## P001 — pozostałe wyniki i ograniczenia
- R247 zapisujący sprawdza długości zapisów i rozmiar uzbrojonego pliku. Gdy okno upłynie bez odcięcia zasilania, zwraca false i wyraźnie opisuje brak power loss. To prawidłowe rozróżnienie.
- R247 odzyskujący sprawdza nagłówek, CRC pełnych rekordów, maksymalny rozmiar i długość niepełnego ogona. Dopuszcza także 3 pełne rekordy bez ogona. Nie ma w tej funkcji sprawdzenia, czy od uzbrojenia nastąpił restart. Wynik „recovery OK” jest dowodem odczytu prefiksu, **nie dowodem fizycznego zaniku zasilania** — uzupełnienie #68.
- R248: przed publikacją jest ponowny odczyt `.part`, weryfikacja dokładnego rozmiaru, nagłówka, CRC i numerów wszystkich trzech rekordów. Poprawny kierunek ochrony przed publikowaniem niepełnej kopii.
- R248 nadal używa usunięcia starego pliku przed rename (37510–37520); ograniczenie odporności na przerwę jest analogiczne do #18, lecz tu dotyczy pliku testowego. Przy błędzie rename kod celowo zachowuje `.part` i nie raportuje sukcesu.
- Odtworzony trzeci rekord R248 jest nowym rekordem testowym, a nie odzyskaniem brakujących 44 bajtów pierwotnego pomiaru. Takie zachowanie nie może być przeniesione jako „odzyskiwanie pomiarów” do normalnej telemetrii.
- Początek R249 odczytano do 37570; dalsza część funkcji jest zadaniem P002. Nie oznaczamy jej całej jako zakończonej.

# Praca w ponumerowanych odcinkach — P002 i P003

## P002: 37571–38070 — R249, R250, początek R251
Odczytano wszystkie 500 linii w trzech nieuciętych wynikach narzędzia. Nie dodano odrębnego numerowanego błędu: poniższe ograniczenia uzupełniają już istniejące ustalenia.

- **R249 — kontrola pozytywna z analizy kodu:** weryfikator pełnej strony sprawdza rozmiar, znaczniki nagłówka, CRC każdego rekordu, oczekiwaną sekwencję wszystkich 768 rekordów, sumę CRC strony, stopkę i dokładną pozycję końca. Jest bardziej restrykcyjny niż ogólny skaner z #67. Sukces tego testu nie dowodzi poprawności ogólnego skanera używanego w produkcji.
- **R249 — uzupełnienie #18:** publikacja 37810–37819 usuwa poprzedni plik przed rename. Przy nieudanym rename zachowuje `.part` i zwraca błąd; nie przywraca poprzedniej nazwy. To plik testowy, nie dowód utraty aktualnych danych użytkownika.
- **R249 — czas i blokada:** tworzenie 768 rekordów i wielokrotne odczytanie całej strony odbywa się przy zajętym logMutex. W pętlach zapisu/weryfikacji nie ma jawnego oddania czasu innym zadaniom ani obsługi WDT. Czas blokowania zależy od LittleFS i sprzętu; nie zmierzono go i nie przypisujemy temu dotychczasowego restartu. Do Z2/Z3: zmierzyć czas na urządzeniu kontrolnym, nie na piecu.
- **R250 — kontrola pozytywna z analizy kodu:** uszkodzenie jest wykonywane po wyliczeniu CRC rekordu #2. Odbiorca sprawdza nagłówek, CRC i sekwencję, zatrzymuje się na pierwszym błędnym rekordzie oraz wymaga prefiksu długości 1 i błędu na rekordzie 2. Nie zalicza całej strony jako poprawnej. Nie uruchamiano tej komendy na centrali.
- **R251 — uzupełnienie #68:** funkcja pobiera kilka osobnych snapshotów miejsca i wywołuje admission ponownie; nie zapewnia jednego niezmiennego obrazu stanu plików. Zmiana miejsca między odczytami może dać wynik FAIL mimo prawidłowego admission. Przy `q.ok=false` i quota=0 żądanie 0 bajtów również jest odrzucane przez `fsQuotaTelemetryAdmission`, więc samo FAIL nie dowodzi błędnego porównania granicy. Dalszy ciąg funkcji przeczytano w P003 (38071–38072).

## P003: 38071–38570 — diagnostyka, klawiatury, początek OTA
Odczytano wszystkie 500 linii w trzech nieuciętych wynikach narzędzia.

- **Test klawiatur:** nowy `sprawdz_menu.py` wycina 9 rzeczywistych funkcji budujących menu. Zastępuje tylko Arduino String, stan urządzenia i nadawcę; niczego nie wysyła. Sprawdzono 108 klawiatur (pompa ON/OFF, Telegram ON/OFF, trzy tryby serwa): wszystkie są poprawnym JSON, zawierają niepuste przyciski i callback_data długości 1–64 bajtów. Są warianty ON/OFF pompy oraz TAK/NIE dla restartu i kasowania. Test nie dowodzi, że odbiorca prawidłowo autoryzuje/wykonuje komendy — #23 nadal obowiązuje.
- **Uzupełnienie #64:** SSID w 38286 nadal jest bezpośrednio wstawiane do tekstu HTML. Poprawny JSON klawiatur nie usuwa problemu kodowania treści wiadomości.
- **Diagnostyka telemetrii:** 38200–38264 czyta ostatnią globalną próbkę przez referencję, nie kopię chronioną przed równoległą publikacją. Jakość/wiek są stanem zapisanym w tej próbce; funkcja nie przelicza wieku na chwilę przygotowania raportu. Nie dowiedziono konkretnej niespójnej wiadomości; dodać do macierzy współdzielonych danych w Z2. Nie jest to ponowne zgłoszenie braku jakości w /status z #65 — ten osobny raport jakość pokazuje.
- **Opis restartu (38493):** obietnica niedostępności „ok. 10–15 s” i automatycznego powrotu działania nie wynika z tej funkcji. To tekst potwierdzenia, nie test startu. Trzeba zestawić go z rzeczywistą inicjalizacją i #32, zamiast traktować jako gwarancję zdalnego odzyskania dostępu.
- **OTA:** 38555 ponownie potwierdza `setInsecure()` (#25). Funkcja sprawdzania wydania urywa się na granicy odcinka po `http.GET()`. Parsowanie odpowiedzi i wybór wersji należą do P004; nie oznaczamy całej funkcji jako zakończonej.

**Wynik tej odpowiedzi:** dwa dalsze odcinki odczytane; jeden nowy test offline zakończony kodem 0; liczba numerowanych ustaleń pozostaje 69. Bez zmian firmware, kompilacji całej centrali, połączenia z urządzeniem ani OTA.

# Praca w ponumerowanych odcinkach — P004 i P005

### 70. Pobranie obrazu panelu nie weryfikuje kompletnego firmware — wysoki dla aktualizacji panelu
38772–38823: funkcja ogranicza rozmiar do 2 MiB, sprawdza pierwszy bajt 0xE9, długość zapisu i ewentualny Content-Length. To nie jest weryfikacja całego obrazu. Przy nieznanym rozmiarze zarówno zerwanie połączenia, jak i 15 sekund ciszy kończy pętlę bez osobnego stanu błędu. Dodatni prefiks rozpoczynający się od 0xE9 przechodzi późniejsze bramki.

**Doprecyzowanie po pierwszej rundzie researchu 24.10.2026:** Arduino Update ma również etap końcowy i aktywację partycji. W pierwszej rundzie nie audytowano dokładnej ścieżki odbiornika LCD; błąd publikacji pliku nie dowodzi jego skutecznej instalacji. R09. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

Test `sprawdz_ota.py` wycina rzeczywisty fragment walidacji: **1 bajt 0xE9** przy oczekiwanej długości -1 lub 1 przechodzi; **512-bajtowy prefiks** przy nieznanej długości też przechodzi. Znana długość 1024 i zapis 512 są prawidłowo odrzucane. Test dotyczy dopuszczenia pliku do podmiany na centrali, **nie dowodzi zainstalowania wadliwego obrazu przez panel** — późniejszy mechanizm Update na panelu może go odrzucić. Po podmianie stara kopia pliku na centrali może już nie istnieć (#18, również 38833–38834).

Wymagana kontrola zakończenia transferu i integralności/typu obrazu przed publikacją. To osobne od braku weryfikacji certyfikatu (#25). Limit 15 s dotyczy ciszy między bajtami, nie całkowitego czasu transferu; powolny dopływ może długo utrzymywać operację OTA.

**R09, druga runda:** [kod LCD z ZIP i pełna funkcja pobierania](narzedzia/audyt-v3.55.2/research2/R09_AKTUALIZACJE_LCD.md) potwierdzają rozdzielenie publikacji od instalacji. Rozmiar slotu panelu jest o 64 KiB mniejszy od limitu stagingu. Nie ustalono wersji faktycznie wgranego LCD ani jego konfiguracji rollbacku.

### 71. Sprawdzanie wydania OTA zależy od formatowania JSON, a porównanie wersji nie jest pełnym SemVer — średni, odtworzony
38591–38597 wyszukuje dokładny ciąg `"tag_name":"`. Poprawny JSON `"tag_name": "v3.55.2"` nie przechodzi; zwarty odpowiednik przechodzi. W pierwszej rundzie nie sprawdzano rzeczywistej odpowiedzi GitHuba. W R09 wykonano odczyt metadanych przez gh: odpowiedź była zwarta i rozpoznawalna przez ten wzorzec; nie jest to dowód identycznego formatowania odpowiedzi dla ESP ani naprawa parsera.

38609–38629 porównuje tylko trzy segmenty przez toInt. Test na rzeczywistej funkcji wykazał równość `3.0.0.2` i `3.0.0.1`, a także `3.55.2-rc1` i `3.55.2`. Zwykłe trzysegmentowe wersje, np. 10.0.0 > 9.9.9, są porównywane prawidłowo. W 44441–44444 równość powoduje odmowę aktualizacji jako „już masz najnowszą”. Brak obsługi czwartego segmentu dotyczy historycznej konwencji projektu; reguły wersji trzeba jednoznacznie ustalić, a nie deklarować pełny SemVer dla tego parsera. Test korzysta z atrapy String; małe liczby nie wymagają modelowania różnic szerokości long host/ESP32.

### 72. Przycisk „Notyfikacje” wyłącza również odbieranie komend Telegrama — wysokie ryzyko operacyjne przy dostępie zdalnym
38432 opisuje przycisk jako przełącznik notyfikacji. 44384–44402 przełącza wspólne `tgEnabled` i zapisuje stan w NVS. 38879 przestaje wtedy wykonywać polling. **Po wyłączeniu nie można ponownie włączyć Telegrama poleceniem /start ani tym samym przyciskiem przez Telegram.** Kod świadomie informuje o konieczności użycia panelu WWW, ale dopiero komunikatem podczas wyłączania; nie wymaga uprzedniego potwierdzenia.

To nie jest przypadkowy błąd implementacji — komentarz wprost opisuje ten zamiar. Jest to konflikt nazwy przycisku i sposobu obsługi z wymaganiem zachowania zdalnego dostępu użytkownika. Nie oznacza wyłączenia GitHuba/Firebase/WiFi, lecz utratę jednego ważnego kanału sterowania. Zalecenie do późniejszej decyzji: osobny przełącznik automatycznych powiadomień i obsługi poleceń, albo jednoznaczne ostrzeżenie przed wyłączeniem całego bota. W audycie nie naciskano tego przycisku.

## P004 — pozostałe wyniki (38571–39070)
- Cały odcinek odczytano bez ucięć. Zakończono analizę funkcji rozpoczętej w P003, porównania wersji, aktualizacji centrali i pobierania obrazu panelu. Odbiornik komend jest kontynuowany w P005.
- Dobre kontrole w pobieraniu panelu: pre-check miejsca, limit rozmiaru podczas strumieniowania, odrzucenie krótkiego zapisu na LittleFS, usuwanie `.tmp` po błędzie, podmiana pod logMutex. Nie znoszą ograniczenia #70.
- Ponownie prześledzono #23/#24/#51: brak autoryzacji nadawcy w odczytanym odbiorniku; podniesienie update_id przed pełnym przetworzeniem; wspólny tekst provisioningowy poza pozycją kolejki. Nie dopisujemy tych samych błędów jako nowych.
- Uzupełnienie #63: 38917–38920 także odbiera dane w wewnętrznej pętli bez limitu liczby bajtów/czasu, tutaj dodatkowo bez ograniczenia rozmiaru Stringa.
- Rozszerzono test `sprawdz_menu.py`: wszystkie callback_data z 9 sprawdzonych menu mają rozpoznawaną trasę w odbiorniku. To nie jest test uprawnień ani wykonania operacji.

## P005 — pozostałe wyniki (39071–39570)
- Wszystkie 500 linii odczytano bez ucięć: pozostałe polecenia tekstowe, konfiguracja Firebase, budowanie paczki JSON i początek funkcji zamykającej gniazdo Firebase.
- **Testy na działającym urządzeniu (#48):** napisy „tylko na lokalnej płytce” są ostrzeżeniami, nie blokadą sprzętową/kompilacyjną. `/rtdbmega` kolejkuje test bez dodatkowego TAK; `/spoolrt TAK/STOP` zmienia tryb bezpośrednio w zadaniu odbioru. Rozkazy workerów mogą startować niezależnie od kolejki. Macierz wyłączności nadal wymaga Z2.
- **Kontrola pozytywna testcrash:** świeże uzbrojenie i trwałe oznaczenie update_id następują przed wywołaniem crashu, a flaga jest zerowana przed wykonaniem. Nie uruchamiano tego testu na urządzeniu; znaczenie helperów weryfikujemy osobno.
- **Trwałość ustawień:** 39286–39287 zmienia tryb solarny i wywołuje putBool bez sprawdzenia wyniku, a odpowiedź zapewnia o przetrwaniu restartu/OTA. Dodać do Z2 razem z innymi niezweryfikowanymi zapisami NVS — komunikat nie jest dowodem skutecznego zapisu.
- **Fallback Firebase:** 39395–39401 po wyborze domyślnych danych ustawia apiOk/emailOk/passOk na true bez ponownej walidacji ani kontroli zapisu NVS. Nie świadczy to o błędnym aktualnym haśle użytkownika. Nie ujawniano danych uwierzytelniających w raporcie.
- **Telemetria:** 39465–39556 używa ograniczonych buforów i sprawdza wynik vsnprintf; zamknięcie JSON po cofnięciu ostatniego za dużego rekordu nadpisuje nieprzyjętą końcówkę. Pola daty klucza są UTC, a rekord bez czasu trafia do `unknown/`. Nie znaleziono w tym odcinku nowego błędu formatu. To nie weryfikuje potwierdzeń zapisu Firebase (#49).
- `fbLoopWorkerZajety()` nadal zawsze zwraca false (39564–39566), mimo historycznego komentarza wyżej opisującego robotnika. Znaczenie tego sprawdzenia w testach należy oceniać po kodzie, nie po starym komentarzu (#48).

**Test offline tej rundy:** `python3 narzedzia/audyt-v3.55.2/sprawdz_ota.py` — kod 0, odtworzenie #70/#71 i poprawne przypadki kontrolne. Atrapy String/mutex/FS, bez pobierania HTTP, flashowania i kontaktu z centralą. `sprawdz_menu.py` ponownie zakończony kodem 0. Firmware bez zmian.

# Duża paczka P006–P009 — 39571–41570 (2000 linii)

### 73. Awaryjne wyszukiwanie plików bufora offline odrzuca własne poprawne nazwy — średni/wysoki, odtworzony
`spoolRuntimeParsePageSeq`, 40033–40049, ma dwie niezależne pomyłki: po znalezieniu `page_` bez ukośnika przesuwa wskaźnik o 6 zamiast 5 znaków; wymaga co najmniej 15 pozostałych znaków, choć 10 cyfr i `.bin` mają razem 14. Dodatkowo nie wymaga końca nazwy po rozszerzeniu.

Test `sprawdz_odzyskiwanie.py` wycina **rzeczywisty generator nazwy i parser**. Dla numeru 3758096385: pełna nazwa `/telemetry_spool/page_3758096385.bin` i sama nazwa pliku są odrzucone. Pełna ścieżka z końcówką `.binX` jest przyjęta. Null jest poprawnie odrzucane.

Skutek: skan katalogu w `spoolRuntimeFindOldestFinalPage` nie odzyska kanonicznych stron przez ten parser. **Nie oznacza to, że każdy replay jest niesprawny**: wcześniejsze ścieżki korzystają z podpowiedzi `oldest` w NVS lub przeszukują numery względem `lastseq`, konstruując nazwę bez tego parsera. Błąd ujawnia się, gdy potrzebny jest awaryjny skan katalogu. Potwierdzono wadę funkcji, nie utratę konkretnych danych użytkownika.

### 74. Test odmowy uprawnień Firebase może zaliczyć dowolny błąd jako poprawne Rules — średni, błąd wiarygodności testów
39814–39826 (R354) oraz 39839–39859 (R355) uznają samo `isError()` za `DENIED`. Nie sprawdzają rodzaju błędu, kodu odpowiedzi ani tego, czy faktycznie odmówiono dostępu do badanego zasobu. Błąd transportu, serwera lub uwierzytelnienia może więc udawać pozytywny wynik testu reguł.

Test offline wykonuje **rzeczywistą gałąź R354** z atrapą wyniku: każde `isError=true` pozostawia sukces i komunikat `foreignDeviceRead=DENIED`; udany odczyt i brak wyniku są poprawnie odrzucane. R355 ma analogiczny warunek — potwierdzenie przez odczyt kodu, nie osobną symulację połączenia. Nie dowodzi to, że wdrożone Rules są niepoprawne ani że cały H2 zawsze przejdzie przy awarii sieci (wcześniejsze kroki mogą zawieść). Po niespodziewanym dozwolonym zapisie R355 próbuje usunąć probe, ale nie sprawdza wyniku sprzątania. Osobne od tautologicznych testów spool w #68.

### 75. Pomocniczy test zakresu RTDB nie sprawdza kompletności HTTP i nie dekoduje chunked — średni, ścieżka diagnostyczna
41198–41370: `rtdbRangeReadAll` po upływie limitu zwraca sukces, jeśli odebrano choć jeden bajt. `rtdbExtractHttpBody` sprawdza separator nagłówków i status 200, lecz nie porównuje Content-Length, nie dekoduje Transfer-Encoding i nie waliduje JSON. Worker uznaje potem za sukces każdą taką treść różną od dokładnego `null`.

Test na rzeczywistej funkcji ekstrakcji: odpowiedź HTTP 200 deklarująca **100 bajtów**, z treścią tylko `{`, przechodzi. Odpowiedź chunked zawierająca `{}` zwracana jest ze znacznikami chunków, nie jako sam JSON. Kompletna prosta odpowiedź przechodzi prawidłowo, a HTTP 403 jest odrzucone.

R370/R371 sprawdzają następnie fragmenty tekstu i liczbę wystąpień `"ts"`, a nie kompletny dokument. Mogą odrzucić część uszkodzonych odpowiedzi, ale nie gwarantują wykrycia każdej, np. ucięcia po wymaganych polach. **Dotyczy testowego odczytu zakresu**, nie dowiedziono tego problemu w zwykłym kliencie Firebase. Nie badano bieżącego sposobu kodowania odpowiedzi serwera.

## P006 — pozostałe wyniki (39571–40070)
- Oczekiwanie Firebase ogranicza czas między wywołaniami `fbApp.loop`, nie czas pojedynczego blokującego wywołania. Kolejne ustawienia `setHandshakeTimeout(4000)` powielają wcześniej zgłoszone ryzyko jednostek API; nie przypisujemy im nowego numeru.
- H2 pracuje w osobnym zadaniu Core0; historyczny opis wyłączności przez fbLoopWorker nie odpowiada funkcji zawsze zwracającej false. Wymaga macierzy właścicieli klienta w Z2 (#48).
- Telemetria przed usunięciem paczki z ringu sprawdza `sampleSeq` ostatniego rekordu; to istotna kontrola tożsamości. Nie obejmuje `sessionNonce`, a bezpieczeństwo zależy również od modelu jednego producenta/odbiorcy. #49 nadal jest ryzykiem integracyjnym, nie dowodem fałszywego ACK.

## P007 — pozostałe wyniki (40071–40570)
- Alokator numeru strony sprawdza wynik zapisu NVS i przepełnienie. Arm/Stop zapisują trwały przełącznik, a błąd trwałego zapisu komunikują; stan w RAM może mimo niego zostać zmieniony. Stary komentarz „po restarcie OFF” nie opisuje aktualnego kodu.
- Zrzut na dysk blokuje się przy aktywnej/oczekującej paczce telemetrii, sprawdza quota, zapis i rename; usuwa rekordy z RAM dopiero po ponownym otwarciu i weryfikacji strony. To poprawna kolejność, niezależnie od luk skanera #67.
- Ponownie prześledzono zgłoszoną już utratę `logMutex`: wyjścia w gałęzi pełnego kursora oraz po nieudanej alokacji JSON w replay pozostają bez zwolnienia blokady. Nie dublujemy ustalenia.
- Wyszukiwanie po NVS obejmuje 4097 numerów; dla każdej znalezionej strony zapisuje cofniętą podpowiedź w NVS. Nie ma w tej pętli jawnego yield/obsługi WDT. „Najstarsza” oznacza najstarszą w tym oknie, niekoniecznie całego katalogu. Do Z2/Z3: czas pod blokadą, liczba zapisów NVS, starsze zaległości poza oknem; bez przypisywania niezmierzonego restartu.
- Warunek wieku zaległości odejmuje `unixTs` dwóch próbek. Korekta zegara pomiędzy nimi może go przyspieszyć lub wyłączyć; komentarz o odporności na korektę nie jest uzasadniony. Nadal działają osobne wyzwalacze ilościowy i braku łączności. Nie jest to dowód utraty próbek.

## P008 — pozostałe wyniki (40571–41070)
- Final/Stability/Recovery Mega zbierają wyniki licznych podtestów. Powtarzanie testów z #68/#74 nie usuwa ich luk dowodowych. R397 i podobne kontrole `!fbLoopWorkerZajety()` są stale prawdziwe przy obecnej implementacji.
- Uruchamianie mega-testów ma niesymetryczne warunki: Final nie sprawdza aktywnego Stability/Recovery, choć odwrotne kierunki są częściowo blokowane. Kontrole stanu nie stanowią jednej atomowej blokady. Dodać do istniejącego #48 i Z2; nie uruchamiano równoległych testów.
- Przyspieszenie historii zmienia pola schedulera z workera i czyta wspólny snapshot bez blokady. Wymaga analizy publikacji w Z2, nie samego sprawdzenia niezerowego numeru.
- Test tymczasowego pliku sprawdza długość zapisu, identyczność odczytanych bajtów i wynik usunięcia. Test obecności starego coredumpu nie dowodzi braku nowych awarii. R443 sprawdza override, nie rzeczywisty rollback bootloadera.

## P009 — pozostałe wyniki (41071–41570)
- Nazwa helpera `gateNoMutate` nie gwarantuje braku zmian: Recovery uruchamia podtesty producenta/Commit G i żywe testy RTDB. R447/R448 dostają stałe true; nie są instrumentowanym dowodem niewykonania flash/restartu (#48/#68).
- Osobny worker zakresu RTDB ma własnego klienta z CA i jawną nazwą TLS, sprawdza pełny zapis żądania i ogranicza odebraną treść do 8192 bajtów. To dobre kontrole; nie naprawiają #75 ani błędnej jednostki timeout handshake.
- Upływ 20 sekund oczekiwania nie anuluje workera; jego globalny kontekst nie znika, ale zadanie może nadal działać. Do Z2: publikacja `done/running`, odczyty stanu i bezpieczeństwo ponownego uruchomienia, szczególnie przy kończeniu poprzedniej operacji.
- R368–R373 naprawdę zapisują i odczytują wydzieloną ścieżkę testową; R371 sprawdza przynależność rekordów bez zakładania kolejności JSON. Jednak zliczanie tokenów nie zastępuje walidacji treści rekordów. R374 wywołuje produkcyjny serializer dla czasu 0. R375 zaczyna się tutaj; **dalszy ciąg i sprzątanie RTDB wymagają P010**.

**Wynik paczki:** 2000 linii odczytane w dziesięciu nieuciętych porcjach po 200. Nowe ustalenia #73–75. `sprawdz_odzyskiwanie.py`: kod 0, g++ C++17 `-Wall -Wextra -Werror`, rzeczywiste fragmenty i jawne atrapy. Bez sieci/urządzenia, zmian firmware, OTA i push. Łączne udokumentowane rozłączne pokrycie: **10328/46810 linii main**; audyt całego programu nadal trwa.

# Duża paczka P010–P019 — 41571–46570 (5000 linii)

## Nowe ustalenia

### 76. Test „pustej partycji” sprawdza tylko 64 bajty i może dopuścić formatowanie niepustej pamięci — wysoki dla danych
45846–45857: `littlefsPartycjaPusta()` czyta wyłącznie początek partycji. Same 0xFF w pierwszych 64 bajtach nie dowodzą, że pozostała część nie zawiera danych. Po nieudanym montowaniu, 45938–45952 używa tego wyniku jako zgody na `LittleFS.begin(true)` — montowanie z możliwością formatowania.

**Doprecyzowanie po researchu 24.10.2026:** W Arduino 3.3.8 begin(true) formatuje po ESP_FAIL montowania, nie po każdym możliwym błędzie. Ta zależność nie unieważnia błędnego testu pustki 64 bajtów. R04. [Analiza i źródła](narzedzia/audyt-v3.55.2/RESEARCH_2026-10-24.md).

**Odtworzenie:** rzeczywista funkcja i rzeczywista gałąź startu, atrapa partycji 8192 bajtów. Pierwsze 64 bajty = 0xFF, ale bajt pod adresem 4096 = 0x42. Funkcja zwraca „pusta”; po zasymulowanym błędzie montowania kod wywołuje `begin(true)`. Gdy pierwszy bajt jest zapisany, nie dopuszcza tej operacji. Brak partycji/błąd odczytu także poprawnie zwracają false.

Dowód dotyczy błędnej kwalifikacji i wejścia w ścieżkę dopuszczającą formatowanie. **Nie wykonano formatowania ani nie udowodniono, że aktualny system plików użytkownika ma taki stan.** Scenariusz dotyczy m.in. uszkodzonego/wymazanego początku przy danych zachowanych dalej. Nie można obiecywać „braku czegoś do stracenia” na podstawie tego testu. Zalecenie: żadnego automatycznego formatowania po tak krótkiej próbie; jeśli wyjątek dla fabrycznie pustej partycji pozostanie, musi obejmować całą właściwą partycję z obsługą czasu/WDT.

### 77. Nieudana wysyłka alarmu Telegram jest traktowana jak obsłużona zmiana — wysoki dla powiadomień
44580–44604: dla alarmu CO, solarnego i dymu `sendTelegramMessage` jest wywoływane bez sprawdzania wyniku, po czym stan `poprzedniAlarm*` zostaje ustawiony na bieżący. Przy utrzymującym się alarmie kolejne obiegi nie ponowią wiadomości, nawet gdy połączenie już wróciło. Dotyczy także nieudanej wiadomości o końcu alarmu.

**Odtworzenie na trzech rzeczywistych blokach:** pierwsze trzy wysyłki zwracają false; następne wywołanie przy sprawnym nadawcy i niezmienionych alarmach nie wysyła niczego. Zmiana alarmów na nieaktywne uruchamia trzy nowe wysyłki — kontrola pozytywna. Test używa atrap nadawcy i stanu, bez sieci.

To nie wyłącza lokalnych mechanizmów ochrony, ale może pozostawić operatora bez ostrzeżenia. Krótkie zdarzenie między dwoma odczytami flag także nie jest rejestrowane w tej sekcji. Osobne od #6, który dotyczy jednorazowego raportu nadzorcy. Potrzebny trwały/ograniczony bufor zdarzeń albo stan oczekującej wysyłki z kontrolowanym ponawianiem; sama bieżąca flaga nie jest historią alarmów.

### 78. Polecenie OTA może dostać ACK „OK”, mimo że nie weszło do kolejki — średni/wysoki operacyjnie
42836–42843: wykonawca `update`, `update_centrala`, `update_panel` pozostawia `wynikOk=true` po wywołaniu `tgKolejkujAlboOdpowiedzBledem`. Helper 38870–38876 ma wynik void; przy pełnej/nieistniejącej kolejce tylko próbuje wysłać błąd Telegramem. Nie przekazuje porażki z powrotem do wykonawcy. Firebase/LAN mogą więc potwierdzić sukces i zakończyć obsługę ID, choć zadanie aktualizacji nie zostało przyjęte.

**Odtworzenie:** rzeczywisty helper i dwie rzeczywiste gałęzie komend, atrapiona pełna kolejka. Dla centrali i panelu liczba przyjętych zadań = 0, `wynikOk=true`, błąd pusty. Przy dostępnej kolejce zadanie zostaje poprawnie przyjęte. Nie wywoływano aktualizacji. ACK kolejki i tak nie jest dowodem zakończenia flashowania — w tym przypadku nie dowodzi nawet przyjęcia zlecenia. Zalecenie: jawny wynik kolejkowania i rozróżnienie „przyjęto”, „wykonano”, „odrzucono”.

### 79. Odtwarzanie historii może uznać obcięty lub nadmiarowy wiersz CSV za prawidłową próbkę — średni dla wiarygodności danych
45630–45633: po przekroczeniu 512 bajtów `HistoriaStrumien` pomija końcówkę wiersza, ale zwraca jego prefiks jako zwykły sukces. Nie oznacza go jako uszkodzonego. Parser krótkiej historii kończy zbieranie po 14 polach, a długiej po 36; sprawdzana liczba pól jest liczbą już ograniczoną przez pętlę. Nadmiarowe kolumny nie są więc wykrywane.

**Druga runda R14:** Ponowiono 48 bazowych prób i rzeczywisty czytnik/parser z uszkodzeniami: nadmiar kolumn i obcięty prefiks nadal przechodzą; mała liczba kolumn jest odrzucana. Atrapa String historii używa strtof/strtol i nie jest dowodem bitowej zgodności konwersji z Arduino. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R14_WALIDACJA_TESTY.md).

**Odtworzenie na rzeczywistym czytniku i parserze krótkiej historii:**
- poprawne 14 kolumn daje jedną próbkę;
- ten sam wiersz z 15. kolumną `EXTRA_COLUMN` również daje jedną próbkę;
- wiersz z końcówką 600 znaków `X` przy ostatniej kolumnie zostaje obcięty do 512 bajtów, a potem przyjęty jako próbka; `toInt()` akceptuje liczbowy prefiks czasu;
- za mało kolumn jest poprawnie odrzucane.

Dodatkowo wykazano małą stratę na granicy okna: dla `old\nnew\n`, limit ogona 4 bajty ustawia pozycję dokładnie na początku `new\n`, a czytnik mimo to odrzuca ten pełny wiersz jako „niepełny pierwszy”. W normalnym dużym pliku jest to co najwyżej jeden wiersz na początku czytanego okna; często poza końcowymi N próbkami, więc nie utożsamiać tego z utratą całej historii.

Istniejące **48 regresji historii nadal przechodzi**. Dotychczasowe przypadki nie odrzucały takich uszkodzeń. Atrapy File/String i normalizacji czasu pochodzą z istniejącego testu; nie jest to test fizycznego LittleFS. Zalecenie: sygnalizować przepełniony wiersz, sprawdzać dokładną strukturę i wartości, jawnie rozstrzygnąć politykę niepełnego ostatniego wiersza.

### 80. Powiadomienia o potwierdzeniu/rollbacku OTA wykonują blokującą sieć w pętli sterującej — wysokie ryzyko dostępności sterowania
`loop()` wywołuje `otaPotwierdzTick()` w 46756, przed nadzorcą zadań i dalszym sterowaniem. Ta funkcja wysyła Telegram bezpośrednio w 45032–45036, a przy potwierdzaniu wywołuje `potwierdzPartycjeJesliCzeka()`, które również wysyła Telegram (44925–44940). Nadawca nie jest kolejką: wykonuje operacje mutex/TCP/TLS/HTTP synchronicznie.

**Druga runda R15:** R15 uruchamia oryginalny własny scheduler: syntetyczna funkcja trwająca 30 s opóźnia gotowe następne zadanie; termin osiągnięty w trakcie blokady może być zauważony dopiero w kolejnej iteracji. To nie zmierzona latencja operacji sieciowej; okresy schedulera i karmienie WDT nie wystarczają jako gwarancja ochrony. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

Zatem problem dotyczy nie tylko callbacków WWW z #53. W trakcie takiego powiadomienia **główna pętla nie przejdzie do następnych zadań**, dopóki wywołanie nie wróci. Nie zmierzono czasu na sprzęcie i nie przypisujemy temu poprzedniej awarii, ale samo przeniesienie zwykłego Telegrama do Core0 nie chroni tej ścieżki. Do tego dochodzi znany problem limitu TLS (#15). Zalecenie: do zadania sieciowego przekazywać samo oczekujące powiadomienie; potwierdzenie partycji nie powinno wymagać blokującej wysyłki z pętli sterowania.

## Wyniki odcinków i uzupełnienia (bez dublowania numerów)

### P010 — 41571–42070
- Dokończono R375–R379. R377 rzeczywiście usuwa własny węzeł testowy i weryfikuje `null`; R376 sprawdza różność dwóch ID, nie wielokrotne wygenerowanie tego samego ID, więc etykieta „deterministic” jest szersza od sprawdzenia (#68).
- Telemetria zachowuje jeden przygotowany JSON na ponowienia; po wyczerpaniu prób porzuca kopię, nie usuwa samych próbek. Zrzut do flasha jest przed bramką WiFi. To właściwe kontrole, nie dowód końcowego ACK (#49) ani całej integracji.
- `fbInitialize` unika powtórnego initializeApp przy aktywnym stanie. Mimo komentarzy `fbApp.loop` nie ma tu twardego limitu pojedynczego wywołania. DNS pre-warm odrzuca 0.0.0.0 i przerywa po porażce; to pozytywne zabezpieczenie. CA jest ustawiane jawnie.
- **#21:** wspólny builder statusu (42000) także wstawia niekodowane SSID, więc wada dotyczy `/piec/status` i `/api/lan/status`, nie tylko listy WiFi. Pola liczbowe nie mają tu oddzielnej kontroli wartości skończonych; powiązać z #55 w Z2.

### P011 — 42071–42570
- Status pokazuje flagi symulacji per czujnik i stan bufora offline; wcześniejsze ustalenie #65 o raporcie Telegram nie oznacza braku tych pól wszędzie.
- **#3:** `minutyDoFb`, turbo i cooldown nadal porównują bezpośrednio terminy millis. Kod raportu może pokazywać błędne pozostałe minuty w okolicy zawinięcia.
- Bufor statusu 3072 B nie sygnalizuje odrzuconego fragmentu w lambdzie append; mniejsze późniejsze fragmenty nadal mogą wejść. Nie dowiedziono przepełnienia przy aktualnym normalnym zestawie danych. Z2/Z3: test pełnego buildera ze skrajnymi dopuszczalnymi wartościami.
- `fbJsonEscape` koduje cudzysłów, ukośnik odwrotny, LF/CR/TAB, lecz nie wszystkie znaki sterujące JSON (np. backspace). Parser wejściowy również nie jest pełnym parserem JSON. Uzupełnienie istniejących #20/#21/#55, nie dowód wadliwej obecnej odpowiedzi RTDB.
- Ponownie potwierdzono ręczne pompa OFF bez nadrzędnej ochrony alarmowej (#7) i niespójne wymuszenia serw (#43). Nie wykonywano fizycznych poleceń.

### P012 — 42571–43070
- Komendy symulacji używają `toFloat`/`toInt` i samych porównań zakresów (#55). Dla NaN oba porównania zakresów są fałszywe; nie ma `isfinite`. Nieprawidłowy czas może zostać zastąpiony domyślną godziną zamiast błędem.
- `rtc_sync` sprawdza dzień 1–31, nie rzeczywistą liczbę dni miesiąca/lata przestępnego; dopuszcza np. 31 lutego. Nie sprawdza też gotowości RTC przed `adjust`. Z2: wspólna walidacja czasu i serializacja I2C; nie twierdzimy, że aktualny zegar użytkownika jest błędny.
- `tg_config`, provisioning i część ustawień potwierdzają zapis bez sprawdzenia rezultatów NVS. Znane ryzyko trwałości, a nie dowód rzeczywistego braku zapisu. Spool zwraca informację o błędzie trwałości tekstem, ale wykonawca panelu nie przenosi całej tej informacji do ACK.
- **#36–38:** ponownie prześledzono parser ID/token/cmd, stan deduplikacji w RAM i jawne logowanie komendy konfiguracyjnej Telegrama. Nie kopiowano sekretów. Niepoprawny payload pozostający w `/piec/cmd` odnawia okno turbo co polling, więc komentarz o najwyżej jednej minucie zbędnego turbo nie jest gwarancją.

### P013 — 43071–43570
- Dokończono wykonanie/ACK/bezwarunkowy DELETE (#36). Identyczne ID chroni przed ponownym wykonaniem wyłącznie w bieżącej sesji RAM (#37).
- Prognoza deszczu: parser traktuje `null`/nienumeryczny element jak zero przez `toFloat`, co może udawać suchy slot. Przy błędzie pobrania poprzedni stan pozostaje; w tej funkcji nie ma oznaczenia wieku ani nieważności. To ograniczenie informacji pogodowej, nie sterowanie ochroną pieca.
- Indeks minutely_15 jest liczony z godziny, bez porównania tablicy `time`; poprawność zależy od początku tablicy i zachowania API w dniach zmiany czasu. Do weryfikacji w Z2/Z3, nie oznaczono jako potwierdzony błąd DST dostawcy.
- **#57:** pełna prognoza oblicza mróz/trend od indeksu 0, nie od bieżącego czasu. Współrzędne w obu URL poprawnie wskazują Trzemchę Górną. Powtórzenia `setInsecure` i timeoutu TLS pozostają znanymi ryzykami, bez nowych numerów.

### P014 — 43571–44070
- Dokończono tablice prognozy i publikację cache (#58). Nie ma kontroli `overflowed()` dokumentu wyjściowego; alokator nie ma fallbacku bez PSRAM. Błąd deserializacji wejścia jest sprawdzany. Odczyt `http.getStream()` wymaga zgodności z kodowaniem transportowym odpowiedzi; nie sprawdzano bieżącego API.
- LAN kopiuje komendę i publikuje ACK pod krótką sekcją krytyczną, a wykonanie jest poza nią. Komendy konfiguracyjne Telegrama są maskowane w logu LAN, w przeciwieństwie do Firebase (#38).
- Raport nadzorcy kasuje stan przed skuteczną wysyłką (#6). Przejrzano dyspozytor komend testowych: brak blokady „tylko płytka testowa”, znane #48/#68.
- `tgTask` sprawdza Stability przed całą obsługą, ale Final/Recovery dopiero po raportach i kolejce. Po uruchomieniu workera może przetworzyć następną komendę z tej samej kolejki przed wyjściem. Macierz wyłączności Z2 nadal otwarta.

### P015 — 44071–44570
- Provisioning odrzuca zmianę gotowej sesji, lecz warunek nie dowodzi braku aktywnej, jeszcze niegotowej autoryzacji; usuwa obiekt UserAuth przed ponownym initializeApp. Do Z2 z rzeczywistą wersją FirebaseClient; nie udowodniono wiszącego wskaźnika w bibliotece.
- **#51/#72:** kasowanie solarnej daty bierze ją ze wspólnego Stringa, a przełącznik „notyfikacji” wyłącza też polecenia Telegrama. Raporty/operacje plikowe mogą wykonywać kilka wysyłek w jednej komendzie; budżet iteracji jest sprawdzany między komendami, nie przerywa pojedynczej operacji.
- OTA centrali odrzuca starszą wersję liczbowo, panel porównuje tylko równość tekstową wersji. Zatem panel może dostać starszy release niż zgłaszana wersja. Nie dowodzi instalacji przez panel; wymaga sprawdzenia jego własnej walidacji poza zadeklarowanym audytem centrali.
- Alarm i zajętość OTA sprawdzane są przed pobraniem wersji/wiadomościami, nie ponownie bezpośrednio przed flashowaniem. Alarm może wystąpić w międzyczasie. Rozszerzenie #27: kontrola na wejściu nie jest gwarancją braku alarmu przez całą operację. Nie symulowano tego na piecu.

### P016 — 44571–45070
- Nowe #77 i #80. Bloki wysyłki alarmów nie mają dodatkowej bramki budżetu; status/komendy/pogoda mają bramki, ale wywołania synchroniczne mogą je przekroczyć. Samo karmienie WDT nie dowodzi dostępności zadania.
- Solar działa także bez WiFi przez `solarArchiveLoopLocalOnly`, lecz obie ścieżki zależą od pozostałego budżetu tgTask. Nie jest to niezależny zegar działający mimo zawieszenia sieciowego workera.
- **#1:** potwierdzenie OTA wymaga 10 min uptime i WiFi w chwili sprawdzenia, nie 10 min ciągłego zdrowego połączenia. Po 60 min nie wymaga WiFi. Flaga oczekiwania zerowana jest przed wywołaniem API, więc błąd zatwierdzenia nie ma tu ponowienia.
- Sprawdzenie zgodności taga następuje już po `mark_app_valid_cancel_rollback`; niezgodność powoduje ostrzeżenie, nie rollback. Plik znacznika jest wskazówką, nie dowodem przyczyny zmiany wersji (np. ręczne USB też może zmienić wersję).

### P017 — 45071–45570
- **#19:** WebSocket wywołuje `handleWsCmd` bez własnej kontroli tokenu; serwerowa komenda restart nie wymaga osobnego potwierdzenia — opisane potwierdzenie jest tylko w UI. Handler przyjmuje tylko pojedynczą pełną ramkę tekstową; nie ma limitu jej długości przed alokacją Stringa.
- Terminal ma nieaktualną etykietę „Trociny (palenisko)” dla wody w płaszczu przystawki. Brak flag symulacji/jakości w tym statusie; to nie jest ten sam builder co status LAN/Firebase.
- **#28–30/#41/#42:** krótka historia po krótkim zapisie usuwa całą kopię z RAM; przy małym bloku sterty kasuje pending. Długa historia gubi rekord już zdjęty z kolejki przy błędzie zapisu. Resync otwiera docelowy plik w trybie `w` i nie sprawdza długości `print`, więc nie jest transakcją odporną na przerwanie. Nie powielamy numerów tych znanych błędów.
- Samo przeniesienie zapisu flash na drugi rdzeń nie eliminuje sprzętowych przerw dostępu do flash; komentarze o „nigdy nie blokuje Core1” są zbyt kategoryczne. Nie mierzono długości przerw.

### P018 — 45571–46070
- Czytnik strumieniowy oddaje czas po każdej porcji 4096 B i sprawdza limit czasu między porcjami. To istotna poprawa startu; nie jest jednak twardym ograniczeniem czasu pojedynczego odczytu z warstwy File. Nowe przypadki uszkodzeń opisuje #79.
- Nowy #76. Start nadal wykonuje montowanie, historię i logi przed konfiguracją bezpiecznych GPIO (#32). Nie zmieniano kolejności ani danych.
- WDT/PM mają sprawdzane wyniki API i logowanie błędów. Błąd reconfigure nie uruchamia alternatywnej inicjalizacji WDT; wymaga potwierdzenia konfiguracji frameworka, nie założenia, że nadzór został skonfigurowany mimo błędu.

### P019 — 46071–46570
- **#9 — nowy dowód startowy:** 46375–46386 kończy retry inicjalizacji wszystkich DS18B20, gdy odpowie dowolna z trzech magistral. Test rzeczywistej pętli: pierwsza odpowiada od razu, pozostałe odpowiedziałyby przy drugiej próbie; każda dostaje tylko jedną próbę. Gdy nie odpowiada żadna, wszystkie dostają trzy próby. To rozszerzenie tego samego problemu wspólnego OR, nie nowy numer.
- **#32:** zasilanie DS/DHT i bezpieczne GPIO są ustawiane po testach czujników, WiFi i starcie zadań Telegrama. Zadania sieciowe mogą już przyjmować polecenia przed ukończeniem inicjalizacji sprzętu. Faktyczny stan elektryczny przed pinMode zależy od układu — nie zakładamy, że czujniki zawsze są wtedy wyłączone.
- **#2:** tworzenie tgTask/tgPollTask/historiaTask ignoruje wynik. Przy historii poprawnie nie startuje task bez semafora/kolejki, ale sam błąd utworzenia taska również wymaga obsługi.
- **#33:** przywracane nastawy mają tylko częściowe zakresy; krok 0 nadal dozwolony. Pozycje fizyczne klapy 0–180 i syberka 0–90 są natomiast ograniczane zgodnie z jednostkami. Rozpalanie nie odtwarza aktywnego wymuszenia opartego na dawnym millis — kontrola pozytywna.
- Rejestracja schedulera: 18 zadań w trybie sprzętowym, 17 w ławkowym, przy limicie 20. To kontrola liczby deklaracji, nie test czasów wykonania. **Setup kończy się poza odcinkiem; dalszy ciąg od 46571 należy do P020.**

## Testy i stan po paczce
- `sprawdz_start_i_alarmy.py`: kod 0, rzeczywiste fragmenty, g++17 `-Wall -Wextra -Werror`; #76–78 i rozszerzenie #9, bez flash/sieci.
- `sprawdz_historia_uszkodzenia.py`: kod 0, #79 oraz 48/48 wcześniejszych regresji na kodzie 3.55.2. Kompilator zgłasza dwa ostrzeżenia o nieużywanych helperach odziedziczonych z atrapy (`F`, `ps_calloc`); nie są błędami firmware.
- Ponownie uruchomiono wszystkie dziewięć skryptów reprodukcji `odtworz_bledy.py` i `sprawdz_*.py` — kod 0. `ODTWORZONO` nie znaczy „naprawiono”. Regresje modelu telemetrii: 20/20; nie zastępują Firebase na urządzeniu.
- Całe **5000 linii** odczytano i przeanalizowano. Pierwszy odczyt 500-liniowy został ucięty; zastąpiono go pełnymi odczytami 41571–41770, 41771–41920, 41921–42070. Pozostałe odczyty były nieucięte. Punktowe powroty do definicji oraz 46754–46791 nie zwiększają pokrycia.
- **80 ustaleń; 19/88 odcinków; 15328/46810 linii udokumentowanego rozłącznego przeglądu.** Cztery zadania końcowe pozostają otwarte. Nie jest to deklaracja ukończenia audytu ani gotowości do OTA.
- Kontrola końcowa paczki: regresje nadzorcy **34/34**, numery ustaleń 1–80 bez luk, tabela planu 19 odczytanych odcinków i suma 15328 linii. SHA256 głównego firmware identyczne z bazowym; `git diff --exit-code -- firmware-patch` = 0.

## Paczka P020–P030 — 5240 linii, zamknięta 24.10.2026

**Pokrycie: 20568/46810 linii (~44%), 30/88 odcinków. Nadal 80 ustaleń.** Brak nowego numeru: potwierdzony problem oczekiwania na panel był już opisany w #26. Nie mnożymy tych samych błędów.

### Co rzeczywiście przeczytano
- P020: **46571–46810** — końcówka setup, przywrócenie OTA panelu, cały nadzorca i loop.
- P021–P030: **1–5000**, dwadzieścia nieuciętych odczytów po 250 linii — historia zmian i komentarze. To pokrycie tekstu pliku, **nie 5000 linii wykonywanego kodu**, nie dowód poprawności dawnych napraw. Końcowy wpis historii jest kontynuowany od 5001.
- Punktowy powrót 28250–28295 i wyszukanie wszystkich przypisań flag OTA nie są ponownie doliczane do pokrycia.

| Odcinek | Zakres | Wynik analizy |
|---|---|---|
| P020 | 46571–46810 | #26: oczekiwanie na panel usypia nadzór; #80: sieć przed schedulerem. Zerowanie licznika po 30 min mierzy uptime, nie sprawność komunikacji. Zapis NVS przed restartem nie sprawdza wyniku. Ograniczenia nadzorcy pozostają w ustaleniach #1–6. |
| P021 | 1–500 | Najnowsze opisy solar/telemetrii/startu. Deklaracja ochrony niepustej partycji nie unieważnia odtworzonego #76; opisy kanału bojlera z dawnych wersji nie są bieżącym mapowaniem solar. |
| P022 | 501–1000 | Historia testów i przenoszenia Firebase pomiędzy zadaniami. Etykieta „test-only” w komentarzu nie stanowi blokady wykonania; #48/#68 pozostają otwarte. |
| P023 | 1001–1500 | Historia spool, mount/coredump, UserAuth, telemetryki i watchdogów. Opisy testów historycznych nie są wynikiem obecnego uruchomienia. Zachowanie danych sprawdzamy w implementacji, nie w deklaracji „bez strat”. |
| P024 | 1501–2000 | Komendy, turbo, TLS i dawne workery. Komentarz o handshake 4000 ms jest sprzeczny z API Arduino 3.3.8 (sekundy), co opisano już w audycie. Usunięte architektury nie opisują dzisiejszej własności klienta Firebase. |
| P025 | 2001–2500 | Prognozy, DNS, alarmy, historia, scheduler. Budżet sprawdzany pomiędzy wywołaniami nie ogranicza pojedynczego blokującego wywołania. Historyczne „17 zadań” nie zastępuje aktualnego liczenia rejestracji. |
| P026 | 2501–3000 | Deszcz, alarmy, WiFi, symulacje, czujnik dymu. Stare opisy wspólnego alarmu CO/panel wymagają aktualnych funkcji; nie dopisano historycznej usterki jako nowej. Symulacje oceniamy według bieżących znaczników jakości. |
| P027 | 3001–3500 | WDT/FS, historia, UI i terminal. Karmienie Task WDT przed zapisem nie dowodzi usunięcia INT_WDT ani zapewnienia czasu reakcji alarmu; dawne rozpoznanie przyczyny nie jest pomiarem z obecnej centrali. |
| P028 | 3501–4000 | Historia OTA, TLS, UI i blokad plików. Potwierdzenie partycji nie jest dowodem właściwej wersji ani zdalnego dostępu; #1 i #26. „Atomowy odczyt 32-bit” nie zastępuje synchronizacji całego stanu. |
| P029 | 4001–4500 | Cache FS, RTC, czujniki, serwa i pompa. Dawne opisy dwóch magistral/MOSFET nie potwierdzają obecnego okablowania. Zapisana intencja filtra nie dowodzi bezpieczeństwa skoków temperatury (#8/#9). |
| P030 | 4501–5000 | Antystop, logowanie, DNS, mechanizmy ograniczania prób i rotacja. `volatile` i wspólny rdzeń nie gwarantują spójnej wielopolowej aktualizacji; do macierzy współbieżności Z2. Historia kończy się w połowie opisu rotacji — ciąg dalszy P031. |

### Mocniejszy dowód #26 — bez nowego numeru błędu
`panOtaGotowa` z NVS odtwarza `g_panelOtaDostepna` i `otaOperacjaWTrakcie` (46582–46598), bez sprawdzenia gotowości LittleFS, istnienia i rozmiaru pliku. To przypuszczenie o stanie pliku, nie jego weryfikacja. Przywrócenie próbuje też synchronicznie wysłać Telegram; komentarz „brak połączenia nie zawiesza” nie gwarantuje czasu DNS/TLS.

Nadzorca obejmuje tę flagę bezwarunkową blokadą (46711–46724). W normalnym cyklu oczekiwania na panel zwolnienie następuje po zgłoszeniu oczekiwanej wersji przez `/api/status?panel_fw=...` (28273–28278); brak osobnego terminu oczekiwania. Nieobecny panel może więc pozostawić centralę bez tego mechanizmu odzyskiwania sieci. Dotyczy także poprawnego pliku oczekującego na panel — brak pliku nie jest wymagany do reprodukcji.

**Nowy test offline:** `python3 narzedzia/audyt-v3.55.2/sprawdz_panel_nadzor.py` — kod 0, g++ C++17 `-Wall -Wextra -Werror`. Test wycina rzeczywisty blok przywrócenia, wyrażenie blokady i `nadzorDecyzja()`. Przy identycznym starym heartbeat bez blokady otrzymuje RESTART, po przywróceniu flagi USPIONY, także dla czasu jednej doby. Kontrole: brak znaczników nie ustawia blokady, przerwane pobieranie czyści `panOtaTrwa`. NVS/Telegram to atrapy; brak badania flash i fizycznego czasu wysyłki. Pierwsze uruchomienie skryptu zatrzymał zbyt szeroki warunek szukający `LittleFS` także w komentarzu; po ograniczeniu kontroli do kodu test przeszedł.

Ponownie `tests/host/run_nadzor_tests.py`: **34 sprawdzenia, 0 błędów**. Zielony wynik tych testów nie obala luki integracyjnej #26. Nie powtarzano całego zestawu testów z P019, nie zmieniano firmware ani nie wykonywano OTA.

**Następna paczka: P031–P040, 5001–10000 (5000 linii).** Kontynuacja historii, deklaracje i początek osadzonego interfejsu. Z1–Z4 nadal otwarte.

### 81. Własny alokator TLS nie sprawdza przepełnienia mnożenia rozmiarów — błąd kontraktu, osiągalność sieciowa nieudowodniona
**5595–5606**, instalacja hooka **45903**: `mbedtls_psram_calloc(n, size)` liczy `size_t total = n * size` bez kontroli przepełnienia, po czym przydziela i zeruje tylko `total` bajtów. Dla 32-bitowego `size_t` ESP32 argumenty `n=0x80000001`, `size=2` dają `total=2`, choć żądany rozmiar nie mieści się w przestrzeni adresowej. Funkcja może zwrócić niepusty bufor dwóch bajtów zamiast odmówić alokacji. Gdyby wywołujący polegał na pierwotnej liczbie elementów, grozi zapis poza buforem.

**Druga runda R14:** R14 porównuje oryginalny hook z oryginalnym heap_caps_calloc_base IDF 5.5.4: ten drugi wykrywa iloczyn przez __builtin_mul_overflow przed malloc. Przejrzane alokacje TLS/ASN.1/X509 nie stanowią kompletnego dowodu osiągalności; nadal nie wykazano ataku sieciowego. Dodatnie kontrole zerowania/fallback/OOM przechodzą. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R14_WALIDACJA_TESTY.md).

**Dowód:** `sprawdz_deklaracje.py` kompiluje oryginalną funkcję z atrapą sterty; na hoście używa odpowiednio `SIZE_MAX/2+2` i `2`, otrzymując rzeczywiste przepełnienie natywnego `size_t` i bufor 2 B. Osobno potwierdza arytmetykę 32-bit. Test nie zapisuje poza buforem. Kontrole zwykłej alokacji, zerowania, braku PSRAM, awarii PSRAM i braku obu rodzajów pamięci przechodzą.

**Ograniczenie:** nie wykazano, że aktualne mbedTLS przekazuje taki iloczyn pod wpływem odpowiedzi sieciowej. To odtworzona wada funkcji zastępującej `calloc`, **nie dowód ataku, uszkodzenia pamięci na centrali ani przyczyna wcześniejszego coredumpu**. Przyszła poprawka: odrzucić `size != 0 && n > SIZE_MAX / size` przed mnożeniem, z testami granicznymi. Firmware pozostawiony bez zmian.

## Paczka P031–P040 — 5000 linii, zamknięta 24.10.2026

**Aktualne pokrycie: 25568/46810 linii (~55%), 40/88 odcinków, 81 ustaleń.** Przeczytano 5001–10000 bez luk. To mieszanka komentarzy, C++ i osadzonego HTML/CSS; nie oznacza 55% potwierdzonego bezpieczeństwa programu.

### Wynik każdego odcinka
| Odcinek | Linie | Wynik |
|---|---|---|
| P031 | 5001–5500 | Koniec historii, include, override rollbacku, deklaracje, RTC, wrappery NVS i sekrety. Wrapper akceptuje literal 15 znaków i odrzuca 16 podczas kompilacji. Sprawdzenie długości nie sprawdza powodzenia zapisu ani wszystkich bezpośrednich `config.put*`. `verifyRollbackLater()` nie dowodzi konfiguracji bootloadera (#1). |
| P032 | 5501–6000 | Konfiguracja OTA/TG, alokator TLS, blokada wysyłki, enum i kolejka poleceń, zadania, początek CA. Nowe #81. `tgWezmijSendMutex` może czekać 90 s; karmienie WDT nie wykonuje schedulera, istotne dla #53/#80. Kolejka nadal zawiera tylko cmd/arg, nie tekst (#51). |
| P033 | 6001–6500 | Koniec CA, Firebase i telemetria, makra ograniczające ponawianie połączeń, stan WiFi/OTA, początek HTML/CSS (6387). CA odczytane OpenSSL, zgodność odcisków potwierdzona. Wielopolowy stan `volatile` nie jest transakcją. Terminy ograniczania prób podlegają #3. |
| P034 | 6501–7000 | Responsywność, nagłówek, stan połączenia, alarm, główna karta, szybkie sterowanie i początek kafelków. To prezentacja, nie blokada bezpieczeństwa. Drobna duplikacja `.tile:active` — wygrywa późniejsze scale(.985); nie wpływa na automatykę. |
| P035 | 7001–7500 | Stany kafelków, symulacja, nieświeże dane, wykresy, legenda próbek, początek pogody. Same klasy `stale`/`sim` nie dowodzą poprawnego przypisywania jakości przez JS; integrację kontynuujemy w dalszych odcinkach. |
| P036 | 7501–8000 | Pogoda, łuk słoneczny, archiwum, nawigacja i okno ustawień. W CSS `.solar-daily-status.ok` odwołuje się do niezdefiniowanej tu zmiennej `--ok`; deklaracja koloru będzie nieważna bez definicji w czasie wykonania. Kosmetyczna uwaga do późniejszego porządkowania. |
| P037 | 8001–8500 | Formularze, symulacja, pozycje serw, pompa, toast, wykres pełnoekranowy i panel demo. `--border-h` również pochodzi ze starego stylu i nie ma deklaracji w tym osadzonym panelu. `disabled` w UI nie zastępuje sprawdzania komend przez centralę. |
| P038 | 8501–9000 | Powtórzone style demo, animacje, logowanie i początek WiFi. Powtórzenia identycznych reguł nie są nową awarią. Okno logowania nie dowodzi ochrony lokalnych tras (#19). |
| P039 | 9001–9500 | WiFi, Telegram, terminal diagnostyczny i OTA: style, nie logika transportu. Nie uznano opisów „ACK”, „szyfrowanie” czy „weryfikacja” za dowód rzeczywistego potwierdzenia/uwierzytelnienia. |
| P040 | 9501–10000 | Koniec CSS, HTML pulpitu, wykresów, pogody, solar, ustawień i początek modala logowania. Tekst solar prawidłowo rozdziela czujnik zbiornika, 100 l i równoważnik cieplny od zmierzonego uzysku. Są pozostałości przykładowych danych w HTML, wymagające kontroli ścieżek inicjalizacji JS. Modal kontynuujemy od 10001. |

### Kontrole i granice wniosków
- Nowy `python3 narzedzia/audyt-v3.55.2/sprawdz_deklaracje.py`: **kod 0**. Rzeczywisty alokator i wrappery NVS, g++17 `-Wall -Wextra -Werror`; ujemny test klucza 16 znaków ma oczekiwany błąd `static_assert`, nie awarię testera.
- OpenSSL poprawnie parsuje wszystkie trzy osadzone bloki PEM (R4 osobno oraz R1 cross-signed + R4 w trust store). Odciski SHA256 zgodne z komentarzami. R1 cross-signed ważny do **28.01.2028**, R4 do **22.06.2036**; nie są przeterminowane na datę audytu. To nie test bieżącego łańcucha serwera, czasu systemowego ESP ani handshake.
- `volatile` przy wspólnym IP/cache i makrach Telegrama nie gwarantuje spójności: punktowo ponownie przeczytano **32644–32745**, gdzie dwa zadania mogą dojść do wspólnego DNS i bramki. Komentarz o „dokładnie jednej próbie” nie jest realizowany osobnym stanem/rezerwacją HALF_OPEN. Pełny test przeplotów i ocena skutku pozostają do Z2; nie ogłoszono odtworzonej awarii.
- HTML startuje z tekstami `LIVE`, „odczyt stabilny”, przykładową fazą **58.4°C** i przykładowymi godzinami słońca. Punktowo sprawdzono **14893–14955**: JS zastępuje fazę także komunikatem o braku danych. Nie twierdzimy, że 58.4°C pozostaje po normalnym załadowaniu. Pozostaje test nieudanego startu skryptu/żądania i stanu przed pierwszą odpowiedzią.
- Punktowe **16815–16875** ujawniło trop do dalszego audytu pogody: elewacja to `sin(p*PI)*48`, nie pozycja astronomiczna dla daty/szerokości geograficznej; kolejny wschód po zachodzie to `+24h`. Przy dojściu do P054 (16501–17000) należy przeczytać całą funkcję i wejścia, sprawdzić zmianę czasu oraz etykiety. Nie doliczono tego wycinka do ciągłego pokrycia.
- Prosty skan zmiennych CSS wykazał też `--cols`, `--tile-h`, `--sc`. Nie uznano ich automatycznie za błędy: pierwsze dwie mają fallbacki, trzecia może być nadawana dynamicznie. Nie wykonano testu wizualnego w przeglądarce.
- Nie uruchamiano ponownie pełnego zestawu wcześniejszych testów ani nie kompilowano całego firmware. Brak dostępu do urządzenia, OTA, zmian kodu produkcyjnego i publikacji.

**Następna paczka: P041–P050, 10001–15000 (5000 linii)** — dalszy HTML i JavaScript panelu. Z1–Z4 pozostają otwarte.


## Paczka P041–P050 — 5000 linii, zamknięta 24.10.2026

### 82. Świeżość danych jest mierzona od odczytu bazy, nie pomiaru centrali — wysoki
`pollFirebase` 11214–11233 ustawia `_lastFirebaseFetch = Date.now()` po każdym obiekcie z `/piec/status`. `applyIncomingData` 11235–11373 nie sprawdza wieku źródła ani postępu uptime; brak `online` zastępuje obecnością dowolnego poprawnego pola. Kafelek LIVE i warunek świeżości rozpalania (10928–10932) korzystają z tego czasu odczytu. Stary zapis w dostępnej bazie może więc stale odnawiać „świeżość”, mimo że centrala już nie publikuje. Test rzeczywistego `pollFirebase` z atrapą odbiorcy i identycznym obiektem po dobie potwierdza odnowienie czasu i połączenia. Nie jest to test rozpalania na urządzeniu; lokalne zabezpieczenia centrali nadal decydują o wykonaniu komendy.
**Późniejsza poprawka:** znacznik czasu/próbki źródłowej i kontrola jej wieku; osobne stany „baza dostępna” oraz „centrala nadaje”. Nie zmieniać przy tym ustalonego rytmu publikacji.

**Druga runda R13:** R13 ponawia test dobowego zastoju i dodaje null, pusty obiekt/tablicę oraz odwrotną kolejność dwóch odpowiedzi. Null pozostawia wcześniejszy stan połączenia; starszy odczyt kończący później trafia do odbiorcy jako ostatni. Nie testowano fizycznego rozpalania. HTTP no-store i dostępność klienta bazy nie zastępują czasu/próbki źródła. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md).

### 83. Logowanie może zgłaszać sukces mimo braku dostępu do statusu — średni
`pollFirebase` przechwytuje błąd bez przekazania wyniku wywołującemu. `doFirebaseLogin` 11450–11519 po `await pollFirebase()` przechodzi do komunikatu „Połączono ze sterownikiem”, jeżeli dalsze operacje nie zgłoszą błędu. `tryAutoLogin` 11556–11588 zwraca `true` po odświeżeniu tokenu i odczycie, nawet gdy odczyt zakończył się odmową; blokuje to również przejście do zapasowej ścieżki logowania hasłem. Test rzeczywistych `tryAutoLogin` i `pollFirebase`: odpowiedź odczytu HTTP 403 → wynik true, `_isFirebaseConnected=false`. Odświeżanie tokenu jest atrapą; sam `fbRefreshIdToken` także połyka wyjątki. Nie utożsamiać zalogowania konta z działającym sterownikiem.

**Druga runda R13:** R13 ponownie uruchamia rzeczywiste tryAutoLogin razem z pollFirebase: po odświeżeniu tokenu odczyt statusu 403 nadal kończy się true z auto-login. Bez prawdziwego konta, wszystkie tokeny w teście są atrapami. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md).

### 84. Wyłączenie „zapamiętaj” nie usuwa starych danych trwałych — średni, odtworzony
`safeStorage` 11084–11120 najpierw odczytuje localStorage. `set(..., false)` zapisuje sessionStorage, lecz nie usuwa starego localStorage. Test: trwałe OLD, następnie sesyjne NEW → `get` nadal zwraca OLD. Dotyczy danych konta/hasła/tokenów zapisanych przez logowanie 11488–11494. Dodatkowo odświeżanie 11207 wymusza trwały zapis refresh tokenu niezależnie od zaznaczenia „zapamiętaj”. Hasło jest przechowywane jako tekst dostępny skryptom tej samej strony, co zwiększa skutki #39; test używa wyłącznie fikcyjnych wartości. Wylogowanie usuwa hasło i tokeny — kontrola `remove` potwierdza usunięcie obu magazynów.

### 85. Błąd GitHub zamienia się w fikcyjne wydanie produkcyjne — średni, odtworzony
`checkGithubRelease` 13569–13603 po błędzie sieci/HTTP wpisuje przykładowe v3.31.21 i pliki z URL `#`, bez warunku DEMO. Wywołuje ten sam callback, który w 13674–13679 pokazuje „Sprawdzono wydania” jako sukces. Karta 13648–13659 ocenia aktualizację przez nierówność tagów, nie kolejność wersji, więc starsze lub fikcyjne wydanie bywa „dostępną aktualizacją”. Test rzeczywistej funkcji przy błędzie fetch potwierdza fikcyjny tag i plik. **Nie oznacza to, że centrala pobierze `#`**: przycisk wysyła odrębną komendę `update`, a firmware sam pobiera wydanie. Błąd dotyczy wiarygodności informacji dla operatora.

### 86. Paginacja historii pomija rekordy o tym samym czasie na granicy strony — średni, odtworzony
`historyFetchShard` 14457–14491 pobiera 300 rekordów sortowanych po `ts`, po czym ustawia `cursor=maxTs+1`. Jeśli granica przecina kilka rekordów z tym samym `ts`, reszta tej sekundy znika z wyniku. Test rzeczywistego parsera, sortowania i pobierania z atrapą kontraktu RTDB: 299 wcześniejszych rekordów + 2 rekordy z ts=300 → tylko 300 z 301; brak ostatniego. Nie wykazano takiej kolizji w aktualnej bazie użytkownika. Potrzebny kursor uwzględniający klucz i czas albo obsługa remisów; nie kasować danych źródłowych. Limit 100 stron i zapasowe pobieranie całej doby to dodatkowe granice wydajności, nie dowód obecnej utraty danych.

**Druga runda R13:** R13 rozszerza próby: 601 remisów daje 300 rekordów; 30001 różnych czasów daje 30000 bez sygnału niepełności; dowolny błąd zawierający 400 ustawia fallback. Rzeczywisty serializer REST SDK potwierdza parę wartości/klucza, ale wariant bez klucza serializuje sentinele. Model prawidłowego kursora nie jest testem serwera ani poprawką firmware; zachować źródłową historię. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md).

### 87. Opisy czujników nadal mylą zbiornik solarny i płaszcz przystawki — średni
`ALL_SENSORS` 12120–12123 opisuje solar jako „Kolektor glikolowy · 1-Wire / PT1000”, a trociny jako zasobnik paliwa. Nie odpowiada to ustalonej instalacji: solar to temperatura wody w zbiorniku 100 l, a trociny to woda w płaszczu przystawki. `solarMetricsCard` 12633 opisuje dzienną zmianę z `record.actual` jako „Δ t_bojler dziś”, choć ten rekord dotyczy t_panel. To błąd znaczenia etykiet, nie dowód zmiany kanału zapisującego archiwum. Poprawić później zgodnie z mapą czujników; nie zgadywać typu czujnika ani medium.

### Uzupełnienia wcześniejszych ustaleń i ograniczenia
- **#39:** dodatkowe wejścia HTML: komunikat `showToast` 11601, logi `renderLogs` 13555 oraz notatki wydania w 13664. Test nieszkodliwym znacznikiem potwierdza zachowanie HTML w toście, nie wykonanie ataku w przeglądarce. Terminal właściwie koduje tekst przez `terminalEscape`; nie należy uogólniać braku kodowania na cały panel.
- **#38:** `tg_config` trafia też w całości do lokalnego `addLog`/toast poprzez `sendCommand` (11610, 11630 i 13086), a kopiowanie logów obejmuje pełny tekst. Nie kopiowano prawdziwych sekretów do testów.
- Walidacja `applyIncomingData` odrzuca niepoprawne typy, ułamkowe pozycje serwa i brakujące pola; symulacje odbiera przed wartościami. Jednocześnie sztywne zakresy (np. piec 10–99°C, zbiornik solarny 0–140°C) mogą ukryć prawdziwy odczyt poza tym zakresem. To pokrewne #8, ale dotyczy prezentacji: nie jest testem sterowania wyjściami. Filtr wykresu również używa takich granic.
- Panel WiFi ma jeszcze stały MAC, zastępcze −65 dBm i napis „POŁĄCZONO” niezależny od wyniku zapytania (12699–12723); skan oznacza `hasPass` na podstawie `known`, nie szyfrowania (12807). Uwagi do wiarygodności prezentacji, bez osobnych numerów.
- Żądania logowania/odświeżania tokenu i wiele lokalnych fetch nie korzystają z timeoutu. W `fetchWithTimeout` timer kończy się po odebraniu nagłówków, przed odczytem całego JSON. Nie zmierzono zawieszenia przeglądarki; do integracyjnej kontroli Z2/Z3.
- Terminal ma ograniczenie 3500 wyświetlanych linii i kodowanie tekstu. `paused` w `terminalRender` blokuje przewijanie, nie samą wymianę treści. Odbiór sesji, deduplikacja oraz faktyczne auto-OFF wymagają dalszej kontroli wywołań; sam licznik `autoOffAt` nie wyłącza centrali.
- Parsery lokalnej historii oznaczają `sim_available=false`; brak informacji o symulacji nie jest dowodem, że próbka była rzeczywista. Rysowanie i oznaczenia kontynuujemy od 15001. Nie zamknięto oceny całego silnika wykresów.

### Wynik każdego odcinka
| Odcinek | Linie | Wynik |
|---|---|---|
| P041 | 10001–10500 | Formularz logowania, ilustracje SVG i ich klucze. Ilustracja nie jest źródłem telemetrii; kosmetyczny trop niepełnego klucza odświeżania pozostawiono bez nowego numeru. |
| P042 | 10501–11000 | Stan S, braki danych, kafelki, alarmy, symulacja i warunki rozpalania. Powiązanie z #82; brak alarmu nie jest automatycznie zakładany przy brakujących flagach. |
| P043 | 11001–11500 | Trend, magazyny przeglądarki, REST, odczyt i walidacja statusu, logowanie. Nowe #82–84; dalszy ciąg logowania w P044. |
| P044 | 11501–12000 | DEMO, auto-login, potwierdzenie komend, sterowanie, kontrolki i symulacja. Zgodny cmdId i bool ACK wymagane; znany wspólny slot poleceń #36 pozostaje problemem. |
| P045 | 12001–12500 | Symulacje, lista i opis czujników, pompa i początek serw. Ostrzeżenie o realnym działaniu symulacji jest obecne. #87 i granice walidacji. |
| P046 | 12501–13000 | Serwa, RTC, karta solarna, WiFi i początek Telegrama. #87, rozszerzenie #39; lokalny IP nie zapewnia zdalnego dostępu przez LTE. |
| P047 | 13001–13500 | Telegram, terminal, filtry i sterowanie DLOG, początek logów. #38; odróżniono potwierdzenie przyjęcia komendy od wykonania testu. |
| P048 | 13501–14000 | Logi, wydania/OTA, sesja, nawigacja, preferencje wykresu. #85 i #39; brak fikcyjnej deklaracji ukończenia OTA w procedurze przycisku. |
| P049 | 14001–14500 | Wykrywanie lokalnego ESP, parsery CSV, historia RTDB i naprawa dawnych czasów. #86; naprawa DST nie zgaduje niejednoznacznej godziny. Pełna walidacja migracji pozostaje w Z3. |
| P050 | 14501–15000 | Pobieranie/łączenie historii, jakość i symulacje, filtr, stan wykresu i początek rysowania. Oś i wielojednostkowe serie wymagają dalszego ciągu w P051. |

### Testy i aktualny postęp
`node narzedzia/audyt-v3.55.2/sprawdz_panel_www.cjs`: **kod 0**, sześć scenariuszy rzeczywistych fragmentów JS z atrapami sieci/DOM/storage. Odtworzone zachowania #82–86 oraz wejście HTML #39; #87 wynika z analizy etykiet. To nie test przeglądarkowy, sieci Firebase ani sprzętu. Nie ponawiano pełnego zestawu poprzednich regresji i nie kompilowano firmware.

**30568/46810 linii (~65%), 50/88 odcinków, 87 ustaleń.** Firmware bez zmian, bez OTA i publikacji. Z1–Z4 nadal otwarte. **Następna paczka: P051–P060, 15001–20000 (5000 linii).**

## Paczka P051–P060 — 5000 linii, zamknięta 24.10.2026

### 88. Dwie osie nie obsługują dowolnej liczby różnych jednostek — średni, odtworzony
Wybór osi 14978–15000 i skalowanie 15046–15140 / 15235–15246 wybiera tylko jedną grupę pomocniczą. Można jednak włączyć temperaturę, wilgotność i ciśnienie jednocześnie. Wówczas wilgotność trafia na prawą oś, a ciśnienie jest przeliczane skalą temperatury; przycinanie Canvas ukrywa linię poza obszarem. Test rzeczywistego `updateChartCanvas`, z atrapą kontekstu rysowania, dla 50–60°C, 50–60% i 1000–1010 hPa potwierdza współrzędne ciśnienia ponad górną granicą wykresu. To nie brak próbek w bazie. Później ograniczyć zestawy jednostek, dodać osobne wykresy albo jawne osie dla każdej jednostki. W NORMALIZED kolejność warunków nadal daje pierwszeństwo osi bocznej nad normalizacją — osobna uwaga do tej samej logiki.

### 89. Pompa nie jest domyślnie rysowana schodkowo — niski/średni, odtworzony
15278–15289: `s.group === 'pump'` wybiera gałąź bez krzywej Bezier, lecz dodatkowy punkt poziomy jest dodawany wyłącznie przy `chartLineStyle === 'STEPPED'`. Domyślny SMOOTH łączy STOP i PRACA przekątną, mimo opisu „rysowany schodkowo” w analizie. Test rzeczywistej funkcji na dwóch próbkach 0/100 wykazuje dwa punkty o różnym x i y zamiast schodka. Nie zmienia stanu fizycznej pompy; może sugerować stopniową zmianę zamiast przełączenia.

### 90. „Analiza PRO” miesza symulacje i brak danych z rzeczywistymi statystykami — średni, odtworzony
15923–15967: statystyki używają wszystkich skończonych wartości z `chartDataset`, bez sprawdzenia maski symulacji i bez oznaczenia jej udziału. Różnica A−B używa `(S[ch] || 0)`, więc dwa brakujące odczyty NaN dają 0°C. Sekcja „Wykryte cykle” jest stałym tekstem, nie detekcją cykli, a próg 60°C/histereza 10°C są wpisane na stałe, nie odczytane z ustawień. Test rzeczywistego budowania arkusza: prawdziwe 20°C + symulowane 100°C → MAX 100°C; brak A i B → aktualna różnica 0°C. Później oddzielić statystyki rzeczywiste i symulowane, zachować brak danych i nie nazywać opisu wykrytą analizą. Obecne statystyki liczą cały pobrany zakres, nie tylko widoczne okno powiększenia; średnia nie jest ważona czasem.

### 91. Panel archiwum zachowuje skasowany rekord w pamięci — średni, odtworzony
`SolarDailyArchive.refreshDays` 16402–16428 tylko dopisuje do `records`. Gdy kolejny udany odczyt nie zawiera poprzedniego dnia, stary rekord nie znika; `currentEntry` nadal wskazuje go przez `records.get(today)`. Test: odpowiedź z dzisiejszym rekordem, potem pusta lista → w pamięci nadal jeden rekord i ten sam `currentEntry`. Po `/archiwum_usun` panel może więc nadal pokazywać/eksportować starą kopię, aż do przeładowania strony. Nie jest to odtworzenie danych w centrali ani utrata pliku. Trzeba rozróżnić pustą odpowiedź od błędu transportu oraz usuwać cache w granicach potwierdzonego zakresu odpowiedzi.
Dodatkowa luka walidacji w 16445–16468: `Number(null)` daje 0. Test odpowiedzi z jawnym null dla delta/energii pokazuje +0.0 zamiast braku danych. **Bieżący zapis centrali przy braku pomiarów pomija te pola** (23159–23210), więc nie twierdzimy, że każda pusta doba już wyświetla zero. Panel nie sprawdza też `schemaVersion` ani `actual.channel` przed prezentacją; starego rekordu bojlera CO nie wolno traktować jako solarnego (#87).

**Druga runda R13:** R13 uruchamia obie metody: dwa zdalne błędy dają pustą listę i komunikat braku dnia przy zachowanej kopii. Po udanym zdalnym pobraniu poprzedni lastFirebaseError nadal pozostaje. Lokalne HTTP 503 jest rozpoznawane osobno. Nie wolno czyścić całej Map po błędzie albo odpowiedzi obejmującej tylko dziś/wczoraj. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md).

### 92. Pogoda może być zmyślona bez wyraźnego oznaczenia DEMO — średni, odtworzony
`refreshWeatherTab` 16731–16756 przy starcie/wymuszonym odświeżeniu natychmiast podstawia `generateWeatherData`, nawet poza DEMO. Po błędzie zapytania generator pozostaje; `renderWeatherTab` 16907–17008 pokazuje te wartości jak zwykłą pogodę, bez etykiety źródła. Samo `weatherDataIsReal=false` chroni część kart GHI, lecz nie główne temperatury, opad i wiatr. Przycisk odświeżania 17219–17223 ogłasza sukces bez oczekiwania na wynik obietnicy.
Parser 16645–16722 ponadto uzupełnia brakujące pola np. 15°C, wilgotność 60%, UV=3, ciśnienie 1013; wynik będzie uznany za rzeczywisty. `wind_direction_10m || 180` zamienia prawidłowe 0° (północ) na 180° (południe). Test funkcji z niepełną odpowiedzią odtwarza te wartości i kierunek S; osobny test odświeżenia potwierdza pozostawienie generatora po błędzie. Nie wykonywano zapytania do Open-Meteo. Później pokazywać „brak danych”, przechowywać czas ostatniej poprawnej prognozy i oznaczać dane przykładowe niezależnie od źródła telemetrii pieca.

**Druga runda R12:** R12 nie ponawia testu JS; potwierdza konieczność zachowania jakości danych także dalej. Odczytany mostek Kotlin GTI→GHI→0 nie przenosi informacji o zmianie źródła do klasy Hour. Nie uruchamiano tego modelu ani APK. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R12_POGODA_SOLAR.md).

### 93. „Astronomia” wyświetla umowną elewację i zgaduje kolejny wschód — średni, odtworzony
`updateSunArcAndSolarMultiDay` 16800–16883: elewacja to `round(sin(p*PI)*48)`, bez szerokości geograficznej i deklinacji. W środku odcinka wschód–zachód zawsze daje **48°**, także zimą. Test dwóch dat na rzeczywistej funkcji potwierdza identyczne 48°. Przy szerokości 51.066389° zimowe maksimum jest w przybliżeniu 90−51.066389−23.44 ≈ **15.5°**, nie 48° (przybliżenie geometryczne, nie obliczenie efemerydy dla konkretnej chwili).
Po zachodzie kod dodaje 24 h do dzisiejszego wschodu, zamiast użyć prognozy `daily[1]`; test celowo różniącego się jutrzejszego wschodu daje 13h00 zamiast 13h02. Samo przejście DST nie powoduje automatycznie błędu o godzinę przy epoch — błędem jest ignorowanie rzeczywistego następnego wschodu, używanie stale `daily[0]` i formatu godzin strefy przeglądarki. Przy brakach pojawiają się stałe 06:42/18:15. Łuk może być dekoracyjny, ale podpis „nad horyzontem” wymaga prawdziwych obliczeń albo jasnej etykiety przybliżenia; południe słoneczne nie jest zenitem w miejscowości Trzemcha Górna.

**Druga runda R12:** R12: geometria płaszczyzny i korekta promieniowania nie zmieniają dekoracyjnego łuku w efemerydę. Czytany serwer Open-Meteo używa geometrii Słońca i założeń izotropowego nieba/albedo; pięć prób funkcji pvlib dotyczy skalarnej geometrii, nie rzeczywistej wysokości Słońca w konkretnej chwili. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R12_POGODA_SOLAR.md).

### 94. Aktywny panel ma terminal DLOG bez podłączonego odbioru — średni, potwierdzony statycznie
W kompletnym aktywnym skrypcie 100xx–17557 `terminalAppendChunk` (13224) jest wyłącznie definiowane — nie ma wywołania. Jedyne dopisywanie do `rawRendered` jest wewnątrz tej funkcji; nie ma połączenia WebSocket ani odczytu ścieżki diagnostics. `terminalRemoteOn` włącza zdalne logowanie komendą i pokazuje oczekiwanie, ale panel nie odbiera treści. `init` i jego timery 17388–17549 odświeżają status, pogodę i historię, nie DLOG. Kontrola całego aktywnego raw stringu potwierdza brak połączenia; nie jest to próba na centrali. Backend może nadal wysyłać diagnostykę do Firebase, a Android może ją czytać — ustalenie dotyczy wyłącznie tego panelu WWW. Stary interfejs zawiera osobny terminal WS, ale GET `/` go obecnie nie serwuje.

### Rozróżnienie aktywnego i starego interfejsu
Punktowo sprawdzono trasę **28235–28261**: GET `/` zwraca `WIFI_LOCAL_PANEL_HTML`. `wifiZbudujStrone()` występuje jako definicja (27219), bez wywołania w bieżącym źródle. `WIFI_STRONA_HTML_HEADER` i `FOOTER` od 17564 to zachowany stary interfejs. Jego odczyt wlicza się do audytu całego pliku, **nie jest dowodem, że jego błędy występują na obecnej stronie głównej**.
- W starym interfejsie: brakujące flagi stają się WYŁ./AUTO, liczby w CSV `parseFloat(...)||0` zmieniają brak na zero, `symWylacz` ignoruje `d.ok`, niektóre ustawienia są zaznaczane przed potwierdzeniem. Brak nowych numerów dla tej niepodłączonej ścieżki.
- #39: stare SSID oraz nazwy plików w `loadFsList` trafiają do HTML/atrybutów bez kodowania. Odczyt zawartości pliku przez `fsViewFile` używa bezpieczniejszego `textContent`. Wzmacnia to potrzebę poprawiania konkretnych miejsc, nie globalnej tezy „wszystko jest HTML”.
- #87: aktywna karta nadal mówi o modelu Δt_bojler i braku odczytów t_bojler, choć rekord jest t_panel; eksport dzienny ma już prawidłowe nazwy kolumn t_panel. Nie eksportuje min/max ani minUtc/maxUtc, mimo że centrala je zapisuje. Stare pomocniki `solarSummarizeTelemetryForDay`/`solarBuildDayComparison` nie mają aktywnych wywołań; nie są obecnym autonomicznym archiwum.
- Aktywne `SolarAnalytics.recordSample` nadal zapisuje dodatkowe lokalne próbki w przeglądarce i wskaźnik mieszający wzrost t_panel/t_bojler; to inny zbiór niż autonomiczne doby. W eksporcie nazwany „zmiana_wskaznika_C”, nie zmierzony uzysk. Świeżość próbek dziedziczy #82; lokalny zapis zachowuje ostatnie 500, podczas gdy RAM do 5000.
- `initLayoutMode` 15729 wywołuje `toast`, którego nie ma w aktywnym dokumencie (jest `showToast`; `toast` zdefiniowano dopiero w osobnym starym HTML). Kliknięcie przełącza układ, potem napotyka niezdefiniowaną funkcję. Niska waga; nie blokuje startowego `applyMode`.
- Gesty i rysowanie mają różne marginesy (38/14 wobec dynamicznych 40/56 i 14/38/56), co przesuwa inspektor. Suwak czasu nie normalizuje pozycji względem zakresu przesunięcia po zoomie. W trybie NORMALIZED zakres min/max jest liczony ponownie dla każdego punktu (koszt kwadratowy); brak pomiaru wydajności telefonu. Uwagi do późniejszych poprawek, nie awaria ESP.

### Wynik każdego odcinka
| Odcinek | Linie | Wynik |
|---|---|---|
| P051 | 15001–15500 | Skale i jednostki, segmenty, symulacje, inspektor, zoom i geometria. #88–89. Linie przy dużych lukach są rozdzielane; maska symulacji kreskuje odcinki. |
| P052 | 15501–16000 | Gesty, układ, narzędzia i analiza, początek czasu solarnego. #90 i uwagi o inspektorze/niezdefiniowanym toast. |
| P053 | 16001–16500 | Granice dnia, GHI, starsze pomocniki podsumowania, lokalna analityka, czytnik autonomicznego archiwum. #91; nie mylono starych helperów z agregacją centrali. |
| P054 | 16501–17000 | Eksport dobowy, lokalizacja, generator/odczyt pogody, łuk słońca, karty godzinowe i dobowe. #92–93; współrzędne poprawne dla Trzemchy Górnej. |
| P055 | 17001–17500 | Wykres pogody, gesty, scenariusze DEMO, inicjalizacja i pierwsza część timerów. #94 potwierdzone po doczytaniu końca skryptu w P056. |
| P056 | 17501–18000 | Koniec aktywnego JS, timery widoczności; początek starego HTML/CSS. Sprawdzono rzeczywistą trasę GET `/`; nie zrównano dwóch interfejsów. |
| P057 | 18001–18500 | Stary terminal CSS/nawigacja, WiFi/TG/OTA, SVG i kafelki. Uwagi bezpieczeństwa #39, ale droga nie jest obecnie serwowana. |
| P058 | 18501–19000 | Stare kafelki, sterowanie pompą/serwami, symulacje, początek statusu rozpalania. Rozdzielono optymistyczne UI od rzeczywistego wykonania. |
| P059 | 19001–19500 | Koniec statusu rozpalania, odczyt lokalnego statusu, CSV historii, początek inspektora SVG. Kodowanie etykiet i błędy brak→zero oceniono osobno. |
| P060 | 19501–20000 | Stare gesty i rysowanie, pogoda, lista/podgląd/usuwanie plików, początek uploadu. Dalszy ciąg uploadu w P061. |

### Testy i postęp
- `node narzedzia/audyt-v3.55.2/sprawdz_wykresy_pogode.cjs`: **kod 0**, 7 scenariuszy: sześć reprodukcji na rzeczywistym JS z atrapami oraz kontrola statyczna niepodłączonego terminala. To nie test pełnego DOM/Canvas w przeglądarce.
- Ponowiony `sprawdz_panel_www.cjs`: **kod 0**, sześć wcześniejszych scenariuszy. Nie ponawiano całego zestawu C++/ESP i nie wykonywano połączeń z urządzeniem.
- **35568/46810 linii (~76%), 60/88 odcinków, 94 ustalenia.** Kod firmware i jego SHA256 bez zmian. Bez OTA, push i publikacji.
- **Następna paczka: P061–P072, 5063 nowe linie**, od 20001 do 25636 z pominięciem zakresów już udokumentowanych w dzienniku (21600–21825, 23453–23571, 23909–24136). Z1–Z4 pozostają otwarte.

Kontrola końcowa paczki: aktywny skrypt wydobyty parserem HTML przechodzi `node --check`; numeracja 1–94, suma pokrycia i następna paczka 5063 linii potwierdzone. To kontrola składni i ewidencji, nie integracyjny test UI.


## Paczka P061–P072 — 5063 nowe linie, zamknięta 24.10.2026

### 95. Dwie jednoczesne zmiany progów pompy mogą złamać wymaganą różnicę temperatur — wysoki
`ustawParametrPieca` 22136–22155 blokuje oddzielnie odczyt przeciwnego progu i zapis nowego. Walidacja pomiędzy tymi sekcjami używa już tylko starej kopii. Przykład z rzeczywistych gałęzi funkcji: początek ON=60°C/OFF=40°C; polecenie ON=42 odczytuje OFF=40 i oddaje blokadę; drugie polecenie ustawia OFF=58, bo nadal widzi ON=60; pierwsze kończy zapis ON=42. **Obie operacje zwracają sukces, końcowe ON=42/OFF=58 łamie wymagane ON≥OFF+2.**
Drogi wejścia są odrębne: callback HTTP 29280–29300 oraz wykonawca komend 42662–42684. Nie ma wspólnej blokady całego sprawdzenia i zmiany. Test offline wymusza przeplot po pierwszym zwolnieniu blokady; nie jest testem harmonogramu ESP32 ani dowodem, że taki stan wystąpił na piecu. Kontrola z tymi samymi poleceniami wykonywanymi kolejno odrzuca drugi rozkaz. To osobne ustalenie, nie #48 dotyczące wyłączności testów serwisowych.
**Późniejsza poprawka:** pod jedną krótką blokadą sprawdzić parę i zmienić RAM; trwałość i kolejność zapisu NVS obsłużyć poza spinlockiem, bez dopuszczenia starszego zapisu nad nowszym. Wyniki `putUChar` są obecnie ignorowane — test z zerowym wynikiem NVS nadal przyjmuje zmianę; jest to uzupełnienie wcześniejszej uwagi o trwałości nastaw, nie nowe zdublowane ustalenie.

### 96. Próg zapisu ciśnienia ma niespójne jednostki — średni
24410 definiuje `HIST_PROG_CISNIENIE=0.05f` jako **bar**, natomiast `cisnienieOdczytane` jest w hPa (30609: odczyt BMP w Pa dzielony przez 100; dodatkowo korekta +18). Historia przekazuje tę liczbę bez konwersji i porównuje ją bezpośrednio z progiem (24643). **Rzeczywisty próg wynosi więc 0,05 hPa, nie deklarowane 0,05 bar = 50 hPa** — różnica tysiąckrotna. Test oryginalnego makra i stałej: 1000→1000,1 hPa już wyzwala warunek; 1000→1000,01 nie.
Skutek: niewielkie wahania mogą powodować zapis długiej historii przy każdej próbce minutowej, zamiast rzadkiego zapisu zdarzeń. Nie zmierzono faktycznej liczby zapisów ani zużycia flash. Jeśli intencją jest czułość 0,05 hPa, trzeba poprawić opis; jeśli bar — przeliczyć próg. Docelową czułość należy świadomie ustalić, a nie automatycznie podnosić do 50 hPa.

### 97. Brak pliku pierwszego raportu awarii zatrzymuje późniejsze raporty — średni/wysoki, warunkowy
`wyslijZalegleRaportyCrash` 21255–21264: nieznaleziony plik dla dodatniego `coredumpSeq` powoduje `break`, bez przesunięcia pozycji ani limitu ponowień. Po 60 sekundach wykonywana jest ta sama próba. **Nowszy raport tekstowy za tą pozycją nie zostanie wysłany**, mimo sprawnego Telegrama. Test całej rzeczywistej funkcji: brak pliku głowy, potem drugi raport bez pliku; dwie próby pozostawiają obie pozycje i zero nowych wysyłek tekstu. Przywrócenie pliku odblokowuje kolejkę.
To test warunku brakującego pliku, nie dowód, że zwykła retencja go usuwa: `coredumpSeqJestOczekiwanyNaWysylke` i `coredumpUsunNajstarszyNieoczekiwany` 25196–25263 chronią niewysłane sekwencje przed normalnym przycinaniem. Przyczynę ewentualnego braku (uszkodzenie pliku/partycji, ręczne usunięcie) trzeba badać osobno. Później umożliwić niezależne wysłanie tekstów, jawnie oznaczyć brak załącznika i zachować informację o niedostarczonym pliku zamiast blokowania całej kolejki.

### Ponowna ocena wcześniejszych ustaleń i ograniczenia
- **#33:** rzeczywiste `wifiWczytajSieci` + `wifiDodajSiec` przy `count=-1` kierują zapis na indeks −1. Atrapa tablicy przechwytuje indeks przed zapisem (test nie wykonuje nielegalnego dostępu do pamięci). Kontrola count=99 ogranicza liczbę do rzeczywistego maksimum 8. Nie dowodzi to uszkodzenia aktualnego NVS użytkownika.
- **#17/#18/#50:** pełny odczyt cyklu solarnego potwierdza brak przeglądania dawnych dni do synchronizacji, ignorowanie błędu finalnego zapisu oraz niepełne odtwarzanie diagnostyki. Nie dodano tych samych problemów ponownie. #49 nadal wymaga integracji z właściwą biblioteką Firebase — atrapa sukcesu nie rozstrzyga ACK.
- **#30/#31/#42:** potwierdzono obchodzenie niezdobytego mutexa przy czyszczeniu, zależność telemetrii od alokacji historii i brak kopii wszystkich rekordów podczas eksportu CSV. Krótka/długa historia CSV nie przechowuje masek jakości/symulacji telemetrii — nie traktować jej jako równoważnego źródła do filtrowania prawdziwych pomiarów.
- `solarArchiveScanLegacy(true, nullptr)` mogłoby dereferencjonować null, ale **obecne wywołania tego nie robią**: skan bez usuwania przekazuje nullptr, usuwanie przekazuje adres licznika (44281/44292). Nie wpisano tego jako osiągalnej awarii.
- Archiwizacja coredump sprawdza integralność źródła, każdy zapis i rozmiar `.part`, następnie rename; źródło kasuje dopiero po publikacji pliku. Alokator seq sprawdza wynik NVS i przepełnienie. Nie jest to jednak próba rzeczywistego zaniku zasilania ani dekodowanie zrzutu zgodnym ELF.
- Koniec **nieaktywnego** starego panelu: licznik `all` może zostać zwiększony dwukrotnie dla kategorii `all`, a rozbieżne limity danych/DOM psują ponowne kolorowanie. Bez osobnej numeracji i bez przypisywania tego aktywnej stronie. #94 pozostaje problemem aktywnego terminala.
- Do Z2: formalny kontrakt ringu SPSC przy testowych producentach (#48), granica uint32 indeksów modulo 1440 (odległa przy próbkowaniu minutowym; bez nowej reprodukcji), zachowanie błędów LEDC w RawServo. Nie przedstawiono tych tropów jako nowych odtworzonych awarii.

### Ewidencja odcinków
| Odcinek | Linie | Wynik |
|---|---|---|
| P061 | 20001–20500 | Koniec starego uploadu, terminal i filtry; błędy liczników starego UI oddzielone od aktywnej strony. |
| P062 | 20501–21000 | Zamknięcie starego skryptu, builder strony, typy telemetrii i ring, początek diagnostyki. #48: kontrakt właścicieli do Z2. |
| P063 | 21001–21500 | Checkpointy, raporty crash i zasilania, scheduler, początek RawServo. #54 oraz nowe #97. |
| P064 | 21501–21599 | Dalszy RawServo, LEDC i stan ruchu; drogi błędów sprzętowych pozostają do Z2. Następny fragment już wcześniej odczytany. |
| P065 | 21826–22325 | Końcówka deklaracji, nastawy i ich walidacja, początek czasu. #34/#45 i nowe #95; NVS bez kontroli wyniku. |
| P066 | 22326–22825 | Konwersja czasu, symulacje, WiFi, historia RAM, metadane telemetrii. #33/#35, granice czasów i jakości. |
| P067 | 22826–23325 | Kodowanie snapshotu, archiwum solarne, zapis i odtwarzanie. #18/#31/#50; właściwy kanał t_panel. |
| P068 | 23326–23452 | Koniec restore, synchronizacja Firebase i URL GHI. #15/#49; ograniczenia 2038 już w #47. |
| P069 | 23572–23908 | Zmiana doby, finalizacja, catch-up, endpoint, skan legacy i usuwanie dnia. #17/#18/#51, bez dublowania. |
| P070 | 24137–24636 | Pobranie próbki, czyszczenie i CSV, historia zdarzeniowa. #30/#31/#42 i nowe #96. |
| P071 | 24637–25136 | Dalszy eksport historii, deklaracje logów, parser nazw i alokacja numeru coredump, wyszukiwanie. #41/#42/#56. |
| P072 | 25137–25636 | Wyszukiwanie i retencja dumpów, archiwizacja `.part`, testcrash/replay guard, start kolejki WS. #97 nie jest winą normalnej retencji. |

### Testy i postęp
- Nowy `python3 narzedzia/audyt-v3.55.2/sprawdz_nastawy_diagnostyke.py`: **kod 0**, cztery grupy reprodukcji (#95, #33, #96, #97) i kontrole negatywne. Rzeczywiste gałęzie nastaw, funkcje WiFi i wysyłki raportów oraz predykat ciśnienia; NVS/blokady/tablica/kolejka/transport zastąpione atrapami. Przeplot deterministyczny, bez prawdziwych wątków i bez ESP.
- Pierwsza kompilacja zatrzymała się na dwóch ostrzeżeniach hostowego g++ o możliwym skróceniu nazw `ssid%d`/`pass%d` w oryginalnym kodzie. Test zostawia te ostrzeżenia widoczne, używa `-Wall -Wextra -Werror -Wno-error=format-truncation`. Nie zmieniono produkcyjnych buforów. W scenariuszu kontrolnym indeksy są 0–7.
- Ponowiony `sprawdz_naglowki.py`: **kod 0**, wcześniejsze reprodukcje DST/GHI/podziału wiadomości i kontrole profili buzzera. Nie jest to pełna regresja ani test fizycznej centrali.
- **40631/46810 linii (~87%), 72/88 odcinków, 97 ustaleń.** Przyrost 5063, bez ponownego doliczania 21600–21825, 23453–23571, 23909–24136. Wcześniejsze podsumowania są historyczne.
- **Następna paczka P073–P083: 5190 nowych linii**, zgodnie z PLAN_PRACY. Po niej pozostaje 989 linii odcinków P084–P088 oraz zadania Z1–Z4. Odczyt wszystkich linii nie zastępuje tych czterech zamknięć. Bez zmian firmware i OTA.

Kontrola końcowa P072: SHA256 źródła zgodne z bazą, numeracja 1–97 bez luk, 72 odcinki i suma 40631 potwierdzone skryptem. Kolejne paczki: 5190 i 989 linii. `py_compile`, `git diff --check` i kontrola niezmienionego firmware zakończone kodem 0.


## Paczka P073–P083 — 5190 nowych linii, zamknięta 24.10.2026

### 98. Przycinanie logów nie chroni całej operacji przed dopisaniem i pomija cache nastaw — wysoki
`logTrimKeepTail` 25940–26001 otwiera źródło i kopiuje jego ogon **bez logMutex**. Blokadę bierze dopiero przed usunięciem oryginału i rename (25976). W tym czasie inny kontekst może prawidłowo dopisać nowy wpis do źródła — w szczególności `zapiszLogKrytyczny` po zwolnieniu blokady wywołuje trim (26644), a log krytyczny jest używany z więcej niż jednego zadania. Ten nowy wpis nie znajdzie się w przygotowanym pliku tymczasowym i zostanie usunięty wraz z oryginałem.
**Reprodukcja rzeczywistej funkcji:** źródło `OLD\nTAIL\n`, zachowujemy 5 bajtów. Atrapa dopisuje `NEW\n` dokładnie przed uzyskaniem blokady podmiany. Funkcja zwraca true, pozostaje wyłącznie `TAIL\n`. To wymuszony przeplot, nie dowód utraty konkretnego wpisu na centrali. Dwa równoległe trimy tej samej ścieżki dodatkowo używają wspólnego pliku tymczasowego.
Druga niespójność w tej funkcji: aktualizacja cache obejmuje tylko log A/B/krytyczny (25985–25987), choć funkcję wywołuje także dziennik nastaw (26679). Test: plik nastaw po trim ma 5 bajtów, `g_logUstawienSize` nadal 9. Późniejszy udany append ponownie odczyta rozmiar; do tego czasu cache jest nieaktualny.
**Później:** jeden właściciel/przemyślany protokół operacji na danym pliku, obejmujący kopiowanie i podmianę; nie dokładać długiego spinlocka ani długiego oczekiwania w pętli sterującej. Uzupełnić aktualizację wszystkich cache. Osobna, znana wada #18 pozostaje: błąd rename po udanym remove usuwa nazwę oryginału, chociaż ogon nadal może istnieć w `.tmp`. Test to potwierdza; nie utożsamia tego z natychmiastową utratą wszystkich bajtów.

### 99. Niepełny zapis zwykłego logu kasuje również niezapisany ogon — średni/wysoki
`logFlushDoFS` 25835 sprawdza liczbę zapisanych bajtów i zgłasza błąd, lecz 25863 **bezwarunkowo czyści cały bufor**. Przy dwóch liniach `abc\ndef\n` i zapisie pierwszych czterech bajtów na dysku zostaje `abc\n`, bufor RAM jest pusty, kolejny flush nie ponawia `def\n`. Odtworzone na całej rzeczywistej funkcji; kontrola pełnego zapisu działa. Guard małego wolnego bloku sterty również jawnie kasuje bufor (25810–25814).
To analogiczna przyczyna jak #28 dla historii, ale oddzielna produkcyjna ścieżka zwykłych logów. Nie wolno obiecywać bezstratnej diagnostyki; PSRAM ring może chwilowo zawierać kopię, lecz restart lub nadpisanie ją usuwa. Log krytyczny i dziennik nastaw z kolei nie sprawdzają wyniku `f.print(linia)` — brak osobnego potwierdzenia trwałości. Później kontrolowane ponowienia kompletnego ogona i jawne liczniki strat, z limitem pamięci (#41).

### 100. Po błędzie DHT wilgotność cofa się o jeden poprawny pomiar — średni
W `wilgotnosc()` 30685–30689, przy udanym odczycie `poprzedniaWilgotnosc` dostaje **poprzednią** `wilgotnoscSkorygowana`, a nie nowy poprawny pomiar. Przy błędzie ten opóźniony stan trafia do `wilgotnoscOdczytana` (30696).
Test rzeczywistej funkcji i obu przeliczeń: DHT zwraca kolejno 40%, 60%, błąd. Globalna wilgotność przyjmuje **40%, 60%, 40%**, zamiast utrzymać ostatnie dobre 60%. W teście atrapa `telemetryMarkInvalid` potwierdza zgłoszenie błędu; **nie jest to test końcowego `telemetryQuality`**. W produkcji świeżość zależy dodatkowo od czasu ostatniej akceptacji i może jeszcze przez krótki okres pozostać GOOD. Nie należy utożsamiać wywołania MarkInvalid z automatycznym unieważnieniem całego statusu.
Później rozdzielić „ostatni poprawny odczyt” od wartości poprzedniej używanej do strzałki trendu. Dodatkowo oba przeliczenia przy tej samej temperaturze są algebraicznie odwrotne — obecna „korekta” wraca praktycznie do wejściowego RH. Nie wprowadza korekty pomiędzy temperaturą DHT a DS18B20, bo temperatura DHT jest pomijana. Nie zmieniano wzoru ani kalibracji czujnika.

### 101. Przerwanie ruchu serwa może pozostawić jego zasilanie włączone — średni/wysoki
`zatrzymajRuchSerwa` 30856–30862 ustawia `aktywny=false`, ale nie zmienia GPIO. `serwoTick` 30958–30968 wyłącza wspólne zasilanie **tylko jeśli na początku tego konkretnego tiku był aktywny ruch** i w tym tiku obydwa ruchy się zakończyły. Jeśli zatrzymanie nastąpi między tikami, warunek „aktywne przed” już jest false — LOW nie zostanie wykonane.
Test rzeczywistych `zatrzymajRuchSerwa`, `serwoTickJeden`, `serwoTick`: pierwszy krok włącza GPIO, następnie anulowanie i dwa następne tiki pozostawiają HIGH mimo braku ruchu. Kontrola zwykłego dojścia do celu wyłącza zasilanie. Anulowanie jest osiągalne m.in. przez `zakonczTrybRozpalania` (30980–31002) oraz zamknięcie okna serw po przekroczeniu 30°C. W całym main inne jawne LOW tego pinu jest przy inicjalizacji.
Test mierzy **stan programowego wyjścia**, nie pobór prądu ani temperaturę rzeczywistego serwa. Nie dowodzi, że siłownik osiągnął wskazany kąt. Później wyznaczać zasilanie również dla stanu „obydwa bezczynne po anulowaniu”, z uwzględnieniem wymaganego czasu mechanicznego i ochron alarmowych. To niezależne od #13, gdzie zlecenie bieżącej pozycji nie odwołuje starego celu ruchu.

**Druga runda R15:** R15 ponawia łańcuch zlecServoRuch→serwoTick→zatrzymajRuchSerwa→serwoTick: po anulowaniu wszystkich ruchów HIGH pozostaje. Dodatnia kontrola zwykłego dojścia do celu daje LOW. NVS/serwo/GPIO są atrapami. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### Potwierdzenia i ograniczenia bez nowych numerów
- **#97:** `/api/fs-delete` 28693–28723 może usunąć dowolną dopuszczoną ścieżkę, także niewysłany `/coredumps/...`; nie pyta `coredumpSeqJestOczekiwanyNaWysylke`. To osiągalny ręczny sposób wywołania braku pliku raportu, w przeciwieństwie do chroniącej go normalnej retencji. Nie wykonywano usuwania na urządzeniu.
- **#19/#20/#21/#22/#27/#53/#55:** pełny backend potwierdza niespójną autoryzację, parsowanie fragmentów body jak pełnych JSON-ów, niekodowane SSID, wspólny stan uploadów, blokujące wysyłki w callbackach i pobłażliwe parsowanie liczb. Upload sprawdza zapas miejsca tylko przed początkiem; nie jest to rezerwacja rozmiaru całego pliku. Lista nazw systemowych pomija m.in. log nastaw; nazwy plików w `/api/fs-list` też nie są kodowane do JSON.
- RTC przez `/api/rtc-sync` (28489–28500) kontroluje dzień 1–31, ale nie liczbę dni miesiąca/rok przestępny: np. 31 lutego przechodzi warunek. Ten GET zmienia zegar bez tokenu, przed pierwszą synchronizacją NTP; aktualizacja I2C wykonywana w callbacku. Uzupełnienie #19/#55 i Z2, bez testu RTC na sprzęcie.
- **#52:** `logPobierzTekst` czyta ring i jego przesuwaną głowę bez blokady; nie daje spójnej kopii przy dopisywaniu. `logSeq++` w 25696 też nie jest chronione wspólną blokadą, więc nie gwarantuje unikalnej sekwencji pomiędzy zadaniami. Zwykły zapis pomija trwałość, gdy zabraknie PSRAM (`!logBufor`) albo nie uda się natychmiast przejąć mutexa.
- **Diagnostyka zdalna:** `diagRemotePeekBatch` dopisuje NUL pod indeksem równym `outCap`. Obecny jedyny caller przekazuje limit 3600 do bufora zaalokowanego na 3601 bajtów (26415–26416), więc **nie wpisano fałszywego przepełnienia obecnej ścieżki**. Kontrakt helpera jest mylący. Numer sesji pochodzi z nietrwałego licznika boot, a numer chunka zeruje się — do Z2 ryzyko ponownego użycia ścieżki po utracie zawartości RTC. #15/#49 nadal dotyczą transportu i ACK.
- Automatyczne wyłączenie zdalnej diagnostyki używa różnicy signed i działa przy typowym zawinięciu; termin równy dokładnie zero koliduje z sentinelem. Nie przyznano nowego wysokiego priorytetu tak wąskiemu przypadkowi. Serwis jest zależny od powrotu swojego zadania, nie jest niezależnym zegarem sprzętowym.
- LAN zachowuje slot zajęty podczas wykonania (43694–43726), więc nie nadpisuje komendy w tym czasie. Pamięta jednak tylko ostatni ACK w RAM — nie jest trwałą ani wieloelementową deduplikacją. Sprawdzono całą funkcję wykonawcy punktowo; nie doliczono powrotu do pokrycia.
- **#8/#9/#10:** ponownie sprawdzono filtr temperatur, wspólny licznik błędów DS i power-cycle. Retry odczytu dotyczy dostępności; sam błędny wynik temperatury nie powoduje ponownego odczytu, bo `odczytUdany=true` następuje przed walidacją. Gdy wszystkie `available()` są false, `Czujnik_DS` omija funkcje odczytu i ustawia temperatury na −1; wymaga domknięcia kontraktu konkretnej biblioteki czujników w Z2. Nie dowiedziono zachowania wszystkich magistral na sprzęcie.
- **#3/#13/#32/#54:** terminy override/serwa, przestarzały cel ruchu, późny start GPIO oraz pomiary diagnostyczne w pętli nadal obecne. Opisy „nieblokujące” nie zastępują pomiaru czasu ArduinoOTA, WiFi, FS czy I2C. Właściciele GPIO i wspólnych stanów pozostają tematem Z2.

### Ewidencja odcinków
| Odcinek | Linie | Wynik |
|---|---|---|
| P073 | 25637–26136 | Kolejka WS, zapis/odtwarzanie logów, flush i trim, początek quota. Nowe #98/#99; #18/#41/#56. |
| P074 | 26137–26636 | Rotacja logów, zdalna diagnostyka, polityka i auto-off; zapis logu krytycznego. Sprawdzone granice bufora, #15/#49 i ograniczenia trwałości. |
| P075 | 26637–26826 | Koniec logu krytycznego, nastawy/baseline, wygaśnięcie symulacji i ograniczanie powtórzeń logów. #98, #3. SD od 26827 wcześniej odczytane. |
| P076 | 27129–27628 | Czyszczenie/odczyt logów, token i nieużywany builder starego HTML, kafelki i nastawy. #52; nie przypisano starego UI do GET `/`. |
| P077 | 27629–28128 | Reszta starego HTML, JSON statusu, początek LAN. #21/#65, brak spójnej migawki wszystkich pól i ograniczenia świeżości. |
| P078 | 28129–28628 | Przyjmowanie LAN/ACK, rejestracja aktywnej strony, status/pogoda/solar, RTC, historia i logi. #19/#53/#55/#58. |
| P079 | 28629–29128 | Pliki/download/delete/upload, WiFi JSON/skan/konfiguracja, początek OTA WWW. #20/#21/#22/#27; uzupełnienie #97. |
| P080 | 29129–29628 | OTA, Telegram, ustawienia, ręczne wyjścia i symulacje, start AP. #7/#19/#20/#43/#55/#95. |
| P081 | 29629–30128 | Łączenie STA, ArduinoOTA, NTP, setup WiFi i wifiLoop. #3/#26/#27/#32/#54; nieczynna stara funkcja łączenia oddzielona od krokowej. |
| P082 | 30129–30628 | DS power-cycle, filtr i odczyt 7 temperatur, ciśnienie oraz wzory wilgotności. #8/#9/#10/#96 i zależności biblioteki do Z2. |
| P083 | 30629–31128 | Wilgotność, mieszadło, override, ruch serw i rozpalanie. Nowe #100/#101; #3/#13. Koniec funkcji rozpalania doczytany punktowo do 31147. |

### Testy i postęp
- **Nowy `python3 narzedzia/audyt-v3.55.2/sprawdz_logi_czujniki.py`: kod 0**, g++17 `-Wall -Wextra -Werror`, bez ostrzeżeń. Cztery grupy nowych ustaleń, w #98 osobne kontrole współbieżnego dopisania, cache, krótkiego zapisu i rename; ponadto pełny flush, błąd DHT i zwykłe zakończenie ruchu. Prawdziwe funkcje wycięte z main; FS, blokady, DHT, czas i GPIO to atrapy. Nie jest to test biblioteki LittleFS ani fizycznego ruchu serwa.
- Ponowione `sprawdz_nastawy_diagnostyke.py` oraz `odtworz_bledy.py`: **kod 0**; pierwszy zachowuje dwa znane ostrzeżenia hostowe format-truncation. Drugi potwierdza wcześniejsze 9 przypadków/przesłanek oraz kontrole solar/DST. Nie uruchamiano pełnej regresji wszystkich skryptów.
- **45821/46810 linii (~98%), 83/88 odcinków, 101 ustaleń.** Przyrost 5190 obejmuje 25637–26826 oraz 27129–31128; wcześniej odczytanego 26827–27128 nie dodano ponownie.
- **Następne: P084–P088, 989 linii i rozpoczęcie zamknięć Z1–Z4.** Samo osiągnięcie 100% odczytu main nie oznacza zakończenia audytu ani zgody na wgranie. Firmware bez zmian; brak OTA/publikacji.

Kontrola końcowa P083: SHA256 zgodne z bazą, numeracja 1–101 bez luk, 83 odcinki i suma 45821 potwierdzone skryptem; przyrost 5190, pozostałe odcinki 989. `py_compile`, `git diff --check` i kontrola niezmienionego firmware: kod 0.


## Paczka P084–P088 — zamknięcie odczytu main

### 102. Stabilizacja powrotu liczona od uruchomienia pompy, nie od początku stabilnej temperatury — średni/wysoki
Stała `czasStabilizacji=30000` w 21875; blok 32333–32359. `tempStabilna` nie przechowuje momentu rozpoczęcia stabilizacji. Gdy różnica ostatniej pary pomiarów spadnie poniżej 1°C, czas jest liczony od `czasStartPompy`, również po wcześniejszych niestabilnych odczytach. Test rzeczywistego bloku: próbki co 5 s, temperatury 2/4/6/8/10/10,5°C → pompa wyłączana przy 30 s, chociaż tylko ostatnia para (5 s) spełnia warunek. Kontrola: stabilna para przy 5 s nie wyłącza pompy. Nie jest to dowód niedogrzania instalacji; to dowód, że kod nie mierzy ciągłego okresu stabilizacji. Czy taki okres jest wymagany i jak długi — wymaga uzgodnienia semantyki tego trybu. Docelową długość i warunek stabilizacji uzgodnić przed zmianą.

**Druga runda R15:** Ponowiono istniejący runner końca sterowania: pierwsza stabilna para po 30 s pracy daje OFF. Nadal wymagana decyzja, czy chodzi o ciągłą stabilność, minimalny czas pracy czy inną regułę. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

### 103. Tryb BEZPIECZNA pozwala dokończyć poprzedni ruch w przeciwną stronę — wysoki
31199–31214: warunek `!ruchSyberka.aktywny` / `!ruchKlapy.aktywny` nie rozróżnia starego ruchu od ruchu już skierowanego do bezpiecznej pozycji. Po wybraniu TrybSerwa=3 aktywne otwieranie klapy do 180° i zamykanie syberka do 0° nadal trwają; bezpieczne cele 0°/90° zostaną zlecone dopiero po zakończeniu. Test wyciętej funkcji `servo_manual`: pozycje 60°/45°, aktywne przeciwne cele → zero nowych zleceń; po usunięciu aktywności → oba cele bezpieczne. Różni się od #13 (nowy cel równy aktualnej pozycji): tutaj nowego celu w ogóle nie zlecono. Test nie mierzy mechaniki ani czasu fizycznego ruchu. Należy odwoływać przeciwny stary ruch, bez ciągłego restartowania już poprawnego ruchu.

**Druga runda R15:** Ponowiono oryginalny servo_manual: istniejące ruchy pozostają, a przy braku ruchu zlecane są cele trybu. Nazwa BEZPIECZNA nie zastępuje potwierdzenia fizycznego stanu i jawnego odwołania starego celu. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R15_WYNIK_KONCOWY.md).

| Odcinek | Linie | Wynik |
|---|---|---|
| P084 | 31129–31436 | Rozpalanie, manual/AUTO, interlock, alarmy CO/panelu. Nowe #103; potwierdzenie #13/#14/#52/#101. Interlock jest tylko rejestratorem naruszeń, nie fizyczną blokadą. |
| P085 | 31600–31855 | Cała obsługa dymu. Bramka temperatury i terminy: #10/#3. Dym może zmieniać cele podczas CO; hierarchia alarmów do Z2, bez dublowania #14. |
| P086 | 32144–32376 | Antystop i trzy tryby pompy. Nowe #102, potwierdzenia #11/#12/#3. |
| P087 | 32566–32643 | DNS: publikacja wyniku release/acquire, dwóch właścicieli; worker zwalnia kontekst po zakończeniu. Timeout callera nie anuluje pracy DNS. Limit i liczba workerów zależą od biblioteki — Z2. Domknięcie klamry 32644 było wcześniej odczytane. |
| P088 | 32989–33102 | Dokument logu krytycznego: #40, #62/#63, snapshot rozmiaru poza blokadą. Nie dopisano drugiego numeru za znany brak kontroli bufora. |

### Testy po P088 i początek zamknięć
- `sprawdz_koniec_sterowania.py`: #102/#103 oraz dwie kontrole, kod 0; g++17 `-Wall -Wextra -Werror`. Prawdziwe fragmenty C++, atrapy czasu/sterowania; nie cały scheduler.
- `uruchom_regresje.py`: **29 poleceń, wszystkie kod 0**: 16 skryptów audytu, 3 oryginalne runnery hostowe v3.55.2 oraz kompilacja i uruchomienie 5 bazowych testów C++. Pełny zapis: `narzedzia/audyt-v3.55.2/WYNIKI_REGRESJI_P088.md`.
- Symulacja całych wiadomości: **3665 sprawdzeń, 0 błędów**; bazowy SolarDailyArchive: **339 sprawdzeń PASS**. To nie test transportu Telegram/Firebase. Kod 0 testu reprodukcji oznacza odtworzenie błędu, nie jego naprawę.
- Pierwszy przebieg runnera: 28 poleceń, jedna porażka kompilacji ReportSimulation z powodu pominiętego `-I` katalogu nagłówka. Poprawiono wyłącznie runner audytu; ponowiono cały zestaw z powyższym wynikiem. Cztery widoczne ostrzeżenia istniejących testów (2 unused, 2 format-truncation) pozostają; starsze runnery nie zawsze wypisują stderr poprawnej kompilacji.
- Z1: potwierdzono cztery lokalne nagłówki, ponownie przeczytano całe AutoIgnitionColdTimer i MieszadloTrocinyAuto oraz trzy runnery v3.55.2. Pozostały przegląd treści zestawów testowych i ich ograniczeń; samo ich uruchomienie nie zamyka Z1.
- **Main: 46810/46810, 88/88 odcinków. Łącznie 103 ustalenia.** Z1/Z2/Z4 pozostają otwarte; część wykonawcza Z3 przeprowadzona, końcowe zamknięcie po Z2. Firmware niezmieniony. Bez OTA i bez zgody na wgranie.


## Z1 — zakończony przegląd nagłówków i zakresu testów
Przeczytano osiem bazowych plików C++, **2763 linie**, w pełnych nieuciętych wynikach narzędzi. Cztery nagłówki logiki i trzy runnery były już odczytane. Zakres, dowody oraz ograniczenia każdego pliku: [Z1_ZAKRES_TESTOW.md](narzedzia/audyt-v3.55.2/Z1_ZAKRES_TESTOW.md). Nie jest to nowy odczyt 2763 linii main ani deklaracja testów sprzętowych.

### 104. Część asercji bazowych testów daje pozytywny wynik bez sprawdzenia deklarowanej właściwości — średni, jakość testów
- `HistoryStreamHostTest.cpp:113–118,192,280,312,329,397`: licznik `g_maxBajtowBezOddechu` jest tylko zerowany i porównywany, nigdy zwiększany. Próba na tej samej atrapie File: odczyt 8192 B bez delay → licznik nadal 0, test `<=4096` przechodzi. Nie dowodzi to błędnego rozmiaru porcji w firmware: produkcyjna porcja ma 4096 B. Pozostałe asercje liczby delay nie są bezwartościowe, ale nie stanowią pomiaru maksimum między oddaniami czasu.
- `SolarDailyArchiveTest.cpp:43–54`: CHECK_NEAR oblicza różnicę i odrzuca tylko `d > eps`. Przy NaN porównanie daje false. **Test mutacyjny:** oryginał 339 PASS; celowo błędny skończony wynik equivalent100lKwh → 1 FAIL; celowo wstrzyknięty NaN → nadal 339 PASS. Mutacje wykonano wyłącznie na kopii w katalogu tymczasowym. Nie twierdzimy, że niezmieniony przelicznik zwraca NaN dla poprawnej liczby.
- `ReportSimulation.cpp:498–505`: S14 nazywa 2027-03-29 dobą 23 h, lecz jest to doba 24 h. Ręczne expectedSamples=1380 sprawdza format, nie kalendarz. Właściwa data to 2027-03-28. Inne testy obejmują prawdziwe doby DST; S24 dla 29 marca obejmuje również 28 marca w 30-dniowym zakresie — nie negujemy jego sprawdzenia `/1380`.
- `NadzorHostTest.cpp:100–101`: opis sugeruje sprawdzenie tuż przed progiem, ale wiek wynosi 1 ms, nie 299999 ms. To luka scenariusza granicznego, nie dowód błędu progu w funkcji.
**Dowód:** `sprawdz_wiarygodnosc_testow.py`, końcowy kod 0; szczegóły w Z1. Osobne od #68, które dotyczy testów spool osadzonych w firmware. Przed poprawkami potrzebne asercje odrzucające NaN, rzeczywisty pomiar porcji i prawidłowe dane graniczne.

**Druga runda R14:** Ponowiono mutacje solaru: oryginał 339 PASS, błędna wartość skończona wykryta, NaN nadal 339 PASS. Nowe próby oryginalnego CHECK_NEAR pokazują też NaN oczekiwania i NaN tolerancji maskujący 100 vs 1. Ponowiono niewiarygodny licznik porcji i kontrolę daty S14; testu granicznego nadzorcy w R14 nie ponawiano. [Raport i próby](narzedzia/audyt-v3.55.2/research2/R14_WALIDACJA_TESTY.md).

### Dalsze ograniczenia wyników „OK”
- Test telemetrii ma własny AsyncResult, uproszczony JSON i ring; spill/replay zwracają stale false. Liczy jako wyprodukowane tylko próbki przyjęte do ringu. Dlatego wynik „zero utraconych” nie dowodzi braku strat przy przepełnieniu, restarcie ani błędnym rzeczywistym ACK (#49). Końcowy komentarz o identyczności paczki nie jest osobną asercją porównania bajtów.
- Próby timera, mieszadła i profili buzzera sprawdzają czystą logikę, nie GPIO/NVS/scheduler. Raporty 3665/339 nadal potwierdzają konkretny zestaw danych, nie dowolne dane z plików lub sieci.

### Z2 rozpoczęte — wspólne alarmy
Ponownie prześledzono alarmy, rozpalanie oraz rejestrację ich okresów. [Z2_POWIAZANIA.md](narzedzia/audyt-v3.55.2/Z2_POWIAZANIA.md) zawiera tabelę: CO zamyka syberek, dym może później otwierać go krokami, panel odwołuje ruchy. To rozszerzenie #14 i wymóg jawnej hierarchii, nie arbitralna zmiana pozycji bezpieczeństwa. Brak dowodu czasu reakcji na urządzeniu; same zadane okresy nie gwarantują latencji.

### Wynik paczki Z1
- Z1 **zamknięte**, Z2 **w toku**, Z3 **regresja wykonana, końcowa ocena po Z2**, Z4 **otwarte**.
- Nowa kontrola jakości testów i ponowienie całego zestawu: **30 poleceń, wszystkie kod 0**, łącznie z 3665 sprawdzeniami kompletnych wiadomości. Kontrola mutacyjna celowo oczekuje exit 1 od wariantu z błędną liczbą; wrapper potwierdza to kodem 0.
- Pierwsze próby nowego narzędzia ujawniły brak include cmath i TZ w samej próbie; uzupełniono tylko narzędzie audytu, potem uruchomiono je i pełną regresję ponownie. Szczegóły aktualnego przebiegu w `WYNIKI_REGRESJI_P088.md` (nazwa historyczna, zawartość rozszerzona po Z1).
- **104 ustalenia, main 46810/46810, 88/88 odcinków.** Firmware i bazowe testy bez zmian. Nie wykonano OTA. Następne: pozostała macierz Z2 (właściciele/blokady, Firebase ACK, zależności, trwałość i OTA), potem Z3/Z4.


## Z2 — zakończony przegląd powiązań; Z3 — regresje offline
Macierz właścicieli danych/wyjść, blokad, łańcuchów trwałości, OTA i ograniczeń bibliotek: [Z2_POWIAZANIA.md](narzedzia/audyt-v3.55.2/Z2_POWIAZANIA.md). Nie jest to formalny dowód bezpieczeństwa współbieżności ani test urządzenia.

### 105. Utrata pamięci RTC może spowodować nadpisanie starszej sesji diagnostyki w Firebase — średni, warunkowy
`logBootCounter` jest w RTC_NOINIT (20566), a `logInit` po niezgodnym magic zeruje go i zwiększa do 1 (25563–25567). `diagInit` przyjmuje ten licznik jako sesję i zeruje seq (26420–26421). `diagRemoteTrySendOneBatch` buduje ścieżkę z sesji i seq (26374–26377) oraz używa `set<object_t>` (26395), nie unikalnego append.

**Druga runda R13:** R13 ponawia rzeczywiste fragmenty z istniejącego runnera: zachowana RTC daje nową sesję, model utraty RTC powtarza ścieżkę sesji 1. To odrębne od zwykłej telemetrii sessionNonce-sampleSeq. Nie wykonywano zimnego startu urządzenia ani nadpisania Firebase. [Raport i testy](narzedzia/audyt-v3.55.2/research2/R13_SWIEZOSC_TOZSAMOSC.md).

**Warunki:** po utracie zawartości RTC (np. po odłączeniu zasilania) magic jest niepoprawne; ten sam UID urządzenia; zdalna diagnostyka włączona; poprzednie rekordy nadal istnieją i zapis jest dozwolony. Nowy start wraca do `diagnostics/1/chunks/0`, potem kolejnych wcześniej użytych nazw. Zapis SET pod tą samą nazwą może zastąpić stary log. To nie problem nazw plików coredump — ich oddzielny licznik nie zabezpiecza tej ścieżki.

**Dowód offline:** `sprawdz_sesje_diagnostyki.py` kompiluje rzeczywiste fragmenty inicjalizacji i budowy ścieżki. Pierwszy start oraz start z niepoprawnym magic dają identyczną ścieżkę `/piec/devices/test-device-uid/telemetry/diagnostics/1/chunks/0`. Kontrola resetu z zachowaną RTC daje sesję 2, bez kolizji. Kod 0, g++17 `-Wall -Wextra -Werror`. Nie wykonywano zaniku zasilania ani zapisu do Firebase — test potwierdza kolizję identyfikatorów, nie zmierzoną utratę danych użytkownika.

**Kierunek późniejszej poprawki:** trwały monotoniczny licznik sesji lub odpowiednio losowy identyfikator, z kontrolą błędów jego zapisu; nie używać samego licznika RTC jako globalnie unikalnej nazwy. Bez zmian kodu w audycie.

### Rozstrzygnięcia Z2 bez mnożenia numerów
- **#49 nadal ryzyko, nie odtworzona awaria.** Sprawdzono upstream FirebaseClient v2.2.9: zwykły object_t PATCH nie jest uploadem plikowym, a samo powstanie wewnętrznego debug nie oznacza natychmiastowego skopiowania go do wyzerowanego zewnętrznego AsyncResult. Publikacja normalnego payloadu następuje po zakończeniu etapu odpowiedzi. Pozostają niepotwierdzone ścieżki timeout/anulowanie/ponowne użycie wyniku i dokładna wersja builda. Źródła, numery linii i SHA256 w Z2. Nie zastępować analizy samym twierdzeniem „isResult zawiera debug, więc dane na pewno giną”.
- **#48 potwierdzone statycznie:** secH2Task używa tych samych obiektów Firebase co tgTask. Flaga H2 nie blokuje zwykłego fbApp.loop; fbLoopWorkerZajety zawsze false. Historyczne komentarze o odłączonym workerze nie odpowiadają obecnemu kodowi. Nie uruchamiano H2 na centrali.
- **#16/#17/#18/#97/#98/#99:** ponownie spięto ścieżki od producenta do zapisu/odtworzenia/usunięcia. Wspólny mutex w części operacji nie chroni całego cyklu pliku. Nie stwierdzono, że pozytywny wynik jednej funkcji dowodzi odporności na utratę zasilania.
- **#1/#15/#25/#70/#71:** OTA, timeouty i deklaracja rollbacku pozostają przeszkodami przed zdalnym wgraniem. Dwa sloty i wpis w sdkconfig.defaults nie potwierdzają rollbacku działającego bootloadera. Faktyczne wersje bibliotek nie wynikają jednoznacznie z archiwalnego platformio.ini (Firebase `^2.2.9`, DS18B20 bez commita).

### Testy i stan pracy po Z2/Z3
- Pełne ponowienie dostępnego zestawu: **31 poleceń, wszystkie kod 0**. W środku: nowa kolizja sesji, wcześniejsze reprodukcje i kontrole, 3 runnery hostowe oraz 5 bazowych testów nagłówków/raportów. Symulator pełnych wiadomości: 3665 sprawdzeń, 0 błędów. SolarDailyArchive: 339 PASS, z jawnie udokumentowaną słabością CHECK_NEAR.
- Wyniki w `narzedzia/audyt-v3.55.2/WYNIKI_REGRESJI_P088.md` (historyczna nazwa pliku, aktualny przebieg po Z2). Z3 zamknięte **tylko dla dostępnego zestawu offline**, nie dla niezrealizowanych testów urządzenia, sieci, flash, browsera i bootloadera.
- Z1/Z2/Z3 zamknięte w powyższym zakresie. **Z4 nadal otwarte:** końcowe uporządkowanie 105 ustaleń, duplikaty, priorytety i warunki przyszłych poprawek.
- Main nadal 46810/46810 linii, 88/88 odcinków; firmware niezmieniony. Brak OTA, kompilacji całego obrazu i publikacji wydania. Audyt nie uprawnia jeszcze do wgrania v3.55.2.


## Z4 — zamknięcie audytu źródeł, 24.10.2026
- Przejrzano treść i ograniczenia wszystkich 105 numerów. Nie dopisano nowych ustaleń; podobne mechanizmy zgrupowano w Z4 bez utraty odniesień. To rejestr ustaleń, nie 105 niezależnych odtworzonych awarii.
- Opublikowano w workspace Z4_WYNIK_KONCOWY.md: indeks każdego numeru, kolejność przyszłych poprawek, nierozstrzygnięte zależności i bramka przed jednym docelowym wgraniem.
- Ponowiony pełny zestaw: 31 poleceń kod 0; kompletne wiadomości 3665 sprawdzeń. Osobna kontrola ewidencji potwierdza 16 źródeł zgodnych z SHA/HEAD, 46810 linii bez luk i duplikatów oraz numery 1–105. Nie jest to test całego obrazu na ESP.
- **Wszystkie 92 zadania planu zakończone. Kod firmware niezmieniony. Brak OTA/publikacji wydania/push/merge w Z4.** Dalszym etapem może być uzgodnienie wariantu poprawek; nie ma zgody na automatyczne rozpoczęcie zmian.

## Kontrola końcowa aktualizacji researchu — 24.10.2026
- `kontrola_zamkniecia.py`: **PASS** — 16 źródeł zgodnych z SHA/HEAD, 46810 linii, 88 odcinków, Z1–Z4 zamknięte, numeracja raportu i indeksu 1–105. Odczytany log 31/0 pochodzi z poprzedniej regresji Z4; nie uruchamiano jej ponownie w tej rundzie.
- Osobna kontrola czterech zmienionych/nowych MD: **17 lokalnych odsyłaczy istnieje**, numeracja 1–105 bez luk, brak końcowych spacji. Potwierdzono 33 SHA256 pobranych źródeł i daty przypiętych commitów.
- Kontrola kalendarza offline przez Python zoneinfo/Europe/Warsaw: 29.03.2026=23 h, 25.10.2026=25 h, 28.03.2027=23 h, 29.03.2027=24 h. To kontrola danych raportu, nie zegara ESP ani odpowiedzi pogodowego API.
- `git diff --exit-code -- firmware-patch android-sync` i `git diff --check`: kod 0. SHA256 głównego firmware nadal `b6c3966e06ec38be155cef0e1980fade0e4399cf999380172cb20dd4a9e00760`.
- Wynik: aktualizacja analizy zakończona; **bez zmian firmware, OTA, kompilacji całego urządzenia i publikacji**. Niewiadome sprzętowe/integracyjne pozostają jawne; research nie jest zgodą na wgranie.
