// ============================================================================
// WYSWIETLACZ CO — panel dotykowy (ESP32 + 3,5" SPI TFT ILI9488 + XPT2046)
// ============================================================================
// Druga, ODDZIELNA plytka wobec centrali (projekt "Piec CO", main.cpp).
// Ta plytka NIE steruje niczym bezposrednio (brak serw, czujnikow, przekaznikow
// tutaj) — jest CIENKIM KLIENTEM HTTP centrali, po tej samej sieci WiFi:
//
//   GET  /api/status           -> co STATUS_INTERWAL_MS, dane live (temp., klapa,
//                                  syberka, pompa, alarmy) — patrz StanPieca nizej
//   GET  /api/ustawienia-get   -> co USTAWIENIA_INTERWAL_MS, wartosci parametrow
//   POST /api/ustawienia-set   -> gdy user dotknie +/- na ekranie Ustawienia
//                                  (pole=<nazwa>&wartosc=<int>) — DOKLADNIE ten
//                                  sam endpoint co panel WWW w przegladarce
//   POST /api/servo-manual     -> gdy user przesunie suwak na ekranie Serwo
//                                  (os=klapa|syberek&procent=<0-100>)
//
// Wszystkie pola w /api/ustawienia-get sa CALKOWITE (byte/int po stronie
// centrali) — zero miejsc po przecinku, wiec ten kod tez traktuje je jako long.
//
// PINY TFT/DOTYKU: piny EKRANU skonfigurowane w platformio.ini (build_flags,
// TFT_eSPI czyta je jako makra -D...). PINY DOTYKU sa NATOMIAST zdefiniowane
// PONIZEJ w tym pliku (TOUCH_SCK/TOUCH_MOSI/TOUCH_MISO/TOUCH_CS) i obslugiwane
// WLASNYM, recznym sterownikiem SPI (bitbang) — NIE przez wbudowana obsluge
// dotyku TFT_eSPI.
//
// [FIX-DOTYK-OSOBNA-MAGISTRALA] — diagnostyka w osobnej sesji wykazala, ze
// na tej konkretnej plytce ILI9488 (klon 3,5" z AliExpress) wspoldzielenie
// linii SCLK/MOSI/MISO miedzy ekranem a dotykiem (typowe podejscie, ktorego
// uzywa domyslna konfiguracja TFT_eSPI z TOUCH_CS) NIE dzialalo — potwierdzone
// wieloma niezaleznymi testami (m.in. reczny bitbang na wspoldzielonych
// pinach zawsze zwracal zero, a analogiczna, potwierdzona dzialajaca
// konfiguracja z innego projektu na tym samym typie modulu uzywala w pelni
// oddzielnej magistrali SPI dla dotyku). Dodatkowo pierwotny GPIO33 uzyty
// pod T_DO okazal sie fizycznie wadliwy na tym konkretnym ESP32 — po
// przelutowaniu dziala poprawnie, wiec zostaje na GPIO33.
// ============================================================================

// ============================================================================
// HISTORIA ZMIAN (ten plik) — najnowsze na gorze, kategorie jak w CHANGELOG.md
// ============================================================================
// [2026-11-12] v1.4.0 - AKTUALIZACJA PRZEZ CENTRALE (OTA): W TRAKCIE PRACY, Z
//   POSTEPEM NA EKRANIE, BEZ WGRYWANIA W KOLKO.
//   Decyzja uzytkownika: "Dzialaj" po raporcie
//   RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md (w repozytorium).
//   Jak bylo (v1.1.0-v1.3.0, z czytania kodu i z uruchomienia na PC):
//   - Panel sprawdzal oferte centrali RAZ, na koncu setup(). Pracujacy panel
//     nie zaczynal aktualizacji, dopoki ktos go nie wylaczyl i nie wlaczyl.
//   - Pobieranie (biblioteka HTTPUpdate) nie wracalo do petli: pusty ekran,
//     bez odpytywania centrali, watchdog wypisany na ten czas. Zaczynalo sie
//     takze w trakcie alarmu.
//   - Panel nie pamietal, co juz wgral. Centrala do 3.58.5 zdejmuje oferte
//     tylko wtedy, gdy panel zglosi wersje identyczna z tagiem wydania
//     ("v1.3.0" kontra "3.57.9") - wiec po udanym wgraniu panel wgrywal ten
//     sam plik przy kazdym starcie.
//   Jak jest (kod: sekcja "[v1.4.0 OTA] AKTUALIZACJA PANELU PRZEZ CENTRALE"
//   nizej; decyzje: diagnostyka_panelu.h, sekcja [v1.4.0 OTA]):
//   - Panel zna identyfikator programu, z ktorego pracuje: poczatek sumy
//     SHA-256, ktora esptool dopisuje na koncu kazdego programu ESP32
//     (esp_partition_get_sha256()). Zglasza go centrali w kazdym zapytaniu
//     o status (panel_obraz=..., a przy pierwszym starcie po wgraniu
//     panel_obraz_proba=...). Centrala 3.58.6 podaje identyfikator pliku,
//     ktory ma dla panelu ("panel_ota_obraz") - rowne = nie ma czego wgrywac.
//   - Panel pamieta w NVS (przestrzen "otapanel"), co zrobil z ostatnia
//     oferta: ile razy zaczynal pobierac, czy wgral, czy nowy program
//     wystartowal. Jedna oferta = najwyzej 3 proby; plik raz wgrany nie jest
//     wgrywany ponownie - takze przy starszej centrali, ktora oferty nie
//     zdejmie nigdy.
//   - Aktualizacja zaczyna sie W TRAKCIE PRACY (otaKrok() w loop()), gdy:
//     nie ma alarmu, dane z centrali sa swieze, minelo 20 s od startu, nikt
//     nie dotykal ekranu od 15 s, nie trwa pobieranie wykresu ani wysylanie
//     polecenia, wolnego RAM-u jest co najmniej 30 kB (najwiekszy blok 12 kB),
//     a na samym ogniwie bateria ma co najmniej 30 %.
//   - Pobieranie nie zatrzymuje petli (StrumienHttpGet::startBinarnie() w
//     siec_nieblokujaca_LCD.h): do 4 kB na obrot, prosto do Update.write().
//     Ekran pokazuje "AKTUALIZACJA PANELU" z paskiem postepu, panel dalej
//     pyta centrale o status, watchdog pilnuje petli jak zawsze.
//   - Alarm albo wycofanie oferty w trakcie pobierania przerywa je od razu;
//     taka proba sie nie liczy. Cisza 15 s albo 5 minut calosci konczy probe.
//   - Przed uruchomieniem nowego programu: naglowek pliku (program ESP32, nie
//     ESP32-S3 centrali), rozmiar, identyfikator rowny temu z oferty, a na
//     koncu sprawdzenie calego obrazu przez ESP-IDF (Update.end()).
//   - Po restarcie: jesli nowy program nie doszedl do zatwierdzenia i
//     bootloader wrocil do poprzedniego, panel to rozpoznaje (dziala inny
//     program niz wgrany) i tej oferty wiecej nie probuje.
//   - Panel mowi centrali, czego nie zrobi i dlaczego
//     (panel_ota_odmowa=<nr>-<powod>) oraz dlaczego jeszcze nie zaczal
//     (panel_ota_czeka=<powod>). Starsza centrala te parametry pomija.
//   - Haslo OTA jest kodowane do adresu (%XX) i nie pojawia sie w logu
//     ([NET-DIAG] pokazuje "token=***").
//   Zgodnosc: z centrala do 3.58.5 panel aktualizuje sie tak samo (sam, w
//   trakcie pracy, raz na tag wydania), ale centrala nie dowie sie o wyniku i
//   zostanie z niezdjeta oferta (blokada /update do recznego zdjecia - patrz
//   README). Pelna obsluga: centrala 3.58.6
//   (firmware-patch/v3.58.6-panel-ota).
//   Usunieto: wykonajAktualizacjeOTAPanelu(), sprawdzIWykonajOtaPanelu(),
//   OTA_TIMEOUT_MS, #include <HTTPClient.h> i <HTTPUpdate.h> (panel nie
//   uzywa juz zadnej z tych bibliotek).
//   Pamiec: w spoczynku bez zmian (stan OTA to ok. 250 B zmiennych
//   globalnych); w trakcie pobierania Update trzyma bufor 4 kB.
//   Sprawdzone na PC (testy_host/): decyzje - test_diagnostyka.cpp; caly
//   przebieg z prawdziwym kodem decyzji centrali 3.58.6 - scenariusze ota_*.
//   Typy i nazwy sprawdzone naglowkami arduino-esp32 3.0.7 i 3.3.8. NIE
//   kompilowane w PlatformIO, NIE uruchamiane na plytce: sam zapis do flash,
//   bootloader i wycofanie nieudanego programu sprawdzi dopiero plytka.
// [2026-11-12] v1.3.0 - WYKRESY: PUNKTY LICZY CENTRALA, POBIERANIE NIE
//   ZATRZYMUJE PANELU.
//   Prosba uzytkownika: "jak wybiore wykresy, to centrala ma pobrac dane z
//   Firebase albo lokalnie z pliku centrali i wyslac do panelu LCD".
//   Jak bylo (v1.2.2, z czytania jej kodu): po "POKAZ WYKRES" panel sam
//   pobieral od centrali CALY CSV historii (/api/history: do ok. 100 kB,
//   /api/history/long: do ok. 1,8 MB) i sam sciskal go do 96 punktow.
//   Pobieral blokujacym HTTPClient: do konca pobierania stal ekran, dotyk i
//   odpytywanie /api/status. Na odczyt mial 8 s - czego nie zdazyl pobrac,
//   tego na wykresie nie bylo, bez zadnej informacji. Kazde niepowodzenie
//   dawalo ten sam napis "Brak danych (WiFi/centrala niedostepna)" i zadnej
//   linii w logu.
//   Jak jest:
//   - Panel prosi centrale o GOTOWE punkty:
//       GET /api/history/panel?tryb=godz|dzien|tydz&pkt=96
//     Centrala z firmware 3.58.4 (paczka firmware-patch/v3.58.4-panel-wykresy
//     w repozytorium) sama wybiera zakres, dzieli go na 96 rownych odcinkow
//     czasu i odsyla najwyzej 96 wierszy (18-20 kB). Bierze je ze swojej
//     lokalnej historii - tej, z ktorej rysuje strona WWW i ktora dopisuje do
//     plikow /hist_short.csv i /hist_long.csv na swoim flashu. Firebase nie
//     jest uzywany: centrala ma te dane u siebie, a Firebase wymagalby
//     internetu.
//   - Centrala starsza niz 3.58.4 odpowiada na ten adres przekierowaniem
//     (302). Panel pobiera wtedy caly CSV po staremu i rysuje z niego to samo
//     co v1.2.2 (porownane punkt po punkcie z punktami policzonymi kodem
//     v1.2.2: testy_host/wzorce_v122/). Kolejnosc wgrywania panelu i
//     centrali jest wiec dowolna.
//   - Pobieranie (oba sposoby) nie blokuje petli: nowa klasa StrumienHttpGet
//     w siec_nieblokujaca_LCD.h czyta odpowiedz linia po linii w kolejnych
//     obrotach loop(), ok. 4 kB na obrot, do stalego bufora. Ekran, dotyk,
//     ekran alarmu, odpytywanie /api/status i watchdog pracuja w tym czasie
//     normalnie. Limity: 1,5 s na polaczenie, 6 s ciszy, 20 s na calosc.
//   - Wyjscie z wykresu w trakcie pobierania (dotkniecie, X, bezczynnosc,
//     ekran alarmu) zamyka polaczenie.
//   - Pod "Ladowanie danych z centrali..." widac, ile juz doszlo (wiersze,
//     kB).
//   - Gdy wykresu nie ma, ekran mowi dlaczego: brak WiFi, centrala nie
//     przyjela polaczenia, nie odpowiedziala w czasie, blad HTTP, odpowiedz
//     urwana, odpowiedz nie jest historia, brak pamieci, centrala nie ma
//     jeszcze danych, jeden pomiar to za malo. Gdy centrala ma wylaczony
//     zapis historii na flash, napis o braku danych to dopowiada.
//   - Kazde pobranie zostawia w logu linie "tag=WYKRESY" z wynikiem (opis:
//     komentarz "[v1.3.0 WYKRESY] Dane do ekranu Wykresy" nizej).
//   - Wykres z punktow centrali 3.58.4: os czasu jest rowna (v1.2.2 dzielil
//     dane na punkty wg numeru wiersza, a wiersze historii tygodniowej nie
//     sa rowno odlegle w czasie); przerwa w pracy centrali to przerwa w
//     linii o szerokosci tej przerwy; czujnik bez odczytu to przerwa w linii
//     tylko tego czujnika; podpis pod wykresem to zakres podany przez
//     centrale, np. "12.11 13:01  -  13.11 13:00".
//   - Stary sposob, gdy caly CSV nie dojdzie w 20 s albo polaczenie sie
//     urwie: wykres z tego, co doszlo, z dopiskiem NIEPELNE.
//   - Odpowiedz, ktora nie jest historia (np. strona HTML): napis zamiast
//     wykresu. Kod v1.2.2 rysowal z takiej strony wykres - kazda linia tekstu
//     stawala sie punktem (testy_host/wzorce_v122/punkty.txt, wpis
//     dzien_html_zewn).
//   - Zakres osi jest liczony tylko z prawdziwych wartosci. Dotad przerwa
//     jako PIERWSZY punkt serii dawala zakres nieokreslony (porownania z NaN
//     sa zawsze falszywe - wykresy_chart_zakres_osi() w
//     dashboard_lvgl_poc.cpp). Z punktami centrali to zwykly przypadek:
//     czujnik bez odczytu na poczatku zakresu.
//   Usunieto: wykresyFetchWykonaj() i HISTORIA_TIMEOUT_MS. HTTPClient zostal
//   tylko dla aktualizacji OTA.
//   Pamiec (testy_host/pomiar_pamieci.sh, model 32-bitowy na PC): po starcie
//   firmware trzyma tyle samo co v1.2.2 (129 244 B). Zmiennych statycznych
//   przybylo 512 B (4 468 B zamiast 3 956 B: bufor jednej linii 300 B, adres
//   zapytania, stan pobierania). Kod pobierania nie prosi o pamiec ze sterty.
//   W czasie pobierania i rysowania wykresu sterta rosnie chwilowo o 2,9 kB
//   (punkty z centrali) albo 5,6 kB (caly CSV) - to rysowanie LVGL i zwykle
//   odpytanie /api/status, ktore wypada w tym samym czasie; mniej niz limit
//   jednego odpytania (7,5 kB). Piec wykresow z rzedu nie zostawia nic.
//   Pamieci, ktora na czas pobierania bierze stos sieciowy plytki (lwIP),
//   ten pomiar nie widzi - pokaze ja "ram_min=" w linii "zdarzenie=pobrano".
//   Sprawdzone: na PC (testy_host/: 262 sprawdzenia jednostkowe, 34
//   scenariusze / 377 sprawdzen, takze z AddressSanitizer/UBSan i w modelu
//   32-bitowym; 8 nowych scenariuszy wykresy_*; odpowiedzi centrali uzyte w
//   tych testach sa plikami wypisanymi przez kod centrali 3.58.4 -
//   testy_host/wzorce_centrali/) i skladniowo prawdziwymi naglowkami
//   arduino-esp32 3.0.7 / 3.3.8. NIE budowane przez PlatformIO i NIE
//   uruchamiane na plytce. Czasy pobierania z testow na PC nie sa pomiarem
//   plytki (tam siec jest lokalna).
//
// [2026-11-12] v1.2.2 - PRZYCZYNA: PANELOWI BRAKUJE PAMIECI RAM.
//   Uzytkownik doslal log PANELU (v1.2.0) z tego samego zdarzenia co log
//   centrali opisany przy v1.2.1. Co w nim jest:
//   - Po starcie prawie kazda odpowiedz centrali konczy sie linia
//       tag=STATUS lvl=ERR zdarzenie=odpowiedz_nieczytelna przyczyna=json
//       http=200 json=NoMemory bajtow=1811 cialo_bajtow=1683 ...
//     Odpowiedz przyszla cala (HTTP 200, poczatek i koniec poprawne), a
//     ArduinoJson nie dostal od malloc() pamieci na jej rozpakowanie. Tak
//     bylo przez ok. 18 minut; kafelki pokazywaly "--".
//   - Linie "tag=PANEL" podaja heap=47032 heap_min=39624 blok_max=38900 i
//     blok_max jest IDENTYCZNY we wszystkich liniach przez godzine. To nie
//     jest pamiec, z ktorej korzysta malloc(): ESP.getFreeHeap() i
//     getMaxAllocHeap() w arduino-esp32 3.x doliczaja niezajety kawalek
//     obszaru na kod w RAM-ie (IRAM), a z niego zwykly malloc() nie
//     przydziela (sprawdzone w zrodlach arduino-esp32 3.0.7/3.3.8 i ESP-IDF
//     5.1.4/5.5 - opis przy ramWolny()). Naprawde wolne bylo: ok. 8 kB w
//     spoczynku, ok. 2 kB w chwili bledu, 0,6-0,9 kB w najgorszej chwili.
//   - Wczesniejszy restart w tym logu to "task_wdt ... loopTask (CPU 1)":
//     petla glowna krecila sie w miejscu przez 15 s. Slad wywolan jest
//     gleboki i ma powtarzajace sie adresy, jak rysowanie drzewa obiektow
//     LVGL; LVGL 8 po nieudanym przydziale pamieci staje w "while(1);".
//     Adresow nie da sie tu zamienic na nazwy funkcji (brak pliku .elf z
//     tej kompilacji), wiec to NAJPRAWDOPODOBNIEJ ta sama przyczyna, nie
//     pewnik.
//   - Od ok. 19. minuty do konca logu (ponad 40 minut) kazda proba konczy sie
//     limitem czasu, przy wifi=1 i zmieniajacym sie rssi. Z tego logu NIE da
//     sie rozstrzygnac, czy to panel gubil ramki z braku pamieci, czy
//     centrala przestala mu odpowiadac. W obu przypadkach pomogl dopiero
//     reczny reset - tym zajmuje sie drabina z v1.2.1 (ponowne polaczenie po
//     12 s, restart po 45 s).
//   Skad brak pamieci (pomiar: testy_host/pomiar_pamieci.sh - ten firmware
//   jako program 32-bitowy z licznikiem malloc(), z narzutem sterty ESP32):
//     v1.1.0 i v1.2.0 trzymaly po starcie 171 kB: ekrany LVGL (wszystkie
//     budowane przy starcie, ok. 2400 blokow) 110 kB, bufor rysowania LVGL
//     38,4 kB, bufory punktow ekranu Wykresy 18,5 kB, bufory odpowiedzi
//     HTTP 2,5 kB, reszta 1,6 kB.
//     Plytka miala wedlug logu 234,6 kB przed budowa ekranow i ok. 8 kB po
//     starcie - na WiFi, lwIP i zadania systemu przypada wiec ok. 56 kB.
//     Jedno odpytanie o status potrzebowalo chwilowo ok. 7,4 kB (bez buforow
//     samego WiFi), czyli prawie wszystko, co bylo wolne.
//     v1.1.0 miala ten sam problem (trzymala o 250 B mniej), tylko o nim nie
//     pisala: nieudane rozpakowanie konczylo sie cichym `return` - stad
//     pierwsze zgloszenie "kafelki sie nie aktualizuja" przy samych liniach
//     "etap=sukces" w logu.
//   Zmieniono:
//   - Bufor rysowania LVGL: 16 wierszy zamiast 40 (15 360 B zamiast
//     38 400 B) - lvgl_port.cpp. Obraz jest identyczny (20 zrzutow ekranu
//     z testow na PC, piksel w piksel); szybkosci rysowania na plytce nie
//     dalo sie tu zmierzyc.
//   - Bufory punktow ekranu Wykresy (18 464 B) istnieja tylko wtedy, gdy ten
//     ekran jest otwarty - dashboard_lvgl_poc.cpp, wykresy_bufory_8_*().
//     Razem firmware trzyma po starcie 129 kB zamiast 171 kB. Zmiennych
//     statycznych przybylo ok. 0,9 kB (glownie bufor logBezPamieci()).
//     Szacunek dla plytki z logu: ok. 48 kB wolnego RAM-u zamiast ok. 8 kB.
//   - Odpowiedz centrali nie jest juz kopiowana przed rozpakowaniem
//     (NonBlockingHttp::odpowiedz() zamiast pobierzOdpowiedz()): jedno
//     odpytanie o status potrzebuje chwilowo 5,6 kB zamiast 7,4 kB.
//   - Brak pamieci na ODEBRANIE odpowiedzi jest wykrywany
//     (siec_nieblokujaca_LCD.h, String::concat() zwraca false). Dotad
//     String po cichu nie dopisywal kawalka i odpowiedz z dziura wygladala
//     jak blad centrali ("json=InvalidInput").
//   - Log podaje prawdziwy RAM: ram=, ram_min=, ram_blok= (zamiast heap=,
//     heap_min=, blok_max=) w liniach "tag=PANEL", "odpowiedz_nieczytelna" i
//     "autorestart"; nowe linie "tag=PAMIEC zdarzenie=po_ekranach|po_starcie"
//     (raz, przy starcie) i "tag=PAMIEC lvl=WARN zdarzenie=malo_pamieci"
//     (ponizej 16 kB, najwyzej raz na minute). Te same liczby ida w
//     zgloszeniu restartu do centrali (pola h i b).
//   - Brak pamieci jest osobna przyczyna braku danych: na ekranie
//     "BRAK PAMIECI 14 s" (bylo "ZLA ODPOWIEDZ", czyli wina centrali), w logu
//     przyczyna=pam, w zgloszeniu do centrali auto_pam_..._em200. Dotyczy
//     json=NoMemory, braku pamieci na odebranie odpowiedzi oraz
//     socket()/connect()/send() odrzuconych z ENOMEM, ENOBUFS, ENFILE.
//     Restart po 45 s jak przy innych przyczynach, bez ograniczenia do
//     dwoch.
//   - Linie logu potrzebne wlasnie przy braku pamieci ("tag=PANEL fw=",
//     odpowiedz_nieczytelna, dane_nieaktualne, malo_pamieci, autorestart,
//     lvgl_assert) ida przez logBezPamieci(): Serial.printf() dla tekstu od
//     64 znakow sam prosi o pamiec i bez niej linia przepada.
//   - Blad wewnetrzny LVGL (w praktyce: brak pamieci w trakcie rysowania)
//     nie zawiesza juz panelu na 15 s: include/lv_conf.h kieruje go do
//     panel_lvgl_assert() - napis "BRAK PAMIECI - RESTART" na ekranie, wpis
//     "tag=PANEL lvl=ERR zdarzenie=lvgl_assert ram=...", restart po 2,5 s i
//     zgloszenie centrali auto_lvgl_...
//   Sprawdzone: na PC (testy_host/: 262 sprawdzenia jednostkowe, 26
//   scenariuszy / 252 sprawdzenia, takze z AddressSanitizer/UBSan; nowe
//   scenariusze brak_pamieci_json, brak_pamieci_odbior, brak_pamieci_lvgl,
//   wykresy_pamiec, w ktorych malloc() odmawia pamieci na zadanie; te same
//   scenariusze w modelu 32-bitowym) i skladniowo prawdziwymi naglowkami
//   arduino-esp32 3.0.7 / 3.3.8. NIE budowane przez PlatformIO i NIE
//   uruchamiane na plytce.
//   "48 kB" to szacunek z jednego logu - prawdziwa liczbe pokaze linia
//   "tag=PAMIEC zdarzenie=po_starcie ram=..." po wgraniu.
//
// [2026-11-12] v1.2.1 - po zgloszeniu "nadal jest problem: podczas pracy
//   zresetowalem LCD i dopiero wtedy odebral swieze dane" (wg uzytkownika na
//   plytce byla v1.2.0). Przyslany zostal tylko log CENTRALI; logu panelu z
//   tej chwili nie ma, wiec PRZYCZYNA NADAL NIE JEST POTWIERDZONA.
//   [dopisek z v1.2.2: przyczyna zostala potem znaleziona w logu panelu -
//   patrz wpis wyzej. Ponizszy opis zostaje, bo mechanizmy z v1.2.1 dalej
//   dzialaja; pola heap=/blok_max= wymienione nizej nazywaja sie od v1.2.2
//   ram=/ram_blok= i licza inna, mniejsza pamiec.]
//   Co wiadomo:
//   - centrala miala aktualne wartosci (piec=134.0C zewn=49.0C
//     alarmCO=AKTYWNY pompa=ON) i buduje /api/status od nowa przy kazdym
//     zapytaniu, z tych samych zmiennych co linia "tag=STATUS" jej logu
//     (main_centrala.cpp 3.58.3) - starych wartosci panelowi nie podaje;
//   - jej log zawiera wpis "tag=HEALTH" (pierwszy pojawia sie 5 min po
//     starcie) z telemetry_produced=4 - wyglada na to, ze centrala
//     wystartowala ok. 5 min wczesniej, czyli krotko przedtem byla
//     restartowana (wniosek z jednej linii, nie pewnik);
//   - log panelu z 2026-11-11 (starszy build): tuz przed takim samym resetem
//     kazda proba konczyla sie od razu "connect() natychmiastowy blad", a po
//     resecie panel mial dane po 1,6 s;
//   - pamiec: ten firmware uruchomiony na PC w modelu 32-bitowym (zig,
//     x86-linux-musl, licznik malloc/free - pomiar jednorazowy, poza
//     testy_host/) po setup() trzyma ok. 132 kB w ok. 2480 blokach (z tego
//     38 kB to bufor rysowania LVGL), cykl statusu bierze chwilowo ok. 6 kB,
//     a przez 4 h pracy z aktywnym alarmem przybylo 4 kB (jeden blok, raz).
//     Wyciek w kodzie panelu jest wiec malo prawdopodobny; zuzycia przez
//     WiFi/lwIP na plytce ten pomiar nie obejmuje (pokaze je heap= i
//     blok_max= w linii "tag=PANEL").
//   Przypuszczenie (NIE potwierdzone): panel traci lacznosc z centrala,
//   byc moze przy jej restarcie, i sam jej nie odzyskuje. Pewne jest tylko
//   to, ze restart panelu pomaga. Na PC nie da sie tego odtworzyc, bo
//   dotyczy radia.
//   Dodano ([v1.2.1 RATUNEK], opis przy panelRatunekKrok()):
//   - Naglowek dashboardu przy braku danych pokazuje, czego brakuje, i licznik
//     sekund: "BRAK WIFI 14 s" / "CENTRALA MILCZY 14 s" / "ZLA ODPOWIEDZ 14 s";
//     w ostatnich 10 s przed restartem "RESTART ZA 7 s: BRAK WIFI".
//   - Ten sam znacznik (takze "SYMULACJA: piec") stoi teraz na ekranie
//     alarmu, nad przyciskami. W logu centrali z 2026-11-12 alarm
//     przegrzania byl aktywny (piec=134.0C na plytce bez czujnikow); przy
//     takim statusie panel pokazuje pelnoekranowy alarm, ktory zaslania
//     kafelki i naglowek - nie bylo na nim widac, czy dane jeszcze
//     przychodza ani ze temperatura pochodzi z symulacji.
//   - WiFi zglasza polaczenie, a centrala od 12 s milczy: panel rozlacza sie
//     i laczy od nowa (po kolejnych 12 s jeszcze raz, bez zapamietanego
//     BSSID). v1.1.0 i v1.2.0 w tym stanie nie robily nic - reconnect ruszal
//     tylko wtedy, gdy WiFi samo zglosilo rozlaczenie.
//   - 45 s bez poprawnego statusu: panel restartuje sie sam. Kolejne takie
//     restarty z rzedu: po 90 s, 3 min, potem co 5 min; licznik wraca do zera
//     po 2 min nieprzerwanie poprawnych danych. Gdy centrala odpowiada, ale
//     nieczytelnie, restarty sa najwyzej dwa (wiecej nic nie da, a kazdy
//     konczylby sie wiadomoscia z centrali). Czas liczony jest z obrotow
//     petli (najwyzej 1 s na obrot), wiec dlugie pobieranie wykresow nie
//     konczy sie restartem.
//   - Po takim restarcie panel zglasza centrali powod zamiast "sw", np.
//     reset_powod=auto_wifi_r201_n1_t45_h61_b31_en118 (rozpisanie pol:
//     diag::tokenAutorestartu()). Centrala wpisuje go do SWOJEGO logu
//     ("tag=RESET-DIAG ... nietypowy_restart_panelu powod=auto_...") i wysyla
//     wiadomosc - tak jak o kazdym restarcie innym niz "sw"/"deepsleep".
//     Dzieki temu nastepne zgloszenie da sie rozstrzygnac z samego logu
//     centrali. Restart po OTA zostaje "sw".
//     Kazdy restart jest zglaszany raz: po pierwszej odpowiedzi centrali
//     panel wraca do zwyklego "sw", wiec centrala nie oglosi go ponownie po
//     wlasnym restarcie. Z jednej serii restartow (serie konczy godzina
//     poprawnych danych) zglaszane sa pierwsze trzy, potem 6., 12., 24.... -
//     lacznosc, ktora rwie sie raz za razem, nie daje wiadomosci co kilka
//     minut. Numer w serii jest w zgloszeniu ("n6").
//   - Stan alarmu (dym / przegrzanie pieca / panelu, WYCISZ, ZAMKNIJ) jedzie
//     przez taki restart w pamieci RTC: brzeczyk i ekran alarmu wracaja,
//     zanim przyjda dane - restart z braku danych nie gasi alarmu po cichu.
//   - Log: "tag=WIFI zdarzenie=rozlaczono kod=... nazwa=..." (kod przyczyny
//     kazdego rozlaczenia WiFi), "wymuszam_polaczenie", "tag=PANEL
//     zdarzenie=autorestart|start_po_autorestarcie|autorestart_zgloszony|
//     autorestart_pominiety|autorestart_licznik_wyzerowany|alarm_sprzed_restartu";
//     w liniach "GET nieudany" numer i nazwa bledu gniazda (errno); linia
//     "tag=PANEL" ma nowe pola bez_danych_s, przyczyna, wymuszen, rozl_kod,
//     rozl_n, autorestartow.
//   Naprawiono (brzeczyk, znalezione przy okazji; potwierdzone na PC dla
//   v1.2.0 - scenariusz wycisz_alarm):
//   - WYCISZ dzialal dopiero po nastepnym poprawnym statusie, czyli bez
//     danych z centrali wcale; teraz cisza zapada od razu.
//   - Drugie nacisniecie WYCISZ wlaczalo dzwiek z powrotem.
//   - Raz wyciszony typ alarmu zostawal niemy przy kazdym nastepnym
//     wystapieniu, az do restartu panelu; teraz wyciszenie konczy sie razem
//     z alarmem.
//   Sprawdzone tak jak v1.2.0: na PC (testy_host/: 241 sprawdzen
//   jednostkowych, 22 scenariusze / 216 sprawdzen, takze z
//   AddressSanitizer/UBSan) i
//   skladniowo prawdziwymi naglowkami arduino-esp32 3.0.7 / 3.3.8. NIE
//   budowane przez PlatformIO i NIE uruchamiane na plytce. Model WiFi w
//   testach jest zalozeniem - w szczegolnosci "martwe polaczenie" po szybkim
//   restarcie centrali (scenariusz martwe_polaczenie) to przypuszczenie, a
//   nie zaobserwowane zachowanie tych plytek.
// [2026-11-11] v1.2.0 - po zgloszeniu "kafelki sie nie aktualizuja" (log
//   serial-20261110_200814.log: same linie "[NET-DIAG] ... sukces", zero
//   bledow, a ekran stoi). PRZYCZYNA ZGLOSZENIA NIE JEST POTWIERDZONA - log
//   v1.1.0 nie zawieral nic, co pozwalaloby ja wskazac. Ta wersja usuwa
//   bledy, ktore daja taki objaw, i sprawia, ze nastepny log oraz sam ekran
//   beda rozstrzygajace. Sprawdzone na PC (testy_host/: prawdziwy main.cpp i
//   dashboard uruchomione zwyklym g++ z atrapami sprzetu) oraz skladniowo
//   prawdziwymi naglowkami arduino-esp32 3.0.7 / 3.3.8 + ESP-IDF 5.1 (clang,
//   testy_host/sprawdz_naglowki_esp32.sh). W tej sesji NIE budowane przez
//   PlatformIO (GCC, linkowanie) i NIE uruchamiane na plytce.
//   Logi z 2026-11-11 (panel + centrala na golej plytce): panel na biurku ma
//   wgrany build STARSZY niz te zrodla - jeden timeout daje w nim setki linii
//   "[USTAWIENIA] GET nieudany" (brak FIX-LOG-SPAM), a w adresie zapytan
//   widac reset_powod=task_wdt. Centrala podawala wtedy zewn=18.5C (symulacja)
//   i -1.0C dla pozostalych temperatur; te zrodla, uruchomione na PC z takimi
//   danymi, pokazuja je na kafelkach (scenariusz gola_plytka).
//   Naprawiono:
//   - [v1.2.0 DANE-NIEAKTUALNE] "sukces" warstwy sieciowej znaczy tylko, ze
//     centrala zamknela gniazdo. obslugaStatusFetch()/obslugaUstawieniaFetch()
//     nie sprawdzaly kodu HTTP, a przy bledzie JSON robily ciche `return` -
//     kafelki zostawaly ze starymi wartosciami, bez sladu w logu. Teraz kod
//     HTTP, tresc i JSON sa sprawdzane (diag::sprawdzOdpowiedz()), blad daje
//     linie "tag=STATUS|USTAWIENIA lvl=ERR zdarzenie=odpowiedz_nieczytelna
//     przyczyna=..." (najwyzej raz na 10 s), a dane sa uznawane za swieze
//     dopiero po poprawnym odczycie.
//   - [v1.2.0 DANE-NIEAKTUALNE] Wskaznik "dane nieaktualne" (wpis z
//     2026-09-12 nizej: "NIE podpiety") jest podpiety w galezi LVGL: po
//     PROG_NIEAKTUALNYCH_DANYCH_MS (10 s) bez poprawnego statusu kafelki
//     pokazuja "--", a wirnik pompy staje. Log: dane_nieaktualne /
//     dane_wrocily.
//   - [v1.2.0 WIFI-WYBOR] Laczenie po samej nazwie sieci bralo PIERWSZA
//     znaleziona siec (domyslny WIFI_FAST_SCAN), a nie najsilniejsza. Przy
//     dwoch centralach w zasiegu (prawdziwa + testowa: ta sama nazwa, haslo
//     i adres 192.168.4.1) panel mogl pokazywac dane nie tej, w ktorej
//     zmieniano wartosci. Teraz przy zimnym starcie: skan wszystkich kanalow
//     (wifiSkanujCentrale()) i polaczenie po BSSID z najsilniejsza.
//   - [v1.2.0 WIFI-WYBOR] Zapamietany BSSID nigdy nie byl porzucany: po
//     zniknieciu TEJ centrali panel nie laczyl sie z zadna inna az do
//     odlaczenia zasilania. Teraz po 3 nieudanych probach szuka od nowa.
//   - [v1.2.0 FIX-WDT-WIFI] polaczWifi() czekalo do 20 s bez karmienia
//     watchdoga (15 s, panic): bez centrali w zasiegu panel restartowalby
//     sie w kolko (wniosek z kodu i z testu na PC, nie z plytki).
//   - [v1.2.0 FIX-LOG-SPAM-KOMENDA] Jeden nieudany POST wypisywal linie
//     bledu w KAZDYM obrocie loop() (brakowalo komendaFetch.skwitujBlad()).
//   - [v1.2.0 BATERIA-BRAK] Bez ogniwa (0,28 V w logu) wskaznik pokazywal
//     "0%" w kolorze ladowania; teraz ponizej 2,5 V pokazuje "USB".
//   Dodano:
//   - [v1.2.0 CENTRALA-ID] Znacznik w naglowku dashboardu:
//     "SYMULACJA: zewn, pokoj", gdy prawdziwa centrala podaje czujniki jako
//     symulowane (pola "symulacja_*":true w /api/status - symulacja wlaczana
//     z jej panelu WWW); "SYMULATOR", gdy odpowiada centrala-testowa-c3;
//     "2 CENTRALE (XX:YY)", gdy przy starcie byly widoczne co najmniej dwie
//     sieci o nazwie centrali (XX:YY = koncowka BSSID tej, z ktora panel
//     jest polaczony). Jedna centrala bez symulacji = brak znacznika. Log:
//     "tag=PANEL zdarzenie=symulacja_centrali czujniki=...".
//   - [v1.2.0 DIAG] Linia "tag=PANEL fw=... bssid=... centrala=... sym=...
//     status_ok=... t_zewn=... t_ogrz=... dym=... heap=... blok_max=..." 5 s
//     po starcie i potem co 30 s: z kim panel rozmawia, ile odpowiedzi
//     odczytal i jakie wartosci podaje kafelkom (do porownania z linia
//     "tag=STATUS" w logu centrali). Linie "tag=WIFI
//     zdarzenie=skan_centrala|wiele_central|polaczono|reconnect_ok|
//     porzucam_bssid" z BSSID, kanalem i sila sygnalu.
//   Zmieniono:
//   - Hasla (WIFI_PASS, OTA_HASLO_PANEL) nie sa juz wpisane w tym pliku -
//     pochodza z src/secrets.h (poza gitem, wzor: secrets.h.example); ta
//     sama konwencja i te same dwie wartosci co w centrali.
//   - [v1.2.0 LOG-POZIOM] Linie "[NET-DIAG] start/wyslano/sukces" przy
//     kazdym zadaniu (3 linie co 2 s) sa domyslnie wylaczone
//     (SiecCfg::POZIOM_LOGU = 1: bledy + zadania wolniejsze niz 700 ms).
//   - Zimny start trwa o czas skanu dluzej (ok. 1,5-2 s).
//   NIE ZROBIONO: stary dashboard TFT (LVGL_POC_TEST=0, obecnie niebudowany)
//   dostal logi i poprawki WiFi, ale nie przelacza kafelkow na "--".
// [2026-09-12] Naprawiono: BLAD KOMPILACJI ("too few arguments to function
//   'lvgl_dashboard_poc_aktualizuj'") po pierwszym realnym build+upload tej
//   sesji. PRZYCZYNA: rozjazd dwoch rownoleglych wersji main.cpp - ta, na
//   ktorej oparta byla warstwa nieblokujaca (patrz wpis nizej), nie miala
//   jeszcze DESZCZ-ETAP4 (PLAN_DESZCZ_ZEWN.md), doklejonego w miedzyczasie
//   do dashboard_lvgl_poc.h (2 nowe parametry koncowe: deszczStan,
//   deszczMinuty) i do OSOBNEJ kopii main.cpp (starszej architektonicznie -
//   wciaz z blokujacym httpGet()/httpPost(), bez tego pliku non-blocking).
//   ZLACZONO obie linie rozwoju: dodano stan.deszczStan/deszczMinuty (pole
//   struct + parsowanie z /api/status pol deszcz_stan/deszcz_min w
//   obslugaStatusFetch()) i dopisano oba te argumenty na koncu WSZYSTKICH
//   TRZECH wywolan lvgl_dashboard_poc_aktualizuj() w TYM pliku (real -
//   obslugaStatusFetch() i setup() - oraz symulacja LVGL_POC_SYMULACJA,
//   gdzie brak centrali wiec wpisano 0,-1 "brak opadow" na sztywno) -
//   BEZ cofania architektury nieblokujacej (10 mostow LVGL swiadomie NIE
//   odzyskaly wlasnych bezposrednich wywolan tej funkcji - patrz
//   pendingPoKomendzie i uzasadnienie w poprzednim wpisie ponizej). Podniesiono
//   FIRMWARE_VERSION_PANEL v1.0.0 -> v1.1.0 (dwie funkcjonalne zmiany od
//   ostatniego wydania, jeszcze przed pierwszym realnym flashem tej wersji).
// [2026-09-12] Zmieniono: cala warstwa sieciowa (GET /api/status, GET
//   /api/ustawienia-get, POST /api/*-manual i /api/ustawienia-set) przepisana
//   z blokujacego HTTPClient (httpGet()/httpPost(), USUNIETE z tego pliku) na
//   nieblokujaca warstwe z siec_nieblokujaca_LCD.h (klasa NonBlockingHttp -
//   gole gniazda BSD, O_NONBLOCK + select() z zerowym czekaniem, ten sam
//   wzorzec co juz istniejacy wifiReconnectNonBlocking() w tym pliku).
//   PRZYCZYNA: przy wspolbieznym WIFI_MODE_APSTA (centrala), proba STA
//   centrali do domowego routera przerywa jej WLASNY SoftAP na czas
//   przelaczania kanalow (potwierdzony, dlugoletni znany problem sprzetowy
//   ESP32 - jeden radiowy front-end dzielony miedzy AP i STA, patrz
//   espressif/arduino-esp32#1040) - to fizycznie nie do naprawienia od
//   strony sieci, wiec dawny httpGet()/httpPost() (zawsze blokujace) mrozily
//   caly loop() (w tym obsluzDotyk()) na czas ich HTTP_TIMEOUT_MS (1500ms)
//   przy KAZDYM takim zaburzeniu. Research (10+ jezykow + dogĺebny przeglad
//   forow esp32.com/GitHub Espressif) potwierdzil, ze surowe nieblokujace
//   gniazda + select() to udokumentowany, pierwszorzedny wzorzec ESP-IDF
//   (examples/protocols/sockets/non_blocking), a dwie alternatywy (osobny
//   task FreeRTOS na drugim rdzeniu; esp_http_client z is_async=true) sa tu
//   gorszym wyborem - mieszalyby sie zle z glebokim usypianiem (idzSpac()) i
//   psulyby spojnosc z istniejacym wzorcem wifiReconnectNonBlocking().
//   ZMIANY W main.cpp: pobierzStatus()/pobierzUstawienia() to teraz same
//   "starty" zadania (statusFetch.startGet()/ustawieniaFetch.startGet());
//   parsowanie JSON-a + caly dawny "ogon" (aktualizacja sezonu/LVGL/starego
//   dashboardu) przeniesione do nowych obslugaStatusFetch()/
//   obslugaUstawieniaFetch(), wolanych z loop() co iteracje (w OBU galeziach
//   #if LVGL_POC_TEST, bo te funkcje byly wspoldzielone). Nowa funkcja
//   obslugaKomendaFetch() + flaga pendingPoKomendzie (enum PoKomendzie)
//   spinaja z powrotem POST+nastepujacy-po-nim-GET, ktore juz NIE moga byc
//   jednym synchronicznym wywolaniem (patrz obszerny komentarz przy
//   pendingPoKomendzie) - odkryto przy tym, ze siec_nieblokujaca_LCD.h mial
//   na wzgledzie tylko 3 miejsca POST, a w praktyce jest ich 10 (wszystkie
//   mosty LVGL: pompaManualWyslij/pompaWyborWyslij/serwoTrybWyslij/
//   serwoPozycjaWyslij/serwoUstawienieWyslij/pompaUstawienieWyslij/
//   pompaAntystopWyslij/dymProgWyslij/dymUstawienieWyslij/
//   alarmTempProgWyslij) + stary, nie-LVGL EKRAN_TRYBY/EKRAN_USTAWIENIA/
//   EKRAN_SERWO (przez wspoldzielone wyslijTrybPracy()/wyslijUstawienie()/
//   wyslijServoManualny()) - flaga rozwiazuje to bez oddzielnej maszyny
//   stanow na kazde z tych miejsc. Natychmiastowe odswiezenie po udanej
//   komendzie CELOWO zachowane (w odroznieniu od pierwotnego szkicu w
//   siec_nieblokujaca_LCD.h, patrz uwaga tam) - bez niego kazda zmiana z
//   dotyku odswiezalaby sie dopiero po 2-5s zamiast niemal natychmiast.
//   USUNIETO: blokujacy czerwony pasek+delay(1200) na blad POST-a w starym,
//   nie-LVGL EKRAN_TRYBY/EKRAN_USTAWIENIA (wyslijTrybPracy()/
//   wyslijUstawienie()) - to byla DRUGA blokujaca operacja w tym pliku;
//   teraz tylko log po Serialu (jak juz akceptowany wzorzec w mostach LVGL),
//   ekran i tak pokazuje prawdziwa wartosc po automatycznym
//   pobierzUstawienia() z obslugaKomendaFetch(). DODANO: jednorazowe,
//   OGRANICZONE (PIERWSZA_PROBKA_TIMEOUT_MS=2500ms) oczekiwanie na siec w
//   setup() (funkcja poczekajNaPierwszeDane()) - JEDYNE miejsce w pliku,
//   gdzie to jeszcze robimy, bo na starcie nie ma dotyku do zamrozenia, a
//   bez tego pierwszy rysujAktualnyEkran()/lvgl_dashboard_poc_aktualizuj()
//   i sprawdzIWykonajOtaPanelu() dostalyby same zera zamiast realnych
//   danych z centrali. NIE ZROBIONO w tej sesji (celowo, poza zakresem):
//   [dopisek z v1.3.0: punkt (1) zrobiony - wykresyFetchWykonaj() juz nie
//   ma, ekran Wykresy pobiera dane bez blokowania; patrz wpis v1.3.0 wyzej]
//   (1) wykresyFetchWykonaj() (GET /api/history, ekran Wykresy) NADAL uzywa
//   HTTPClient bezposrednio - to osobny, rzadko wywolywany, strumieniowany
//   przypadek (RAM, patrz FIX-WYKRESY-RAM), przy tym samym zaburzeniu
//   kanalu zamrozi tylko ten jeden ekran, nie caly dotyk; (2) wskaznik "dane
//   nieaktualne" na dashboardzie (WskaznikSwiezosci.saNieaktualne()) wdrozony
//   tylko na poziomie sledzenia (swiezoscStatusu/swiezoscUstawien.odnotujSukces()
//   wolane poprawnie), ale NIE podpiety pod zadne rysowanie w rysujNaglowek()/
//   dashboard_lvgl_poc.* - potencjalny nastepny krok. Patrz tez
//   siec_nieblokujaca_LCD.h (naglowek pliku, sekcja "[ZREALIZOWANE]") i
//   HISTORIA ZMIAN w PLAN_SD_LOGI_AWARYJNE.md/PLAN_LOGOWANIE_ANTYSPAM.md w
//   projekcie centrali dla podobnych, wczesniejszych zmian nieblokujacych.
// [2026-09-05] Dodano: obsluga przycisku WYCISZ na nowym EKRANIE ALARMOWYM
//   (dashboard_lvgl_poc.cpp) - patrz PLAN_EKRAN_ALARMOWY.md. Nowe:
//   g_brzeczykWyciszony/g_brzeczykWyciszonyTyp + funkcja alarmWyciszObsluga()
//   (zarejestrowana jako AlarmWyciszCb, w OBU galeziach #if
//   LVGL_POC_SYMULACJA - ten sam powod co WYSWIETLACZ powyzej: brzeczyk to
//   GPIO na TEJ plytce, wiec dziala identycznie niezaleznie od tego, czy
//   centrala jest akurat podlaczona). brzeczykOdswiezZAlarmow()
//   zmodyfikowane, zeby respektowac wyciszenie - patrz komentarz tam.
//   Wyciszenie jest CELOWO powiazane z KONKRETNYM aktywnym typem alarmu
//   (nie globalnym "cisza na zawsze") - samo znika, gdy typ sie zmieni
//   (alarm ustapi albo eskaluje), zeby nie ryzykowac zapomnianego,
//   wyciszonego na trwale przyszlego pozaru. Dotyczy tez
//   dashboard_lvgl_poc.h (typedef AlarmWyciszCb) i dashboard_lvgl_poc.cpp
//   (caly ekran) - patrz HISTORIA ZMIAN w obu tych plikach oraz
//   CHANGELOG.md [Unreleased].
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"    // [FIX-TFT-BL-HOLD, ta sesja] gpio_hold_en/gpio_hold_dis/gpio_deep_sleep_hold_en dla GPIO21 (TFT_BL)
#include <stdarg.h> // [v1.2.2] va_list w logBezPamieci()
#include <math.h>   // cosf/sinf — uzywane w prostych ikonach wektorowych (rysujIkonaSlonce/Snieg), patrz sekcja [UI]
#include "ikony.h"  // [LCD-IKONY] bitmapy 24x24 (plomien/kropla/suwak/alarm) - patrz komentarz w pliku

// [SIEC-NIEBLOKUJACA, ta sesja] Zastepuje synchroniczne HTTPClient::GET/POST
// (httpGet/httpPost, usuniete z tego pliku) - patrz obszerny naglowek w
// siec_nieblokujaca_LCD.h co do PRZYCZYNY (zaburzenie kanalu SoftAP/STA na
// centrali, potwierdzone jako znane ograniczenie sprzetowe ESP32 w trybie
// WIFI_MODE_APSTA - jeden radiowy front-end dzielony miedzy AP i STA, wiec
// skan/connect() po stronie STA centrali fizycznie przerywa AP na czas
// przelaczania kanalow; patrz espressif/arduino-esp32#1040) oraz ROZWIAZANIA
// (gole gniazda BSD w trybie O_NONBLOCK + select(), dokladnie ten wzorzec co
// oficjalny przyklad ESP-IDF protocols/sockets/non_blocking).
#include "siec_nieblokujaca_LCD.h"
// [v1.2.0 DIAG] Male, czyste funkcje diagnostyki (kod HTTP, dlawik logu,
// straznik swiezosci danych, wybor najsilniejszego AP, obecnosc baterii) -
// bez zaleznosci od Arduino, testowane na PC (testy_host/). Opis w pliku.
#include "diagnostyka_panelu.h"

// [OTA-ROLLBACK] Potrzebne do potwierdzania partycji po OTA (prompt #6) i,
// docelowo, do samego wgrywania aktualizacji (prompt #7). Ten sam naglowek
// WDT co w main_centrala.cpp ([FIX-WDT]) — panel dotad nie mial ZADNEJ
// konfiguracji Task WDT, patrz komentarz przy setup() nizej.
// [v1.4.0 OTA] Update.h zamiast HTTPUpdate.h: panel sam pobiera plik (otaFetch,
// bez zatrzymywania petli) i oddaje go do Update.write(). HTTPClient.h i
// HTTPUpdate.h nie sa juz uzywane nigdzie w tym pliku.
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>   // [v1.4.0 OTA] esp_partition_get_sha256() - identyfikator wlasnego programu
#include <esp_task_wdt.h>
#include <esp_wifi.h>    // [v1.2.0 WIFI-WYBOR] esp_wifi_scan_stop() - patrz wifiSkanujCentrale()
#include <esp_heap_caps.h>   // [v1.2.2 PAMIEC] heap_caps_get_*() - patrz ramWolny()
#include "esp_system.h"  // [RESET-DIAG, ta sesja] esp_reset_reason() / esp_reset_reason_t

// [OTA-ROLLBACK] Bez tego override arduino-esp32 SAM potwierdzi biezaca
// partycje jako VALID w initArduino(), zanim setup() zdazy cokolwiek
// sprawdzic — wywolanie esp_ota_mark_app_valid_cancel_rollback() ponizej
// byloby wtedy martwym kodem. Musi byc w zasiegu GLOBALNYM, poza
// jakakolwiek funkcja. Mechanizm zweryfikowany empirycznie w projekcie
// testowym ota_rollback_test — patrz Ustalenie #5 i #6 w
// OTA_PANEL_CALOSC.md.
extern "C" bool verifyRollbackLater() {
  return true;
}

// [LVGL - KROK 4] Wlacz na 1 zeby przetestowac LVGL zamiast normalnego
// dashboardu (podmienia caly setup()/loop() na sciezke LVGL, patrz ponizej).
// Ustaw z powrotem na 0, zeby wrocic do dzialajacego kodu sprite'owego -
// NIC z reszty main.cpp nie jest przez to kasowane, to tylko przelacznik.
#define LVGL_POC_TEST 1
#include "lvgl_port.h"
#include "dashboard_lvgl_poc.h"

// [LVGL - PRACA BEZ CENTRALI] Wlaczone (1), dopoki pracujesz TYLKO na samej
// plytce wyswietlacza (bez podlaczonej/wlaczonej centrali "Piec CO" obok) -
// wtedy caly kod WiFi/HTTP jest pomijany, a dashboard dostaje dane z
// LOKALNEJ symulacji w tym pliku (patrz symPompaAkcja()/symOdswiezDoLvgl()
// nizej), zeby dalo sie testowac layout/dotyk/panel POMPA na biurku, bez
// zadnego innego sprzetu. Ustaw na 0, gdy centrala bedzie fizycznie obok i
// w tej samej sieci/AP - wraca wtedy prawdziwe polaczWifi()/pobierzStatus()/
// POST /api/pompa-manual, dokladnie jak dotychczas. Ma znaczenie TYLKO gdy
// LVGL_POC_TEST=1 (w trybie starego dashboardu nie jest w ogole uzywane).
#define LVGL_POC_SYMULACJA 0

// ----------------------------------------------------------------------------
// KONFIGURACJA — podmien pod swoja siec i adres centrali
//
// [ETAP 3 / krok 1, PIEC_ESPNOW_PLAN.md] Wyswietlacz laczy sie teraz z
// WLASNYM Access Pointem centrali (Piec CO), a NIE z domowa siecia WiFi —
// patrz Podejscie A w planie. Wpisz tu DOKLADNIE te same wartosci, jakie
// centrala ma ustawione jako WIFI_AP_SSID / WIFI_AP_PASSWORD.
// CENTRALA_HOST zostaje na stale 192.168.4.1 (domyslna brama Soft-AP ESP32)
// niezaleznie od tego, czy centrala ma akurat polaczenie z domowym routerem.
//
// Kod odpytywania (httpPobierz, pobierzStatus, pobierzUstawienia,
// wyslijUstawienie, wyslijServoManualny) zostaje BEZ ZMIAN — zmienia sie
// tylko adres docelowy.
// ----------------------------------------------------------------------------
// [SEKRETY] Hasla NIE leza w tym pliku - sa w pliku "secrets.h" (obok, w src/),
// ktory jest wpisany do .gitignore i zostaje TYLKO u Ciebie na komputerze.
// Ta sama konwencja i TE SAME dwie wartosci co w centrali (main_centrala.cpp):
//   1. Skopiuj "secrets.h.example" -> "secrets.h" (folder src/ panelu).
//   2. Wpisz SECRET_WIFI_AP_PASSWORD i SECRET_OTA_HASLO - identyczne jak w
//      secrets.h centrali (mozna tez skopiowac caly tamten plik).
//   3. Dopiero potem kompiluj i wgraj.
// Bez secrets.h kompilacja ZATRZYMUJE SIE z czytelnym komunikatem.
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Brak pliku src/secrets.h - skopiuj src/secrets.h.example jako src/secrets.h i wpisz hasla (te same co w secrets.h centrali)."
#endif
#ifndef SECRET_WIFI_AP_PASSWORD
  #error "secrets.h: brak SECRET_WIFI_AP_PASSWORD (haslo sieci centrali - to samo co w secrets.h centrali)."
#endif
#ifndef SECRET_OTA_HASLO
  #error "secrets.h: brak SECRET_OTA_HASLO (haslo OTA - to samo co w secrets.h centrali)."
#endif
static const char* WIFI_SSID     = "PiecCO_AP";         // SSID AP centrali (WIFI_AP_SSID w main.cpp centrali)
static const char* WIFI_PASS     = SECRET_WIFI_AP_PASSWORD;       // haslo AP centrali (WIFI_AP_PASSWORD w main.cpp centrali)
static const char* CENTRALA_HOST = "192.168.4.1";       // domyslna brama Soft-AP centrali — bez zmian
static const uint16_t CENTRALA_PORT = 80;

// [ETAP 3 KROK 2] Statyczne IP wyswietlacza — glowna, potwierdzona badaniem
// dzwignia szybkosci polaczenia po wybudzeniu (~200 ms zamiast 1,5-3 s,
// bo ESP32 nie czeka na DHCP). Musi byc w tej samej podsieci co AP centrali
// (192.168.4.x) i NIE moze kolidowac z .1 (brama/centrala) ani z adresami,
// ktore centrala moglaby przydzielic innym klientom AP przez DHCP.
IPAddress WIFI_IP_STATYCZNE(192, 168, 4, 50);
IPAddress WIFI_GATEWAY(192, 168, 4, 1);
IPAddress WIFI_MASKA(255, 255, 255, 0);

// [ETAP 3 KROK 2] Cache BSSID+kanalu centrali w pamieci RTC — przetrwa
// deep sleep (w odroznieniu od zwyklych zmiennych), zerowy koszt, przydatne
// przy czestym wybudzaniu/usypianiu (Etap 3b). Samo w sobie nie przyspiesza
// polaczenia na ESP32 (w przeciwienstwie do ESP8266) — patrz research w
// PIEC_ESPNOW_PLAN.md — ale pozwala uniknac ponownego skanowania kanalu.
RTC_DATA_ATTR uint8_t rtcBssidCentrali[6] = {0, 0, 0, 0, 0, 0};
RTC_DATA_ATTR int32_t rtcKanalCentrali = 0;
RTC_DATA_ATTR bool    rtcBssidZnany = false;

static const uint32_t STATUS_INTERWAL_MS     = 2000;   // jak czesto GET /api/status
static const uint32_t USTAWIENIA_INTERWAL_MS = 5000;   // jak czesto GET /api/ustawienia-get
static const uint32_t HTTP_TIMEOUT_MS        = 1500;   // timeout pojedynczego zapytania - patrz tez SiecCfg::CALKOWITY_TIMEOUT_MS w siec_nieblokujaca_LCD.h (ta sama wartosc, zdublowana celowo - ten sam wzorzec co OTA_HASLO_PANEL nizej; siec_nieblokujaca_LCD.h jest samodzielnym naglowkiem wlaczanym PRZED tym blokiem, wiec nie moze odwolac sie tu wprost)

// [SIEC-NIEBLOKUJACA, ta sesja] Trzy stale instancje - jedna na kazdy rodzaj
// ruchu (status/ustawienia/komenda), zeby wolny POST nie blokowal mozliwosci
// wystartowania kolejnego GET-a statusu. Definicje klas w
// siec_nieblokujaca_LCD.h; globalne obiekty tutaj (ANALOGICZNIE do
// rtcBssidCentrali wyzej - jedno miejsce prawdy, widoczne w calym pliku).
NonBlockingHttp statusFetch;
NonBlockingHttp ustawieniaFetch;
NonBlockingHttp komendaFetch;
StrumienHttpGet wykresyFetch;   // [v1.3.0 WYKRESY] historia dla ekranu Wykresy, linia po linii (patrz obslugaWykresyFetch())
StrumienHttpGet otaFetch;       // [v1.4.0 OTA] nowy firmware panelu z centrali (/panel-ota.bin), tresc binarna (patrz otaKrok())
WskaznikSwiezosci swiezoscStatusu;
WskaznikSwiezosci swiezoscUstawien;
// [v1.2.0 DANE-NIEAKTUALNE] Bylo 3 * STATUS_INTERWAL_MS (6 s) i NIGDZIE nie
// bylo uzywane. Teraz to prog, po ktorym kafelki przechodza na "--" (patrz
// panelOznaczDaneJakoNieaktualne()). 5 okresow (10 s), zeby pojedyncze
// zaburzenie kanalu AP centrali (jej wlasna proba STA do routera, patrz
// HISTORIA ZMIAN z 2026-09-12) nie migalo ekranem.
static const uint32_t PROG_NIEAKTUALNYCH_DANYCH_MS = 5 * STATUS_INTERWAL_MS; // 10000 ms

// [v1.2.0 DIAG] Stan diagnostyki - patrz diagnostyka_panelu.h.
static const uint32_t PANEL_DIAG_PIERWSZA_MS = 5000;   // pierwsza linia "tag=PANEL" krotko po starcie
static const uint32_t PANEL_DIAG_INTERWAL_MS = 30000;  // kolejne co 30 s (ten sam rytm co tag=BATERIA)
diag::Liczniki panelLiczniki;                 // zerowane po kazdej linii "tag=PANEL"
diag::StraznikSwiezosci straznikSwiezosci;    // kiedy kafelki maja pokazac "--"
diag::Dlawik dlawikBleduStatusu;              // nieczytelna odpowiedz /api/status - log najwyzej co 10 s
diag::Dlawik dlawikBleduUstawien;             // to samo dla /api/ustawienia-get
// [v1.2.1 RATUNEK] Co panel robi, gdy nie ma danych z centrali - patrz opis
// przy diag::Ratunek w diagnostyka_panelu.h i panelRatunekKrok() nizej.
diag::Ratunek panelRatunek;                   // drabina: ponowne polaczenie z siecia -> restart panelu
bool ostatniBladStatusuToTresc = false;       // ostatnia nieudana proba /api/status: true = odpowiedz nieczytelna, false = brak odpowiedzi
char ostatniBladStatusuTekst[72] = "";        // ...jej opis do logu (np. "timeout calkowity", "connect() natychmiastowy blad errno=118 EHOSTUNREACH")
char ostatniBladStatusuEtap = '-';            // ...etap, na ktorym padla (NonBlockingHttp::etapBledu(); 'o' = odpowiedz nieczytelna, 'm' = brak pamieci na jej odczytanie)
int  ostatniBladStatusuNumer = 0;             // ...i numer bledu gniazda albo kod HTTP
bool ostatniBladStatusuToPamiec = false;      // [v1.2.2] ...true = panelowi zabraklo pamieci (patrz diag::PRZYCZYNA_PAMIEC)

// ----------------------------------------------------------------------------
// [v1.2.2 PAMIEC] Ile RAM-u panel NAPRAWDE ma
// ----------------------------------------------------------------------------
// ESP.getFreeHeap() / getMinFreeHeap() / getMaxAllocHeap() w arduino-esp32 3.x
// licza pamiec MALLOC_CAP_INTERNAL. Na ESP32 wchodzi w to takze niezajety
// kawalek obszaru na kod w RAM-ie (IRAM, 0x40080000-0x4009FFFF) - a z niego
// zwykly malloc() NIE przydziela (ESP-IDF, components/heap/port/esp32/
// memory_layout.c: typ "IRAM" ma MALLOC_CAP_EXEC|32BIT, bez
// MALLOC_CAP_DEFAULT). W logu panelu z 2026-11-12 stalo przez godzine
// "heap=47032 heap_min=39624 blok_max=38900" - z czego 38900 B to wlasnie ten
// obszar. malloc() mial do dyspozycji ok. 8 kB, a w najgorszej chwili ok.
// 0,7 kB, i stad "json=NoMemory" przy "wolnych" 40 kB. Ponizsze funkcje pytaja
// o MALLOC_CAP_DEFAULT, czyli dokladnie o to, z czego korzysta malloc() (a
// wiec ArduinoJson, String, LVGL i lwIP).
static inline uint32_t ramWolny() { return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT); }
static inline uint32_t ramBlok()  { return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT); }
static inline uint32_t ramMin()   { return (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT); }
diag::Dlawik dlawikMaloPamieci;               // ostrzezenie "malo_pamieci" - najwyzej raz na minute

// Linia logu, ktora wypisze sie TAKZE przy braku pamieci. Serial.printf()
// dla tekstu od 64 znakow sam prosi o pamiec (Print::vprintf w arduino-esp32
// 3.0.7 i 3.3.8: bufor 64 B na stosie, dla dluzszego tekstu
// malloc(dlugosc + 1)) i gdy jej nie dostanie, linia przepada bez sladu - a
// wlasnie wtedy linie o braku pamieci sa potrzebne. Tutaj tekst powstaje w
// stalym buforze: ani sterta, ani stos (wazne w panel_lvgl_assert(), wolanym
// z glebi rysowania LVGL). Wolac TYLKO z zadania loop() - bufor jest jeden.
// Liczby zmiennoprzecinkowe (%f) dzialaja tu tak samo jak w Serial.printf():
// biblioteka C (newlib, mprec.c) bierze na nie kilka malych blokow pamieci
// przy pierwszym uzyciu w danym zadaniu i potem uzywa ich ponownie. Gdyby
// juz wtedy pamieci nie bylo, newlib przerywa program (assert "Balloc
// succeeded", restart z powodem "panic") - w obu funkcjach jednakowo.
static void logBezPamieci(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logBezPamieci(const char *fmt, ...) {
  static char linia[640];   // najdluzsza linia ("tag=PANEL fw=...") ma w najgorszym razie ok. 560 znakow
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(linia, sizeof(linia), fmt, ap);
  va_end(ap);
  if (n >= (int)sizeof(linia)) linia[sizeof(linia) - 2] = '\n';   // obcieta - niech przynajmniej konczy sie nowa linia
  Serial.print(linia);
}

// Czego panelowi brakuje, gdy nie ma danych z centrali - jedna funkcja dla
// naglowka ekranu, logu i drabiny ratunkowej (diag::Przyczyna).
static diag::Przyczyna panelPrzyczynaTeraz() {
  return diag::ustalPrzyczyne(WiFi.status() == WL_CONNECTED, ostatniBladStatusuToTresc, ostatniBladStatusuToPamiec);
}
// Przetrwa ESP.restart() (pamiec RTC, ktorej program przy starcie nie
// zeruje). Po wlaczeniu zasilania zawiera przypadkowe wartosci - sprawdzane
// i czyszczone w autorestartOdczytajPrzyStarcie(), zanim cokolwiek ich uzyje.
RTC_NOINIT_ATTR diag::PamiecAutorestartu rtcAutorestart;
bool startPoAutorestarcie = false;            // true = ten start nastapil po samoczynnym restarcie (panelAutorestart())
bool autorestartDoZgloszenia = false;         // true = w zapytaniach /api/status idzie powod "auto_...", a centrala jeszcze na zadne nie odpowiedziala
// Od kiedy dane sa nieprzerwanie poprawne (do zerowania licznika restartow
// z rzedu - patrz diag::CZAS_STABILNYCH_DANYCH_MS).
bool daneDobreSeria = false;
unsigned long daneDobreOdMs = 0;
// Kod przyczyny ostatniego rozlaczenia WiFi i licznik rozlaczen. Wpisuje je
// zadanie zdarzen WiFi (wifiZdarzenie()), czyta petla glowna - stad volatile.
volatile uint8_t  wifiOstatniKodRozlaczenia = 0;
volatile uint32_t wifiLiczbaRozlaczen = 0;
// [v1.2.0 CENTRALA-ID] Z KTORA centrala rozmawia panel. Prawdziwa centrala i
// centrala testowa (centrala-testowa-c3) nadaja TA SAMA siec (WIFI_SSID) pod
// tym samym adresem - bez tego nie da sie odroznic ich ani z ekranu, ani z
// logu. `centralaToSymulator` pochodzi z pola "symulator":true w /api/status
// (wysyla je tylko centrala testowa); `wifiCentralPrzyStarcie` to liczba AP
// o naszej nazwie widzianych przy ostatnim skanowaniu (patrz polaczWifi()).
bool centralaToSymulator = false;
bool centralaRozpoznana = false;   // czy chocby jeden status zostal poprawnie odczytany
int  wifiCentralPrzyStarcie = 0;   // 0 = nie skanowano (szybka sciezka z BSSID z pamieci RTC)
// Ktore czujniki PRAWDZIWA centrala podaje w tej chwili jako symulowane
// (pola "symulacja_<czujnik>":true w /api/status - symulacja wlaczana z jej
// panelu WWW). Bit i = diag::CZUJNIKI_SYMULOWANE[i]. Niezerowa maska znaczy:
// te wartosci na kafelkach nie sa pomiarem.
uint16_t centralaSymulacjaMaska = 0;

// [SIEC-NIEBLOKUJACA, ta sesja] POST przez komendaFetch i nastepujacy po nim
// GET (odswiezenie /api/status albo /api/ustawienia-get) NIE MOGA juz byc
// jednym synchronicznym wywolaniem jak dawniej (httpPost() zwracal bool
// dopiero PO calej wymianie) - GET musi poczekac, az POST faktycznie sie
// zakonczy (patrz obslugaKomendaFetch() przy pobierzStatus()/
// pobierzUstawienia() nizej). Ta flaga niesie "co zrobic PO zakonczeniu
// biezacej komendy" przez ten czas oczekiwania - ustawiana TUZ PRZED
// wystartowaniem POST-a (po sprawdzeniu komendaFetch.wTrakcie(), wiec zawsze
// odpowiada REALNIE wystartowanemu zadaniu, nigdy odrzuconemu), zerowana
// zaraz po odczytaniu w obsludze. USTAWIENIA jest ustawiana WEWNATRZ
// wyslijTrybPracy()/wyslijUstawienie() (wspolny mianownik ich wywolujacych -
// patrz komentarze tam); STATUS ustawiaja jawnie tylko te 4 mosty LVGL,
// ktorych ekran pokazuje pola z /api/status, nie z /api/ustawienia-get
// (pompaManualWyslij/pompaWyborWyslij/serwoTrybWyslij/serwoPozycjaWyslij).
enum class PoKomendzie : uint8_t { BRAK, STATUS, USTAWIENIA };
PoKomendzie pendingPoKomendzie = PoKomendzie::BRAK;

// [OTA-PANEL, ta sesja] Wersja wkompilowana w ten firmware — zglaszana
// centrali przy KAZDYM /api/status (patrz pobierzStatus() nizej), zeby centrala
// wiedziala co panel realnie ma wgrane (panel nie ma internetu, wiec nie
// moze sam nic potwierdzic Telegramowi). PODNIES RECZNIE przy kazdym
// Release'u firmware_panel.bin — bez tego centrala nigdy nie zobaczy
// potwierdzenia udanej aktualizacji (patrz TG_UPDATE_PANEL w main_centrala.cpp).
static const char* FIRMWARE_VERSION_PANEL = "v1.4.0"; // [2026-11-12] v1.4.0: aktualizacja przez centrale zaczyna sie w trakcie pracy, z postepem na ekranie i bez wgrywania tego samego pliku w kolko (pelna obsluga: centrala 3.58.6); opis w HISTORII ZMIAN
// [OTA-PANEL, ta sesja] MUSI byc identyczne z OTA_HASLO w main_centrala.cpp
// (patrz endpoint GET /panel-ota.bin) — celowo zdublowane jako stala tekstowa
// zamiast wspolnego naglowka, bo to dwa osobne projekty PlatformIO bez
// wspolnego katalogu include/. Zmieniajac jedno, zmien i drugie.
static const char* OTA_HASLO_PANEL = SECRET_OTA_HASLO;
// [v1.4.0 OTA] Limity czasu pobierania firmware'u sa w diagnostyka_panelu.h
// (diag::OTA_CISZA_MS: 15 s bez danych konczy probe; diag::OTA_CALOSC_MS:
// 5 minut na calosc). Dawny OTA_TIMEOUT_MS (20 s w HTTPUpdate) zniknal.

// [v1.3.0 WYKRESY] Limity czasu pobierania historii dla ekranu Wykresy sa od
// tej wersji w siec_nieblokujaca_LCD.h (SiecCfg::STRUMIEN_*): pobieranie nie
// blokuje juz petli, wiec dawny HISTORIA_TIMEOUT_MS (8 s na calosc) zniknal.

// [ETAP 5, PIEC_ESPNOW_PLAN.md] Reconnect WiFi z rosnacym opoznieniem miedzy
// probami (1s -> 2s -> 4s -> ... -> WIFI_RECONNECT_MAX_MS), zamiast probowac
// co iteracje loop() — research (watki forum ESP32/Arduino, artykuly o
// exponential backoff dla WiFi na ESP32) potwierdza, ze natychmiastowe,
// powtarzane WiFi.begin() po zerwaniu polaczenia "zasypuje" AP probami i jest
// niezalecane; a domyslny auto-reconnect biblioteki bywa niewystarczajaco
// niezawodny (arduino-esp32 issue #653) — stad wlasna, jawna logika nizej
// (wifiReconnectNonBlocking(), wywolywana w loop()).
static const uint32_t WIFI_RECONNECT_MAX_MS = 30000;
uint32_t wifiOpoznienieReconnectMs = 1000;
unsigned long wifiOstatniaProbaReconnectMs = 0;

// ----------------------------------------------------------------------------
// TFT + DOTYK
// ----------------------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();

// [KROK 3 - sprite'y] Kazdy kafelek byl do tej pory rysowany 5 oddzielnymi
// wywolaniami WPROST na ekran (tlo, ramka, ikona, etykieta, wartosc) — kazde
// z nich to osobny blok danych wyslany po SPI, wiec przy przerysowaniu bylo
// widac krotki "przeblysk" (stare tlo znika, potem osobno pojawia sie ramka,
// osobno tekst...). sprKafelek to bufor w RAM (nie na ekranie) — caly kafelek
// skladamy w nim w pamieci, a na ekran trafia JEDNYM blokowym transferem
// (pushSprite), wiec z zewnatrz widac tylko gotowy, kompletny kafelek.
// Tworzony leniwie (przy pierwszym rysujKafelek) i tylko RAZ zmieniany
// rozmiarem, jesli w,h faktycznie sie zmienia (u nas w praktyce nigdy, bo
// wszystkie kafelki maja te sama geometrie kw x kh).
TFT_eSprite sprKafelek = TFT_eSprite(&tft);
int16_t sprKafelekW = 0, sprKafelekH = 0;
Preferences kalibracja;   // NVS namespace "dotyk" — kalibracja XPT2046

// --- Dotyk: WLASNY sterownik, osobna magistrala SPI (patrz FIX-DOTYK-OSOBNA-MAGISTRALA) ---
#define TOUCH_SCK   25
#define TOUCH_MOSI  26
#define TOUCH_MISO  33
#define TOUCH_CS    27
// Prog "nacisniecia" na podstawie zlozonego wzoru Z1+Z2 (patrz funkcja
// touchOdczytajSurowy nizej) — inna skala niz sam Z1, wiec wartosc startowa
// do potwierdzenia/dostrojenia na podstawie live debug w Serial.
#define TOUCH_Z_PROG 400

// Jesli po kalibracji okaze sie, ze ruch pozioma/pionowy jest zamieniony
// albo odwrocony wzgledem oczekiwanego — przelacz te flagi (nie trzeba
// przelutowywac niczego, to tylko zamiana interpretacji odczytu w kodzie).
#define TOUCH_SWAP_XY   0
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0

struct KalibracjaDotyku {
  uint16_t rawXA, rawYA;  // surowy odczyt w punkcie A (gorny-lewy znacznik)
  uint16_t rawXB, rawYB;  // surowy odczyt w punkcie B (dolny-prawy znacznik)
};
KalibracjaDotyku kalDotyku;

// Wspolrzedne znacznikow kalibracyjnych na ekranie (landscape 480x320)
const int16_t KAL_A_X = 30,  KAL_A_Y = 30;
const int16_t KAL_B_X = 450, KAL_B_Y = 290;

void touchSpiDelay() { delayMicroseconds(3); }

uint8_t touchXferByte(uint8_t out) {
  uint8_t in = 0;
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_MOSI, (out >> i) & 0x01);
    digitalWrite(TOUCH_SCK, LOW);
    touchSpiDelay();
    in <<= 1;
    if (digitalRead(TOUCH_MISO)) in |= 0x01;
    digitalWrite(TOUCH_SCK, HIGH);
    touchSpiDelay();
  }
  return in;
}

uint16_t touchReadChannel(uint8_t cmd) {
  digitalWrite(TOUCH_CS, LOW);
  touchSpiDelay();
  touchXferByte(cmd);
  uint8_t hi = touchXferByte(0x00);
  uint8_t lo = touchXferByte(0x00);
  digitalWrite(TOUCH_CS, HIGH);
  return ((uint16_t)hi << 8 | lo) >> 3;   // 12-bitowy wynik
}

// Mediana z 3 probek — odrzuca pojedyncze zaklocenia bez pelnego usredniania
uint16_t touchReadChannelStabilnie(uint8_t cmd) {
  uint16_t a = touchReadChannel(cmd);
  uint16_t b = touchReadChannel(cmd);
  uint16_t c = touchReadChannel(cmd);
  if (a > b) { uint16_t t = a; a = b; b = t; }
  if (b > c) { uint16_t t = b; b = c; c = t; }
  if (a > b) { uint16_t t = a; a = b; b = t; }
  return b;
}

void touchInicjalizujPiny() {
  pinMode(TOUCH_SCK, OUTPUT);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TOUCH_SCK, LOW);
  digitalWrite(TOUCH_MOSI, LOW);
}

// Surowy odczyt (bez kalibracji/mapowania) — zwraca true jesli wykryto nacisk.
// DEBOUNCE: wymaga kilku KOLEJNYCH probek nad progiem (nie jednej) - pojedyncze
// skoki Z ponizej progu 400 sa normalnym szumem tla nawet bez dotyku, wiec
// jedna probka nie wystarczy, zeby uznac to za realny dotyk.
#define TOUCH_WYMAGANE_PROBKI 4

bool touchOdczytajSurowy(uint16_t &rawX, uint16_t &rawY) {
  int licznik = 0;
  uint16_t x = 0, y = 0;
  for (int i = 0; i < TOUCH_WYMAGANE_PROBKI; i++) {
    uint16_t z1 = touchReadChannel(0xB0);
    uint16_t z2 = touchReadChannel(0xC0);
    int32_t z = (4095 - (int32_t)z2) + (int32_t)z1;
    x = touchReadChannel(0x90);
    y = touchReadChannel(0xD0);
    if (z >= TOUCH_Z_PROG) {
      licznik++;
    } else {
      licznik = 0;   // przerwij serie - potrzeba KOLEJNYCH probek, nie sumy
      break;
    }
  }
  rawX = x;
  rawY = y;
  return licznik >= TOUCH_WYMAGANE_PROBKI;
}

// Zbiera kilka probek POZYCJI w trakcie trwania dotyku i zwraca median —
// pojedynczy odczyt X/Y w momencie samego wykrycia dotyku bywa niestabilny
// (np. na styku "dotkniecie/puszczenie" panelu rezystancyjnego), wiec kilka
// probek + mediana daje dużo pewniejszy wynik. Jesli dotyk zniknie w trakcie
// zbierania (falszywy alarm), zwraca false.
bool touchPobierzStabilnaPozycje(uint16_t &outX, uint16_t &outY) {
  const int PROBKI = 5;
  uint16_t xs[PROBKI], ys[PROBKI];
  for (int i = 0; i < PROBKI; i++) {
    uint16_t rx, ry;
    if (!touchOdczytajSurowy(rx, ry)) return false;   // dotyk znikl - falszywy alarm
    xs[i] = rx; ys[i] = ry;
    delay(15);
  }
  // sortowanie babelkowe (tablica jest mala, wydajnosc bez znaczenia)
  for (int i = 0; i < PROBKI; i++)
    for (int j = i + 1; j < PROBKI; j++) {
      if (xs[j] < xs[i]) { uint16_t t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
      if (ys[j] < ys[i]) { uint16_t t = ys[i]; ys[i] = ys[j]; ys[j] = t; }
    }
  outX = xs[PROBKI / 2];
  outY = ys[PROBKI / 2];
  return true;
}

// Odczyt z zastosowaniem kalibracji — zwraca wspolrzedne piksela ekranu
bool dotykOdczytaj(int16_t &px, int16_t &py) {
  // Zabezpieczenie: jesli kalibracja jest zdegenerowana (oba punkty
  // identyczne/za blisko), map() dzielilby przez zero - olej dotyk zamiast
  // zwracac smieci.
  if (abs((int)kalDotyku.rawXB - (int)kalDotyku.rawXA) < 50 ||
      abs((int)kalDotyku.rawYB - (int)kalDotyku.rawYA) < 50) {
    return false;
  }

  uint16_t rawX, rawY;
  if (!touchOdczytajSurowy(rawX, rawY)) return false;

  if (TOUCH_SWAP_XY) { uint16_t t = rawX; rawX = rawY; rawY = t; }

  long x = map(rawX, kalDotyku.rawXA, kalDotyku.rawXB, KAL_A_X, KAL_B_X);
  long y = map(rawY, kalDotyku.rawYA, kalDotyku.rawYB, KAL_A_Y, KAL_B_Y);

  if (TOUCH_INVERT_X) x = 480 - x;
  if (TOUCH_INVERT_Y) y = 320 - y;

  px = constrain(x, 0, 479);
  py = constrain(y, 0, 319);
  return true;
}

// Podswietlenie (LED) na pinie TFT_BL (GPIO21, patrz platformio.ini) — PRZEZ
// REZYSTOR ~100-220R w szeregu miedzy GPIO a pinem LED na module, nie na styk.
// Sterowane PWM zamiast na sztywno do 3.3V, wiec mozna przygasic jesli za jasno.
void ustawJasnoscPodswietlenia(uint8_t procent) {
  procent = constrain(procent, (uint8_t)0, (uint8_t)100);
  ledcWrite(TFT_BL, map(procent, 0, 100, 0, 255));
}

// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Jasnosc podswietlenia (dzien/
// noc) + automatyczne przyciemnienie w oknie godzinowym. W ODROZNIENIU od
// WSZYSTKICH innych ustawien w tym pliku (POLA[]/wybor/trybSerwa/
// antystopWlaczony) te wartosci NIE naleza do centrali - sa czysto lokalna
// wlasnoscia TEJ PLYTKI (fizyczne podswietlenie TFT_BL), wiec nie ma tu
// zadnego httpPost()/wyslijTrybPracy(). Zamiast tego zapisujemy je w lokalnej
// pamieci NVS (Preferences, DOKLADNIE ten sam mechanizm co `kalibracja` dla
// dotyku wyzej - patrz kalibrujDotykJesliTrzeba()) i stosujemy bezposrednio
// przez ustawJasnoscPodswietlenia() powyzej.
//
// Ta plytka NIE MA wlasnego RTC (patrz komentarz przy stan.godzina nizej) -
// "aktualna godzina" do porownania z oknem przyciemnienia jest wiec zawsze
// OSTATNIA znana godzina, badz to prawdziwa z centrali (stan.godzina w
// pobierzStatus()), badz symulowana lokalnie (symGodzina w symOdswiezDoLvgl(),
// gdy LVGL_POC_SYMULACJA=1). Zamiast przekazywac ja jako parametr do kazdej
// funkcji ponizej (i musiec pamietac, ktora galaz akurat jest aktywna),
// obie galezie zapisuja ja tutaj, w DWOCH wspolnych zmiennych
// (wyswietlaczOstatniaGodzina/wyswietlaczGodzinaZnana) - patrz wywolania
// wyswietlaczHarmonogramOdswiez() w pobierzStatus() i symOdswiezDoLvgl().
Preferences ustawieniaWyswietlacza;   // NVS namespace "wyswietlacz"
uint8_t jasnoscDziennaWartosc = 100;  // %, 0-100 - jasnosc POZA oknem przyciemnienia
uint8_t jasnoscNocnaWartosc   = 30;   // %, 0-100 - jasnosc WEWNATRZ okna przyciemnienia
uint8_t godzinaStartPrzyciemnienia  = 22; // 0-23 - godzina rozpoczecia okna
uint8_t godzinaKoniecPrzyciemnienia = 6;  // 0-23 - godzina zakonczenia okna
bool    harmonogramWlaczonyWartosc  = true; // false = zawsze jasnoscDzienna, godziny ignorowane

int  wyswietlaczOstatniaGodzina = 0;     // ostatnia znana godzina (real. lub sym.), patrz komentarz wyzej
bool wyswietlaczGodzinaZnana    = false; // false dopoki 1. pobierzStatus()/symOdswiezDoLvgl() nie ustawi
bool wyswietlaczPrzyciemnionyTeraz = false; // cache "co jest aktualnie zastosowane", zeby nie spamowac ledcWrite()

void wyswietlaczWczytajZPamieci() {
  // [ROZNICA wzgledem kalibracji] `kalibracja` (wyzej) robi .end() zaraz po
  // jednorazowym uzyciu w setup() - my celowo NIE wolamy .end() tutaj, bo w
  // odroznieniu od kalibracji dotyku (ktora pisze tylko RAZ, w trakcie
  // interaktywnej kalibracji) ten obiekt musi zostac otwarty przez CALY czas
  // dzialania programu - wyswietlaczJasnoscWyslij()/..._ustawieniaWyslij()/
  // ..._harmonogramWyslij() moga dopisac do niego w KAZDEJ chwili, gdy user
  // dotknie panelu WYSWIETLACZ (minuty czy godziny po starcie).
  ustawieniaWyswietlacza.begin("wyswietlacz", false);
  jasnoscDziennaWartosc = ustawieniaWyswietlacza.getUChar("jasnDzien", 100);
  jasnoscNocnaWartosc   = ustawieniaWyswietlacza.getUChar("jasnNoc", 30);
  godzinaStartPrzyciemnienia  = ustawieniaWyswietlacza.getUChar("godzStart", 22);
  godzinaKoniecPrzyciemnienia = ustawieniaWyswietlacza.getUChar("godzKoniec", 6);
  harmonogramWlaczonyWartosc  = ustawieniaWyswietlacza.getBool("harmonWlacz", true);
  // Bezpieczne klipsy na wypadek uszkodzonych/przedawnionych danych w NVS -
  // te same zakresy co suwaki/spinboxy w dashboard_lvgl_poc.cpp.
  jasnoscDziennaWartosc = constrain(jasnoscDziennaWartosc, (uint8_t)0, (uint8_t)100);
  jasnoscNocnaWartosc   = constrain(jasnoscNocnaWartosc,   (uint8_t)0, (uint8_t)100);
  godzinaStartPrzyciemnienia  = constrain(godzinaStartPrzyciemnienia,  (uint8_t)0, (uint8_t)23);
  godzinaKoniecPrzyciemnienia = constrain(godzinaKoniecPrzyciemnienia, (uint8_t)0, (uint8_t)23);
}

// Czy podana godzina (0-23) miesci sie w oknie od start do koniec, koniec
// WYLACZNIE (standardowy polprzedzial czasu). Okno MOZE "zawijac sie" przez
// polnoc (start > koniec, np. 22..6, najbardziej typowy przypadek "wieczor do
// rana") - wtedy godzina jest W OKNIE gdy jest >=start LUB <koniec (suma, nie
// przeciecie przedzialow). Gdy start==koniec, okno obejmuje CALA dobe (24/24)
// - najprostsza, przewidywalna konwencja dla tego brzegowego przypadku.
bool wyswietlaczCzyPrzyciemniona(int godzina) {
  int s = godzinaStartPrzyciemnienia, k = godzinaKoniecPrzyciemnienia;
  if (s == k) return true;
  if (s < k) return (godzina >= s) && (godzina < k);
  return (godzina >= s) || (godzina < k);
}

// Wolane po kazdej swiezej wartosci godziny (patrz wywolania w
// pobierzStatus() i symOdswiezDoLvgl(), oraz z "mostow" ponizej po kazdej
// recznej zmianie na panelu). Idempotentne i tanie (ledcWrite tylko przy
// realnej ZMIANIE stanu dzien/noc, patrz wyswietlaczPrzyciemnionyTeraz), wiec
// bezpieczne do wolania co kilka sekund. Godzina nieznana
// (!wyswietlaczGodzinaZnana, np. tuz po starcie zanim przyjdzie 1. odpowiedz
// centrali) -> nie zgadujemy, zostaw jasnosc jak jest.
void wyswietlaczHarmonogramOdswiez() {
  if (!harmonogramWlaczonyWartosc || !wyswietlaczGodzinaZnana) {
    if (wyswietlaczPrzyciemnionyTeraz) { // wracamy z ew. przyciemnienia (np. user wlasnie wylaczyl harmonogram)
      wyswietlaczPrzyciemnionyTeraz = false;
      ustawJasnoscPodswietlenia(jasnoscDziennaWartosc);
    }
    return;
  }
  bool przyciemniona = wyswietlaczCzyPrzyciemniona(wyswietlaczOstatniaGodzina);
  if (przyciemniona != wyswietlaczPrzyciemnionyTeraz) {
    wyswietlaczPrzyciemnionyTeraz = przyciemniona;
    ustawJasnoscPodswietlenia(przyciemniona ? jasnoscNocnaWartosc : jasnoscDziennaWartosc);
  }
}

// [LVGL-API] Lokalny "most" dla panelu WYSWIETLACZ - w odroznieniu od
// WSZYSTKICH innych mostow w tym pliku (pompaManualWyslij() i dalej) NIE ma
// tu HTTP: zmiana trafia prosto do NVS + na sprzet (PWM), efekt jest wiec
// natychmiastowy i zawsze "prawdziwy" (nic nie moze "nie dojsc" jak przy
// POST do centrali, wiec nie ma czego czekac/potwierdzac kolejnym
// pobierzStatus()). Te same 3 funkcje sa rejestrowane w OBU galeziach
// setup() (symulacja i prawdziwa centrala) - w odroznieniu od reszty
// callbackow w tym pliku, tu nie ma czego symulowac osobno, bo podswietlenie
// tej plytki dziala identycznie niezaleznie od tego, czy centrala jest
// akurat podlaczona.
//
// `id`: 0=jasnoscDzien, 1=jasnoscNoc (ten sam ksztalt co serwoPozycjaWyslij
// dla klapa/syberek).
void wyswietlaczJasnoscWyslij(int id, int procent, bool koncowa) {
  procent = constrain(procent, 0, 100);
  if (id == 0) jasnoscDziennaWartosc = (uint8_t)procent;
  else         jasnoscNocnaWartosc   = (uint8_t)procent;
  // [FIX, ta sesja - "jasnosc widoczna dopiero po puszczeniu"] PWM ponizej
  // wykonuje sie przy KAZDYM wywolaniu (rowniez w trakcie przeciagania) -
  // tanie. Zapis do NVS (patrz `if (koncowa)` ponizej) TYLKO na koncu gestu -
  // flash ma ograniczona liczbe cykli kasowania sektora.
  //
  // Zastosuj NATYCHMIAST tylko jesli edytowany poziom jest AKURAT aktywny -
  // inaczej user zobaczylby chwilowy, mylacy "przeskok" jasnosci przy edycji
  // poziomu, ktory teraz i tak nie jest uzywany (np. ustawia jasnosc nocna w
  // srodku slonecznego dnia).
  bool celujeWAktywny = (id == 0) ? !wyswietlaczPrzyciemnionyTeraz : wyswietlaczPrzyciemnionyTeraz;
  if (celujeWAktywny) ustawJasnoscPodswietlenia((uint8_t)procent);
  if (koncowa) {
    ustawieniaWyswietlacza.putUChar(id == 0 ? "jasnDzien" : "jasnNoc", (uint8_t)procent);
  }
}

void wyswietlaczHarmonogramWyslij(int wlaczony) {
  harmonogramWlaczonyWartosc = (wlaczony != 0);
  ustawieniaWyswietlacza.putBool("harmonWlacz", harmonogramWlaczonyWartosc);
  wyswietlaczHarmonogramOdswiez(); // przelicz i zastosuj od razu wzgledem ostatniej znanej godziny
}

void wyswietlaczUstawieniaWyslij(const char *pole, long wartosc) {
  uint8_t godz = (uint8_t)constrain(wartosc, 0L, 23L);
  if (strcmp(pole, "godzStart") == 0) {
    godzinaStartPrzyciemnienia = godz;
    ustawieniaWyswietlacza.putUChar("godzStart", godz);
  } else if (strcmp(pole, "godzKoniec") == 0) {
    godzinaKoniecPrzyciemnienia = godz;
    ustawieniaWyswietlacza.putUChar("godzKoniec", godz);
  }
  wyswietlaczHarmonogramOdswiez(); // granice okna sie zmienily - przelicz i zastosuj od razu
}

// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Piezo (najlepiej PASYWNY -
// patrz "UWAGA SPRZETOWA" nizej) na pinie 13, sterowany PWM tak samo jak
// podswietlenie (ledcAttach/ledcWriteTone), tylko ton zamiast jasnosci. Gra
// dwa RZECZYWISCIE UZYWANE, rozpoznawalne wzorce alarmowe zamiast jednego
// monotonnego piknia - research (na prosbe z tej sesji) ponizej.
//
// UWAGA SPRZETOWA: kod zaklada brzeczyk PASYWNY (bez wlasnego oscylatora -
// gra dokladnie te czestotliwosc, ktora dostanie z ledcWriteTone()). Z
// brzeczykiem AKTYWNYM (ma wlasny oscylator, ignoruje czestotliwosc, gra
// zawsze swoim jednym stalym tonem) kod i tak zadziala - RYTM obu wzorcow
// (patrz nizej) zostaje w pelni slyszalny, ginie tylko roznica WYSOKOSCI
// dzwieku miedzy POZAREM a OGRZEWANIEM. Pasywny da pelny efekt.
//
// RESEARCH - skad te konkretne wzorce (nie wymyslone, dwa prawdziwe,
// standaryzowane sygnaly alarmowe o swiadomie innym "charakterze"):
// 1. POZAR/DYM = "Temporal-Three" (T3) - miedzynarodowy standard alarmow
//    pozarowych (NFPA 72 / ISO 8201 / ANSI S3.41): 3x 0.5s ton + 0.5s cisza,
//    potem 1.5s ciszy, cykl 4.0s. To DOKLADNIE ten rytm z prawdziwych czujek
//    dymu - rozpoznawalny instynktownie, bez zastanowienia. Oficjalna norma
//    ustala tez czestotliwosc na 520Hz (nizsze tony skuteczniej budza ze
//    snu), ale to zalozenie dla dedykowanych syren - maly, tani brzeczyk
//    piezo (jak tutaj) jest znacznie glosniejszy w swoim naturalnym pasmie
//    rezonansu ok. 3-4kHz, wiec zostaje sam RYTM T3 (najbardziej
//    rozpoznawalna czesc wzorca), a czestotliwosc jest dobrana praktycznie
//    pod ten konkretny, tani podzespol.
// 2. OGRZEWANIE/PANEL = dwuton "hi-lo" (europejska syrena typu "nee-naw") -
//    ciagle, plynne przelaczanie miedzy dwiema stalymi czestotliwosciami co
//    ok. 0.5s, BEZ przerw w dzwieku. Celowo skrajnie INNY charakter niz
//    pozar (ciagly zamiast urywanego, nizszy zakres tonow) - zeby od razu,
//    bez patrzenia na ekran, slyszec "to NIE pozar, ale cos wymaga
//    sprawdzenia".
//
// Priorytet gdy kilka alarmow naraz: POZAR (dymAlarm) > OGRZEWANIE/PANEL >
// cisza - patrz brzeczykOdswiezZAlarmow() nizej.
#define BRZECZYK_PIN 13

struct BrzeczykFaza { uint16_t czestotliwoscHz; uint16_t czasMs; }; // czestotliwoscHz=0 -> cisza w tej fazie

static const BrzeczykFaza WZORZEC_POZAR[] = {
  {3000, 500}, {0, 500}, {3000, 500}, {0, 500}, {3000, 500}, {0, 1500},
};
static const BrzeczykFaza WZORZEC_OGRZEWANIE[] = {
  {1200, 500}, {1600, 500},
};

enum BrzeczykAlarmTyp { BRZECZYK_BRAK, BRZECZYK_POZAR, BRZECZYK_OGRZEWANIE };
BrzeczykAlarmTyp brzeczykAktywnyTyp = BRZECZYK_BRAK;

// [EKRAN ALARMOWY, ta sesja] Wyciszenie z przycisku WYCISZ (dashboard_lvgl_poc.cpp) -
// patrz alarmWyciszObsluga()/brzeczykOdswiezZAlarmow() nizej i
// PLAN_EKRAN_ALARMOWY.md. Powiazane z KONKRETNYM typem, nie globalnym
// przelacznikiem - g_brzeczykWyciszonyTyp pamieta, KTORY typ user wyciszyl,
// zeby wyciszenie samo przestalo obowiazywac, gdy chciany typ sie zmieni
// (alarm ustapi albo eskaluje na inny/gorszy).
static bool g_brzeczykWyciszony = false;
static BrzeczykAlarmTyp g_brzeczykWyciszonyTyp = BRZECZYK_BRAK;
int  brzeczykFazaIdx = 0;
unsigned long brzeczykFazaStartMs = 0;

const BrzeczykFaza *brzeczykWzorzec(BrzeczykAlarmTyp typ, int &liczbaFaz) {
  if (typ == BRZECZYK_POZAR) {
    liczbaFaz = sizeof(WZORZEC_POZAR) / sizeof(WZORZEC_POZAR[0]);
    return WZORZEC_POZAR;
  }
  liczbaFaz = sizeof(WZORZEC_OGRZEWANIE) / sizeof(WZORZEC_OGRZEWANIE[0]);
  return WZORZEC_OGRZEWANIE;
}

void brzeczykZastosujFaze() {
  int liczbaFaz;
  const BrzeczykFaza *wzorzec = brzeczykWzorzec(brzeczykAktywnyTyp, liczbaFaz);
  ledcWriteTone(BRZECZYK_PIN, wzorzec[brzeczykFazaIdx].czestotliwoscHz);
}

void brzeczykUstawTyp(BrzeczykAlarmTyp nowy) {
  if (nowy == brzeczykAktywnyTyp) return; // bez zmian - nie przerywaj w polowie juz granej frazy
  brzeczykAktywnyTyp = nowy;
  brzeczykFazaIdx = 0;
  brzeczykFazaStartMs = millis();
  if (brzeczykAktywnyTyp == BRZECZYK_BRAK) ledcWriteTone(BRZECZYK_PIN, 0);
  else brzeczykZastosujFaze(); // wejdz w faze 0 NATYCHMIAST, nie czekaj na 1. obrot obsluzBrzeczyk()
}

// Wolane po kazdej swiezej wartosci alarmow (patrz wywolania w
// pobierzStatus() i symOdswiezDoLvgl()) - decyduje KTORY wzorzec (jesli
// ktorykolwiek) powinien teraz grac. Idempotentne - brzeczykUstawTyp() sam
// pilnuje, zeby nie przerywac juz granego wzorca bez potrzeby.
void brzeczykOdswiezZAlarmow(bool dym, bool ogrzewanie, bool panel) {
  BrzeczykAlarmTyp chciany = dym ? BRZECZYK_POZAR
                            : (ogrzewanie || panel) ? BRZECZYK_OGRZEWANIE
                            : BRZECZYK_BRAK;
  // [EKRAN ALARMOWY, ta sesja] Jesli user wyciszyl DOKLADNIE ten typ, ktory
  // wlasnie chcemy zagrac - podstaw cisze. W chwili, gdy `chciany` przestanie
  // byc rowne zapamietanemu g_brzeczykWyciszonyTyp (alarm ustapil albo
  // eskalowal na inny typ), ten warunek sam przestaje byc prawdziwy - zero
  // recznego zerowania flagi gdziekolwiek indziej. Patrz alarmWyciszObsluga().
  // [v1.2.1] Wyciszenie dotyczy TEGO wystapienia alarmu. Do v1.2.0 obie
  // zmienne nie byly nigdy zerowane, wiec raz wyciszony typ alarmu zostawal
  // niemy takze przy KAZDYM nastepnym wystapieniu - az do restartu panelu
  // (alarm ustapil, wrocil po godzinie: cisza). Od v1.2.1 stan alarmu
  // przechodzi tez przez samoczynny restart, wiec bez tej poprawki takie
  // wyciszenie nie konczyloby sie juz nigdy.
  if (g_brzeczykWyciszony && chciany != g_brzeczykWyciszonyTyp) {
    g_brzeczykWyciszony = false;
    g_brzeczykWyciszonyTyp = BRZECZYK_BRAK;
  }
  if (g_brzeczykWyciszony && chciany == g_brzeczykWyciszonyTyp) {
    chciany = BRZECZYK_BRAK;
  }
  brzeczykUstawTyp(chciany);
}

// [EKRAN ALARMOWY, ta sesja] Zarejestrowana jako AlarmWyciszCb (patrz setup(),
// obie galezie #if LVGL_POC_SYMULACJA) - wolana z przycisku WYCISZ na nowym
// ekranie alarmowym w dashboard_lvgl_poc.cpp. W 100% lokalna: brzeczyk to
// GPIO na TEJ plytce, wiec zero HTTP do centrali - w odroznieniu od
// wiekszosci pozostalych callbackow w tym pliku.
// [v1.2.1] Cisza zapada OD RAZU. Do v1.2.0 ta funkcja tylko ustawiala dwie
// zmienne, a brzeczyk milkl dopiero w brzeczykOdswiezZAlarmow(), czyli po
// nastepnym poprawnym statusie z centrali. Skutki: (1) bez danych z centrali
// WYCISZ nie dzialal wcale - a wlasnie wtedy alarm zostaje "zamrozony" i gra
// dalej, takze po samoczynnym restarcie (alarmPrzywrocPoAutorestarcie());
// (2) drugie nacisniecie WYCISZ, gdy juz bylo cicho, zapamietywalo "wyciszony
// typ: zaden" i przy kolejnym statusie alarm zaczynal grac od nowa.
void alarmWyciszObsluga() {
  if (brzeczykAktywnyTyp == BRZECZYK_BRAK) return;   // nic nie gra - poprzednie wyciszenie zostaje bez zmian
  g_brzeczykWyciszony = true;
  g_brzeczykWyciszonyTyp = brzeczykAktywnyTyp;
  brzeczykUstawTyp(BRZECZYK_BRAK);
}

// Wolane co obrot loop() (ten sam wzorzec co obsluzDotyk()) - awansuje
// wzorzec do kolejnej fazy, gdy czas biezacej fazy uplynal. ledcWriteTone()
// wolane TYLKO przy realnej zmianie fazy, NIE w kazdym obrocie petli (patrz
// arduino-esp32 issue #3617 - zbyt czeste wywolania ledcWriteTone() daja
// niedokladny dzwiek).
void obsluzBrzeczyk() {
  if (brzeczykAktywnyTyp == BRZECZYK_BRAK) return;
  int liczbaFaz;
  const BrzeczykFaza *wzorzec = brzeczykWzorzec(brzeczykAktywnyTyp, liczbaFaz);
  if (millis() - brzeczykFazaStartMs >= wzorzec[brzeczykFazaIdx].czasMs) {
    brzeczykFazaIdx = (brzeczykFazaIdx + 1) % liczbaFaz;
    brzeczykFazaStartMs = millis();
    brzeczykZastosujFaze();
  }
}

void brzeczykZainicjuj() {
  ledcAttach(BRZECZYK_PIN, 3000, 8); // czestotliwosc startowa/rozdzielczosc - ledcWriteTone() i tak zmienia freq w locie
  ledcWriteTone(BRZECZYK_PIN, 0);    // cisza na starcie
}

// Motyw kolorow — spojny z panelem WWW centrali (ciemne tlo, akcent ognisty)
#define KOL_TLO       0x0801   // ciemny granat/czern
#define KOL_KARTA     0x10A3
#define KOL_RAMKA     0x2A8B
#define KOL_TEKST     0xC638
#define KOL_TEKST_DIM 0x4A69
#define KOL_OGIEN     0x07FF   // cyan (jak --flame w panelu WWW)
#define KOL_OK        0x4726   // zielony
#define KOL_WARN      0xFDA0   // bursztyn
#define KOL_ERR       0xF8B2   // czerwony
#define KOL_SNIEG     0xAEDC   // jasny blekit — ikona sniezynki (zima)
#define KOL_KROPLA    0x4C9F   // niebieski — ikona kropli (pompa)
#define KOL_SUWAK     0xFD20   // pomaranczowy — ikony suwakow (klapa/syberka), jak w makiecie referencyjnej
#define KOL_ZAR       0xFA80   // czerwono-pomaranczowy — ikony plomienia/termometru (bojler, ogrzewanie, panel)
#define KOL_CHMURKA   0x2E7F   // turkusowy — ikona chmury/sniegu (zewn)

// ----------------------------------------------------------------------------
// [UI] PROSTE IKONY WEKTOROWE — rysowane wprost prymitywami TFT_eSPI (brak
// bitmap/dekodera = zero dodatkowego miejsca we flash, spojne z reszta UI).
// Kazda przyjmuje (x,y) = SRODEK ikony i "r" = promien/polowa rozmiaru, zeby
// wszystkie ladnie centrowaly sie w tym samym miejscu niezaleznie od ksztaltu.
// ----------------------------------------------------------------------------
// [KROK 3] Wszystkie ikony przyjmuja teraz "gfx" (referencja do TFT_eSPI LUB
// TFT_eSprite — TFT_eSprite dziedziczy po TFT_eSPI, wiec ten sam kod dziala
// z obiema powierzchniami) zamiast zawsze rysowac na globalnym tft. Dzieki
// temu te same funkcje ikon dzialaja bez zmian teraz, gdy kafelki renderuja
// sie do sprite'a w pamieci, a nie bezposrednio na ekran.
void rysujIkonaSlonce(TFT_eSPI &gfx, int16_t cx, int16_t cy, int16_t r, uint16_t kolor) {
  gfx.fillCircle(cx, cy, r / 2, kolor);
  // 8 promyczkow dookola — proste linie od krawedzi kola na zewnatrz
  for (int i = 0; i < 8; i++) {
    float kat = i * (PI / 4.0f);
    int16_t x1 = cx + (int16_t)((r / 2 + 2) * cosf(kat));
    int16_t y1 = cy + (int16_t)((r / 2 + 2) * sinf(kat));
    int16_t x2 = cx + (int16_t)(r * cosf(kat));
    int16_t y2 = cy + (int16_t)(r * sinf(kat));
    gfx.drawLine(x1, y1, x2, y2, kolor);
  }
}

void rysujIkonaSnieg(TFT_eSPI &gfx, int16_t cx, int16_t cy, int16_t r, uint16_t kolor) {
  // 3 linie przez srodek (co 60 stopni) + krotkie "galazki" na koncach —
  // klasyczny uproszczony symbol sniezynki, tani w rysowaniu.
  for (int i = 0; i < 3; i++) {
    float kat = i * (PI / 3.0f);
    int16_t x1 = cx - (int16_t)(r * cosf(kat)), y1 = cy - (int16_t)(r * sinf(kat));
    int16_t x2 = cx + (int16_t)(r * cosf(kat)), y2 = cy + (int16_t)(r * sinf(kat));
    gfx.drawLine(x1, y1, x2, y2, kolor);
    float katGal = PI / 5.0f;
    auto galazka = [&](int16_t bx, int16_t by, float kierunek) {
      int16_t gx1 = bx + (int16_t)((r / 3) * cosf(kierunek + katGal));
      int16_t gy1 = by + (int16_t)((r / 3) * sinf(kierunek + katGal));
      int16_t gx2 = bx + (int16_t)((r / 3) * cosf(kierunek - katGal));
      int16_t gy2 = by + (int16_t)((r / 3) * sinf(kierunek - katGal));
      gfx.drawLine(bx, by, gx1, gy1, kolor);
      gfx.drawLine(bx, by, gx2, gy2, kolor);
    };
    galazka(x2, y2, kat);
    galazka(x1, y1, kat + PI);
  }
}

void rysujIkonaKropla(TFT_eSPI &gfx, int16_t cx, int16_t cy, int16_t r, uint16_t kolor) {
  gfx.drawBitmap(cx - IKONA_ROZMIAR / 2, cy - IKONA_ROZMIAR / 2, IKONA_KROPLA, IKONA_ROZMIAR, IKONA_ROZMIAR, kolor);
}

void rysujIkonaSuwak(TFT_eSPI &gfx, int16_t cx, int16_t cy, int16_t r, uint16_t kolor) {
  gfx.drawBitmap(cx - IKONA_ROZMIAR / 2, cy - IKONA_ROZMIAR / 2, IKONA_SUWAK, IKONA_ROZMIAR, IKONA_ROZMIAR, kolor);
}

void rysujIkonaPlomien(TFT_eSPI &gfx, int16_t cx, int16_t cy, int16_t r, uint16_t kolor) {
  gfx.drawBitmap(cx - IKONA_ROZMIAR / 2, cy - IKONA_ROZMIAR / 2, IKONA_PLOMIEN, IKONA_ROZMIAR, IKONA_ROZMIAR, kolor);
}

void rysujIkonaAlarm(TFT_eSPI &gfx, int16_t cx, int16_t cy, int16_t r, uint16_t kolor) {
  gfx.drawBitmap(cx - IKONA_ROZMIAR / 2, cy - IKONA_ROZMIAR / 2, IKONA_ALARM, IKONA_ROZMIAR, IKONA_ROZMIAR, kolor);
}

// ----------------------------------------------------------------------------
// [ANIMACJA-PRZYCISKU] Krotki, celowo blokujacy "flash" stanu wcisnietego —
// zamiast utrzymywac osobny stan "wcisniety/puszczony" przez cala petle
// dotyku (co wymagaloby przebudowy obsluzDotyk()), rysujemy przycisk w
// stylu wcisnietym NATYCHMIAST po wykryciu dotkniecia (zbocze narastajace),
// czekamy krotko — na tyle dlugo, zeby oko zdazylo to zarejestrowac jako
// realna reakcje (typowy prog postrzeganej "natychmiastowosci" UI to rzedu
// 100ms), po czym wywolujacy i tak zaraz rysuje normalny ekran z juz
// zaktualizowanym stanem (rysujAktualnyEkran()) — to naturalnie daje efekt
// "wcisniety -> puszczony". Prosty, przewidywalny, spojny ze stylem reszty
// tego pliku (ktory juz uzywa delay() przy krotkich komunikatach blokujacych,
// np. blad wysylki ustawienia).
void animujWcisniecie(int16_t x, int16_t y, int16_t w, int16_t h) {
  tft.fillRoundRect(x, y, w, h, 6, KOL_OGIEN);
  tft.drawRoundRect(x, y, w, h, 6, KOL_TEKST);
  delay(70);
}

// ----------------------------------------------------------------------------
// STAN POBIERANY Z CENTRALI
// ----------------------------------------------------------------------------
struct StanPieca {
  bool  polaczono = false;
  String ip;
  int   rssi = 0;
  float tZewn = 0, tBojler = 0, tOgrz = 0, tOgrzSr = 0, tPanel = 0, tPokoj = 0, tTrociny = 0, tPowrot = 0;
  float wilgotnosc = 0, cisnienie = 0;
  int   dym = 0;
  bool  dymAlarm = false;
  int   klapa = 0, syberka = 0;
  bool  pompa = false;
  // [LVGL-API] Minuty do cichego powrotu pompy do AUTO (0 = brak override,
  // automatyka centrali decyduje) - centrala liczy to sama (pompaManualOverrideUntil),
  // LCD tylko odczytuje, zeby nie miec wlasnego, rozjezdzajacego sie timera.
  int   pompaOverrideMin = 0;
  bool  alarmOgrzewanie = false, alarmPanel = false;
  // [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Z centrali (/api/status pola
  // deszcz_stan/deszcz_min, v2.13.2) - 0=brak opadu w widocznym oknie,
  // 1=nadciaga (deszczMinuty=za ile zacznie), 2=pada teraz (deszczMinuty=za
  // ile skonczy, -1=nieznane w oknie 6h). Domyslne 0/-1 dopasowane do tego,
  // co zwraca centrala, gdy jeszcze nie zdazyla policzyc pierwszej prognozy
  // (identyczny duch co reszta domyslnych wartosci w tej strukturze).
  int   deszczStan = 0, deszczMinuty = -1;
  // [LCD-SEZON] Z centrali (RTC sprzetowy tam, nie tutaj — wyswietlacz nie
  // ma wlasnego RTC z podtrzymaniem, dziala na baterii/deep sleep) — patrz
  // aktualizujSezon() nizej.
  int   miesiac = 0;      // 1-12, albo 0 gdy centrala zglasza brak RTC
  bool  rtcOk = false;
  // [LVGL-DATA-CZAS, ta sesja] Kafelek DATA I CZAS - godzina/minuta/dzien/rok,
  // ten sam zrodlo (RTC centrali) i ten sam fallback (0 gdy !rtcOk) co miesiac.
  int   godzina = 0, minuta = 0, dzien = 0, rok = 0;
  // [UI-INTERNET, ta sesja] Dostepnosc internetu (fbAppReady na centrali) -
  // odrozniona od `polaczono` (to ostatnie oznacza tylko "LCD ma kontakt z
  // centrala przez lokalny WiFi", nie mowi nic o tym, czy sama centrala ma
  // internet). Ten sam wzorzec co rssi: centrala liczy/mierzy, LCD tylko
  // odczytuje z /api/status.
  bool  internet = false;
  // [OTA-PANEL, ta sesja] Oferta aktualizacji z centrali (/api/status) — patrz
  // otaKrok() nizej i TG_UPDATE_PANEL w main_centrala.cpp.
  // [v1.4.0 OTA] Stale tablice zamiast String (bez przydzialu pamieci co 2 s)
  // i dwa nowe pola od centrali 3.58.6.
  bool     panelOtaDostepna = false;
  char     panelOtaWersja[32] = "";   // "panel_ota_wersja": tag wydania, z ktorego jest plik
  char     panelOtaObraz[17] = "";    // "panel_ota_obraz": identyfikator pliku; "" = centrala do 3.58.5 (nie podaje)
  uint32_t panelOtaNr = 0;            // "panel_ota_nr": numer oferty (centrala 3.58.6+)
};
StanPieca stan;

// [OTA-PANEL, ta sesja] Blokuje wejscie w deep sleep na czas pobierania/
// flashowania panelu (prompt #8 — patrz idzSpac() nizej). Zadeklarowane TUTAJ
// (a nie przy funkcjach OTA, blisko setup()) bo idzSpac() jest zdefiniowane
// wczesniej w pliku i musi juz ja widziec.
// [v1.4.0 OTA] Ustawiana przez otaZacznij(), zdejmowana przez
// otaSprzatnijPoProbie(). Wstrzymuje tez panelRatunekKrok().
bool trwaAktualizacjaOtaPanelu = false;

// [v1.4.0 OTA] Stan aktualizacji przez centrale. Zmienne sa TUTAJ (a nie przy
// funkcjach OTA blizej setup()), bo pobierzStatus() i odczyt odpowiedzi
// centrali, zdefiniowane wczesniej w pliku, juz z nich korzystaja.
static char otaObrazWlasny[17] = "";        // identyfikator programu, z ktorego panel pracuje ("" = nie dalo sie odczytac)
static bool otaObrazZatwierdzony = true;    // false = pierwszy start po wgraniu przez OTA, partycja jeszcze nie zatwierdzona
static diag::OtaZapis otaZapis = {};        // co panel zrobil z ostatnia oferta (kopia zapisu z NVS)
// To, co panel dokleja do adresu zapytan /api/status (pobierzStatus()):
static bool     otaZglosOdmowe = false;     // &panel_ota_odmowa=<nr>-<powod>
static uint32_t otaOdmowaNr = 0;
static uint8_t  otaOdmowaPowod = diag::OTA_POWOD_BRAK;
static char     otaCzekaPowod[16] = "";     // &panel_ota_czeka=<powod>; "" = bez tego parametru
// Trwajace pobieranie.
struct OtaPobieranie {
  bool          trwa;
  diag::OtaSkan skan;               // pierwsze 24 i ostatnie 32 bajty pliku
  uint8_t       blad;               // diag::OtaPowod; OTA_POWOD_BRAK = dotad bez bledu
  bool          ostateczny;         // blad, przy ktorym ponawianie nie ma sensu
  const char   *opisBledu;          // do logu
  bool          zapisZaczety;       // Update.begin() sie udal
  uint32_t      zapisano;           // bajty oddane do Update.write()
  uint32_t      rozmiar;            // rozmiar pliku zapowiedziany przez centrale (Content-Length)
  char          klucz[diag::OTA_KLUCZ_MAKS];   // ktora oferte panel pobiera
  char          obrazOferty[17];    // identyfikator pliku z oferty ("" = starsza centrala go nie podaje)
  bool          nowaCentrala;
  int           procentNaEkranie;
};
static OtaPobieranie otaP = {};
static bool otaBylaProba = false;           // od startu panelu byla juz proba pobierania...
static unsigned long otaKoniecProbyMs = 0;  // ...i wtedy sie skonczyla (albo zostala przerwana)

// [RESET-DIAG, ta sesja] Wypelniane RAZ, na samym poczatku setup() —
// patrz ponizej. Wysylane centrali w kazdym /api/status (analogicznie do
// FIRMWARE_VERSION_PANEL), zeby centrala mogla odroznic normalne
// wybudzenie z deep sleep od czegos, co wymaga uwagi.
String globalnyPowodResetu;

// Podzbior /api/ustawienia-get, ktory ten panel pozwala PODGLADAC i EDYTOWAC.
// Zeby dodac kolejne pole: dopisz wpis do tablicy POLA[] nizej — nic wiecej.
struct PoleUstawien {
  const char* klucz;     // dokladnie jak w JSON-ie i w ustawParametrPieca()
  const char* etykieta;  // nazwa wyswietlana na ekranie
  long   krok;           // wielkosc kroku +/-
  long   minZakres, maxZakres; // TYLKO informacyjnie/klips lokalny — centrala i tak waliduje
  long   wartosc;        // ostatnia znana wartosc (z GET /api/ustawienia-get)
};

PoleUstawien POLA[] = {
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] czasOn/czasOff
  // brakowalo tu mimo ze centrala je juz waliduje/zwraca od dawna
  // (ustawParametrPieca()/"/api/ustawienia-get") - ten sam powod co
  // odchylTemp/mnoznik/antystopDni nizej: ekran LVGL USTAWIENIA POMPY
  // (patrz lvgl_dashboard_poc_aktualizuj_ustawienia_pompy() nizej) potrzebuje
  // ich w POLA[], bo to jedyna sciezka odczytu z centrali.
  { "czasOn",         "Pompa ON [min]",       1,   1, 180, 0 },
  { "czasOff",        "Pompa OFF [min]",      1,   1, 180, 0 },
  { "tempOn",         "Zal. pompe pow. [C]",  1,   0, 100, 0 },
  { "tempOff",        "Wyl. pompe pon. [C]",  1,   0, 100, 0 },
  { "tempZadServo",   "Temp. zadana serwo",   1,   0, 100, 0 },
  { "histerServo",    "Histereza serwo",      1,   0,  50, 0 },
  { "skokKlapy",      "Skok klapy [%]",       1,   1, 100, 0 },
  { "skokSyberka",    "Skok syberka [%]",     1,   1, 100, 0 },
  // [ROZSZERZENIE, ta sesja] Brakowalo tych dwoch mimo ze centrala je juz
  // waliduje/zwraca od dawna (ustawParametrPieca()/"/api/ustawienia-get") -
  // ekran LVGL USTAWIENIA SERWA (patrz lvgl_dashboard_poc_aktualizuj_ustawienia_serwa()
  // nizej) potrzebuje ich w POLA[], bo to jedyna sciezka odczytu z centrali.
  { "odchylTemp",     "Prog odchylenia [C]",  1,   1,  50, 0 },
  { "mnoznik",        "Mnoznik korekty",      1,   1,  10, 0 },
  { "antystopDni",    "Antystop co [dni]",    1,   1,  45, 0 },
  { "mieszadloCzasOn","Mieszadlo ON [s]",     5,   1, 255, 0 },
  { "mieszadloCzasOff","Mieszadlo OFF [s]",   5,   1, 180, 0 },
  { "progAlarmTemp",  "Prog alarmu temp.",    1,   0, 100, 0 },
  { "progAlarmDym",   "Prog alarmu dymu",    10,   0,4095, 0 },
  // [ROZSZERZENIE, ta sesja - PANEL DYM] Ekran LVGL DYM (patrz
  // lvgl_dashboard_poc_aktualizuj_ustawienia_dymu() nizej) potrzebuje ich w
  // POLA[], ten sam powod co reszta pol dopisanych w poprzednich sesjach -
  // centrala je juz waliduje/zwraca od dawna (main_centrala.cpp,
  // ustawParametrPieca()), tylko nie byly dotad tu wystawione.
  { "dymCzasOn",           "Czas WL. dymu [s]",     5, 20, 600, 0 },
  { "dymCzasOff",          "Czas WYL. dymu [s]",    5, 10, 600, 0 },
  { "dymCzasStabilizacji", "Stabilizacja dymu [s]", 5,  5, 590, 0 },
};
const int LICZBA_POL = sizeof(POLA) / sizeof(POLA[0]);
bool ustawieniaWczytane = false;

// [LCD-DYM-PANEL] Uzywane przez panel czulosci dymu (rysujPanelDym() nizej)
// zeby nie trzymac osobnego, twardo zakodowanego indeksu do POLA[] — jesli
// ktos przestawi kolejnosc pol w POLA[], to i tak dalej dziala.
int znajdzIndeksPola(const char* klucz) {
  for (int i = 0; i < LICZBA_POL; i++) if (strcmp(POLA[i].klucz, klucz) == 0) return i;
  return -1;
}

// [LCD-TRYBY] "wybor" i "trybSerwa" to enumy (nie liczby +/-), wiec zyja
// OSOBNO od POLA[] — ten sam JSON /api/ustawienia-get, ten sam generyczny
// /api/ustawienia-set (pole=wybor&wartosc=..), ale inny widget na ekranie
// (segmentowany przelacznik, patrz rysujTryby() i EKRAN_TRYBY nizej).
// Wartosci i zakresy 1:1 z main.cpp centrali (ustawParametrPieca()):
//   wybor:     1=Trociniak  2=Kopciuch  3=Automatyczny
//   trybSerwa: 1=Auto       2=Reczny    3=Bezpieczna
long trybWyboru  = 1;
long trybSerwaWartosc = 1;
// [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] `antystopWlaczony`
// to bool (glowny wylacznik funkcji antystop pompy - patrz PompaAntystopCb w
// dashboard_lvgl_poc.h), nie liczba +/-, wiec ten sam powod jak przy
// wybor/trybSerwa powyzej: zyje OSOBNO od POLA[]. Domyslnie true - ten sam
// domyslny stan co centrala uzywa przy pierwszym uruchomieniu
// (main_centrala.cpp, antystopWlaczony=true).
bool antystopWlaczonyWartosc = true;

// ----------------------------------------------------------------------------
// [LCD-SEZON] Automatyczne przelaczanie widoku Lato/Zima (dashboard chowa
// wtedy kafelki niepotrzebne sezonowo — patrz kafelekWidoczny() przy
// rysujDashboard()). Zrodlo prawdy: "miesiac" z /api/status (RTC centrali,
// patrz [LCD-SEZON] w main.cpp centrali, v1.16.0), z fallbackiem na
// histereze t_panel gdy centrala zglasza brak RTC (miesiac==0) — ORAZ jako
// naklada sie NA kalendarz w miesiacach sezonu grzewczego: goracy panel w
// dzien sloneczny (np. marzec) czasowo przelacza widok na Lato, bo wtedy to
// panel/bojler sa istotne, a nie piec — ustalone z uzytkownikiem wprost.
// ----------------------------------------------------------------------------
enum Sezon { SEZON_ZIMA, SEZON_LATO };
Sezon aktualnySezon = SEZON_ZIMA;
bool  sezonZainicjalizowany = false;

// Sezon grzewczy = wrzesien-maj (IX-V) — najbardziej typowy dla PL, ustalone
// z uzytkownikiem. Miesiace VI-VIII to zawsze Lato bez wzgledu na t_panel.
bool miesiacWSezonieGrzewczym(int m) { return (m >= 9 || (m >= 1 && m <= 5)); }

// Histereza na t_panel, zeby przelaczanie nie "migalo" przy wahaniach wokol
// progu (np. chmura na chwile zasloni slonce) — proste podejscie znane z
// termostatow: inny prog wejscia niz wyjscia.
const float SEZON_PANEL_PROG_LATO = 25.0f;  // powyzej -> (albo utrzymaj) Lato
const float SEZON_PANEL_PROG_ZIMA = 20.0f;  // ponizej -> (albo utrzymaj) Zima

// Wywolywane po kazdym pobierzStatus() (setup() i loop()). Zwraca true, gdy
// sezon sie realnie zmienil w tym wywolaniu — wywolujacy wtedy wymusza pelne
// przerysowanie dashboardu (uklad kafelkow sie zmienia).
bool aktualizujSezon() {
  Sezon poprzedni = aktualnySezon;

  if (stan.miesiac == 0) {
    // Brak RTC w centrali — jedyne zrodlo to t_panel, czysta histereza.
    if (!sezonZainicjalizowany) {
      aktualnySezon = (stan.tPanel > SEZON_PANEL_PROG_LATO) ? SEZON_LATO : SEZON_ZIMA;
    } else if (aktualnySezon == SEZON_ZIMA && stan.tPanel > SEZON_PANEL_PROG_LATO) {
      aktualnySezon = SEZON_LATO;
    } else if (aktualnySezon == SEZON_LATO && stan.tPanel < SEZON_PANEL_PROG_ZIMA) {
      aktualnySezon = SEZON_ZIMA;
    }
  } else if (!miesiacWSezonieGrzewczym(stan.miesiac)) {
    // Czerwiec-sierpien — kalendarz rozstrzyga jednoznacznie, zawsze Lato.
    aktualnySezon = SEZON_LATO;
  } else {
    // Miesiac sezonu grzewczego — domyslnie Zima, ale goracy panel czasowo
    // przelacza na Lato (histereza jak wyzej), np. sloneczny dzien w marcu.
    if (!sezonZainicjalizowany) {
      aktualnySezon = (stan.tPanel > SEZON_PANEL_PROG_LATO) ? SEZON_LATO : SEZON_ZIMA;
    } else if (aktualnySezon == SEZON_ZIMA && stan.tPanel > SEZON_PANEL_PROG_LATO) {
      aktualnySezon = SEZON_LATO;
    } else if (aktualnySezon == SEZON_LATO && stan.tPanel < SEZON_PANEL_PROG_ZIMA) {
      aktualnySezon = SEZON_ZIMA;
    }
  }

  sezonZainicjalizowany = true;
  return aktualnySezon != poprzedni;
}

// ----------------------------------------------------------------------------
// EKRANY
// ----------------------------------------------------------------------------
enum Ekran { EKRAN_DASHBOARD, EKRAN_USTAWIENIA, EKRAN_SERWO, EKRAN_TRYBY };
Ekran aktualnyEkran = EKRAN_DASHBOARD;
int ustawieniaStrona = 0;              // paginacja listy POLA (kilka na ekran)
const int POZYCJI_NA_STRONE = 4;

// Suwak recznego serwa (ekran EKRAN_SERWO) — wysylany dopiero po puszczeniu
int serwoKlapaProcent = 0, serwoSyberekProcent = 0;
bool serwoPrzeciaganyKlapa = false, serwoPrzeciaganySyberek = false;

// ----------------------------------------------------------------------------
// PROSTY SYSTEM PRZYCISKOW DOTYKOWYCH (hit-test na prostokatach)
// ----------------------------------------------------------------------------
struct Przycisk {
  int16_t x, y, w, h;
  bool zawiera(int16_t px, int16_t py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

// Dolny pasek zakladek — 3 zakladki na cala szerokosc ekranu (480x320, landscape)
const int PASEK_WYS = 44;
// [LCD-TRYBY] 4 zakladki zamiast 3 — szerokosc 480/4=120 zamiast 480/3.
Przycisk zakladkaDashboard { 0,               320 - PASEK_WYS, 480 / 4, PASEK_WYS };
Przycisk zakladkaUstawienia{ 480 / 4,         320 - PASEK_WYS, 480 / 4, PASEK_WYS };
Przycisk zakladkaSerwo     { 2 * 480 / 4,     320 - PASEK_WYS, 480 / 4, PASEK_WYS };
Przycisk zakladkaTryby     { 3 * 480 / 4,     320 - PASEK_WYS, 480 / 4, PASEK_WYS };

// [LCD-KAFELKI] Dashboard = siatka kafelkow (ikona + jedna liczba), dokladnie
// jak w makiecie lcd_tile_ui_concept.html: dotkniecie kafelka OZNACZONEGO
// JAKO STEROWALNY otwiera panel nakladke (overlay) na srodku ekranu,
// dopasowany do typu urzadzenia — suwak dla KLAPA/SYBERKA (0-100%,
// wyslijServoManualny jak na ekranie Serwo), male +/- dla DYM (czulosc
// alarmu, jak dawniej), a czyste odczyty (temperatury, POMPA, wilgotnosc,
// cisnienie) NIE reaguja na dotyk — sa tylko do podgladu. Rozne typy
// urzadzen -> rozny typ nakladki, wiec trzymamy WSPOLNY "ktory panel jest
// otwarty" (indeks kafelka, -1 = zaden) zamiast osobnej flagi bool per typ
// jak dawniej (pokazPanelDym) — latwiej dodac kolejny sterowalny kafelek
// w przyszlosci bez mnozenia flag.
enum {
  KAF_ZEWN, KAF_BOJLER, KAF_OGRZ, KAF_OGRZ_SR, KAF_PANEL, KAF_POKOJ,
  KAF_WILGOTNOSC, KAF_CISNIENIE, KAF_POMPA, KAF_KLAPA, KAF_SYBERKA, KAF_DYM,
  LICZBA_KAFELKOW
};
Przycisk kafelekRect[LICZBA_KAFELKOW]; // wypelniane na biezaco w rysujDashboard(); {0,0,0,0} = niewidoczny w tym sezonie
int panelOtwartyId = -1; // -1 = brak otwartego panelu nakladki; wpp. jeden z KAF_*
Przycisk panelMinus, panelPlus, panelZamknij; // wspolne dla nakladki DYM (+/-) i suwakow (tylko Zamknij)
const Przycisk PANEL_KARTA{ 140, 90, 200, 150 };       // karta nakladki DYM (+/-), jak dawniej
const Przycisk PANEL_SUWAK_KARTA{ 90, 96, 300, 128 };  // szersza karta nakladki KLAPA/SYBERKA (suwak poziomy)
Przycisk suwakPanelSuwak; // prostokat samego paska suwaka wewnatrz PANEL_SUWAK_KARTA, do obslugi drag

// ----------------------------------------------------------------------------
// HTTP — pomocnicze
// ----------------------------------------------------------------------------
// [SIEC-NIEBLOKUJACA, ta sesja] httpGet()/httpPost() (synchroniczne
// HTTPClient::GET/POST) USUNIETE z tego miejsca - zastapione przez
// statusFetch/ustawieniaFetch/komendaFetch (siec_nieblokujaca_LCD.h,
// instancje globalne wyzej). Odpowiedniki:
//   httpGet("/api/status?...")   -> statusFetch.startGet(...)     + obslugaStatusFetch() nizej
//   httpGet("/api/ustawienia-get")-> ustawieniaFetch.startGet(...) + obslugaUstawieniaFetch() nizej
//   httpPost(sciezka, body, blad) -> komendaFetch.startPostFormUrlEncoded(...) + obslugaKomendaFetch() nizej
// [v1.3.0 WYKRESY] Ekran Wykresy tez juz nie uzywa HTTPClient - patrz nizej.

// ----------------------------------------------------------------------------
// [v1.3.0 WYKRESY] Dane do ekranu Wykresy: od centrali, bez blokowania petli
// ----------------------------------------------------------------------------
// Panel nie ma internetu ani wlasnej historii - wszystko do wykresow daje mu
// centrala, ktora trzyma historie u siebie (pamiec + pliki /hist_short.csv i
// /hist_long.csv na jej flashu).
//
// Jak to idzie po dotknieciu "POKAZ WYKRES" (dashboard_lvgl_poc.cpp wola
// wykresyFetchStart() przez WykresyFetchCb):
//   1. GET /api/history/panel?tryb=godz|dzien|tydz&pkt=96 - centrala od wersji
//      3.58.4 sama wybiera zakres i odsyla gotowe punkty (ok. 20 kB, pierwsza
//      linia "#panel-hist ..."). Panel tylko rysuje.
//   2. Centrala starsza niz 3.58.4 tego adresu nie zna (odpowiada
//      przekierowaniem 302). Wtedy - i tylko wtedy - panel pobiera po staremu
//      caly CSV (/api/history albo /api/history/long, ponad 1 MB) i sam sciska
//      go do 96 punktow, jak w v1.2.2.
// W obu przypadkach odpowiedz jest czytana linia po linii przez wykresyFetch
// (StrumienHttpGet), w kolejnych obrotach loop() - ekran, dotyk i odpytywanie
// /api/status pracuja w tym czasie normalnie. Do v1.2.2 robil to blokujacy
// HTTPClient: ekran stal do konca pobierania (limit 8 s, po ktorym wykres
// tygodniowy byl po cichu rysowany z samego poczatku danych), a watchdog nie
// byl karmiony.
//
// Kazde pobranie zostawia w logu jedna linie "tag=WYKRESY" z wynikiem:
//   zdarzenie=pobrano       wykres narysowany (liczba punktow, bajtow, czas)
//   zdarzenie=pobrano_czesc wykres z czesci danych, na ekranie dopisek NIEPELNE
//                           (tylko starsza centrala: caly CSV nie zdazyl dojsc)
//   zdarzenie=brak_danych   centrala odpowiedziala, ale nie ma jeszcze historii
//   zdarzenie=blad          z przyczyna - ta sama przyczyna jest na ekranie
//                           zamiast dawnego ogolnego "Brak danych"
//   zdarzenie=przerwano     uzytkownik wyszedl z ekranu przed koncem
static const char* const WYKRESY_TRYB_ADRES[3] = { "godz", "dzien", "tydz" };   // indeks = LVGL_WYKRESY_TRYB_*

struct WykresyPobieranie {
  bool aktywne = false;
  bool nowySposob = true;        // true = pytamy o gotowe punkty; false = stary CSV (centrala < 3.58.4)
  uint8_t tryb = LVGL_WYKRESY_TRYB_DZIEN;
  int kodNowego = 0;             // kod HTTP odpowiedzi na /api/history/panel (do logu)
  unsigned long startMs = 0;
  unsigned long postepMs = 0;
};
static WykresyPobieranie wykresyPob;

static void wykresyLinia(const char* linia) { lvgl_dashboard_poc_wykresy_strumien_linia(linia); }

// Koniec pobierania (udany albo nie): przekazuje wynik ekranowi, pisze linie
// logu, zwalnia gniazdo. `wynik` to to, co wie warstwa sieci; ekran moze go
// jeszcze zmienic (np. odpowiedz poprawna, ale bez danych) - do logu idzie
// to, co faktycznie widac na ekranie.
static void wykresyZakoncz(int wynik, int szczegol) {
  const bool dlugi = (wykresyPob.tryb == LVGL_WYKRESY_TRYB_TYDZIEN);
  const int naEkranie = lvgl_dashboard_poc_wykresy_strumien_koniec(dlugi, wykresyFetch.liniiCiala(), wynik, szczegol);
  LvglWykresyStatystyka st;
  lvgl_dashboard_poc_wykresy_statystyka(&st);
  const char* sposob = wykresyPob.nowySposob ? "centrala" : "panel";
  const unsigned long ms = (unsigned long)(millis() - wykresyPob.startMs);
  if (naEkranie == LVGL_WYKRESY_OK) {
    // sposob=centrala: punkty policzyla centrala (3.58.4+); sposob=panel: panel
    // sam scisnal caly CSV starszej centrali. wierszy_centrali: ile wierszy
    // historii centrala wziela pod uwage (-1 = nie podala, stary CSV).
    logBezPamieci("tag=WYKRESY zdarzenie=pobrano tryb=%s sposob=%s punktow=%d przerw=%d wierszy_centrali=%ld linii=%lu ucietych_linii=%lu bajtow=%lu ms=%lu ram=%lu ram_min=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, st.punktow, st.przerw, st.wierszyZrodla,
                  (unsigned long)wykresyFetch.liniiCiala(), (unsigned long)wykresyFetch.zaDlugichLinii(),
                  (unsigned long)wykresyFetch.bajtowCiala(), ms, (unsigned long)ramWolny(), (unsigned long)ramMin());
  } else if (naEkranie == LVGL_WYKRESY_NIEPELNE) {
    // Tylko przy starszej centrali: caly CSV nie doszedl (powod=limit_czasu
    // albo urwana), wykres narysowany z tego, co doszlo, z dopiskiem NIEPELNE.
    logBezPamieci("tag=WYKRESY lvl=WARN zdarzenie=pobrano_czesc tryb=%s sposob=%s powod=%s etap=%c punktow=%d linii=%lu bajtow=%lu z_zapowiedzianych=%ld ms=%lu msg=\"%s\"\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, lvgl_dashboard_poc_wykresy_wynik_nazwa(wynik),
                  wykresyFetch.etapBledu(), st.punktow, (unsigned long)wykresyFetch.liniiCiala(),
                  (unsigned long)wykresyFetch.bajtowCiala(), wykresyFetch.dlugoscZapowiedziana(), ms, wykresyFetch.ostatniBlad());
  } else if (naEkranie == LVGL_WYKRESY_PUSTO || naEkranie == LVGL_WYKRESY_ZA_MALO) {
    // Odpowiedz poprawna, tylko centrala nie ma jeszcze z czego zrobic wykresu
    // (swiezo po pierwszym uruchomieniu albo po wyczyszczeniu historii).
    logBezPamieci("tag=WYKRESY zdarzenie=brak_danych tryb=%s sposob=%s przyczyna=%s punktow=%d wierszy_centrali=%ld linii=%lu bajtow=%lu ms=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, lvgl_dashboard_poc_wykresy_wynik_nazwa(naEkranie), st.punktow,
                  st.wierszyZrodla, (unsigned long)wykresyFetch.liniiCiala(), (unsigned long)wykresyFetch.bajtowCiala(), ms);
  } else {
    logBezPamieci("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=%s sposob=%s przyczyna=%s kod=%d etap=%c errno=%d punktow=%d linii=%lu bajtow=%lu ms=%lu ram=%lu ram_blok=%lu msg=\"%s\"\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, lvgl_dashboard_poc_wykresy_wynik_nazwa(naEkranie),
                  wykresyFetch.kodHttp(), wykresyFetch.etapBledu(), wykresyFetch.numerBledu(), st.punktow,
                  (unsigned long)wykresyFetch.liniiCiala(), (unsigned long)wykresyFetch.bajtowCiala(), ms,
                  (unsigned long)ramWolny(), (unsigned long)ramBlok(), wykresyFetch.ostatniBlad());
  }
  wykresyFetch.skwituj();
  wykresyPob.aktywne = false;
}

// Blad warstwy sieci -> przyczyna dla ekranu i logu.
static int wykresyWynikZBledu() {
  if (diag::bladGniazdaToBrakPamieci(wykresyFetch.numerBledu())) return LVGL_WYKRESY_BRAK_PAMIECI;
  switch (wykresyFetch.etapBledu()) {
    case 'c': case 'h': case 'p': return LVGL_WYKRESY_LIMIT_CZASU;       // centrala nie zdazyla
    case 'z': case 'k':           return LVGL_WYKRESY_ZLA_ODPOWIEDZ;     // to nie byla odpowiedz HTTP
    case 'r':                     return (wykresyFetch.bajtowCiala() > 0) ? LVGL_WYKRESY_URWANA : LVGL_WYKRESY_BRAK_POLACZENIA;
    default:                      return LVGL_WYKRESY_BRAK_POLACZENIA;   // g n l a s: nie udalo sie polaczyc / wyslac
  }
}

// Centrala nie zna /api/history/panel: ta sama prosba po staremu.
static void wykresyPrzejdzNaStarySposob(int kod) {
  const bool dlugi = (wykresyPob.tryb == LVGL_WYKRESY_TRYB_TYDZIEN);
  logBezPamieci("tag=WYKRESY zdarzenie=stara_centrala kod=%d msg=\"centrala nie zna /api/history/panel (firmware starszy niz 3.58.4) - pobieram cala historie po staremu\"\n", kod);
  wykresyPob.nowySposob = false;
  wykresyPob.kodNowego = kod;
  lvgl_dashboard_poc_wykresy_strumien_start(dlugi);   // od nowa: to, co przyszlo do tej pory, nie bylo danymi
  if (!wykresyFetch.start(dlugi ? "/api/history/long" : "/api/history", wykresyLinia)) {
    wykresyZakoncz(wykresyWynikZBledu(), 0);
  }
}

// Wolane z dashboard_lvgl_poc.cpp (WykresyFetchCb), gdy uzytkownik chce
// zobaczyc wykres. Tylko STARTUJE pobieranie i od razu wraca; reszta dzieje
// sie w obslugaWykresyFetch() w kolejnych obrotach loop().
// (Bez "static": przy LVGL_POC_SYMULACJA=1 podpinana jest zamiast niej
// symWykresyFetchAkcja() i kompilator ostrzegalby o nieuzywanej funkcji.)
void wykresyFetchStart(int tryb) {
  if (tryb < 0 || tryb > 2) tryb = LVGL_WYKRESY_TRYB_DZIEN;
  if (wykresyPob.aktywne) {
    // Nowa prosba w trakcie poprzedniej (zmiana zakresu): stara przepada.
    logBezPamieci("tag=WYKRESY zdarzenie=przerwano powod=nowa_prosba tryb=%s po_ms=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], (unsigned long)(millis() - wykresyPob.startMs));
  }
  wykresyFetch.wyczysc();   // takze: porzuca poprzednie zadanie, jesli jeszcze trwalo
  wykresyPob.aktywne = true;
  wykresyPob.nowySposob = true;
  wykresyPob.tryb = (uint8_t)tryb;
  wykresyPob.kodNowego = 0;
  wykresyPob.startMs = millis();
  wykresyPob.postepMs = wykresyPob.startMs;
  const bool dlugi = (tryb == LVGL_WYKRESY_TRYB_TYDZIEN);
  // Przed kazda mozliwoscia wczesnego wyjscia: ekran czeka na _koniec().
  lvgl_dashboard_poc_wykresy_strumien_start(dlugi);
  Serial.printf("tag=WYKRESY zdarzenie=start tryb=%s ram=%lu\n", WYKRESY_TRYB_ADRES[tryb], (unsigned long)ramWolny());

  if (WiFi.status() != WL_CONNECTED) {
    wykresyZakoncz(LVGL_WYKRESY_BRAK_WIFI, 0);
    return;
  }
  char sciezka[56];
  snprintf(sciezka, sizeof(sciezka), "/api/history/panel?tryb=%s&pkt=%d", WYKRESY_TRYB_ADRES[tryb], LVGL_WYKRESY_PUNKTOW);
  if (!wykresyFetch.start(sciezka, wykresyLinia)) {
    wykresyZakoncz(wykresyWynikZBledu(), 0);
  }
}

// Co obrot loop(): popycha pobieranie historii i konczy je, gdy przyszlo
// wszystko albo cos poszlo nie tak.
void obslugaWykresyFetch() {
  if (!wykresyPob.aktywne) return;

  // Uzytkownik wyszedl z podstrony Wykres (strzalka, X, gest, bezczynnosc,
  // ekran alarmu) - nie ma dla kogo pobierac.
  if (!lvgl_dashboard_poc_wykresy_czeka_na_dane()) {
    logBezPamieci("tag=WYKRESY zdarzenie=przerwano powod=wyjscie_z_ekranu tryb=%s po_ms=%lu linii=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], (unsigned long)(millis() - wykresyPob.startMs),
                  (unsigned long)wykresyFetch.liniiCiala());
    wykresyFetch.przerwij();
    wykresyPob.aktywne = false;
    return;
  }

  wykresyFetch.poll();

  if (wykresyFetch.wTrakcie()) {
    if (wykresyPob.nowySposob) {
      const int kodTeraz = wykresyFetch.kodHttp();   // 0, dopoki nie przyszla linia statusu
      // Kod inny niz 200 i 503: centrala nie zna nowego adresu (wersje do
      // 3.58.3 odpowiadaja przekierowaniem 302). Tresci takiej odpowiedzi nie
      // czytamy - od razu pytamy po staremu.
      if (kodTeraz != 0 && kodTeraz != 200 && kodTeraz != 503) {
        wykresyPrzejdzNaStarySposob(kodTeraz);
        return;
      }
      // Kod 200, ale pierwsza linia tresci to nie "#panel-hist": to nie jest
      // centrala 3.58.4 (bardzo stare wersje oddawaly pod kazdym adresem
      // /api/history/... caly CSV). Tego tez nie czytamy do konca.
      if (kodTeraz == 200 && wykresyFetch.liniiCiala() >= 1 && !lvgl_dashboard_poc_wykresy_strumien_zredukowany()) {
        wykresyPrzejdzNaStarySposob(kodTeraz);
        return;
      }
    }
    const unsigned long teraz = millis();
    if (teraz - wykresyPob.postepMs >= 300) {
      wykresyPob.postepMs = teraz;
      lvgl_dashboard_poc_wykresy_postep(wykresyFetch.liniiCiala(), wykresyFetch.bajtowCiala());
    }
    return;
  }
  if (!wykresyFetch.gotowyWynik()) {   // nie powinno sie zdarzyc: aktywne pobieranie bez zadania
    wykresyPob.aktywne = false;
    return;
  }

  if (!wykresyFetch.bylSukces()) {
    wykresyZakoncz(wykresyWynikZBledu(), 0);
    return;
  }
  const int kod = wykresyFetch.kodHttp();
  if (wykresyPob.nowySposob) {
    if (kod == 200 && lvgl_dashboard_poc_wykresy_strumien_zredukowany()) {
      wykresyZakoncz(wykresyFetch.kompletna() ? LVGL_WYKRESY_OK : LVGL_WYKRESY_URWANA, 0);
    } else if (kod == 503) {
      wykresyZakoncz(LVGL_WYKRESY_HTTP, kod);   // centrala zna adres, ale zabraklo jej pamieci
    } else {
      wykresyPrzejdzNaStarySposob(kod);         // 302 / 404 / cos innego: centrala < 3.58.4
    }
    return;
  }
  if (kod != 200) {
    wykresyZakoncz(LVGL_WYKRESY_HTTP, kod);
    return;
  }
  wykresyZakoncz(wykresyFetch.kompletna() ? LVGL_WYKRESY_OK : LVGL_WYKRESY_URWANA, 0);
}

void pobierzStatus(); // forward declaration - pompaManualWyslij() wola ja przed jej definicja nizej
void pobierzUstawienia(); // forward declaration - serwoUstawienieWyslij() (ponizej) wola ja przed jej definicja (linia ~959)
void wyslijTrybPracy(const char* pole, long wartosc); // forward declaration - pompaWyborWyslij() (ponizej) wola ja przed jej definicja (linia ~1394); BLAD KOMPILACJI bez tego: "'wyslijTrybPracy' was not declared in this scope"
// [POPRAWKA, ta sesja - panel SERWO v2] Ten sam powod co wyzej -
// serwoTrybWyslij()/serwoPozycjaWyslij() (ponizej) wolaja wyslijServoManualny()
// przed jej definicja (linia ~1487).
void wyslijServoManualny(const char* os, int procent);

// [LVGL-API] Lokalny odpowiednik ustawSerwo()/ustawTryb() dla pompy - jedna
// funkcja ktora dashboard_lvgl_poc.cpp woła przez callback (patrz
// lvgl_dashboard_poc_ustaw_callback_pompy() w setup()), zeby ten plik nie
// musial wiedziec NIC o HTTPClient/WiFi. `stanTekst` to dokladnie to, czego
// oczekuje centrala: "auto" / "wl" / "wyl" (patrz /api/pompa-manual).
// Po udanym POST od razu odswieza stan (pobierzStatus() + przekazanie do
// LVGL) - zeby uzytkownik nie czekal do kolejnego cyklu STATUS_INTERWAL_MS
// (2s) na potwierdzenie, ze dotkniecie przycisku faktycznie cos zmienilo.
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] POST + nastepujacy po nim GET
// statusu NIE MOGA juz byc jednym synchronicznym wywolaniem (patrz komentarz
// przy pendingPoKomendzie wyzej) - ta funkcja tylko STARTUJE komende, wynik
// (sukces/porazka) i nastepujacy po nim pobierzStatus()+aktualizacja LVGL
// obsluzy obslugaKomendaFetch() nizej, gdy komendaFetch faktycznie sie
// zakonczy (jedna-dwie iteracje loop() pozniej, nie w tym samym wywolaniu).
void pompaManualWyslij(const char* stanTekst) {
  if (komendaFetch.wTrakcie()) {
    Serial.println("[LVGL] pompaManualWyslij: poprzednia komenda jeszcze w toku, pomijam");
    return; // user i tak zaraz zobaczy prawdziwy stan przy nastepnym apistatus
  }
  pendingPoKomendzie = PoKomendzie::STATUS; // ten kafelek czerpie z /api/status
  komendaFetch.startPostFormUrlEncoded("/api/pompa-manual", String("stan=") + stanTekst);
  // Celowo NIC nie zmieniamy tu lokalnie w UI - obslugaKomendaFetch() (przy
  // sukcesie) doprowadzi do tego samego pobierzStatus()+aktualizacja LVGL co
  // dawniej, a przy porazce kafelek po prostu nie "klamie" (jak dawniej,
  // patrz TODO #1 w STATUS_PROJEKTU_LVGL.md).
}

// [ROZBUDOWA, ta sesja - strona POMPA / wybor strategii] Analogiczny
// "lokalny most" dla wyboru Trociniak/Kopciuch/Automatyczny - dokladnie ten
// sam ksztalt co pompaManualWyslij() wyzej, tylko woła JUZ ISTNIEJACA
// wyslijTrybPracy() (do tej pory uzywana tylko przez stary, nie-LVGL
// EKRAN_TRYBY - patrz komentarz [LCD-TRYBY] przy trybWyboru) zamiast
// wlasnego httpPost(). Aktualizuje `trybWyboru` PRZED wyslaniem (ten sam
// wzorzec co stary rysujTryby()/segmentyWybor callback), zeby stary,
// nie-LVGL EKRAN_TRYBY (jesli ktos do niego wroci) tez od razu widzial
// spojna wartosc, a nie musial czekac na kolejny pobierzStatus().
void pompaWyborWyslij(int wartosc) {
  trybWyboru = wartosc;
  if (komendaFetch.wTrakcie()) {
    Serial.println("[LVGL] pompaWyborWyslij: poprzednia komenda jeszcze w toku, pomijam");
    return;
  }
  wyslijTrybPracy("wybor", wartosc); // ustawia domyslnie pendingPoKomendzie=USTAWIENIA (patrz komentarz przy wyslijTrybPracy)
  pendingPoKomendzie = PoKomendzie::STATUS; // nadpisujemy - ten ekran czerpie trybWyboru z /api/status, nie z POLA[]
}

// [POPRAWKA, ta sesja - panel SERWO v2] Dwa nowe "lokalne mosty", dokladnie
// ten sam ksztalt co pompaWyborWyslij() wyzej - wywoluja JUZ ISTNIEJACE
// funkcje HTTP (wyslijTrybPracy/wyslijServoManualny, patrz linie ~1401 i
// ~1448), zeby dashboard_lvgl_poc.cpp nie musial nic wiedziec o
// HTTPClient/WiFi. Rejestrowane w setup() przez
// lvgl_dashboard_poc_ustaw_callback_serwo_tryb()/..._pozycja().
void serwoTrybWyslij(int wartosc) {
  trybSerwaWartosc = wartosc;
  if (komendaFetch.wTrakcie()) {
    Serial.println("[LVGL] serwoTrybWyslij: poprzednia komenda jeszcze w toku, pomijam");
    return;
  }
  wyslijTrybPracy("trybSerwa", wartosc); // ustawia domyslnie pendingPoKomendzie=USTAWIENIA
  pendingPoKomendzie = PoKomendzie::STATUS; // nadpisujemy - trybSerwaWartosc tez czerpiemy z /api/status
}

// `id`: 0=klapa, 1=syberka (tak jak dashboard_lvgl_poc.cpp go uzywa) ->
// mapowane na string, ktorego oczekuje centrala. UWAGA: centrala oczekuje
// "syberek", NIE "syberka" (patrz main_centrala.cpp ~4777-4779) - dokladnie
// ta literowka byla przyczyna bledu z poprzedniej sesji.
void serwoPozycjaWyslij(int id, int pozycja) {
  const char *os = (id == 0) ? "klapa" : "syberek";
  if (komendaFetch.wTrakcie()) {
    Serial.println("[LVGL] serwoPozycjaWyslij: poprzednia komenda jeszcze w toku, pomijam");
    return;
  }
  pendingPoKomendzie = PoKomendzie::STATUS; // klapa/syberka wracaja przez /api/status
  wyslijServoManualny(os, pozycja); // sam nie ustawia flagi (patrz komentarz przy jego definicji) - potrzebne bo suwak na starym, nie-LVGL EKRAN_SERWO woła go bez zadnego odswiezenia
}

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Jeden generyczny "most",
// dokladnie ten sam ksztalt co pompaWyborWyslij()/serwoTrybWyslij() wyzej -
// woła JUZ ISTNIEJACA wyslijTrybPracy(pole, wartosc) (dokladnie ta sama
// funkcja/endpoint /api/ustawienia-set co stary EKRAN_TRYBY uzywa dla
// POLA[]) i od razu odswieza POLA[] z centrali, zeby ekran LVGL pokazal
// PRAWDZIWA (zwalidowana przez centrale) wartosc, nie tylko to co user
// klepnal lokalnie - ten sam "nie klam po nieudanym poleceniu" wzorzec co
// pompaManualWyslij(). Rejestrowana w setup() przez
// lvgl_dashboard_poc_ustaw_callback_serwo_ustawienia().
void serwoUstawienieWyslij(const char *pole, long wartosc) {
  // wyslijTrybPracy() samodzielnie sprawdza komendaFetch.wTrakcie(), ustawia
  // pendingPoKomendzie=USTAWIENIA i startuje POST; obslugaKomendaFetch()
  // zrobi pobierzUstawienia() (odswieza POLA[] + lvgl_dashboard_poc_aktualizuj_ustawienia_serwa())
  // dopiero PO faktycznym zakonczeniu POST-a, nie od razu tutaj.
  wyslijTrybPracy(pole, wartosc);
}

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Dokladnie ten sam ksztalt
// co serwoUstawienieWyslij() powyzej - jeden generyczny "most" dla 5 pol
// numerycznych (czasOn/czasOff/tempOn/tempOff/antystopDni). Rejestrowana w
// setup() przez lvgl_dashboard_poc_ustaw_callback_pompa_ustawienia().
void pompaUstawienieWyslij(const char *pole, long wartosc) {
  wyslijTrybPracy(pole, wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej - ten sam wzorzec
}

// Osobny "most" dla przelacznika antystopWlaczony (bool, poza POLA[] - ten
// sam powod co trybWyboru/trybSerwaWartosc, patrz komentarz przy deklaracji
// antystopWlaczonyWartosc). Rejestrowana przez
// lvgl_dashboard_poc_ustaw_callback_pompa_antystop().
void pompaAntystopWyslij(int wartosc) {
  antystopWlaczonyWartosc = (wartosc != 0);
  wyslijTrybPracy("antystopWlaczony", wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}

// [ROZSZERZENIE, ta sesja - PANEL DYM] Dwa "mosty", dokladnie ten sam ksztalt
// co serwoUstawienieWyslij()/pompaUstawienieWyslij() powyzej - progAlarmDym
// (suwak) i 3 pola czasowe uzywaja tego samego generycznego
// /api/ustawienia-set (wyslijTrybPracy()) co reszta ekranow. Rejestrowane w
// setup() przez lvgl_dashboard_poc_ustaw_callback_dym_prog()/
// ..._dym_ustawienia().
void dymProgWyslij(int wartosc) {
  wyslijTrybPracy("progAlarmDym", wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}
void dymUstawienieWyslij(const char *pole, long wartosc) {
  wyslijTrybPracy(pole, wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}

// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Ten sam wzorzec co
// dymProgWyslij() powyzej - jeden wspolny prog dla kafelkow PIEC CO i
// PANEL. progAlarmTemp juz jest w POLA[] (patrz main.cpp linia ~466), wiec
// wyslijTrybPracy()/pobierzUstawienia() dzialaja bez zmian.
void alarmTempProgWyslij(int wartosc) {
  wyslijTrybPracy("progAlarmTemp", wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}

#if LVGL_POC_TEST && LVGL_POC_SYMULACJA
// ----------------------------------------------------------------------------
// [LVGL-SYMULACJA] Lokalna atrapa centrali - UZYWANA WYLACZNIE gdy testujesz
// sama plytke wyswietlacza bez fizycznej centrali obok. Odtwarza dokladnie
// ten sam kontrakt co prawdziwa integracja (pompaManualWyslij() +
// pobierzStatus() wyzej): dashboard_lvgl_poc.cpp NIC o tym nie wie, dostaje
// tylko wywolania lvgl_dashboard_poc_aktualizuj() jak zawsze. Dzieki temu
// przelaczenie LVGL_POC_SYMULACJA 1 -> 0 pozniej (gdy centrala bedzie pod
// reka) nie wymaga ZADNYCH zmian w dashboard_lvgl_poc.*.
//
// Wzorzec "temporary hold" + "self-healing" jest tu SWIADOMIE odtworzony
// lokalnie (ten sam ksztalt, co POMPA_OVERRIDE_MS na centrali, patrz
// main_centrala.cpp) - to jedyny sposob, zeby przetestowac countdown/arc
// na kafelku bez prawdziwego /api/status.
#define SYM_OVERRIDE_MIN_CALKOWITE 60   // musi byc zgodne z tym co rysuje pompa_tile_odswiez() (POMPA_OVERRIDE_CALKOWITE_MIN w dashboard_lvgl_poc.cpp) - inaczej luk % bedzie klamal
// [TESTOWANIE] Prawdziwy hold trwa godzine - zeby nie czekac godziny miedzy
// testami self-healing powrotu do AUTO, symulacja skraca go do 2 minut.
// Sam WYGLAD (tekst/procent luku) i tak jest liczony wzgledem
// SYM_OVERRIDE_MIN_CALKOWITE (60), wiec luk "topnieje" po prostu szybciej
// niz w realu - to jedyna roznica. Zmien na 60UL gdy zechcesz przetestowac
// realny czas trwania.
static const unsigned long SYM_OVERRIDE_TRWANIE_MS = 2UL * 60UL * 1000UL;

static bool          symPompaWlaczona = false;
static unsigned long symOverrideDoMs  = 0;   // 0 = tryb AUTO (brak aktywnego override)

// Statyczne, wymyslone wartosci ZEWN/BOJLER/DYM - w tej symulacji liczy sie
// tylko interakcja z kafelkiem POMPA (layout/dotyk/panel), reszta kafelkow
// ma po prostu pokazywac cokolwiek sensownego, zeby ekran nie byl pusty.
static const float SYM_T_ZEWN    = 3.5f;
static const float SYM_T_BOJLER  = 52.0f;
// [ROZSZERZENIE tej sesji] PIEC CO/SR nie jest juz podgladem z demo-tekstem
// w dashboard_lvgl_poc.cpp - potrzebuje realnych (tu: symulowanych) danych.
static const float SYM_T_OGRZ    = 45.0f;
static const float SYM_T_OGRZ_SR = 42.0f;
// [Partia 5, ta sesja] Nowy kafelek PANEL (patrz dashboard_lvgl_poc.cpp)
// potrzebuje symulowanej wartosci tak samo jak reszta - 32.0 st.C.
static const float SYM_T_PANEL   = 32.0f;
static const int   SYM_DYM       = 12;
// [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Domyslnie brak alarmow w
// symulacji (jak reszta linii falszywych alarmow ponizej) - zmien
// POJEDYNCZO na true i wgraj ponownie, zeby na biurku (bez centrali)
// przetestowac odpowiedni wzorzec brzeczyka z pinu 13.
static const bool  SYM_DYM_ALARM            = false;
static const bool  SYM_ALARM_OGRZEWANIE     = false;
static const bool  SYM_ALARM_PANEL          = false;
// [UI-WIFI-SYGNAL, ta sesja] Symulacja nie ma realnego WiFi/RSSI centrali,
// wiec stala wartosc "srodkowa" (ok. polowa z 7 progow kresek w
// wifi_liczba_kresek(), dashboard_lvgl_poc.cpp) - wystarczy, zeby wskaznik
// mial cos sensownego do pokazania podczas testowania samej plytki.
static const int   SYM_RSSI      = -60;
static const bool  SYM_INTERNET  = true; // [UI-INTERNET, ta sesja] atrapa - w symulacji zawsze "jest internet"
// [Partia 2, ta sesja] Symulowane wartosci 5 nowych kafelkow.
// [FIX tej sesji] `symZima` usuniety - podzial sezonowy grupy kafelkow w
// LVGL zostal calkowicie zlikwidowany (wszystkie kafelki, w tym PANEL, sa
// teraz zawsze widoczne), wiec symulacja nie ma juz nic do przelaczania.
static const float SYM_T_POKOJ     = 21.5f;
static const float SYM_WILGOTNOSC  = 45.0f;
static const float SYM_CISNIENIE   = 1013.0f;
// [LVGL-DATA-CZAS, ta sesja] Symulacja nie ma prawdziwego RTC (ani centrali,
// zeby go odczytac) - godzina/minuta licza sie z millis() od startu programu
// (patrz symOdswiezDoLvgl() nizej), zeby kafelek DATA I CZAS mial na ekranie
// cos, co faktycznie plynie, zamiast zamrozonej stalej. Data zostaje stala
// (SYM_DZIEN/SYM_MIESIAC/SYM_ROK) - w symulacji nie ma znaczenia ktory to
// dzien, tylko to, ze zegar dziala.
static const int   SYM_DZIEN    = 12;
static const int   SYM_MIESIAC  = 8;
static const int   SYM_ROK      = 2026;
// [POPRAWKA, ta sesja - panel SERWO v2] SYM_KLAPA/SYM_SYBERKA zostaja jako
// wartosci POCZATKOWE - od momentu pierwszej interakcji z panelem SERWO w
// symulacji ich miejsce zajmuja symKlapaPozycja/symSyberkaPozycja ponizej
// (te dwie stale sluza tylko jako wartosc startowa).
static const int   SYM_KLAPA       = 30;
static const int   SYM_SYBERKA     = 60;

// [ROZBUDOWA, ta sesja] Symulowana strategia pompy (1=Trociniak domyslnie,
// tak jak `Wybor` na centrali) - pozwala przetestowac ikone "tryb pracy" +
// cala podstrone wyboru bez fizycznej centrali obok, tym samym duchem co
// reszta tego bloku symulacyjnego.
static int symWybor = 1;

// [POPRAWKA, ta sesja - panel SERWO v2] Symulowany stan SERWA - jeden
// wspolny tryb (1=Auto/2=Reczny/3=Bezpieczna, jak `TrybSerwa` na centrali)
// + osobne pozycje per os, zeby dalo sie przetestowac panel SERWO (3
// przyciski trybu + slider + presety) na biurku, bez fizycznej centrali,
// tym samym duchem co symWybor/symPompaWlaczona wyzej.
static int symTrybSerwa     = 1;
static int symKlapaPozycja   = SYM_KLAPA;
static int symSyberkaPozycja = SYM_SYBERKA;

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Symulowane wartosci 6
// parametrow automatyki - domyslne = te same defaulty co centrala uzywa przy
// pierwszym uruchomieniu (patrz main_centrala.cpp, deklaracje Skok_Klapy=5
// itd.), tylko zeby ekran mial czym sie wypelnic w symulacji.
static int symTempZadServo = 45;
static int symHisterServo  = 3;
static int symSkokKlapy    = 5;
static int symSkokSyberka  = 5;
static int symOdchylTemp   = 5;
static int symMnoznik      = 1;

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Symulowane wartosci 5
// parametrow pompy + antystopWlaczony - ten sam powod/wzorzec co blok SERWO
// powyzej, domyslne = te same defaulty co centrala (main_centrala.cpp:
// Czas_ON/Czas_OFF/Temp_ON/Temp_OFF/antystopDni=10/antystopWlaczony=true).
static int  symCzasOn          = 20;
static int  symCzasOff         = 10;
static int  symTempOn          = 45;
static int  symTempOff         = 40;
static int  symAntystopDni     = 10;
static bool symAntystopWlaczony = true;

// [ROZSZERZENIE, ta sesja - PANEL DYM] Symulowane wartosci - ten sam wzorzec
// co bloki SERWO/POMPA powyzej, domyslne = te same defaulty co centrala
// (main_centrala.cpp: progAlarmowyDymu=500, dymCzasOn=130/dymCzasOff=60/
// dymCzasStabilizacji=90).
static int symDymProg              = 500;
static int symDymCzasOn            = 130;
static int symDymCzasOff           = 60;
static int symDymCzasStabilizacji  = 90;

// Przelicza aktualny stan symulacji na wywolanie do LVGL - wolane zarowno po
// kazdej akcji z panelu, jak i cyklicznie z loop() (zeby countdown/arc sie
// faktycznie ruszaly na ekranie, a nie tylko przy zmianie trybu).
static void symOdswiezDoLvgl() {
  int pompaOverrideMin = 0;
  if (symOverrideDoMs > 0) {
    long pozostaloMs = (long)(symOverrideDoMs - millis());
    if (pozostaloMs <= 0) {
      // Cichy powrot do AUTO - dokladnie ten sam wzorzec self-healing co
      // centrala (POMPA_OVERRIDE_MS), tylko liczony lokalnie w tej symulacji.
      symOverrideDoMs = 0;
      pompaOverrideMin = 0;
    } else {
      // Zaokraglenie w gore (min. 1), zeby countdown nie pokazywal "0m" tuz
      // przed cichym powrotem do AUTO - taka sama logika jak po stronie
      // centrali dla pompa_override_min.
      pompaOverrideMin = (int)((pozostaloMs + 59999UL) / 60000UL);
      // Skala minut wzgledem SKROCONEGO czasu testowego, ale WYSWIETLANA
      // wartosc ma sensownie wypelniac luk liczony wzgledem 60 (patrz
      // SYM_OVERRIDE_MIN_CALKOWITE) - przeskalowujemy proporcjonalnie, zeby
      // przy SYM_OVERRIDE_TRWANIE_MS=2min luk i tak zaczynal sie od "pelna
      // minuta odpowiadajaca 60".
      pompaOverrideMin = (int)(((long)pompaOverrideMin * SYM_OVERRIDE_MIN_CALKOWITE)
                                / (long)(SYM_OVERRIDE_TRWANIE_MS / 60000UL));
      if (pompaOverrideMin < 1) pompaOverrideMin = 1;
      if (pompaOverrideMin > SYM_OVERRIDE_MIN_CALKOWITE) pompaOverrideMin = SYM_OVERRIDE_MIN_CALKOWITE;
    }
  }

  // [LVGL-DATA-CZAS, ta sesja] Zegar symulacji liczony z millis() od startu
  // programu (00:00 przy starcie, plynie realnie) - patrz komentarz przy
  // SYM_DZIEN/SYM_MIESIAC/SYM_ROK wyzej co do tego, dlaczego data jest stala.
  unsigned long symMinutyOdStartu = millis() / 60000UL;
  int symGodzina = (int)((symMinutyOdStartu / 60) % 24);
  int symMinuta  = (int)(symMinutyOdStartu % 60);

  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Ten sam hak co w
  // pobierzStatus() (galaz prawdziwej centrali) - symulacja NIE dotyka
  // `stan`, wiec harmonogram przyciemnienia musi dostac swiezy symGodzina
  // wprost stad. rtcOk=true - symulacja ma zawsze "gotowy zegar" (ten sam
  // zapis co kilka linii nizej w lvgl_dashboard_poc_aktualizuj()).
  wyswietlaczOstatniaGodzina = symGodzina;
  wyswietlaczGodzinaZnana    = true;
  wyswietlaczHarmonogramOdswiez();

  // [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Ten sam hak co WYSWIETLACZ
  // powyzej - symulacja NIE dotyka `stan`, wiec brzeczyk musi dostac swiezy
  // stan alarmow wprost stad (te same 3 stale, ktore zaraz nizej trafiaja
  // tez do lvgl_dashboard_poc_aktualizuj() - jedno miejsce prawdy).
  brzeczykOdswiezZAlarmow(SYM_DYM_ALARM, SYM_ALARM_OGRZEWANIE, SYM_ALARM_PANEL);

  lvgl_dashboard_poc_aktualizuj(true, SYM_T_ZEWN, SYM_T_BOJLER,
                                 SYM_T_OGRZ, SYM_T_OGRZ_SR, SYM_T_PANEL,
                                 symPompaWlaczona, pompaOverrideMin,
                                 SYM_DYM, SYM_DYM_ALARM,
                                 SYM_T_POKOJ, SYM_WILGOTNOSC, SYM_CISNIENIE,
                                 symKlapaPozycja, symSyberkaPozycja, symWybor,
                                 symTrybSerwa,
                                 SYM_ALARM_OGRZEWANIE, SYM_ALARM_PANEL,
                                 symGodzina, symMinuta, SYM_DZIEN, SYM_MIESIAC, SYM_ROK,
                                 true, // rtcOk=true - symulacja ma zawsze "gotowy zegar"
                                 SYM_RSSI,
                                 SYM_INTERNET,
                                 0, -1); // [DESZCZ-ETAP4] symulacja: brak opadow (te same domyslne co stan.deszczStan/deszczMinuty) - bez centrali nie ma prawdziwej prognozy do pokazania
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Wartosci demo w trybie
  // symulacji (bez centrali nie ma skad ich pobrac) - tylko zeby ekran nie
  // pokazywal samych zer podczas testowania samej plytki.
  lvgl_dashboard_poc_aktualizuj_ustawienia_serwa(symTempZadServo, symHisterServo,
                                                  symSkokKlapy, symSkokSyberka,
                                                  symOdchylTemp, symMnoznik);
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Ten sam demo-wzorzec co
  // wywolanie serwa powyzej.
  lvgl_dashboard_poc_aktualizuj_ustawienia_pompy(symCzasOn, symCzasOff,
                                                  symTempOn, symTempOff,
                                                  symAntystopDni);
  lvgl_dashboard_poc_aktualizuj_ustawienia_pompy_antystop(symAntystopWlaczony);
  // [ROZSZERZENIE, ta sesja - PANEL DYM] Ten sam demo-wzorzec co serwo/pompa
  // powyzej.
  lvgl_dashboard_poc_aktualizuj_ustawienia_dymu(symDymProg, symDymCzasOn,
                                                 symDymCzasOff, symDymCzasStabilizacji);
}

// Podpiete pod lvgl_dashboard_poc_ustaw_callback_pompy() w setup() zamiast
// pompaManualWyslij() - zero HTTP, tylko zmiana lokalnych zmiennych + od
// razu odswiezenie ekranu (dokladnie ten sam UX co przy prawdziwym POST:
// dotkniecie przycisku w panelu daje natychmiastowy efekt na kafelku).
static void symPompaAkcja(const char *stanTekst) {
  Serial.printf("[LVGL-SYM] pompa -> %s\n", stanTekst);
  if (strcmp(stanTekst, "auto") == 0) {
    symOverrideDoMs = 0;
  } else {
    symPompaWlaczona = (strcmp(stanTekst, "wl") == 0);
    symOverrideDoMs = millis() + SYM_OVERRIDE_TRWANIE_MS;
  }
  symOdswiezDoLvgl();
}

// [ROZBUDOWA, ta sesja] Sim-odpowiednik pompaWyborWyslij() - zero HTTP,
// tylko lokalna zmienna + odswiezenie, ten sam wzorzec co symPompaAkcja()
// wyzej. Podpiete pod lvgl_dashboard_poc_ustaw_callback_wyboru() w setup().
static void symWyboraAkcja(int wartosc) {
  Serial.printf("[LVGL-SYM] wybor strategii -> %d\n", wartosc);
  symWybor = wartosc;
  symOdswiezDoLvgl();
}

// [POPRAWKA, ta sesja - panel SERWO v2] Sim-odpowiedniki serwoTrybWyslij()/
// serwoPozycjaWyslij() - zero HTTP, ten sam wzorzec co symPompaAkcja()/
// symWyboraAkcja() wyzej. Podpiete pod
// lvgl_dashboard_poc_ustaw_callback_serwo_tryb()/..._pozycja() w setup().
static void symServoTrybAkcja(int wartosc) {
  Serial.printf("[LVGL-SYM] serwo tryb -> %d\n", wartosc);
  symTrybSerwa = wartosc;
  symOdswiezDoLvgl();
}
static void symServoPozycjaAkcja(int id, int pozycja) {
  Serial.printf("[LVGL-SYM] serwo pozycja id=%d -> %d%%\n", id, pozycja);
  if (id == 0) symKlapaPozycja = pozycja; else symSyberkaPozycja = pozycja;
  symOdswiezDoLvgl();
}
// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Symulowany "most" -
// dokladnie ten sam ksztalt co symServoTrybAkcja() wyzej, tylko generyczny
// po polu (jedna funkcja dla wszystkich 6 zamiast 6 osobnych).
static void symServoUstawieniaAkcja(const char *pole, long wartosc) {
  Serial.printf("[LVGL-SYM] serwo ustawienie %s -> %ld\n", pole, wartosc);
  if      (strcmp(pole, "tempZadServo") == 0) symTempZadServo = wartosc;
  else if (strcmp(pole, "histerServo")  == 0) symHisterServo  = wartosc;
  else if (strcmp(pole, "skokKlapy")    == 0) symSkokKlapy    = wartosc;
  else if (strcmp(pole, "skokSyberka")  == 0) symSkokSyberka  = wartosc;
  else if (strcmp(pole, "odchylTemp")   == 0) symOdchylTemp   = wartosc;
  else if (strcmp(pole, "mnoznik")      == 0) symMnoznik      = wartosc;
  symOdswiezDoLvgl();
}
// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Sim-odpowiedniki, ten sam
// ksztalt co symServoUstawieniaAkcja() wyzej (5 pol numeryczne) + osobna
// funkcja dla przelacznika antystopWlaczony (bool, ten sam podzial co
// PompaUstawieniaCb/PompaAntystopCb w dashboard_lvgl_poc.h).
static void symPompaUstawieniaAkcja(const char *pole, long wartosc) {
  Serial.printf("[LVGL-SYM] pompa ustawienie %s -> %ld\n", pole, wartosc);
  if      (strcmp(pole, "czasOn")      == 0) symCzasOn      = wartosc;
  else if (strcmp(pole, "czasOff")     == 0) symCzasOff     = wartosc;
  else if (strcmp(pole, "tempOn")      == 0) symTempOn      = wartosc;
  else if (strcmp(pole, "tempOff")     == 0) symTempOff     = wartosc;
  else if (strcmp(pole, "antystopDni") == 0) symAntystopDni = wartosc;
  symOdswiezDoLvgl();
}
static void symPompaAntystopAkcja(int wartosc) {
  Serial.printf("[LVGL-SYM] pompa antystop -> %d\n", wartosc);
  symAntystopWlaczony = (wartosc != 0);
  symOdswiezDoLvgl();
}

// [ROZSZERZENIE, ta sesja - PANEL DYM] Sim-odpowiedniki, ten sam ksztalt co
// bloki SERWO/POMPA powyzej - jeden dla suwaka progu, jeden generyczny dla
// 3 pol czasowych.
static void symDymProgAkcja(int wartosc) {
  Serial.printf("[LVGL-SYM] dym prog -> %d\n", wartosc);
  symDymProg = wartosc;
  symOdswiezDoLvgl();
}
static void symDymUstawieniaAkcja(const char *pole, long wartosc) {
  Serial.printf("[LVGL-SYM] dym ustawienie %s -> %ld\n", pole, wartosc);
  if      (strcmp(pole, "dymCzasOn")           == 0) symDymCzasOn           = wartosc;
  else if (strcmp(pole, "dymCzasOff")          == 0) symDymCzasOff          = wartosc;
  else if (strcmp(pole, "dymCzasStabilizacji") == 0) symDymCzasStabilizacji = wartosc;
  symOdswiezDoLvgl();
}

// [PARTIA 5e, ta sesja - EKRAN WYKRESY] Atrapa CSV dla trybu symulacji (bez
// centrali obok) - PLAN_WYKRESY_LVGL.md, "Tryb LVGL_POC_SYMULACJA dostaje
// atrapę CSV do testów na biurku". Liczba kolumn MUSI pokryc najwiekszy
// indeks, jakiego oczekuje parser w dashboard_lvgl_poc.cpp
// (WYKRESY_SERIE[].kolKrotki/kolDlugiSr) - SYBERKO w buforze dlugim ma
// kolDlugiSr=31, +2 na min/max = 33, wiec 34 kolumny (0=etykieta + 33
// danych) starczaja na OBA formaty CSV (bufor dlugi uzywa mniej kolumn niz
// deklarowane tu, ale generujemy tyle samo w obu - nadmiarowe kolumny
// parser po prostu ignoruje). Wartosci to prosta sinusoida przesunieta
// per-kolumna, zeby na oku bylo widac realny ksztalt linii, nie plaska
// atrape.
static String symWykresyCsvZbuduj(bool dlugi) {
  const int LICZBA_KOLUMN = 34;
  const int LICZBA_WIERSZY = dlugi ? 40 : 60;
  String csv;
  csv.reserve(LICZBA_WIERSZY * LICZBA_KOLUMN * 5);
  for (int w = 0; w < LICZBA_WIERSZY; w++) {
    if (dlugi) { csv += "01.01 "; csv += String(w % 24); csv += ":00"; }
    else       { csv += "12:";    csv += String(w); }
    for (int k = 1; k < LICZBA_KOLUMN; k++) {
      float baza = 20.0f + 10.0f * sinf((w + k) * 0.15f);
      csv += ",";
      csv += String(baza, 1);
    }
    csv += "\n";
  }
  return csv;
}

// Podpiete pod lvgl_dashboard_poc_ustaw_callback_wykresy_fetch() w setup()
// (galaz symulacji) - zero HTTP, ten sam wzorzec "udawaj centrale lokalnie"
// co reszta symXAkcja() powyzej, tylko tu zwracamy dane od razu
// (synchronicznie) zamiast zmieniac stan i wolac symOdswiezDoLvgl() - ekran
// Wykresy nie jest czescia glownego cyklu odswiezania dashboardu.
static void symWykresyFetchAkcja(int tryb) {
  const bool dlugi = (tryb == LVGL_WYKRESY_TRYB_TYDZIEN);   // [v1.3.0] most podaje tryb (godz/dzien/tydz), nie sam format
  // [FIX-WYKRESY-RAM, ta sesja] symWykresyCsvZbuduj() i tak buduje caly CSV
  // w RAM jako String (to atrapa, dane sa male) - brak RAM tu nie grozi.
  // Dzielimy go na linie i podajemy do TEGO SAMEGO strumieniowego API co
  // prawdziwy fetch (wykresyFetchStart()/obslugaWykresyFetch() powyzej), zeby obie sciezki
  // (symulacja/sprzet) uzywaly jednego zrodla prawdy dla logiki redukcji
  // punktow po stronie dashboard_lvgl_poc.cpp.
  Serial.printf("[LVGL-SYM] wykresy fetch (atrapa CSV) dlugi=%d\n", dlugi ? 1 : 0);
  String csv = symWykresyCsvZbuduj(dlugi);
  lvgl_dashboard_poc_wykresy_strumien_start(dlugi);
  uint32_t wierszy = 0;
  const char *p = csv.c_str();
  char bufLinii[256];
  while (*p) {
    const char *koniec = strchr(p, '\n');
    size_t dl = koniec ? (size_t)(koniec - p) : strlen(p);
    if (dl >= sizeof(bufLinii)) dl = sizeof(bufLinii) - 1;
    memcpy(bufLinii, p, dl);
    bufLinii[dl] = '\0';
    if (dl > 0) { lvgl_dashboard_poc_wykresy_strumien_linia(bufLinii); wierszy++; }
    if (!koniec) break;
    p = koniec + 1;
  }
  lvgl_dashboard_poc_wykresy_strumien_koniec(dlugi, wierszy, LVGL_WYKRESY_OK, 0);
}
#endif // LVGL_POC_TEST && LVGL_POC_SYMULACJA

// ----------------------------------------------------------------------------
// [v1.2.0 DIAG] DIAGNOSTYKA: co przyszlo z centrali i co trafilo na ekran
// ----------------------------------------------------------------------------
// Loguje odpowiedz centrali, ktorej NIE dalo sie uzyc - z przyczyna (kod HTTP
// / brak ciala / blad JSON), rozmiarem oraz poczatkiem i koncem tresci. W
// v1.1.0 ten przypadek konczyl sie cichym `return`: warstwa sieciowa pisala
// "sukces" (bo gniazdo sie zamknelo), a ekran po prostu przestawal sie
// odswiezac. Przy bledzie JSON dochodzi `podejrzane=` - pierwsze pole, ktorego
// wartosc nie jest JSON-em (typowo `"cisnienie":nan`, patrz
// diag::znajdzZlaWartosc()); puste, gdy takiego pola nie znaleziono.
// Dlawik: najwyzej jeden wpis na 10 s na endpoint, z licznikiem pominietych.
static void zglosZlaOdpowiedz(const char *co, diag::Dlawik &dlawik, diag::Wynik wynik, int kodHttp,
                               const char *bladJson, const String &surowa, const char *cialo) {
  if (!dlawik.pozwol(millis(), 10000UL)) return;
  const char *tresc = (cialo != nullptr) ? cialo : surowa.c_str();
  char poczatek[41], koniec[33], podejrzane[49];
  diag::skrotDoLogu(tresc, poczatek, sizeof(poczatek));
  diag::skrotDoLogu(diag::ogon(tresc, sizeof(koniec) - 1), koniec, sizeof(koniec));
  diag::skrotDoLogu((wynik == diag::WYNIK_JSON) ? diag::znajdzZlaWartosc(tresc) : nullptr, podejrzane, sizeof(podejrzane));
  // ram/ram_blok: json=NoMemory albo ucieta odpowiedz (IncompleteInput) przy
  // malym ram_blok to brak pamieci w panelu, nie blad centrali. [v1.2.2] Te
  // dwie liczby to pamiec, z ktorej korzysta malloc() - patrz ramWolny();
  // v1.2.0/v1.2.1 pisaly tu "heap=/blok_max=" zawyzone o ok. 39 kB.
  // logBezPamieci(): ta linia musi sie wypisac wlasnie wtedy, gdy pamieci nie ma.
  logBezPamieci("tag=%s lvl=ERR zdarzenie=odpowiedz_nieczytelna przyczyna=%s http=%d json=%s bajtow=%u cialo_bajtow=%u pominieto=%lu ram=%lu ram_blok=%lu poczatek=\"%s\" koniec=\"%s\" podejrzane=\"%s\"\n",
                co, diag::wynikTekst(wynik), kodHttp, bladJson,
                (unsigned)surowa.length(), (unsigned)(cialo != nullptr ? strlen(cialo) : 0),
                (unsigned long)dlawik.zabierzPominiete(),
                (unsigned long)ramWolny(), (unsigned long)ramBlok(),
                poczatek, koniec, podejrzane);
}

#if LVGL_POC_TEST
// Biezacy `stan` -> dashboard LVGL. Ta sama lista argumentow co w
// obslugaStatusFetch() i setup() (tamte dwa wywolania zostaly bez zmian).
static void lvglPokazStan() {
  lvgl_dashboard_poc_aktualizuj(stan.polaczono, stan.tZewn, stan.tBojler,
                                 stan.tOgrz, stan.tOgrzSr, stan.tPanel,
                                 stan.pompa, stan.pompaOverrideMin,
                                 stan.dym, stan.dymAlarm,
                                 stan.tPokoj, stan.wilgotnosc, stan.cisnienie,
                                 stan.klapa, stan.syberka, (int)trybWyboru,
                                 (int)trybSerwaWartosc,
                                 stan.alarmOgrzewanie, stan.alarmPanel,
                                 stan.godzina, stan.minuta, stan.dzien, stan.miesiac, stan.rok,
                                 stan.rtcOk,
                                 stan.rssi,
                                 stan.internet,
                                 stan.deszczStan, stan.deszczMinuty);
}

// Znacznik na srodku naglowka dashboardu - Z KTORA centrala rozmawia panel
// i czy pokazywane wartosci sa pomiarem:
//   "SYMULATOR"              - odpowiada centrala-testowa-c3,
//   "SYMULACJA: zewn, pokoj" - prawdziwa centrala podaje te czujniki jako
//                              symulowane (wlaczone z jej panelu WWW),
//   "2 CENTRALE (A4:1C)"     - przy starcie w zasiegu byly 2 sieci o nazwie
//                              WIFI_SSID; w nawiasie dwa ostatnie bajty BSSID
//                              tej, z ktora panel jest polaczony (pelny BSSID
//                              jest w logu: tag=WIFI zdarzenie=polaczono).
// Pusty = jedna, zwykla centrala bez symulacji (nic sie nie wyswietla).
// [v1.2.1 RATUNEK] Gdy panel NIE MA danych (kafelki pokazuja "--"), w tym
// samym miejscu stoi powod i licznik sekund, np. "CENTRALA MILCZY 14 s", a w
// ostatnich 10 s przed samoczynnym restartem "RESTART ZA 7 s: BRAK WIFI"
// (patrz diag::znacznikBrakuDanych() i panelRatunekKrok()).
static void lvglPokazZnacznikCentrali() {
  char znacznik[48];
  if (straznikSwiezosci.oznaczoneJakoStare) {
    const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();
    const uint32_t progRestartu = diag::progRestartuMs(rtcAutorestart.kolejnych);
    diag::znacznikBrakuDanych(znacznik, sizeof(znacznik), przyczyna,
                              panelRatunek.bezDanychMs / 1000UL,
                              diag::restartMaSens(przyczyna, rtcAutorestart.kolejnych)
                                  ? (panelRatunek.doRestartuMs(progRestartu) + 999UL) / 1000UL
                                  : diag::BEZ_RESTARTU);
  } else {
    diag::znacznikCentrali(znacznik, sizeof(znacznik),
                           centralaRozpoznana && centralaToSymulator,
                           wifiCentralPrzyStarcie,
                           (WiFi.status() == WL_CONNECTED) ? WiFi.BSSID() : nullptr,
                           centralaSymulacjaMaska);
  }
  lvgl_dashboard_poc_aktualizuj_centrale(znacznik);
}
#endif

// [v1.2.0 DANE-NIEAKTUALNE] Wolane RAZ, gdy od ostatniego poprawnie
// odczytanego /api/status minelo PROG_NIEAKTUALNYCH_DANYCH_MS (patrz loop()).
// W v1.1.0 po utracie danych kafelki pokazywaly ostatnie znane wartosci bez
// zadnego znaku, ze sa stare - na ekranie sterownika pieca to grozniejsze
// niz brak wartosci. lvgl_dashboard_poc_aktualizuj(polaczono=false) pokazuje
// "--" na kafelkach i "X" na wskazniku WiFi; ta galaz istniala od dawna, ale
// v1.1.0 docieral do niej wylacznie z setup() (gdy nie udal sie PIERWSZY
// odczyt) - w trakcie pracy nieudany odczyt konczyl sie `return` przed
// jakimkolwiek rysowaniem.
void panelOznaczDaneJakoNieaktualne() {
  stan.polaczono = false;
  // wiek_s=-1: od wlaczenia panelu nie bylo jeszcze ANI JEDNEGO poprawnego
  // statusu. wifi=1 przy braku danych znaczy: siec jest, ale centrala nie
  // odpowiada albo jej odpowiedzi sa nieczytelne (patrz tag=STATUS lvl=ERR).
  // [v1.2.1] przyczyna=wifi|milczy|odp|pam - to samo, co panel pokazuje w naglowku
  // (BRAK WIFI / CENTRALA MILCZY / ZLA ODPOWIEDZ / BRAK PAMIECI), patrz diag::Przyczyna.
  // restart_za_s=-1: przy tej przyczynie restartu juz nie bedzie (diag::restartMaSens()).
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();
  daneDobreSeria = false;   // koniec nieprzerwanie poprawnych danych
  logBezPamieci("tag=PANEL lvl=WARN zdarzenie=dane_nieaktualne wiek_s=%ld prog_s=%lu wifi=%d przyczyna=%s restart_za_s=%ld\n",
                swiezoscStatusu.byloKiedykolwiek ? (long)swiezoscStatusu.wiekSekund() : -1L,
                (unsigned long)(PROG_NIEAKTUALNYCH_DANYCH_MS / 1000UL),
                (int)wifiOk,
                diag::przyczynaSkrot(przyczyna),
                diag::restartMaSens(przyczyna, rtcAutorestart.kolejnych)
                    ? (long)(panelRatunek.doRestartuMs(diag::progRestartuMs(rtcAutorestart.kolejnych)) / 1000UL) : -1L);
#if LVGL_POC_TEST
  lvglPokazStan();   // polaczono=false -> "--" na wszystkich kafelkach
  lvglPokazZnacznikCentrali();   // [v1.2.1] naglowek: czego brakuje
#endif
}

// ----------------------------------------------------------------------------
// [v1.2.1 RATUNEK] Panel bez danych ratuje sie sam
// ----------------------------------------------------------------------------
// Zgloszenie z 2026-11-12 (wg uzytkownika na plytce byla v1.2.0): panel nie
// pokazywal swiezych danych, az uzytkownik go zresetowal - po resecie dane
// byly od razu. Logu panelu z tej chwili nie ma, wiec PRZYCZYNA NIE JEST
// USTALONA. Wiadomo tylko tyle: (1) w logu z 2026-11-11 (starszy build), tuz
// przed takim samym resetem, kazda proba polaczenia z centrala konczyla sie
// od razu bledem "connect() natychmiastowy blad"; (2) log centrali z
// 2026-11-12 wyglada na zebrany ok. 5 minut po JEJ starcie, czyli centrala
// byla zapewne krotko wczesniej restartowana. v1.2.1 robi wiec dwie rzeczy: automatyzuje
// to, co pomaga (restart), i zapisuje to, czego brakuje do ustalenia przyczyny.
//
// Kolejnosc (czas liczony od ostatniego poprawnie odczytanego statusu):
//   10 s  kafelki "--" (od v1.2.0) + w naglowku powod: BRAK WIFI /
//         CENTRALA MILCZY / ZLA ODPOWIEDZ i licznik sekund,
//   12 s  WiFi zglasza polaczenie, a centrala milczy -> rozlacz i polacz od
//         nowa (wifiWymusPolaczenie()); po kolejnych 12 s milczenia jeszcze
//         raz, juz bez zapamietanego BSSID. v1.1.0 i v1.2.0 w tym stanie nie
//         robily NIC - reconnect ruszal tylko wtedy, gdy WiFi samo zglosilo
//         rozlaczenie,
//   45 s  restart panelu (panelAutorestart()). Kolejne restarty z rzedu: po
//         90 s, 3 min i dalej co 5 min (diag::progRestartuMs()); "z rzedu"
//         konczy sie po 2 min nieprzerwanie poprawnych danych. Przy
//         nieczytelnych odpowiedziach centrali najwyzej 2 restarty
//         (diag::restartMaSens()).
// Po takim restarcie panel zglasza centrali powod (reset_powod=auto_...), a
// centrala wpisuje go do SWOJEGO logu: "tag=RESET-DIAG ... powod=auto_...".
// Zgloszenie idzie raz (do pierwszej odpowiedzi centrali) i nie po kazdym
// restarcie z serii - patrz autorestartOdczytajPrzyStarcie().
void wifiWymusPolaczenie(bool bezBssid, uint8_t nr);   // definicja w sekcji WIFI

// Zapisuje w pamieci RTC to, co ma przetrwac samoczynny restart (powod dla
// centrali, liczniki, stan alarmow). Bez przydzialow pamieci i bez dlugich
// linii logu - wolane takze wtedy, gdy pamieci juz nie ma.
static void autorestartZapiszPamiec(diag::Przyczyna przyczyna, uint8_t etap, int numer, uint32_t ram, uint32_t blok) {
  const uint32_t bezDanychS = panelRatunek.bezDanychMs / 1000UL;
  rtcAutorestart.przyczyna = (uint8_t)przyczyna;
  rtcAutorestart.kodRozlaczenia = wifiOstatniKodRozlaczenia;
  rtcAutorestart.odnotujRestart();   // liczniki: z rzedu, w serii, lacznie
  rtcAutorestart.bezDanychS = (bezDanychS > 65535UL) ? 65535 : (uint16_t)bezDanychS;
  rtcAutorestart.heapKb = (uint16_t)(ram / 1024UL);
  rtcAutorestart.blokKb = (uint16_t)(blok / 1024UL);
  rtcAutorestart.bladEtap = etap;
  rtcAutorestart.bladNumer = (numer < 0 || numer > 65535) ? 65535 : (uint16_t)numer;
  // Ostatni znany stan alarmow jedzie przez restart - patrz diag::spakujAlarmy()
  // i alarmPrzywrocPoAutorestarcie(). `stan` trzyma ostatnie wartosci z
  // centrali takze wtedy, gdy danych juz nie ma.
#if LVGL_POC_TEST
  const int alarmOdrzuconyPoziom = lvgl_dashboard_poc_alarm_odrzucony();
#else
  const int alarmOdrzuconyPoziom = 0;
#endif
  rtcAutorestart.alarmy = diag::spakujAlarmy(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel,
                                             g_brzeczykWyciszony, (int)g_brzeczykWyciszonyTyp, alarmOdrzuconyPoziom);
  rtcAutorestart.zapieczetuj();
}

void panelAutorestart(diag::Przyczyna przyczyna) {
  const uint32_t bezDanychS = panelRatunek.bezDanychMs / 1000UL;
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  const uint8_t kodRozlaczenia = wifiOstatniKodRozlaczenia;
  const uint32_t ram = ramWolny();
  const uint32_t blok = ramBlok();
  autorestartZapiszPamiec(przyczyna, (uint8_t)ostatniBladStatusuEtap, ostatniBladStatusuNumer, ram, blok);

  logBezPamieci("tag=PANEL lvl=ERR zdarzenie=autorestart przyczyna=%s bez_danych_s=%lu nr_z_rzedu=%u w_serii=%u lacznie=%u wifi=%d rssi=%d "
                "rozl_kod=%u rozl_nazwa=%s rozl_n=%lu ostatni_blad=\"%s\" ram=%lu ram_blok=%lu ram_min=%lu alarmy=0x%02X "
                "msg=\"brak danych z centrali - restart panelu\"\n",
                diag::przyczynaSkrot(przyczyna), (unsigned long)bezDanychS,
                (unsigned)rtcAutorestart.kolejnych, (unsigned)rtcAutorestart.seria, (unsigned)rtcAutorestart.lacznie,
                (int)wifiOk, wifiOk ? (int)WiFi.RSSI() : 0,
                (unsigned)kodRozlaczenia, diag::nazwaKoduRozlaczenia(kodRozlaczenia), (unsigned long)wifiLiczbaRozlaczen,
                ostatniBladStatusuTekst, (unsigned long)ram, (unsigned long)blok, (unsigned long)ramMin(),
                (unsigned)rtcAutorestart.alarmy);
  Serial.flush();
  delay(100);
  ESP.restart();
}

// [v1.2.2 PAMIEC] Blad wewnetrzny LVGL (LV_ASSERT_HANDLER w include/lv_conf.h).
// W praktyce: LVGL poprosil o pamiec w trakcie rysowania i jej nie dostal.
// Domyslnie LVGL 8 staje wtedy w pustej petli "while(1);" - ekran zamiera, a
// po 15 s panel restartuje watchdog (log z 2026-11-12: "task_wdt ... loopTask",
// slad wywolan konczy sie w rysowaniu LVGL), bez sladu, co sie stalo. Tutaj:
// napis na ekranie, wpis w logu, powod zapisany dla centrali
// ("auto_lvgl_...", z iloscia wolnej pamieci) i restart od razu.
// Funkcja nie moze wrocic (LVGL uzylby pustego wskaznika) i nie moze prosic
// o pamiec (stad logBezPamieci()).
extern "C" void panel_lvgl_assert(void) {
  static bool wTrakcie = false;
  if (wTrakcie) ESP.restart();   // blad w trakcie obslugi bledu - nie probuj drugi raz
  wTrakcie = true;
  const uint32_t ram = ramWolny();
  const uint32_t blok = ramBlok();
  logBezPamieci("tag=PANEL lvl=ERR zdarzenie=lvgl_assert ram=%lu ram_blok=%lu ram_min=%lu msg=\"blad wewnetrzny LVGL (zwykle brak pamieci) - restart panelu\"\n",
                (unsigned long)ram, (unsigned long)blok, (unsigned long)ramMin());
  autorestartZapiszPamiec(diag::PRZYCZYNA_LVGL, 'v', 0, ram, blok);
  // Napis wprost przez sterownik ekranu (LVGL juz nic nie narysuje). LVGL
  // wola nas spoza funkcji wysylajacej piksele, wiec magistrala jest wolna.
  tft.fillRect(0, 0, 480, 44, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(blok < 4096 ? "BRAK PAMIECI - RESTART" : "BLAD EKRANU - RESTART", 240, 22, 4);
  Serial.flush();
  delay(2500);   // zeby napis dalo sie przeczytac (watchdog zadania ma 15 s)
  ESP.restart();
}

// Wolane w kazdym obrocie loop().
void panelRatunekKrok() {
  // [v1.4.0 OTA] W trakcie pobierania nowego firmware'u drabina ratunkowa stoi:
  // ponowne laczenie WiFi albo restart panelu w srodku zapisu do flash nie
  // pomoglyby, a pobieranie ma wlasne limity (15 s ciszy, 5 minut calosci) i
  // samo sie konczy, gdy centrala zamilknie. Panel w tym czasie nadal pyta o
  // status (do v1.3.0 nie pytal - pobieranie zatrzymywalo petle).
  if (trwaAktualizacjaOtaPanelu) return;
  const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();
  const uint32_t progRestartu = diag::progRestartuMs(rtcAutorestart.kolejnych);
  switch (panelRatunek.krok(millis(), przyczyna == diag::PRZYCZYNA_MILCZY, progRestartu)) {
    case diag::Ratunek::POLACZ_PONOWNIE:           wifiWymusPolaczenie(false, panelRatunek.wymuszen); break;
    case diag::Ratunek::POLACZ_PONOWNIE_BEZ_BSSID: wifiWymusPolaczenie(true, panelRatunek.wymuszen);  break;
    case diag::Ratunek::RESTART:
      if (diag::restartMaSens(przyczyna, rtcAutorestart.kolejnych)) {
        panelAutorestart(przyczyna);
      } else {
        // Odpowiedzi centrali sa nieczytelne mimo dwoch restartow z rzedu:
        // kolejny nic nie da (patrz diag::restartMaSens()). krok() zglosi
        // RESTART ponownie przy nastepnym obrocie - gdy przyczyna sie zmieni
        // (np. zniknie WiFi), restart nastapi od razu.
        panelRatunek.restartOdrzucony();
        static bool zalogowane = false;
        if (!zalogowane) {
          zalogowane = true;
          Serial.printf("tag=PANEL lvl=WARN zdarzenie=autorestart_pominiety przyczyna=%s nr_z_rzedu=%u ostatni_blad=\"%s\" "
                        "msg=\"odpowiedzi centrali nadal nieczytelne - kolejny restart panelu nic nie da\"\n",
                        diag::przyczynaSkrot(przyczyna), (unsigned)rtcAutorestart.kolejnych, ostatniBladStatusuTekst);
        }
      }
      break;
    case diag::Ratunek::NIC:                       break;
  }
  // Dane poprawne nieprzerwanie od 2 minut: nastepny samoczynny restart znow
  // po pierwszym, najkrotszym progu. Od godziny: koniec serii restartow -
  // numeracja zgloszen dla centrali zaczyna sie od nowa
  // (diag::zglosicRestart()).
  if (daneDobreSeria) {
    const unsigned bylo = rtcAutorestart.kolejnych;
    const unsigned byloWSerii = rtcAutorestart.seria;
    if (rtcAutorestart.poDobrychDanych(millis() - daneDobreOdMs)) {
      Serial.printf("tag=PANEL zdarzenie=autorestart_licznik_wyzerowany z_rzedu=%u->%u w_serii=%u->%u dobre_dane_s=%lu\n",
                    bylo, (unsigned)rtcAutorestart.kolejnych, byloWSerii, (unsigned)rtcAutorestart.seria,
                    (unsigned long)((millis() - daneDobreOdMs) / 1000UL));
    }
  }
#if LVGL_POC_TEST
  // Licznik sekund w naglowku ("BRAK WIFI 14 s") - raz na sekunde i tylko
  // wtedy, gdy kafelki pokazuja "--".
  if (straznikSwiezosci.oznaczoneJakoStare) {
    static uint32_t pokazanaSekunda = 0xFFFFFFFFUL;
    const uint32_t sekunda = panelRatunek.bezDanychMs / 1000UL;
    if (sekunda != pokazanaSekunda) {
      pokazanaSekunda = sekunda;
      lvglPokazZnacznikCentrali();
    }
  }
#endif
}

// [v1.2.2 PAMIEC] Raz na sekunde: gdy wolnego RAM-u robi sie malo, wpis w
// logu (najwyzej raz na minute) - zanim zabraknie go na odczytanie
// odpowiedzi centrali.
void panelPamiecKrok() {
  static unsigned long ostatnio = 0;
  const unsigned long teraz = millis();
  if (teraz - ostatnio < 1000UL) return;
  ostatnio = teraz;
  const uint32_t ram = ramWolny();
  if (ram >= diag::RAM_PROG_OSTRZEZENIA_B) return;
  if (!dlawikMaloPamieci.pozwol(teraz, 60000UL)) return;
  dlawikMaloPamieci.zabierzPominiete();
  logBezPamieci("tag=PAMIEC lvl=WARN zdarzenie=malo_pamieci ram=%lu ram_blok=%lu ram_min=%lu prog=%lu\n",
                (unsigned long)ram, (unsigned long)ramBlok(), (unsigned long)ramMin(), (unsigned long)diag::RAM_PROG_OSTRZEZENIA_B);
}

// Jedna linia co PANEL_DIAG_INTERWAL_MS z odpowiedzia na trzy pytania, na
// ktore log v1.1.0 nie odpowiadal: (1) z KTORYM punktem dostepowym panel jest
// polaczony, (2) ile odpowiedzi udalo sie odczytac i ile razy dane poszly na
// ekran, (3) jakie wartosci panel ma teraz w pamieci. Zastepuje trzy linie
// [NET-DIAG] na kazde zapytanie (patrz SiecCfg::POZIOM_LOGU).
void panelDiagOkresowo() {
  static unsigned long ostatnio = 0;
  static bool pierwszaLinia = true;
  const unsigned long teraz = millis();
  if (teraz - ostatnio < (pierwszaLinia ? PANEL_DIAG_PIERWSZA_MS : PANEL_DIAG_INTERWAL_MS)) return;
  ostatnio = teraz;
  pierwszaLinia = false;

  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  char bssid[18];
  diag::formatBssid(wifiOk ? WiFi.BSSID() : nullptr, bssid, sizeof(bssid));
  char sym[72];
  diag::listaSymulowanych(centralaSymulacjaMaska, sym, sizeof(sym));
  // Wartosci t_*/wilg/cisn/dym/klapa/syberka/pompa to DOKLADNIE to, co panel
  // ma w pamieci i podaje kafelkom - do porownania z linia "tag=STATUS" w
  // logu centrali.
  // [v1.2.2 PAMIEC] ram= wolny RAM, ram_min= najmniej wolnego od startu,
  // ram_blok= najwiekszy ciagly wolny blok - wszystko dla pamieci, z ktorej
  // korzysta malloc() (patrz ramWolny()). Zastepuja heap=/heap_min=/blok_max=
  // z v1.2.0 i v1.2.1, ktore byly zawyzone o ok. 39 kB. Zdrowy panel ma tu
  // kilkadziesiat kB; ponizej ok. 16 kB zaczynaja sie klopoty.
  // logBezPamieci(): ta linia ma sie wypisac takze wtedy, gdy pamieci brakuje.
  logBezPamieci("tag=PANEL fw=%s wifi=%d bssid=%s kanal=%d rssi=%d ap_przy_starcie=%d centrala=%s sym=%s "
                "status_ok=%lu status_blad=%lu ust_ok=%lu ust_blad=%lu ekran=%lu wiek_danych_s=%ld "
                "t_zewn=%.1f t_pokoj=%.1f t_ogrz=%.1f t_ogrz_sr=%.1f t_bojler=%.1f t_panel=%.1f "
                "wilg=%.0f cisn=%.0f dym=%d klapa=%d syberka=%d pompa=%d czas=%02d:%02d "
                "ram=%lu ram_min=%lu ram_blok=%lu "
                "bez_danych_s=%lu przyczyna=%s wymuszen=%u rozl_kod=%u rozl_n=%lu autorestartow=%u\n",
                FIRMWARE_VERSION_PANEL, (int)wifiOk, bssid,
                wifiOk ? (int)WiFi.channel() : 0, wifiOk ? (int)WiFi.RSSI() : 0,
                wifiCentralPrzyStarcie,
                !centralaRozpoznana ? "nieznana" : (centralaToSymulator ? "symulator" : "sterownik"),
                sym[0] != '\0' ? sym : "brak",
                (unsigned long)panelLiczniki.statusOk, (unsigned long)panelLiczniki.statusBlad,
                (unsigned long)panelLiczniki.ustawieniaOk, (unsigned long)panelLiczniki.ustawieniaBlad,
                (unsigned long)panelLiczniki.ekranOdswiezen,
                swiezoscStatusu.byloKiedykolwiek ? (long)swiezoscStatusu.wiekSekund() : -1L,
                stan.tZewn, stan.tPokoj, stan.tOgrz, stan.tOgrzSr, stan.tBojler, stan.tPanel,
                stan.wilgotnosc, stan.cisnienie, (int)stan.dym,
                stan.klapa, stan.syberka, (int)stan.pompa,
                stan.godzina, stan.minuta,
                (unsigned long)ramWolny(), (unsigned long)ramMin(), (unsigned long)ramBlok(),
                // [v1.2.1 RATUNEK] od ilu sekund bez poprawnego statusu; czego brakuje ("-" = nic,
                // wifi | milczy | odp | pam); ile razy wymuszono ponowne polaczenie; kod i liczba rozlaczen
                // WiFi od startu; ile samoczynnych restartow od wlaczenia zasilania.
                (unsigned long)(panelRatunek.bezDanychMs / 1000UL),
                straznikSwiezosci.oznaczoneJakoStare ? diag::przyczynaSkrot(panelPrzyczynaTeraz()) : "-",
                (unsigned)panelRatunek.wymuszen,
                (unsigned)wifiOstatniKodRozlaczenia, (unsigned long)wifiLiczbaRozlaczen,
                (unsigned)rtcAutorestart.lacznie);
  panelLiczniki.zeruj();
}

// ----------------------------------------------------------------------------
// POBIERANIE DANYCH
// ----------------------------------------------------------------------------
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Tylko STARTUJE zadanie (albo nic
// nie robi, jesli poprzednie jeszcze trwa - kolejne wywolanie z loop() i tak
// nadejdzie za STATUS_INTERWAL_MS). Parsowanie odpowiedzi i caly "ogon"
// (aktualizacja sezonu/ekranu) przeniesione do obslugaStatusFetch() nizej,
// bo GET juz NIE konczy sie zanim ta funkcja wroci (patrz komentarz przy
// pendingPoKomendzie na poczatku pliku).
void pobierzStatus() {
  if (statusFetch.wTrakcie()) return;
  // [OTA-PANEL, ta sesja] Zgloszenie wlasnej wersji przy kazdym odpytaniu —
  // jedyny sposob, w jaki centrala (panel nie ma internetu) dowiaduje sie,
  // co panel realnie ma wgrane. Patrz FIRMWARE_VERSION_PANEL wyzej.
  // [RESET-DIAG, ta sesja] Doklejony tym samym mechanizmem — powod
  // ostatniego restartu, wypelniony raz na starcie setup(). Wartosci to
  // stale, bezpieczne tokeny (patrz powodResetuTekst()), bez potrzeby
  // url-encode.
  // [v1.4.0 OTA] Dochodza: identyfikator programu, z ktorego panel pracuje
  // (panel_obraz / panel_obraz_proba), a gdy centrala cos oferuje - odmowa
  // albo powod czekania. Adres sklada diag::otaSciezkaStatusu() (opis i
  // testy: diagnostyka_panelu.h); najdluzszy mozliwy ma 164 znaki.
  char sciezka[200];
  if (!diag::otaSciezkaStatusu(sciezka, sizeof(sciezka), FIRMWARE_VERSION_PANEL, globalnyPowodResetu.c_str(),
                               otaObrazWlasny, otaObrazZatwierdzony, otaZglosOdmowe, otaOdmowaNr, otaOdmowaPowod, otaCzekaPowod)) {
    // Nie zmiescilo sie (nie powinno): adres w postaci sprzed v1.4.0.
    snprintf(sciezka, sizeof(sciezka), "/api/status?panel_fw=%s&reset_powod=%.60s", FIRMWARE_VERSION_PANEL, globalnyPowodResetu.c_str());
  }
  statusFetch.startGet(sciezka);   // startGet() od razu kopiuje adres do wlasnego pola
}

// [SIEC-NIEBLOKUJACA, ta sesja] Wywolywana z loop() PRZY KAZDEJ iteracji
// (tanio, gdy statusFetch bezczynny - patrz NonBlockingHttp::poll() w
// siec_nieblokujaca_LCD.h). Cialo parsowania ponizej jest DOKLADNIE dawna
// "druga polowa" pobierzStatus() - tylko teraz uruchamiana gdy GET faktycznie
// sie zakonczy, moze wiele iteracji loop() po jego wystartowaniu.
void obslugaStatusFetch() {
  statusFetch.poll();
  if (!statusFetch.gotowyWynik()) return;
  bool sukces = statusFetch.bylSukces();
  if (!sukces) {
    stan.polaczono = false;
    panelLiczniki.statusBlad++;
    ostatniBladStatusuToTresc = false;   // [v1.2.1 RATUNEK] centrala nie odpowiedziala (albo nie ma do niej drogi)
    snprintf(ostatniBladStatusuTekst, sizeof(ostatniBladStatusuTekst), "%s", statusFetch.ostatniBlad().c_str());
    ostatniBladStatusuEtap = statusFetch.etapBledu();
    ostatniBladStatusuNumer = statusFetch.numerBledu();
    // [v1.2.2 PAMIEC] socket()/connect()/send() odrzucone z ENOMEM, ENOBUFS
    // albo ENFILE: to panelowi zabraklo pamieci (albo gniazd), a nie
    // centrala milczy. Przy limitach czasu numer bledu to 0.
    ostatniBladStatusuToPamiec = diag::bladGniazdaToBrakPamieci(ostatniBladStatusuNumer);
    Serial.printf("[STATUS] GET nieudany: %s\n", ostatniBladStatusuTekst);
    // [FIX-LOG-SPAM, ta sesja] Bez tego stan_ zostawal w ERROR_ na stale -
    // gotowyWynik() (a wiec i ten log) powtarzalby sie na KAZDEJ iteracji
    // loop() az do kolejnej proby, zamiast raz na niepowodzenie. Patrz
    // obszerny komentarz przy skwitujBlad() w siec_nieblokujaca_LCD.h.
    statusFetch.skwitujBlad();
    return;
  }
  // [v1.2.0 DIAG] "sukces" warstwy sieciowej znaczy tylko, ze centrala
  // zamknela gniazdo. Zanim uznamy dane za dobre, sprawdzamy kod HTTP i
  // obecnosc ciala (diag::sprawdzOdpowiedz), a potem sam JSON - i KAZDA
  // porazke logujemy z przyczyna (v1.1.0: ciche `return`, ekran przestawal
  // sie odswiezac bez sladu w logu). JSON parsowany wprost ze wskaznika na
  // cialo w `surowa` (bez dodatkowej kopii substring() jak dotad) - `surowa`
  // zyje do konca tej funkcji, a wszystkie pola sa przepisywane do `stan`
  // ponizej, wiec nic nie wskazuje na nia po wyjsciu.
  // [v1.2.2 PAMIEC] Bez kopii odpowiedzi (bylo: pobierzOdpowiedz(), czyli
  // drugie 1,8 kB na kazde odpytanie): `surowa` to odwolanie do bufora w
  // statusFetch; zadanie wraca do stanu IDLE przy wyjsciu z tej funkcji.
  OdpowiedzDoKonca odpowiedzStatusu(statusFetch);
  const String &surowa = odpowiedzStatusu.tekst();
  int kodHttp = 0;
  const char *cialo = nullptr;
  diag::Wynik wynik = diag::sprawdzOdpowiedz(surowa.c_str(), &kodHttp, &cialo);
  JsonDocument doc;
  DeserializationError bladJson = DeserializationError::Ok;
  if (wynik == diag::WYNIK_OK) {
    bladJson = deserializeJson(doc, cialo);
    // is<JsonObject>(): poprawny JSON, ktory nie jest obiektem (np. sama
    // liczba), dalby same zera we wszystkich polach - to tez blad.
    if (bladJson != DeserializationError::Ok || !doc.is<JsonObject>()) wynik = diag::WYNIK_JSON;
  }
  if (wynik != diag::WYNIK_OK) {
    stan.polaczono = false;
    panelLiczniki.statusBlad++;
    ostatniBladStatusuToTresc = true;    // [v1.2.1 RATUNEK] odpowiedz przyszla, ale jest nieczytelna
    // [v1.2.2 PAMIEC] ArduinoJson 7 zwraca NoMemory tylko wtedy, gdy malloc()
    // odmowil pamieci na rozpakowanie odpowiedzi - czyli odpowiedz centrali
    // jest w porzadku, a zabraklo RAM-u w panelu (log z 2026-11-12).
    ostatniBladStatusuToPamiec = (bladJson == DeserializationError::NoMemory);
    snprintf(ostatniBladStatusuTekst, sizeof(ostatniBladStatusuTekst), "%s: %s http=%d json=%s",
             ostatniBladStatusuToPamiec ? "brak pamieci" : "odpowiedz nieczytelna",
             diag::wynikTekst(wynik), kodHttp, bladJson.c_str());
    ostatniBladStatusuEtap = ostatniBladStatusuToPamiec ? 'm' : 'o';
    ostatniBladStatusuNumer = kodHttp;
    zglosZlaOdpowiedz("STATUS", dlawikBleduStatusu, wynik, kodHttp, bladJson.c_str(), surowa, cialo);
    return;
  }
  stan.polaczono   = true;
  stan.ip          = doc["ip"].as<String>();
  stan.rssi        = doc["rssi"].as<int>();
  stan.internet    = doc["internet"].as<bool>(); // [UI-INTERNET, ta sesja]
  stan.tZewn       = doc["t_zewn"].as<float>();
  stan.tBojler     = doc["t_bojler"].as<float>();
  stan.tOgrz       = doc["t_ogrz"].as<float>();
  stan.tOgrzSr     = doc["t_ogrz_sr"].as<float>();
  stan.tPanel      = doc["t_panel"].as<float>();
  stan.tPokoj      = doc["t_pokoj"].as<float>();
  stan.tTrociny    = doc["t_trociny"].as<float>();
  stan.tPowrot     = doc["t_powrot"].as<float>();
  stan.wilgotnosc  = doc["wilgotnosc"].as<float>();
  stan.cisnienie   = doc["cisnienie"].as<float>();
  stan.dym         = doc["dym"].as<int>();
  stan.dymAlarm    = doc["dym_alarm"].as<bool>();
  stan.klapa       = doc["klapa"].as<int>();
  stan.syberka     = doc["syberka"].as<int>();
  stan.pompa       = doc["pompa"].as<bool>();
  // [LVGL-API] Moze nie istniec w JSON starszej centrali (przed dodaniu
  // /api/pompa-manual) - isNull() zabezpiecza tak samo jak miesiac/rtc_ok nizej.
  stan.pompaOverrideMin = doc["pompa_override_min"].isNull() ? 0 : doc["pompa_override_min"].as<int>();
  stan.alarmOgrzewanie = doc["alarm_ogrzewanie"].as<bool>();
  stan.alarmPanel      = doc["alarm_panel"].as<bool>();
  // [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Patrz obszerny komentarz przy polu
  // stan.deszczStan/deszczMinuty (definicja struct wyzej).
  stan.deszczStan    = doc["deszcz_stan"].isNull() ? 0  : doc["deszcz_stan"].as<int>();
  stan.deszczMinuty  = doc["deszcz_min"].isNull()  ? -1 : doc["deszcz_min"].as<int>();
  // [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Swiezy stan wszystkich 3
  // alarmow jest juz znany - przelicz, ktory wzorzec (jesli ktorykolwiek)
  // powinien teraz grac. Tanie/idempotentne, bezpieczne przy kazdym udanym
  // pobierzStatus() (patrz obszerny komentarz przy brzeczykOdswiezZAlarmow()).
  brzeczykOdswiezZAlarmow(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel);
  // [LCD-SEZON] Pola moga nie istniec w JSON-ie starszej centrali (przed
  // v1.16.0) — isNull() zabezpiecza przed nadpisaniem sensownych domyslnych
  // wartosci smieciami z ArduinoJson (0/false to i tak bezpieczne domyslne:
  // miesiac=0 => "brak RTC", wchodzi wprost w fallback na t_panel nizej).
  stan.miesiac = doc["miesiac"].isNull() ? 0 : doc["miesiac"].as<int>();
  // [LVGL-DATA-CZAS, ta sesja] Analogiczne pola dla kafelka DATA I CZAS -
  // ten sam wzorzec isNull() co miesiac/rtc_ok (centrala starsza niz ten
  // FIX po prostu nie bedzie mala tych kluczy w JSON, wiec zostana 0, co
  // dashboard_lvgl_poc.cpp i tak pokazuje jako "--" dopoki stan.rtcOk==false).
  stan.godzina = doc["godzina"].isNull() ? 0 : doc["godzina"].as<int>();
  stan.minuta  = doc["minuta"].isNull() ? 0 : doc["minuta"].as<int>();
  stan.dzien   = doc["dzien"].isNull() ? 0 : doc["dzien"].as<int>();
  stan.rok     = doc["rok"].isNull() ? 0 : doc["rok"].as<int>();
  stan.rtcOk   = doc["rtc_ok"].isNull() ? false : doc["rtc_ok"].as<bool>();
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Swieza godzina jest juz
  // znana - zapisz ja do wspolnych zmiennych i przelicz harmonogram
  // przyciemnienia (patrz obszerny komentarz przy wyswietlaczHarmonogramOdswiez()
  // wyzej). Tanie/idempotentne, bezpieczne przy kazdym udanym pobierzStatus().
  wyswietlaczOstatniaGodzina = stan.godzina;
  wyswietlaczGodzinaZnana    = stan.rtcOk;
  wyswietlaczHarmonogramOdswiez();
  // [OTA-PANEL, ta sesja] isNull() zabezpiecza tak samo jak miesiac/rtc_ok
  // wyzej — starsza centrala (przed tym dodatkiem) po prostu nie bedzie
  // mial tych pol, panel wtedy nigdy nie sprobuje sciagac OTA.
  stan.panelOtaDostepna = doc["panel_ota_dostepna"].isNull() ? false : doc["panel_ota_dostepna"].as<bool>();
  {
    // [v1.4.0 OTA] Tag wydania, identyfikator pliku i numer oferty. Pola,
    // ktorych centrala nie podaje (albo podaje w innej postaci), daja "" i 0.
    // Identyfikator jest przyjmowany tylko w poprawnej postaci (16 znakow
    // 0-9 a-f) - inaczej oferta jest traktowana jak od starszej centrali.
    const char *tagOferty = doc["panel_ota_wersja"] | "";
    const char *obrazOferty = doc["panel_ota_obraz"] | "";
    snprintf(stan.panelOtaWersja, sizeof(stan.panelOtaWersja), "%s", tagOferty);
    snprintf(stan.panelOtaObraz, sizeof(stan.panelOtaObraz), "%s", diag::otaObrazPoprawny(obrazOferty) ? obrazOferty : "");
    stan.panelOtaNr = doc["panel_ota_nr"] | (uint32_t)0;
  }

  // [v1.2.0 CENTRALA-ID] Pole "symulator":true wysyla wylacznie centrala
  // testowa (centrala-testowa-c3); prawdziwa centrala go nie ma. Patrz
  // komentarz przy centralaToSymulator na gorze pliku.
  const bool symulatorTeraz = doc["symulator"].isNull() ? false : doc["symulator"].as<bool>();
  bool centralaZmieniona = !centralaRozpoznana || (symulatorTeraz != centralaToSymulator);
  centralaRozpoznana = true;
  centralaToSymulator = symulatorTeraz;
  if (centralaZmieniona) {
    Serial.printf("tag=PANEL zdarzenie=centrala_rozpoznana typ=%s\n", symulatorTeraz ? "symulator" : "sterownik");
  }
  // Prawdziwa centrala: ktore czujniki sa teraz symulowane (patrz komentarz
  // przy centralaSymulacjaMaska). Wpis w logu i znacznik w naglowku TYLKO
  // przy zmianie - to takze najprostszy dowod, ze panel odczytal swiezy
  // status: po wlaczeniu symulacji w centrali napis pojawia sie tutaj w
  // ciagu jednego okresu odpytywania.
  uint16_t maskaSym = 0;
  for (int i = 0; i < diag::LICZBA_CZUJNIKOW_SYMULOWANYCH; i++) {
    // "| false": brak pola (starsza centrala) albo wartosc innego typu = nie symulowany
    if (doc[diag::CZUJNIKI_SYMULOWANE[i].pole] | false) maskaSym |= (uint16_t)(1u << i);
  }
  if (maskaSym != centralaSymulacjaMaska) {
    centralaSymulacjaMaska = maskaSym;
    centralaZmieniona = true;
    char lista[72];
    diag::listaSymulowanych(maskaSym, lista, sizeof(lista));
    Serial.printf("tag=PANEL zdarzenie=symulacja_centrali czujniki=%s\n", lista[0] != '\0' ? lista : "brak");
  }

  // [v1.2.0 DANE-NIEAKTUALNE] Dopiero TU dane sa "swieze" - po sprawdzeniu
  // kodu HTTP i JSON-a. W v1.1.0 swiezoscStatusu.odnotujSukces() bylo wolane
  // juz po samym zamknieciu gniazda, czyli takze dla odpowiedzi, ktorej nie
  // dalo sie odczytac (a sam wskaznik nie byl nigdzie uzywany).
  swiezoscStatusu.odnotujSukces();
  panelLiczniki.statusOk++;
  // [v1.2.1 RATUNEK] Sa dane - drabina ratunkowa od poczatku. Licznik
  // samoczynnych restartow z rzedu zeruje panelRatunekKrok(), dopiero gdy
  // dane sa poprawne nieprzerwanie przez diag::CZAS_STABILNYCH_DANYCH_MS.
  panelRatunek.daneSwieze();
  if (!daneDobreSeria) {
    daneDobreSeria = true;
    daneDobreOdMs = millis();
  }
  // [v1.2.1 RATUNEK] Kazde zapytanie od startu nioslo powod "auto_...", wiec
  // ta odpowiedz znaczy, ze centrala go dostala (zapisuje go i oglasza,
  // zanim odpowie). Od teraz panel zglasza zwykle "sw": centrala nie oglosi
  // tego samego restartu drugi raz po WLASNYM restarcie - chocby za tydzien.
  if (autorestartDoZgloszenia) {
    autorestartDoZgloszenia = false;
    Serial.printf("tag=PANEL zdarzenie=autorestart_zgloszony powod=%s msg=\"centrala dostala powod restartu - dalej zglaszam sw\"\n",
                  globalnyPowodResetu.c_str());
    rtcAutorestart.zgloszenieDotarlo();
    globalnyPowodResetu = "sw";
  }
  if (straznikSwiezosci.odnotujSwiezeDane()) {
    Serial.println(F("tag=PANEL zdarzenie=dane_wrocily msg=\"kafelki znow pokazuja wartosci z centrali\""));
    centralaZmieniona = true;   // [v1.2.1] naglowek: zamiast "CENTRALA MILCZY 14 s" znow zwykly znacznik centrali
  }

  // [SIEC-NIEBLOKUJACA, ta sesja] Ponizszy "ogon" byl dawniej doklejany
  // OSOBNO przez KAZDEGO wywolujacego pobierzStatus() (petla loop() w obu
  // galeziach + 4 mosty LVGL: pompaManualWyslij/pompaWyborWyslij/
  // serwoTrybWyslij/serwoPozycjaWyslij) - scentralizowany tutaj RAZ, bo byl
  // identyczny wszedzie w danej galezi, a aktualizujSezon()/
  // lvgl_dashboard_poc_aktualizuj()/rysujDashboard() sa tanie/idempotentne
  // (bezpieczne przy kazdym udanym odswiezeniu, niezaleznie kto je sprowokowal).
#if LVGL_POC_TEST
  aktualizujSezon(); // patrz komentarz [FIX, Partia 2] przy pierwotnym wywolaniu w setup()
  lvgl_dashboard_poc_aktualizuj(stan.polaczono, stan.tZewn, stan.tBojler,
                                 stan.tOgrz, stan.tOgrzSr, stan.tPanel,
                                 stan.pompa, stan.pompaOverrideMin,
                                 stan.dym, stan.dymAlarm,
                                 stan.tPokoj, stan.wilgotnosc, stan.cisnienie,
                                 stan.klapa, stan.syberka, (int)trybWyboru,
                                 (int)trybSerwaWartosc,
                                 stan.alarmOgrzewanie, stan.alarmPanel,
                                 stan.godzina, stan.minuta, stan.dzien, stan.miesiac, stan.rok,
                                 stan.rtcOk,
                                 stan.rssi,
                                 stan.internet,
                                 stan.deszczStan, stan.deszczMinuty);
  panelLiczniki.ekranOdswiezen++;
  if (centralaZmieniona) lvglPokazZnacznikCentrali(); // [v1.2.0 CENTRALA-ID] "SYMULATOR" / "2 CENTRALE (..)" w naglowku
#else
  // [LCD-SEZON] Zmiana sezonu wymusza PELNE przerysowanie dashboardu (uklad
  // kafelkow sie zmienia) i naglowka (ikona slonce/sniezynka) — w
  // przeciwnym razie zostalyby resztki starego ukladu/ikony na ekranie.
  bool sezonZmieniony = aktualizujSezon();
  if (aktualnyEkran == EKRAN_DASHBOARD && panelOtwartyId == -1) rysujDashboard(sezonZmieniony);
  rysujNaglowek(sezonZmieniony);
  panelLiczniki.ekranOdswiezen++;
  (void)centralaZmieniona; // znacznik centrali istnieje tylko na dashboardzie LVGL
#endif
}

// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Tylko STARTUJE zadanie - patrz
// komentarz przy pobierzStatus() wyzej, ten sam wzorzec.
void pobierzUstawienia() {
  if (ustawieniaFetch.wTrakcie()) return;
  ustawieniaFetch.startGet("/api/ustawienia-get");
}

// Wywolywana z loop() przy kazdej iteracji - patrz obslugaStatusFetch() wyzej.
void obslugaUstawieniaFetch() {
  ustawieniaFetch.poll();
  if (!ustawieniaFetch.gotowyWynik()) return;
  bool sukces = ustawieniaFetch.bylSukces();
  if (!sukces) {
    panelLiczniki.ustawieniaBlad++;
    Serial.printf("[USTAWIENIA] GET nieudany: %s\n", ustawieniaFetch.ostatniBlad().c_str());
    // [FIX-LOG-SPAM, ta sesja] Patrz identyczny komentarz w
    // obslugaStatusFetch() wyzej / skwitujBlad() w siec_nieblokujaca_LCD.h.
    ustawieniaFetch.skwitujBlad();
    return;   // zostaw ostatnie znane wartosci
  }
  // [v1.2.0 DIAG] Ten sam wzorzec co w obslugaStatusFetch() wyzej: kod HTTP
  // + cialo + JSON, kazda porazka z przyczyna w logu (bylo: ciche `return`).
  // [v1.2.2 PAMIEC] Bez kopii odpowiedzi - jak w obslugaStatusFetch().
  OdpowiedzDoKonca odpowiedzUstawien(ustawieniaFetch);
  const String &surowa = odpowiedzUstawien.tekst();
  int kodHttp = 0;
  const char *cialo = nullptr;
  diag::Wynik wynik = diag::sprawdzOdpowiedz(surowa.c_str(), &kodHttp, &cialo);
  JsonDocument doc;
  DeserializationError bladJson = DeserializationError::Ok;
  if (wynik == diag::WYNIK_OK) {
    bladJson = deserializeJson(doc, cialo);
    if (bladJson != DeserializationError::Ok || !doc.is<JsonObject>()) wynik = diag::WYNIK_JSON;
  }
  if (wynik != diag::WYNIK_OK) {
    panelLiczniki.ustawieniaBlad++;
    zglosZlaOdpowiedz("USTAWIENIA", dlawikBleduUstawien, wynik, kodHttp, bladJson.c_str(), surowa, cialo);
    return;   // zostaw ostatnie znane wartosci
  }
  swiezoscUstawien.odnotujSukces();
  panelLiczniki.ustawieniaOk++;
  for (int i = 0; i < LICZBA_POL; i++) {
    if (!doc[POLA[i].klucz].isNull()) POLA[i].wartosc = doc[POLA[i].klucz].as<long>();
  }
  // [LCD-TRYBY] Poza POLA[] — patrz komentarz przy deklaracji trybWyboru/
  // trybSerwaWartosc wyzej.
  if (!doc["wybor"].isNull())     trybWyboru      = doc["wybor"].as<long>();
  if (!doc["trybSerwa"].isNull()) trybSerwaWartosc = doc["trybSerwa"].as<long>();
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] Ten sam wzorzec
  // co wybor/trybSerwa powyzej - bool, wiec poza POLA[].
  if (!doc["antystopWlaczony"].isNull()) antystopWlaczonyWartosc = doc["antystopWlaczony"].as<bool>();
  ustawieniaWczytane = true;
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA SERWA] Przekaz swiezo
  // pobrane wartosci dalej do dashboard_lvgl_poc.cpp - ten sam wzorzec co
  // lvgl_dashboard_poc_aktualizuj() dla /api/status, tylko osobna funkcja
  // (patrz obszerny komentarz w dashboard_lvgl_poc.h) bo to inne zrodlo
  // (/api/ustawienia-get, nie /api/status) i inna czestotliwosc odswiezania.
  lvgl_dashboard_poc_aktualizuj_ustawienia_serwa(
      (int)POLA[znajdzIndeksPola("tempZadServo")].wartosc,
      (int)POLA[znajdzIndeksPola("histerServo")].wartosc,
      (int)POLA[znajdzIndeksPola("skokKlapy")].wartosc,
      (int)POLA[znajdzIndeksPola("skokSyberka")].wartosc,
      (int)POLA[znajdzIndeksPola("odchylTemp")].wartosc,
      (int)POLA[znajdzIndeksPola("mnoznik")].wartosc);
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] Ten sam wzorzec co
  // wywolanie serwa powyzej, tylko dla dashboard_lvgl_poc.h ustawien pompy -
  // dwie osobne funkcje (5 pol numerycznych + osobno bool antystopWlaczony),
  // patrz uzasadnienie w dashboard_lvgl_poc.h.
  lvgl_dashboard_poc_aktualizuj_ustawienia_pompy(
      (int)POLA[znajdzIndeksPola("czasOn")].wartosc,
      (int)POLA[znajdzIndeksPola("czasOff")].wartosc,
      (int)POLA[znajdzIndeksPola("tempOn")].wartosc,
      (int)POLA[znajdzIndeksPola("tempOff")].wartosc,
      (int)POLA[znajdzIndeksPola("antystopDni")].wartosc);
  lvgl_dashboard_poc_aktualizuj_ustawienia_pompy_antystop(antystopWlaczonyWartosc);
  // [ROZSZERZENIE, ta sesja - PANEL DYM] Ten sam wzorzec co wywolania
  // serwa/pompy powyzej. progAlarmDym juz byl w POLA[] (stary EKRAN_TRYBY go
  // uzywal), dymCzasOn/Off/Stabilizacji dopisane wyzej w tej sesji.
  lvgl_dashboard_poc_aktualizuj_ustawienia_dymu(
      (int)POLA[znajdzIndeksPola("progAlarmDym")].wartosc,
      (int)POLA[znajdzIndeksPola("dymCzasOn")].wartosc,
      (int)POLA[znajdzIndeksPola("dymCzasOff")].wartosc,
      (int)POLA[znajdzIndeksPola("dymCzasStabilizacji")].wartosc);
  // [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] progAlarmTemp juz byl
  // w POLA[] od dawna (stary EKRAN_TRYBY go uzywal) - tylko dopisujemy
  // przekazanie dalej do LVGL, ten sam wzorzec co progAlarmDym powyzej.
  lvgl_dashboard_poc_aktualizuj_ustawienia_alarmu_temp(
      (int)POLA[znajdzIndeksPola("progAlarmTemp")].wartosc);
  // Nie pobieramy tu klapy/syberka recznych procentow celowo — te sa juz
  // widoczne w /api/status (klapa/syberka), a suwak na ekranie Serwo
  // zaczyna od tych samych wartosci co status przy pierwszym wejsciu na ekran.
}

// ----------------------------------------------------------------------------
// RYSOWANIE — DASHBOARD
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// [LCD-KAFELKI] Zawartosc kafelkow dashboardu — jedno miejsce, ktore wie co
// pokazac (etykieta/wartosc/kolory) dla kazdego z LICZBA_KAFELKOW id, zeby
// petla petla rysujaca w rysujDashboard() nie musiala znac szczegolow kazdego
// czujnika. "Sterowalny" (true dla KLAPA/SYBERKA/DYM) rysuje cienka ramke w
// kolorze KOL_OGIEN — "da sie w to kliknac", dokladnie jak subtelna obwodka
// w makiecie HTML; reszta to czyste odczyty z ramka KOL_RAMKA (bez akcji).
// ----------------------------------------------------------------------------
bool kafelekWidoczny(int id) {
  switch (id) {
    case KAF_OGRZ: case KAF_OGRZ_SR: case KAF_KLAPA: case KAF_SYBERKA: case KAF_DYM:
      return aktualnySezon == SEZON_ZIMA;
    case KAF_PANEL:
      return aktualnySezon == SEZON_LATO;
    default:
      return true;
  }
}

// [LCD-KAFELKI] Kolor jako stan zyje TERAZ wylacznie w ikonie — dokladnie
// jak w makiecie referencyjnej: kazdy typ czujnika ma swoj wlasny, stale
// przypisany kolor ikony (kropla=niebieski, suwak=pomaranczowy,
// plomien/termometr=czerwono-pomaranczowy, chmura=turkusowy...), a WARTOSC
// liczbowa jest zawsze biala/jasna (KOL_TEKST) — kolor nie "podwaja sie"
// na tekscie, dzieki czemu kafelki nie wygladaja jednolicie na jeden akcent.
void tresciKafelka(int id, String& etykieta, String& wartosc, uint16_t& kolorWartosci,
                    uint16_t& kolorIkony, uint16_t& kolorRamki, bool& sterowalny) {
  sterowalny = (id == KAF_KLAPA || id == KAF_SYBERKA || id == KAF_DYM);
  kolorRamki = sterowalny ? KOL_OGIEN : KOL_RAMKA;
  kolorWartosci = KOL_TEKST;   // zawsze bialy/jasny — patrz komentarz wyzej
  kolorIkony = KOL_TEKST_DIM;  // domyslnie (nadpisywane ponizej dla kafelkow, ktore maja ikone)
  switch (id) {
    case KAF_ZEWN:        etykieta = "ZEWN";       wartosc = String(stan.tZewn, 1) + " C"; kolorIkony = KOL_CHMURKA; break;
    case KAF_BOJLER:      etykieta = "BOJLER";     wartosc = String(stan.tBojler, 1) + " C"; kolorIkony = KOL_ZAR; break;
    case KAF_OGRZ:        etykieta = "OGRZ.";      wartosc = String(stan.tOgrz, 1) + " C"; kolorIkony = KOL_ZAR; break;
    case KAF_OGRZ_SR:     etykieta = "OGRZ SR";    wartosc = String(stan.tOgrzSr, 1) + " C"; kolorIkony = KOL_ZAR; break;
    case KAF_PANEL:       etykieta = "PANEL";      wartosc = String(stan.tPanel, 1) + " C"; kolorIkony = KOL_ZAR; break;
    case KAF_POKOJ:       etykieta = "POKOJ";      wartosc = String(stan.tPokoj, 1) + " C"; break;
    case KAF_WILGOTNOSC:  etykieta = "WILGOTNOSC"; wartosc = String(stan.wilgotnosc, 0) + " %"; break;
    case KAF_CISNIENIE:   etykieta = "CISNIENIE";  wartosc = String(stan.cisnienie, 1) + " hPa"; break;
    case KAF_POMPA:
      // [LCD-KAFELKI] POMPA to STAN z czujnika (centrala steruje nia sama
      // automatyka), nie ma odpowiednika /api/servo-manual dla pompy — wiec
      // w odroznieniu od makiety HTML (gdzie pompa jest przelacznikiem
      // demo) tutaj to CZYSTY ODCZYT. Kolor jako stan (nie dekoracja) zyje
      // w ikonie: niebieski gdy pracuje, przygaszony gdy stoi.
      etykieta = "POMPA";
      wartosc = stan.pompa ? "ZALACZONA" : "WYLACZONA";
      kolorIkony = stan.pompa ? KOL_KROPLA : KOL_TEKST_DIM;
      break;
    case KAF_KLAPA:   etykieta = "KLAPA";   wartosc = String(stan.klapa) + " %";  kolorIkony = KOL_SUWAK; break;
    case KAF_SYBERKA: etykieta = "SYBERKA"; wartosc = String(stan.syberka) + " %"; kolorIkony = KOL_SUWAK; break;
    case KAF_DYM:
      etykieta = "DYM";
      wartosc = String(stan.dym);
      kolorIkony  = stan.dymAlarm ? KOL_ERR : KOL_WARN;
      kolorRamki  = stan.dymAlarm ? KOL_ERR : KOL_OGIEN;
      break;
  }
}

// Ikona (opcjonalna) dla danego kafelka — brak case = bez ikony (etykieta
// tekstowa wystarcza, np. WILGOTNOSC/CISNIENIE/POKOJ). Kolor ikony ustalany
// w tresciKafelka (kolorIkony) — stale przypisany do typu czujnika, nie do
// wartosci — zeby ikona niosla stan, a nie powielala kolor tekstu.
void rysujIkoneKafelka(int id, int16_t cx, int16_t cy, uint16_t kolor, TFT_eSPI &gfx) {
  switch (id) {
    case KAF_ZEWN:
      if (aktualnySezon == SEZON_LATO) rysujIkonaSlonce(gfx, cx, cy, 9, kolor);
      else                             rysujIkonaSnieg(gfx, cx, cy, 9, kolor);
      break;
    case KAF_BOJLER: case KAF_OGRZ: case KAF_OGRZ_SR: case KAF_PANEL:
      rysujIkonaPlomien(gfx, cx, cy, 8, kolor); break;
    case KAF_POMPA:
      rysujIkonaKropla(gfx, cx, cy, 8, kolor); break;
    case KAF_KLAPA: case KAF_SYBERKA:
      rysujIkonaSuwak(gfx, cx, cy, 8, kolor); break;
    case KAF_DYM:
      rysujIkonaAlarm(gfx, cx, cy, 8, kolor); break;
    default:
      break; // POKOJ, WILGOTNOSC, CISNIENIE — bez ikony, sama liczba wystarczy
  }
}

// Pojedynczy kafelek: etykieta + duza wartosc — DOKLADNIE ten sam uklad co w
// poprzedniej wersji rysujKafelek (sprawdzona geometria, kh=46 dopasowane do
// fontu 4), plus mala ikona w prawym-gornym rogu (jesli kafelek ja ma) i
// ramka w kolorze zaleznym od tego, czy kafelek jest sterowalny.
void rysujKafelek(int x, int y, int w, int h, int id) {
  String etykieta, wartosc;
  uint16_t kolorWartosci, kolorIkony, kolorRamki;
  bool sterowalny;
  tresciKafelka(id, etykieta, wartosc, kolorWartosci, kolorIkony, kolorRamki, sterowalny);

  // [KROK 3] Sprite tworzony/skalowany tylko gdy rozmiar faktycznie sie
  // zmienil wzgledem poprzedniego wywolania — tworzenie sprite'a (alokacja
  // RAM) jest kosztowne, wiec nie robimy tego przy kazdym kafelku.
  if (sprKafelekW != w || sprKafelekH != h) {
    if (sprKafelekW != 0) sprKafelek.deleteSprite();
    sprKafelek.setColorDepth(16);
    sprKafelek.createSprite(w, h);
    sprKafelekW = w;
    sprKafelekH = h;
  }

  // Wszystko ponizej rysuje sie DOKLADNIE tak samo jak wczesniej, tylko
  // (a) na sprKafelek zamiast na tft, (b) wspolrzedne wzgledem (0,0) sprite'a
  // zamiast bezwzglednych x,y na ekranie.
  sprKafelek.fillSprite(KOL_TLO); // tlo "za" zaokraglonymi rogami, patrz bg_color nizej

  sprKafelek.fillSmoothRoundRect(0, 0, w, h, 6, KOL_KARTA, KOL_TLO);
  sprKafelek.drawSmoothRoundRect(0, 0, 6, 4, w, h, kolorRamki, KOL_KARTA);

  rysujIkoneKafelka(id, w - 16, 15, kolorIkony, sprKafelek);

  sprKafelek.setTextDatum(TL_DATUM);
  sprKafelek.setTextColor(KOL_TEKST_DIM, KOL_KARTA);
  sprKafelek.setTextFont(1);
  sprKafelek.drawString(etykieta, 8, 6);
  sprKafelek.setTextColor(kolorWartosci, KOL_KARTA);
  sprKafelek.setTextFont(4);
  sprKafelek.drawString(wartosc, 8, 20);

  // Jeden blokowy transfer na ekran — to jest krok, ktory realnie eliminuje
  // migotanie/przeblyski przy przerysowaniu kafelka.
  sprKafelek.pushSprite(x, y);
}

// ----------------------------------------------------------------------------
// [LCD-KAFELKI] PANELE NAKLADKI (overlay) — otwierane dotknieciem kafelka
// sterowalnego. Karta na srodku ekranu, jedno "Zamknij" wraca do dashboardu
// (ten sam wzorzec "tap outside to dismiss" co w makiecie HTML) zamiast
// przelaczania na osobny ekran — szybszy powrot do podgladu calosci.
// ----------------------------------------------------------------------------

// Nakladka DYM — czulosc alarmu (progAlarmDym), +/- jak w Ustawieniach, tylko
// bez wchodzenia w cala liste. Bez zmian wzgledem poprzedniej wersji poza
// nazwami zmiennych (panelMinus/panelPlus/panelZamknij, wczesniej z prefiksem
// "Dym" — teraz wspolne dla wszystkich nakladek, patrz panelOtwartyId wyzej).
void rysujPanelDym() {
  const Przycisk& k = PANEL_KARTA;
  tft.fillRoundRect(k.x, k.y, k.w, k.h, 8, KOL_KARTA);
  tft.drawRoundRect(k.x, k.y, k.w, k.h, 8, KOL_OGIEN);

  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(KOL_TEKST, KOL_KARTA);
  tft.drawString("Czulosc alarmu dymu", k.x + k.w / 2, k.y + 10);

  int idx = znajdzIndeksPola("progAlarmDym");
  long wartosc = (idx >= 0) ? POLA[idx].wartosc : 0;
  tft.setTextFont(4);
  tft.setTextColor(KOL_OGIEN, KOL_KARTA);
  tft.drawString(String(wartosc), k.x + k.w / 2, k.y + 44);

  panelMinus = { (int16_t)(k.x + 16),        (int16_t)(k.y + 84), 50, 40 };
  panelPlus  = { (int16_t)(k.x + k.w - 66),  (int16_t)(k.y + 84), 50, 40 };
  tft.fillRoundRect(panelMinus.x, panelMinus.y, panelMinus.w, panelMinus.h, 6, KOL_RAMKA);
  tft.fillRoundRect(panelPlus.x,  panelPlus.y,  panelPlus.w,  panelPlus.h,  6, KOL_RAMKA);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(KOL_TEKST, KOL_RAMKA);
  tft.drawString("-", panelMinus.x + panelMinus.w / 2, panelMinus.y + panelMinus.h / 2);
  tft.drawString("+", panelPlus.x  + panelPlus.w  / 2, panelPlus.y  + panelPlus.h  / 2);

  panelZamknij = { (int16_t)(k.x + k.w / 2 - 40), (int16_t)(k.y + k.h - 34), 80, 24 };
  tft.fillRoundRect(panelZamknij.x, panelZamknij.y, panelZamknij.w, panelZamknij.h, 6, KOL_KARTA);
  tft.drawRoundRect(panelZamknij.x, panelZamknij.y, panelZamknij.w, panelZamknij.h, 6, KOL_RAMKA);
  tft.setTextFont(1);
  tft.setTextColor(KOL_TEKST_DIM, KOL_KARTA);
  tft.drawString("Zamknij", panelZamknij.x + panelZamknij.w / 2, panelZamknij.y + panelZamknij.h / 2);
}

// Nakladka KLAPA/SYBERKA — jeden poziomy suwak 0-100%, przeciaganie obslugiwane
// w obsluzDotyk() (ten sam wzorzec co suwaki na ekranie Serwo: wysylka
// wyslijServoManualny() dopiero PO puszczeniu palca, nie na kazda klatke
// przeciagania). serwoKlapaProcent/serwoSyberekProcent to te same zmienne co
// na ekranie Serwo — otwarcie nakladki inicjalizuje je z /api/status, wiec
// oba miejsca zawsze pokazuja te sama, aktualna wartosc.
void rysujPanelSuwak(int id) {
  const Przycisk& k = PANEL_SUWAK_KARTA;
  const char* etykieta = (id == KAF_KLAPA) ? "Klapa" : "Syberko";
  int procent = (id == KAF_KLAPA) ? serwoKlapaProcent : serwoSyberekProcent;

  tft.fillRoundRect(k.x, k.y, k.w, k.h, 8, KOL_KARTA);
  tft.drawRoundRect(k.x, k.y, k.w, k.h, 8, KOL_OGIEN);

  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(KOL_TEKST, KOL_KARTA);
  tft.drawString(String(etykieta) + ": " + String(procent) + " %", k.x + k.w / 2, k.y + 12);

  suwakPanelSuwak = { (int16_t)(k.x + 20), (int16_t)(k.y + 44), (int16_t)(k.w - 40), 30 };
  tft.fillRoundRect(suwakPanelSuwak.x, suwakPanelSuwak.y, suwakPanelSuwak.w, suwakPanelSuwak.h, 6, KOL_RAMKA);
  int wypelnienie = map(procent, 0, 100, 0, suwakPanelSuwak.w - 6);
  tft.fillRoundRect(suwakPanelSuwak.x + 3, suwakPanelSuwak.y + 3, wypelnienie, suwakPanelSuwak.h - 6, 4, KOL_OGIEN);

  panelZamknij = { (int16_t)(k.x + k.w / 2 - 40), (int16_t)(k.y + k.h - 34), 80, 24 };
  tft.fillRoundRect(panelZamknij.x, panelZamknij.y, panelZamknij.w, panelZamknij.h, 6, KOL_KARTA);
  tft.drawRoundRect(panelZamknij.x, panelZamknij.y, panelZamknij.w, panelZamknij.h, 6, KOL_RAMKA);
  tft.setTextFont(1);
  tft.setTextColor(KOL_TEKST_DIM, KOL_KARTA);
  tft.drawString("Zamknij", panelZamknij.x + panelZamknij.w / 2, panelZamknij.y + panelZamknij.h / 2);
}

// Dysponent nakladek — wywolywany z rysujAktualnyEkran() gdy panelOtwartyId != -1.
void rysujPanelOverlay(int id) {
  if (id == KAF_DYM) rysujPanelDym();
  else                rysujPanelSuwak(id);
}

void rysujPasekZakladek() {
  int y = 320 - PASEK_WYS;
  tft.fillRect(0, y, 480, PASEK_WYS, KOL_KARTA);
  tft.drawFastHLine(0, y, 480, KOL_RAMKA);
  struct { const char* nazwa; Przycisk* p; Ekran e; } zak[] = {
    {"Dashboard",  &zakladkaDashboard,  EKRAN_DASHBOARD},
    {"Ustawienia", &zakladkaUstawienia, EKRAN_USTAWIENIA},
    {"Serwo",      &zakladkaSerwo,      EKRAN_SERWO},
    {"Tryby",      &zakladkaTryby,      EKRAN_TRYBY},
  };
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  for (auto& z : zak) {
    bool aktywna = (aktualnyEkran == z.e);
    if (aktywna) tft.drawFastHLine(z.p->x + 10, y, z.p->w - 20, KOL_OGIEN);
    tft.setTextColor(aktywna ? KOL_OGIEN : KOL_TEKST_DIM, KOL_KARTA);
    tft.drawString(z.nazwa, z.p->x + z.p->w / 2, y + PASEK_WYS / 2);
  }
}

// [FIX migotanie #3] Ten sam wzorzec co w rysujDashboard() - pelne
// tft.fillRect(0,0,480,26) na kazdym wywolaniu z loop() (co 2s) powodowalo
// widoczne "pulsowanie" paska naglowka, najbardziej zauwazalne w stanie
// "WiFi OK, centrala nie odpowiada" - bo ten tekst jest STATYCZNY (nie
// zmienia sie co cykl), a mimo to byl czyszczony i przerysowywany co 2s
// identycznym tekstem. Poprawka: "Piec CO" (tytul, nigdy sie nie zmienia)
// rysowany tylko przy pelneCzyszczenie; obszar statusu po prawej czyszczony
// i przerysowywany TYLKO gdy tekst statusu faktycznie sie zmienil (stan
// polaczono/wifi_ok/brak_wifi lub odliczanie sekund w stanie brak_wifi).
void rysujNaglowek(bool pelneCzyszczenie = true) {
  if (pelneCzyszczenie) {
    tft.fillRect(0, 0, 480, 26, KOL_TLO);
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(KOL_OGIEN, KOL_TLO);
    tft.drawString("Piec CO", 8, 5);
    // [LCD-SEZON] Ikona aktualnego sezonu obok tytulu — szybki wizualny
    // sygnal, w ktorym widoku jestesmy, bez czytania tekstu. Rysowana
    // TYLKO przy pelnym czyszczeniu (jak tytul) — sezon zmienia sie rzadko
    // (godziny/dni), a przy kazdej jego zmianie i tak wymuszamy pelne
    // przerysowanie z loop() (patrz aktualizujSezon()).
    int16_t ix = 80;
    if (aktualnySezon == SEZON_LATO) rysujIkonaSlonce(tft, ix, 13, 9, KOL_WARN);
    else                             rysujIkonaSnieg(tft, ix, 13, 9, KOL_SNIEG);
  }

  String tekst;
  uint16_t kolor;
  if (stan.polaczono) {
    kolor = KOL_OK;
    tekst = "polaczono  " + stan.ip + "  " + String(stan.rssi) + "dBm";
  } else if (WiFi.status() == WL_CONNECTED) {
    // [ETAP 5] WiFi/AP centrali dziala, ale ostatni GET /api/status sie nie
    // udal (np. centrala akurat restartuje/robi OTA) — odroznione od braku
    // samego WiFi, bo to inna przyczyna i inna reakcja uzytkownika.
    kolor = KOL_ERR;
    tekst = "WiFi OK, centrala nie odpowiada";
  } else {
    // [ETAP 5] Brak WiFi -> odliczanie do kolejnej proby reconnect
    // (wifiReconnectNonBlocking() w loop()), zeby bylo widac ze urzadzenie
    // nie utknelo, tylko czeka zgodnie z rosnacym opoznieniem. Ten tekst
    // CELOWO zmienia sie co sekunde (odliczanie) - to nie jest blad.
    long pozostaloMs = (long)(wifiOstatniaProbaReconnectMs + wifiOpoznienieReconnectMs) - (long)millis();
    long pozostaloS = pozostaloMs > 0 ? (pozostaloMs / 1000) + 1 : 0;
    kolor = KOL_ERR;
    tekst = "brak WiFi, kolejna proba za " + String(pozostaloS) + "s";
  }

  static String poprzedniTekstStatus = "";
  if (pelneCzyszczenie || tekst != poprzedniTekstStatus) {
    tft.fillRect(170, 0, 310, 26, KOL_TLO); // czyscimy tylko prawa czesc paska, nie caly naglowek
    tft.setTextDatum(TR_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(kolor, KOL_TLO);
    tft.drawString(tekst, 472, 5);
    poprzedniTekstStatus = tekst;
  }
}

// [FIX migotanie] pelneCzyszczenie=true tylko przy przelaczeniu na zakladke
// Dashboard (usuwa pozostalosci po Ustawieniach/Serwie) - przy cyklicznym
// odswiezaniu danych (co STATUS_INTERWAL_MS) NIE czyscimy calego tla, bo
// kazdy kafelek/znacznik i tak maluje swoje pelne tlo przez fillRoundRect -
// zbedny fillRect calego obszaru powodowal widoczny "przeblysk" tla co 2s.
//
// [FIX migotanie #2] Nawet bez pelnego czyszczenia tla, kazdy kafelek robi
// dwuetapowe nadpisanie (fillRoundRect = tlo, potem drawString = tekst) -
// krotkie okno miedzy tymi krokami samo w sobie miga, NIEZALEZNIE od tego,
// czy wartosc sie zmienila. Research (Bodmer/TFT_eSPI issue #346, forum
// M5Stack): standardowa metoda to cache poprzedniej wartosci i rysowanie
// kafelka/znacznika TYLKO gdy realnie sie zmienil - wiekszosc temperatur nie
// zmienia sie co 2s, wiec to eliminuje wiekszosc zbednych przerysowan.
//
// [LCD-KAFELKI] Siatka 4 kolumny x 3 wiersze (12 slotow), kafelki wypelniane
// PO KOLEI wg kolejnosci id (KAF_ZEWN..KAF_DYM) pomijajac te niewidoczne w
// danym sezonie (kafelekWidoczny()) — czyli uklad "sciska sie" bez dziur, tak
// jak siatka w makiecie HTML. Pod spodem (bo to juz nie kafelki, tylko krotkie
// wskazniki bool bez wlasnej liczby) zostaja dwa paski alarmow — ten sam,
// juz sprawdzony wzorzec "kolor jako stan" co poprzednio.
void rysujDashboard(bool pelneCzyszczenie = true) {
  if (pelneCzyszczenie) {
    tft.fillRect(0, 26, 480, 320 - 26 - PASEK_WYS, KOL_TLO);
  }
  // [FIX siatka] kw byl 116 -> 4 kolumny + 3 odstepy + lewy margines dawaly
  // prawa krawedz na x=482, czyli 2px POZA ekranem (480px szerokosci). Efekt:
  // fillRoundRect/drawRoundRect dla calej 4. kolumny (OGRZ SR w wierszu 0,
  // POMPA w wierszu 1) nie rysowaly sie w ogole (przycinane poza granica przez
  // sterownik), zostawala tylko goła wartosc z drawString — dokladnie to bylo
  // widac na zdjeciu z urzadzenia. kw=114 daje prawa krawedz na x=474,
  // symetryczny margines 6px po obu stronach.
  const int x0 = 6, y0 = 32, kw = 114, kh = 46, odstepX = 4, odstepY = 4;

  static float poprzWart[LICZBA_KAFELKOW];
  static bool poprzBoolA[LICZBA_KAFELKOW]; // pompa/dymAlarm — dowolna druga skladowa stanu kafelka
  static bool zainicjalizowano = false;
  if (!zainicjalizowano) {
    for (int i = 0; i < LICZBA_KAFELKOW; i++) { poprzWart[i] = NAN; poprzBoolA[i] = false; }
    zainicjalizowano = true;
  }

  int slot = 0;
  for (int id = 0; id < LICZBA_KAFELKOW; id++) {
    if (!kafelekWidoczny(id)) {
      kafelekRect[id] = { 0, 0, 0, 0 };
      poprzWart[id] = NAN; // reset cache — gdy kafelek pozniej wroci (zmiana sezonu), ma sie zawsze przerysowac
      continue;
    }
    int kol = slot % 4, wiersz = slot / 4;
    int x = x0 + kol * (kw + odstepX);
    int y = y0 + wiersz * (kh + odstepY);
    kafelekRect[id] = { (int16_t)x, (int16_t)y, (int16_t)kw, (int16_t)kh };
    slot++;

    // Wartosc porownawcza do cache — float dla odczytow liczbowych, a dla
    // POMPA/DYM (gdzie liczy sie tez drugi bit stanu: wl/wyl, alarm) dodatkowo
    // poprzBoolA[]. Upraszczamy: kazda zmiana ktoregokolwiek pola stanu pieca
    // wymusza przerysowanie WSZYSTKICH widocznych kafelkow (tanie, bo i tak
    // caly stan przychodzi w jednym GET /api/status co STATUS_INTERWAL_MS) —
    // ale nadal TYLKO gdy cos sie realnie zmienilo od ostatniego rysowania,
    // wiec flicker-fix z [FIX migotanie #2] zostaje zachowany.
    float wartPorownanie;
    bool boolPorownanie;
    switch (id) {
      case KAF_ZEWN: wartPorownanie = stan.tZewn; boolPorownanie = false; break;
      case KAF_BOJLER: wartPorownanie = stan.tBojler; boolPorownanie = false; break;
      case KAF_OGRZ: wartPorownanie = stan.tOgrz; boolPorownanie = false; break;
      case KAF_OGRZ_SR: wartPorownanie = stan.tOgrzSr; boolPorownanie = false; break;
      case KAF_PANEL: wartPorownanie = stan.tPanel; boolPorownanie = false; break;
      case KAF_POKOJ: wartPorownanie = stan.tPokoj; boolPorownanie = false; break;
      case KAF_WILGOTNOSC: wartPorownanie = stan.wilgotnosc; boolPorownanie = false; break;
      case KAF_CISNIENIE: wartPorownanie = stan.cisnienie; boolPorownanie = false; break;
      case KAF_POMPA: wartPorownanie = 0; boolPorownanie = stan.pompa; break;
      case KAF_KLAPA: wartPorownanie = stan.klapa; boolPorownanie = false; break;
      case KAF_SYBERKA: wartPorownanie = stan.syberka; boolPorownanie = false; break;
      case KAF_DYM: wartPorownanie = stan.dym; boolPorownanie = stan.dymAlarm; break;
      default: wartPorownanie = 0; boolPorownanie = false; break;
    }
    if (pelneCzyszczenie || wartPorownanie != poprzWart[id] || boolPorownanie != poprzBoolA[id]) {
      rysujKafelek(x, y, kw, kh, id);
      poprzWart[id] = wartPorownanie;
      poprzBoolA[id] = boolPorownanie;
    }
  }

  // Dwa kompaktowe paski alarmow pod siatka — czysty odczyt, bez akcji,
  // "kolor jako stan" (zielony/przygaszony = OK, czerwony = alarm).
  int yAlarm = y0 + 3 * (kh + odstepY) + 6;
  auto znacznik = [&](int x, const char* nazwa, bool alarm) {
    uint16_t kol = alarm ? KOL_ERR : KOL_TEKST_DIM;
    tft.fillRoundRect(x, yAlarm, 110, 26, 6, KOL_KARTA);
    tft.drawRoundRect(x, yAlarm, 110, 26, 6, kol);
    tft.setTextColor(kol, KOL_KARTA);
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(1);
    tft.drawString(nazwa, x + 8, yAlarm + 9);
  };
  static bool poprzAlarmOgrz = false, poprzAlarmPanel = false, pierwszyRaz = true;
  if (aktualnySezon == SEZON_ZIMA) {
    if (pelneCzyszczenie || pierwszyRaz || stan.alarmOgrzewanie != poprzAlarmOgrz) {
      znacznik(x0, "ALARM OGRZ", stan.alarmOgrzewanie);
      poprzAlarmOgrz = stan.alarmOgrzewanie;
    }
  } else {
    poprzAlarmOgrz = false; // reset cache na powrot do zimy — alarm ogrzewania bez sensu latem, piec nie pracuje
  }
  if (pelneCzyszczenie || pierwszyRaz || stan.alarmPanel != poprzAlarmPanel) {
    znacznik(x0 + 120, "ALARM PANEL", stan.alarmPanel);
    poprzAlarmPanel = stan.alarmPanel;
  }
  pierwszyRaz = false;
}

// ----------------------------------------------------------------------------
// RYSOWANIE — USTAWIENIA (lista z +/- na paczke POZYCJI_NA_STRONE naraz)
// ----------------------------------------------------------------------------
Przycisk przyciskMinus[POZYCJI_NA_STRONE];
Przycisk przyciskPlus[POZYCJI_NA_STRONE];
Przycisk przyciskStronaWstecz{ 8,   320 - PASEK_WYS - 34, 60, 28 };
Przycisk przyciskStronaDalej { 480-68, 320 - PASEK_WYS - 34, 60, 28 };

void rysujUstawienia() {
  tft.fillRect(0, 26, 480, 320 - 26 - PASEK_WYS, KOL_TLO);
  if (!ustawieniaWczytane) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(KOL_TEKST_DIM, KOL_TLO);
    tft.setTextFont(2);
    tft.drawString("Wczytywanie ustawien z centrali...", 240, 150);
    return;
  }
  int start = ustawieniaStrona * POZYCJI_NA_STRONE;
  int y = 34;
  for (int i = 0; i < POZYCJI_NA_STRONE; i++) {
    int idx = start + i;
    Przycisk pMin{ 300, y + 4, 40, 34 };
    Przycisk pPlus{ 350, y + 4, 40, 34 };
    przyciskMinus[i] = pMin;
    przyciskPlus[i]  = pPlus;
    if (idx >= LICZBA_POL) { y += 46; continue; }
    PoleUstawien& p = POLA[idx];
    tft.fillRoundRect(6, y, 468, 40, 6, KOL_KARTA);
    tft.drawRoundRect(6, y, 468, 40, 6, KOL_RAMKA);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(KOL_TEKST, KOL_KARTA);
    tft.drawString(p.etykieta, 14, y + 20);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(KOL_OGIEN, KOL_KARTA);
    tft.setTextFont(4);
    tft.drawString(String(p.wartosc), 290, y + 20);

    tft.fillRoundRect(pMin.x, pMin.y, pMin.w, pMin.h, 6, KOL_RAMKA);
    tft.fillRoundRect(pPlus.x, pPlus.y, pPlus.w, pPlus.h, 6, KOL_RAMKA);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(KOL_TEKST, KOL_RAMKA);
    tft.drawString("-", pMin.x + pMin.w/2, pMin.y + pMin.h/2);
    tft.drawString("+", pPlus.x + pPlus.w/2, pPlus.y + pPlus.h/2);
    y += 46;
  }
  // pasek stronicowania
  tft.fillRoundRect(przyciskStronaWstecz.x, przyciskStronaWstecz.y, przyciskStronaWstecz.w, przyciskStronaWstecz.h, 6, KOL_KARTA);
  tft.fillRoundRect(przyciskStronaDalej.x, przyciskStronaDalej.y, przyciskStronaDalej.w, przyciskStronaDalej.h, 6, KOL_KARTA);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(KOL_TEKST, KOL_KARTA);
  tft.drawString("< Wstecz", przyciskStronaWstecz.x + 30, przyciskStronaWstecz.y + 14);
  tft.drawString("Dalej >", przyciskStronaDalej.x + 30, przyciskStronaDalej.y + 14);
}

// ----------------------------------------------------------------------------
// RYSOWANIE — SERWO (recznie, dwa poziome suwaki 0-100%)
// ----------------------------------------------------------------------------
struct Suwak { int16_t x, y, w, h; };
Suwak suwakKlapa  { 40, 90,  400, 30 };
Suwak suwakSyberek{ 40, 190, 400, 30 };

void rysujSuwak(const Suwak& s, const char* etykieta, int procent) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(KOL_TEKST, KOL_TLO);
  tft.drawString(String(etykieta) + ": " + String(procent) + " %", s.x, s.y - 20);
  tft.fillRoundRect(s.x, s.y, s.w, s.h, 6, KOL_KARTA);
  tft.drawRoundRect(s.x, s.y, s.w, s.h, 6, KOL_RAMKA);
  int wypelnienie = map(procent, 0, 100, 0, s.w - 6);
  tft.fillRoundRect(s.x + 3, s.y + 3, wypelnienie, s.h - 6, 4, KOL_OGIEN);
}

void rysujSerwo() {
  tft.fillRect(0, 26, 480, 320 - 26 - PASEK_WYS, KOL_TLO);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(KOL_WARN, KOL_TLO);
  tft.drawString("Dziala tylko gdy tryb serwa = Reczny (jak w panelu WWW)", 240, 40);
  rysujSuwak(suwakKlapa,   "Klapa",   serwoKlapaProcent);
  rysujSuwak(suwakSyberek, "Syberek", serwoSyberekProcent);
}

// ----------------------------------------------------------------------------
// [LCD-TRYBY] RYSOWANIE — TRYBY (segmentowane przelaczniki Wybor/TrybSerwa,
// dokladnie te same pola i ten sam generyczny /api/ustawienia-set co reszta
// tego pliku — patrz komentarz przy deklaracji trybWyboru/trybSerwaWartosc).
// ----------------------------------------------------------------------------
struct SegmentTrybu { const char* etykieta; long wartosc; };
const SegmentTrybu SEGMENTY_WYBOR[] = { {"Trociniak", 1}, {"Kopciuch", 2}, {"Automatyczny", 3} };
const SegmentTrybu SEGMENTY_SERWA[] = { {"Auto", 1}, {"Reczny", 2}, {"Bezpieczna", 3} };
const int LICZBA_SEGMENTOW = 3; // obie grupy maja po 3 opcje

// Prostokaty segmentow — wypelniane w rysujTryby(), odczytywane w obsluzDotyku()
Przycisk segmentyWybor[LICZBA_SEGMENTOW];
Przycisk segmentySerwa[LICZBA_SEGMENTOW];

void rysujGrupeSegmentow(int y, const char* tytul, const char* opis, const SegmentTrybu* segmenty,
                          long aktualnaWartosc, Przycisk* wyjscioweRects) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(KOL_TEKST, KOL_TLO);
  tft.drawString(tytul, 14, y);
  tft.setTextColor(KOL_TEKST_DIM, KOL_TLO);
  tft.setTextFont(1);
  tft.drawString(opis, 14, y + 18);

  int segY = y + 34, segH = 40, segX = 14, segW = (480 - 28) / LICZBA_SEGMENTOW;
  for (int i = 0; i < LICZBA_SEGMENTOW; i++) {
    int x = segX + i * segW;
    Przycisk r{ (int16_t)x, (int16_t)segY, (int16_t)(segW - 4), (int16_t)segH };
    wyjscioweRects[i] = r;
    bool aktywny = (segmenty[i].wartosc == aktualnaWartosc);
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, aktywny ? KOL_OGIEN : KOL_KARTA);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, aktywny ? KOL_OGIEN : KOL_RAMKA);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(aktywny ? TFT_BLACK : KOL_TEKST, aktywny ? KOL_OGIEN : KOL_KARTA);
    tft.drawString(segmenty[i].etykieta, r.x + r.w / 2, r.y + r.h / 2);
  }
}

void rysujTryby() {
  tft.fillRect(0, 26, 480, 320 - 26 - PASEK_WYS, KOL_TLO);
  if (!ustawieniaWczytane) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(KOL_TEKST_DIM, KOL_TLO);
    tft.setTextFont(2);
    tft.drawString("Wczytywanie trybow z centrali...", 240, 150);
    return;
  }
  rysujGrupeSegmentow(36,  "Tryb pompy (Wybor)", "Trociniak | Kopciuch | Automatyczny",
                      SEGMENTY_WYBOR, trybWyboru, segmentyWybor);
  rysujGrupeSegmentow(140, "Tryb serwa", "Auto | Reczny | Pozycja bezpieczna",
                      SEGMENTY_SERWA, trybSerwaWartosc, segmentySerwa);
}

// Wysyla zmiane trybu tym samym generycznym /api/ustawienia-set co POLA[]
// (wyslijUstawienie()) — patrz siec_nieblokujaca_LCD.h.
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Woluje ja zarowno stary,
// nie-LVGL EKRAN_TRYBY (bezposrednio) jak i 8 mostow LVGL (patrz komentarze
// tam) - WSZYSCY oprocz pompaWyborWyslij()/serwoTrybWyslij() chca po
// zakonczeniu POST-a dokladnie tego samego: pobierzUstawienia() (u starego
// EKRAN_TRYBY to "przywroc ostatnia znana wartosc po nieudanym poleceniu",
// u reszty LVGL-mostow to "pokaz PRAWDZIWA, zwalidowana przez centrale
// wartosc") - wiec ustawiamy tu USTAWIENIA jako domyslna; te dwa wyjatki
// same nadpisuja flage na STATUS zaraz PO tym wywolaniu (patrz ich definicje
// wyzej - to bezpieczne, bo w tym miejscu POST dopiero co wystartowal,
// wiec obslugaKomendaFetch() jeszcze na pewno nie zdazyl przeczytac flagi).
void wyslijTrybPracy(const char* pole, long wartosc) {
  if (komendaFetch.wTrakcie()) {
    Serial.println("[KOMENDA] wyslijTrybPracy: poprzednia komenda jeszcze w toku, pomijam");
    return;
  }
  pendingPoKomendzie = PoKomendzie::USTAWIENIA;
  komendaFetch.startPostFormUrlEncoded("/api/ustawienia-set", String("pole=") + pole + "&wartosc=" + String(wartosc));
  // Dawny czerwony pasek na TFT + delay(1200) na porazke ZNIKA - to byla
  // druga (obok samego httpPost) blokujaca operacja w tym pliku. Porazka
  // trafia teraz tylko do logu (Serial, patrz obslugaKomendaFetch()) - ten
  // sam, juz zaakceptowany wzorzec co mosty LVGL. Ekran i tak zaraz pokaze
  // prawdziwa wartosc, bo pobierzUstawienia() leci automatycznie w obu
  // przypadkach (sukces i porazka) - patrz obslugaKomendaFetch().
}

void rysujAktualnyEkran() {
  rysujNaglowek();
  switch (aktualnyEkran) {
    case EKRAN_DASHBOARD:  rysujDashboard(); if (panelOtwartyId != -1) rysujPanelOverlay(panelOtwartyId); break;
    case EKRAN_USTAWIENIA: rysujUstawienia(); break;
    case EKRAN_SERWO:      rysujSerwo();      break;
    case EKRAN_TRYBY:      rysujTryby();      break;
  }
  rysujPasekZakladek();
}

// ----------------------------------------------------------------------------
// WYSYLANIE ZMIAN
// ----------------------------------------------------------------------------
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Uzywana wylacznie przez stary,
// nie-LVGL EKRAN_USTAWIENIA (+/- na polu POLA[]) - patrz komentarz przy
// wyslijTrybPracy() wyzej, dokladnie ten sam wzorzec.
void wyslijUstawienie(int idxPola) {
  if (komendaFetch.wTrakcie()) {
    Serial.println("[KOMENDA] wyslijUstawienie: poprzednia komenda jeszcze w toku, pomijam");
    return;
  }
  PoleUstawien& p = POLA[idxPola];
  pendingPoKomendzie = PoKomendzie::USTAWIENIA;
  komendaFetch.startPostFormUrlEncoded("/api/ustawienia-set", String("pole=") + p.klucz + "&wartosc=" + String(p.wartosc));
  // Jak w wyslijTrybPracy() - bez blokujacego paska+delay(1200); porazka
  // trafia do logu, a pobierzUstawienia() (automatycznie z obslugaKomendaFetch())
  // i tak przywroci na ekranie prawdziwa wartosc z centrali.
}

// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Wolana zarowno przez stary,
// nie-LVGL EKRAN_SERWO (suwak klapy/syberka, bez zadnej flagi - BRAK,
// zachowuje dawne "cicho ignoruj") jak i serwoPozycjaWyslij() z mostow LVGL
// (ktora ustawia STATUS PRZED wywolaniem tej funkcji) - dlatego, w
// odroznieniu od wyslijTrybPracy()/wyslijUstawienie() wyzej, ta funkcja SAMA
// NIC nie ustawia w pendingPoKomendzie (zostawia to, co ustawil wywolujacy,
// domyslnie BRAK).
void wyslijServoManualny(const char* os, int procent) {
  if (komendaFetch.wTrakcie()) {
    Serial.println("[KOMENDA] wyslijServoManualny: poprzednia komenda jeszcze w toku, pomijam");
    return;
  }
  komendaFetch.startPostFormUrlEncoded("/api/servo-manual", String("os=") + os + "&procent=" + String(procent));
  // Blad tutaj czesto oznacza po prostu "tryb serwa != Reczny" — nie
  // przerywamy uzytkownikowi obslugi suwaka, tylko cicho ignorujemy
  // (dokladnie tak jak panel WWW, ktory suwak i tak pokazuje tylko w trybie
  // recznym; tutaj panel jest zawsze widoczny, wiec blad jest oczekiwany) -
  // obslugaKomendaFetch() i tak tylko zaloguje ta porazke po Serialu.
}

// [SIEC-NIEBLOKUJACA, ta sesja] Wywolywana z loop() przy kazdej iteracji -
// patrz obslugaStatusFetch() wyzej. Tu POST i "nastepujacy po nim GET"
// (dawniej jedno synchroniczne wywolanie) faktycznie spinaja sie z powrotem
// w jedna calosc - patrz obszerny komentarz przy pendingPoKomendzie na
// poczatku pliku co do PRZYCZYNY, dla ktorej to nie moze juz byc proste.
void obslugaKomendaFetch() {
  komendaFetch.poll();
  if (!komendaFetch.gotowyWynik()) return;
  bool centralaOk = false;
  if (komendaFetch.bylSukces()) {
    // centrala odpowiada {"ok":true,...} albo {"ok":false,"error":"..."} -
    // dokladnie ten sam kontrakt, ktory dawny httpPost() sprawdzal.
    String odp = NonBlockingHttp::wyodrebnijCialo(komendaFetch.pobierzOdpowiedz());
    JsonDocument doc;
    if (deserializeJson(doc, odp) == DeserializationError::Ok) {
      centralaOk = doc["ok"].as<bool>();
      if (!centralaOk) Serial.printf("[KOMENDA] centrala odrzucila: %s\n", doc["error"].as<String>().c_str());
    }
  } else {
    Serial.printf("[KOMENDA] POST nieudany: %s\n", komendaFetch.ostatniBlad().c_str());
    // [v1.2.0 FIX-LOG-SPAM-KOMENDA] Ten sam blad co naprawiony wczesniej w
    // obslugaStatusFetch()/obslugaUstawieniaFetch() (patrz FIX-LOG-SPAM przy
    // skwitujBlad() w siec_nieblokujaca_LCD.h) - tutaj zostal przeoczony.
    // Bez skwitujBlad() komendaFetch zostawal w stanie ERROR_ az do NASTEPNEGO
    // dotkniecia przycisku, wiec gotowyWynik() byl prawda na kazdym obrocie
    // loop() i powyzsza linia szla na Serial przy kazdym z nich, czyli wiele
    // razy na sekunde - po JEDNYM nieudanym POST, np. po dotknieciu przycisku
    // w chwili zaniku WiFi.
    komendaFetch.skwitujBlad();
  }

  PoKomendzie co = pendingPoKomendzie;
  pendingPoKomendzie = PoKomendzie::BRAK;
  if (co == PoKomendzie::USTAWIENIA) {
    // Zawsze, niezaleznie od centralaOk - sukces potwierdza nowa wartosc,
    // porazka przywraca ostatnia znana (oba dawne zachowania na raz, patrz
    // komentarze w wyslijTrybPracy()/wyslijUstawienie() wyzej).
    pobierzUstawienia();
  } else if (co == PoKomendzie::STATUS && centralaOk) {
    // STATUS: tylko po sukcesie - przy porazce CELOWO nic (jak dawniej w
    // pompaManualWyslij() itp.), kolejny okresowy pobierzStatus() i tak
    // wyrówna w ciagu max STATUS_INTERWAL_MS.
    pobierzStatus();
  }
}

// [SIEC-NIEBLOKUJACA, ta sesja] JEDYNE miejsce w tym pliku, gdzie SWIADOMIE
// czekamy na siec - wywolywana WYLACZNIE raz, z setup() (w obu galeziach),
// zaraz po polaczWifi()+pierwszym pobierzStatus()/pobierzUstawienia().
// Na starcie nie ma jeszcze zadnego dotyku do zamrozenia, wiec krotkie,
// OGRANICZONE oczekiwanie tutaj jest bezpieczne - w przeciwienstwie do
// dawnego httpGet()/httpPost(), ktore blokowaly tak samo w KAZDEJ iteracji
// loop() (czyli tez w trakcie normalnej pracy, z dotykiem czekajacym).
// Bez tego pierwszy rysujAktualnyEkran()/lvgl_dashboard_poc_aktualizuj()
// dostalby same zera/domyslne wartosci zamiast prawdziwych danych z centrali
// (do v1.3.0 takze sprawdzIWykonajOtaPanelu(); od v1.4.0 oferte aktualizacji
// sprawdza otaKrok() w loop()). Pompuje poll() tak samo
// jak zrobilaby to petla loop(), tylko w petli while zamiast czekac na
// kolejna iteracje - do PIERWSZA_PROBKA_TIMEOUT_MS, zeby calkiem
// nieosiagalna centrala (np. pierwsze uruchomienie bez pary) nie zawiesila
// setup() na dluzej niz ten jeden, jawny limit.
static const uint32_t PIERWSZA_PROBKA_TIMEOUT_MS = 2500; // troche wiecej niz SiecCfg::CALKOWITY_TIMEOUT_MS (1500) w siec_nieblokujaca_LCD.h - status i ustawienia leca rownolegle, ale dajemy zapas
void poczekajNaPierwszeDane() {
  unsigned long start = millis();
  while ((statusFetch.wTrakcie() || ustawieniaFetch.wTrakcie()) &&
         millis() - start < PIERWSZA_PROBKA_TIMEOUT_MS) {
    obslugaStatusFetch();
    obslugaUstawieniaFetch();
    delay(5);
  }
}

// ----------------------------------------------------------------------------
// OBSLUGA DOTYKU
// ----------------------------------------------------------------------------
void kalibrujDotykJesliTrzeba() {
  kalibracja.begin("dotyk", false);
  bool zapisana = kalibracja.isKey("rawXA");
  if (zapisana) {
    kalDotyku.rawXA = kalibracja.getUShort("rawXA", 0);
    kalDotyku.rawYA = kalibracja.getUShort("rawYA", 0);
    kalDotyku.rawXB = kalibracja.getUShort("rawXB", 0);
    kalDotyku.rawYB = kalibracja.getUShort("rawYB", 0);
  } else {
    tft.fillScreen(KOL_TLO);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(KOL_TEKST, KOL_TLO);
    tft.setTextFont(2);
    tft.drawString("Kalibracja dotyku", 240, 160);
    tft.drawString("Dotknij i przytrzymaj znacznik", 240, 185);
    Serial.println("[kalibracja] start - punkt A (gorny-lewy)");

    bool udana = false;
    while (!udana) {
      // --- znacznik A: gorny-lewy ---
      tft.drawCircle(KAL_A_X, KAL_A_Y, 8, TFT_MAGENTA);
      uint16_t rx, ry, sx, sy;
      bool zlapano = false;
      while (!zlapano) {
        while (!touchOdczytajSurowy(rx, ry)) { delay(10); }
        zlapano = touchPobierzStabilnaPozycje(sx, sy);
        if (!zlapano) Serial.println("[kalibracja] falszywy alarm (dotyk znikl w trakcie) - probuj ponownie");
      }
      kalDotyku.rawXA = sx;
      kalDotyku.rawYA = sy;
      while (touchOdczytajSurowy(rx, ry)) { delay(10); }  // czekaj na puszczenie
      tft.drawCircle(KAL_A_X, KAL_A_Y, 8, KOL_TLO);
      Serial.printf("[kalibracja] punkt A zapisany: x=%u y=%u\n", kalDotyku.rawXA, kalDotyku.rawYA);
      Serial.println("[kalibracja] punkt B (dolny-prawy)");

      delay(300);

      // --- znacznik B: dolny-prawy ---
      tft.drawCircle(KAL_B_X, KAL_B_Y, 8, TFT_MAGENTA);
      zlapano = false;
      while (!zlapano) {
        while (!touchOdczytajSurowy(rx, ry)) { delay(10); }
        zlapano = touchPobierzStabilnaPozycje(sx, sy);
        if (!zlapano) Serial.println("[kalibracja] falszywy alarm (dotyk znikl w trakcie) - probuj ponownie");
      }
      kalDotyku.rawXB = sx;
      kalDotyku.rawYB = sy;
      while (touchOdczytajSurowy(rx, ry)) { delay(10); }
      tft.drawCircle(KAL_B_X, KAL_B_Y, 8, KOL_TLO);
      Serial.printf("[kalibracja] punkt B zapisany: x=%u y=%u\n", kalDotyku.rawXB, kalDotyku.rawYB);

      // --- WALIDACJA: oba punkty musza sie realnie roznic, inaczej map() ---
      // --- dzieli przez zero i caly dotyk pozniej jest zepsuty           ---
      const int MIN_ROZNICA = 300;   // minimalna sensowna odleglosc raw miedzy punktami
      int roznicaX = abs((int)kalDotyku.rawXB - (int)kalDotyku.rawXA);
      int roznicaY = abs((int)kalDotyku.rawYB - (int)kalDotyku.rawYA);
      if (roznicaX < MIN_ROZNICA || roznicaY < MIN_ROZNICA) {
        Serial.printf("[kalibracja] BLAD: punkty za blisko siebie (dX=%d dY=%d, min=%d) - powtarzam\n",
                      roznicaX, roznicaY, MIN_ROZNICA);
        tft.fillScreen(KOL_TLO);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(KOL_ERR, KOL_TLO);
        tft.drawString("Kalibracja nieudana - powtarzam", 240, 160);
        tft.setTextColor(KOL_TEKST, KOL_TLO);
        tft.drawString("Dotknij mocniej i przytrzymaj chwile", 240, 185);
        delay(1200);
        tft.fillScreen(KOL_TLO);
        tft.drawString("Kalibracja dotyku", 240, 160);
        tft.drawString("Dotknij i przytrzymaj znacznik", 240, 185);
        udana = false;
      } else {
        udana = true;
      }
    }

    kalibracja.putUShort("rawXA", kalDotyku.rawXA);
    kalibracja.putUShort("rawYA", kalDotyku.rawYA);
    kalibracja.putUShort("rawXB", kalDotyku.rawXB);
    kalibracja.putUShort("rawYB", kalDotyku.rawYB);
  }
  kalibracja.end();
}

// [ETAP 3b] Forward declaration — definicja nizej (sekcja ZASILANIE), ale
// wywolywana juz tutaj w obsluzDotyk(); to zwykly main.cpp (nie .ino), wiec
// PlatformIO NIE generuje automatycznie prototypow jak Arduino IDE.
void zarejestrujAktywnosc();

void obsluzDotyk() {
  int16_t px, py;
  static bool bylWcisniety = false;
  bool wcisniety = dotykOdczytaj(px, py);

  if (wcisniety) zarejestrujAktywnosc(); // [ETAP 3b] kazdy dotyk (rowniez trzymanie/drag) resetuje timer

  if (wcisniety && !bylWcisniety) {
    // --- pasek zakladek (dziala na kazdym ekranie) — patrz animujWcisniecie() ---
    if (zakladkaDashboard.zawiera(px, py)) {
      animujWcisniecie(zakladkaDashboard.x, zakladkaDashboard.y, zakladkaDashboard.w, zakladkaDashboard.h);
      aktualnyEkran = EKRAN_DASHBOARD; rysujAktualnyEkran();
    }
    else if (zakladkaUstawienia.zawiera(px, py)) {
      animujWcisniecie(zakladkaUstawienia.x, zakladkaUstawienia.y, zakladkaUstawienia.w, zakladkaUstawienia.h);
      panelOtwartyId = -1; // [LCD-KAFELKI] nie zostawiaj panelu "otwartego" pod innym ekranem
      aktualnyEkran = EKRAN_USTAWIENIA; rysujAktualnyEkran();
    }
    else if (zakladkaSerwo.zawiera(px, py)) {
      animujWcisniecie(zakladkaSerwo.x, zakladkaSerwo.y, zakladkaSerwo.w, zakladkaSerwo.h);
      panelOtwartyId = -1;
      aktualnyEkran = EKRAN_SERWO;
      // przy wejsciu na ekran, suwaki startuja od aktualnej pozycji z /api/status
      serwoKlapaProcent = stan.klapa; serwoSyberekProcent = stan.syberka;
      rysujAktualnyEkran();
    }
    else if (zakladkaTryby.zawiera(px, py)) {
      animujWcisniecie(zakladkaTryby.x, zakladkaTryby.y, zakladkaTryby.w, zakladkaTryby.h);
      panelOtwartyId = -1;
      aktualnyEkran = EKRAN_TRYBY; rysujAktualnyEkran();
    }
    else if (aktualnyEkran == EKRAN_USTAWIENIA) {
      int start = ustawieniaStrona * POZYCJI_NA_STRONE;
      for (int i = 0; i < POZYCJI_NA_STRONE; i++) {
        int idx = start + i;
        if (idx >= LICZBA_POL) continue;
        if (przyciskMinus[i].zawiera(px, py)) {
          animujWcisniecie(przyciskMinus[i].x, przyciskMinus[i].y, przyciskMinus[i].w, przyciskMinus[i].h);
          POLA[idx].wartosc -= POLA[idx].krok;
          if (POLA[idx].wartosc < POLA[idx].minZakres) POLA[idx].wartosc = POLA[idx].minZakres;
          wyslijUstawienie(idx);
          rysujAktualnyEkran();
        } else if (przyciskPlus[i].zawiera(px, py)) {
          animujWcisniecie(przyciskPlus[i].x, przyciskPlus[i].y, przyciskPlus[i].w, przyciskPlus[i].h);
          POLA[idx].wartosc += POLA[idx].krok;
          if (POLA[idx].wartosc > POLA[idx].maxZakres) POLA[idx].wartosc = POLA[idx].maxZakres;
          wyslijUstawienie(idx);
          rysujAktualnyEkran();
        }
      }
      if (przyciskStronaWstecz.zawiera(px, py) && ustawieniaStrona > 0) {
        animujWcisniecie(przyciskStronaWstecz.x, przyciskStronaWstecz.y, przyciskStronaWstecz.w, przyciskStronaWstecz.h);
        ustawieniaStrona--; rysujAktualnyEkran();
      } else if (przyciskStronaDalej.zawiera(px, py) &&
                 (ustawieniaStrona + 1) * POZYCJI_NA_STRONE < LICZBA_POL) {
        animujWcisniecie(przyciskStronaDalej.x, przyciskStronaDalej.y, przyciskStronaDalej.w, przyciskStronaDalej.h);
        ustawieniaStrona++; rysujAktualnyEkran();
      }
    }
    else if (aktualnyEkran == EKRAN_TRYBY) {
      // [LCD-TRYBY] Klik segmentu -> animacja -> POST -> lokalny stan od razu
      // (optymistycznie, jak reszta pol) -> przerysowanie calego ekranu.
      for (int i = 0; i < LICZBA_SEGMENTOW; i++) {
        if (segmentyWybor[i].zawiera(px, py)) {
          animujWcisniecie(segmentyWybor[i].x, segmentyWybor[i].y, segmentyWybor[i].w, segmentyWybor[i].h);
          trybWyboru = SEGMENTY_WYBOR[i].wartosc;
          wyslijTrybPracy("wybor", trybWyboru);
          rysujAktualnyEkran();
        } else if (segmentySerwa[i].zawiera(px, py)) {
          animujWcisniecie(segmentySerwa[i].x, segmentySerwa[i].y, segmentySerwa[i].w, segmentySerwa[i].h);
          trybSerwaWartosc = SEGMENTY_SERWA[i].wartosc;
          wyslijTrybPracy("trybSerwa", trybSerwaWartosc);
          rysujAktualnyEkran();
        }
      }
    }
    else if (aktualnyEkran == EKRAN_DASHBOARD && panelOtwartyId == KAF_DYM) {
      // [LCD-KAFELKI] Panel DYM otwarty -> przechwytujemy dotyk tutaj, zamiast
      // pozwalac mu "przelecziec" do kafelkow pod spodem. Dotkniecie -/+
      // dziala jak w Ustawieniach (POLA[]+wyslijUstawienie()); dotkniecie
      // przycisku Zamknij LUB gdziekolwiek POZA karta zamyka panel (ten sam
      // wzorzec "tap outside to dismiss" co w makiecie HTML).
      int idx = znajdzIndeksPola("progAlarmDym");
      if (panelMinus.zawiera(px, py)) {
        animujWcisniecie(panelMinus.x, panelMinus.y, panelMinus.w, panelMinus.h);
        if (idx >= 0) {
          POLA[idx].wartosc -= POLA[idx].krok;
          if (POLA[idx].wartosc < POLA[idx].minZakres) POLA[idx].wartosc = POLA[idx].minZakres;
          wyslijUstawienie(idx);
        }
        rysujPanelDym();
      } else if (panelPlus.zawiera(px, py)) {
        animujWcisniecie(panelPlus.x, panelPlus.y, panelPlus.w, panelPlus.h);
        if (idx >= 0) {
          POLA[idx].wartosc += POLA[idx].krok;
          if (POLA[idx].wartosc > POLA[idx].maxZakres) POLA[idx].wartosc = POLA[idx].maxZakres;
          wyslijUstawienie(idx);
        }
        rysujPanelDym();
      } else if (panelZamknij.zawiera(px, py) || !PANEL_KARTA.zawiera(px, py)) {
        panelOtwartyId = -1;
        rysujAktualnyEkran(); // pelne przerysowanie dashboardu, usuwa karte z ekranu
      }
    }
    else if (aktualnyEkran == EKRAN_DASHBOARD && (panelOtwartyId == KAF_KLAPA || panelOtwartyId == KAF_SYBERKA)) {
      // [LCD-KAFELKI] Panel KLAPA/SYBERKA otwarty — sam suwak obslugiwany
      // ponizej (sekcja "reaguje na caly czas trzymania"), tutaj tylko
      // zamkniecie: przycisk Zamknij LUB dotyk poza karta.
      if (panelZamknij.zawiera(px, py) || !PANEL_SUWAK_KARTA.zawiera(px, py)) {
        panelOtwartyId = -1;
        rysujAktualnyEkran();
      }
    }
    else if (aktualnyEkran == EKRAN_DASHBOARD && panelOtwartyId == -1) {
      // [LCD-KAFELKI] Zaden panel nie jest otwarty -> sprawdz, czy dotyk
      // trafil w ktorys ze STEROWALNYCH kafelkow (cienka ramka KOL_OGIEN —
      // patrz tresciKafelka()). Kafelki czysto-odczytowe (temperatury,
      // POMPA, wilgotnosc, cisnienie) celowo NIE reaguja na dotyk.
      for (int id = 0; id < LICZBA_KAFELKOW; id++) {
        bool sterowalny = (id == KAF_KLAPA || id == KAF_SYBERKA || id == KAF_DYM);
        if (!sterowalny || kafelekRect[id].w == 0) continue;
        if (kafelekRect[id].zawiera(px, py)) {
          animujWcisniecie(kafelekRect[id].x, kafelekRect[id].y, kafelekRect[id].w, kafelekRect[id].h);
          panelOtwartyId = id;
          if (id == KAF_KLAPA || id == KAF_SYBERKA) {
            // suwaki nakladki startuja od aktualnej pozycji z /api/status,
            // tak samo jak przy wejsciu na ekran Serwo
            serwoKlapaProcent = stan.klapa; serwoSyberekProcent = stan.syberka;
          }
          rysujPanelOverlay(id);
          break;
        }
      }
    }
  }

  // --- suwak w nakladce dashboardu (KLAPA/SYBERKA): drag jak na ekranie Serwo ---
  if (aktualnyEkran == EKRAN_DASHBOARD && wcisniety &&
      (panelOtwartyId == KAF_KLAPA || panelOtwartyId == KAF_SYBERKA) &&
      py >= suwakPanelSuwak.y - 10 && py <= suwakPanelSuwak.y + suwakPanelSuwak.h + 10 &&
      px >= suwakPanelSuwak.x && px <= suwakPanelSuwak.x + suwakPanelSuwak.w) {
    int procent = constrain(map(px, suwakPanelSuwak.x, suwakPanelSuwak.x + suwakPanelSuwak.w, 0, 100), 0, 100);
    if (panelOtwartyId == KAF_KLAPA) { serwoKlapaProcent = procent; serwoPrzeciaganyKlapa = true; }
    else                             { serwoSyberekProcent = procent; serwoPrzeciaganySyberek = true; }
    rysujPanelSuwak(panelOtwartyId);
  }

  // --- suwaki na ekranie Serwo: reaguja na caly czas trzymania (drag) ---
  if (aktualnyEkran == EKRAN_SERWO && wcisniety) {
    if (py >= suwakKlapa.y - 10 && py <= suwakKlapa.y + suwakKlapa.h + 10 &&
        px >= suwakKlapa.x && px <= suwakKlapa.x + suwakKlapa.w) {
      serwoKlapaProcent = constrain(map(px, suwakKlapa.x, suwakKlapa.x + suwakKlapa.w, 0, 100), 0, 100);
      rysujSuwak(suwakKlapa, "Klapa", serwoKlapaProcent);
      serwoPrzeciaganyKlapa = true;
    }
    if (py >= suwakSyberek.y - 10 && py <= suwakSyberek.y + suwakSyberek.h + 10 &&
        px >= suwakSyberek.x && px <= suwakSyberek.x + suwakSyberek.w) {
      serwoSyberekProcent = constrain(map(px, suwakSyberek.x, suwakSyberek.x + suwakSyberek.w, 0, 100), 0, 100);
      rysujSuwak(suwakSyberek, "Syberek", serwoSyberekProcent);
      serwoPrzeciaganySyberek = true;
    }
  }
  // Wyslij dopiero PO puszczeniu (jedno zadanie zamiast serii w trakcie ruchu palca)
  if (!wcisniety && bylWcisniety) {
    if (serwoPrzeciaganyKlapa)   { wyslijServoManualny("klapa",   serwoKlapaProcent);   serwoPrzeciaganyKlapa = false; }
    if (serwoPrzeciaganySyberek) { wyslijServoManualny("syberek", serwoSyberekProcent); serwoPrzeciaganySyberek = false; }
  }
  bylWcisniety = wcisniety;
}

// ----------------------------------------------------------------------------
// WIFI
// ----------------------------------------------------------------------------
// [v1.2.0 WIFI-WYBOR] Wpis do logu: z KTORYM punktem dostepowym panel jest
// polaczony. BSSID to adres MAC nadajnika centrali - jedyne, czym od strony
// sieci rozni sie prawdziwa centrala od centrali testowej (obie: ta sama
// nazwa sieci, to samo haslo, ten sam adres 192.168.4.1).
static void wifiZalogujPolaczenie(const char *zdarzenie) {
  char bssid[18];
  diag::formatBssid(WiFi.BSSID(), bssid, sizeof(bssid));
  Serial.printf("tag=WIFI zdarzenie=%s bssid=%s kanal=%d rssi=%d\n",
                zdarzenie, bssid, (int)WiFi.channel(), (int)WiFi.RSSI());
}

// [v1.2.0 WIFI-WYBOR] Skanuje wszystkie kanaly i wybiera NAJSILNIEJSZY punkt
// dostepowy o nazwie WIFI_SSID. Zwraca liczbe znalezionych sieci o tej
// nazwie (0 = zadnej albo skan sie nie udal; `wybrany` jest wtedy nietkniety).
//
// POWOD: v1.1.0 wolal WiFi.begin(ssid, haslo) bez zadnych ustawien, a
// domyslna metoda w arduino-esp32 to WIFI_FAST_SCAN (WiFiSTA.cpp: "First
// SSID match [WIFI_FAST_SCAN] or Sorted [WIFI_ALL_CHANNEL_SCAN]") - sterownik
// laczy sie z PIERWSZA siecia o pasujacej nazwie, jaka znajdzie, a nie z
// najsilniejsza. Gdy w zasiegu sa dwie centrale naraz (prawdziwa przy piecu
// + testowa na biurku - obie nadaja te sama nazwe sieci, z tym samym haslem
// i adresem 192.168.4.1), panel po wlaczeniu zasilania moze trafic na
// dowolna z nich, niezaleznie od tego, ktora stoi obok - i wtedy "kafelki
// nie reaguja na zmiany w centrali", bo pokazuja dane DRUGIEJ.
// To jedna z HIPOTEZ dla zgloszenia z 2026-11-10 (niepotwierdzona: log z
// tamtego dnia nie zawiera BSSID). v1.2.0 usuwa te mozliwosc i wypisuje
// BSSID, wiec nastepny log to rozstrzygnie.
//
// Skan ASYNCHRONICZNY + wlasne czekanie z limitem (a nie blokujace
// WiFi.scanNetworks()): wersja blokujaca czeka w rdzeniu bez karmienia
// watchdoga do 60 s (WiFiScanClass::_scanTimeout w arduino-esp32 3.x), a
// loopTask jest pod watchdogiem 15 s. Sam skan to 13 kanalow x 100-150 ms,
// czyli ok. 1,5-2 s (z ustawien skanu - niezmierzone na plytce); dzieje sie
// tylko raz, przy zimnym starcie, gdy ekran i tak pokazuje komunikat o
// laczeniu.
static int wifiSkanujCentrale(diag::KandydatAp &wybrany) {
  static const int MAKS_KANDYDATOW = 6;
  static const uint32_t SKAN_LIMIT_MS = 6000;
  diag::KandydatAp kandydaci[MAKS_KANDYDATOW];
  int liczba = 0;

  WiFi.scanDelete();
  int znalezione = WiFi.scanNetworks(true /*async*/, false /*ukryte*/, false /*pasywny*/, 150 /*ms na kanal*/);
  if (znalezione == WIFI_SCAN_RUNNING) {
    const unsigned long startSkanu = millis();
    while ((znalezione = WiFi.scanComplete()) == WIFI_SCAN_RUNNING && millis() - startSkanu < SKAN_LIMIT_MS) {
      esp_task_wdt_reset();
      obsluzBrzeczyk();   // [v1.2.1] alarm przywrocony po samoczynnym restarcie ma grac swoim rytmem takze tutaj
      delay(50);
    }
    esp_task_wdt_reset();
  }
  if (znalezione < 0) {
    // Skan sie nie udal albo nie zmiescil w limicie SKAN_LIMIT_MS (radio
    // moze wtedy skanowac dalej). Przerwij go, zeby nie kolidowal z
    // WiFi.begin() zaraz po powrocie z tej funkcji, i daj zadaniu zdarzen
    // WiFi chwile na obsluzenie SCAN_DONE, zanim scanDelete() nizej zwolni
    // wyniki.
    esp_wifi_scan_stop();
    delay(100);
    znalezione = WIFI_SCAN_FAILED;
  }

  for (int i = 0; i < znalezione && liczba < MAKS_KANDYDATOW; i++) {
    if (WiFi.SSID((uint8_t)i) != WIFI_SSID) continue;
    const uint8_t *b = WiFi.BSSID((uint8_t)i);
    if (b == nullptr) continue;
    memcpy(kandydaci[liczba].bssid, b, 6);
    kandydaci[liczba].kanal = WiFi.channel((uint8_t)i);
    kandydaci[liczba].rssi = WiFi.RSSI((uint8_t)i);
    liczba++;
  }
  WiFi.scanDelete();

  for (int i = 0; i < liczba; i++) {
    char bssid[18];
    diag::formatBssid(kandydaci[i].bssid, bssid, sizeof(bssid));
    Serial.printf("tag=WIFI zdarzenie=skan_centrala nr=%d/%d bssid=%s kanal=%d rssi=%d\n",
                  i + 1, liczba, bssid, (int)kandydaci[i].kanal, (int)kandydaci[i].rssi);
  }
  if (liczba == 0) {
    // znalezione: >= 0 = liczba WSZYSTKICH sieci w zasiegu (zadna nie ma
    // naszej nazwy), -2 = skan sie nie udal / nie zmiescil w limicie.
    Serial.printf("tag=WIFI lvl=WARN zdarzenie=skan_brak_centrali ssid=%s wynik_skanu=%d\n", WIFI_SSID, znalezione);
    return 0;
  }
  if (liczba > 1) {
    Serial.printf("tag=WIFI lvl=WARN zdarzenie=wiele_central liczba=%d msg=\"kilka sieci o tej samej nazwie w zasiegu - lacze z najsilniejsza\"\n", liczba);
  }
  wybrany = kandydaci[diag::wybierzNajsilniejszy(kandydaci, liczba)];
  return liczba;
}

// [v1.2.0 WIFI-WYBOR] Jedno miejsce, w ktorym startuje laczenie (wolane z
// polaczWifi() i z wifiReconnectNonBlocking() - w v1.1.0 ten sam if/else byl
// w obu).
//  - Znany BSSID i kanal: szybkie laczenie dokladnie jak w v1.1.0. Szybki
//    skan zaczyna od podanego kanalu i konczy na pierwszym trafieniu, a
//    "trafic" moze tylko ten jeden BSSID - nie ma tu czego wybierac.
//  - Bez BSSID wyboru dokonuje sterownik WiFi: ma przeszukac wszystkie
//    kanaly i wziac NAJSILNIEJSZA siec o tej nazwie (domyslnie wzialby
//    pierwsza znaleziona). To kosztuje pelny skan przed kazda proba, dlatego
//    tylko w tej galezi.
static void wifiRozpocznijLaczenie() {
  if (rtcBssidZnany) {
    WiFi.setScanMethod(WIFI_FAST_SCAN);
    WiFi.begin(WIFI_SSID, WIFI_PASS, rtcKanalCentrali, rtcBssidCentrali, true);
  } else {
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// [v1.2.1 RATUNEK] Kod przyczyny kazdego rozlaczenia WiFi. Wolane przez
// rdzen z OSOBNEGO zadania (zdarzen sieci), dlatego tylko zapisuje dwie
// zmienne - log wypisuje petla glowna (wifiZalogujRozlaczenia()). Te sama
// postac funkcji ma wifiZdarzenieDiag() w centrali.
static void wifiZdarzenie(WiFiEvent_t zdarzenie, WiFiEventInfo_t info) {
  if (zdarzenie == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifiOstatniKodRozlaczenia = info.wifi_sta_disconnected.reason;
    wifiLiczbaRozlaczen = wifiLiczbaRozlaczen + 1;
  }
}

// Jedna linia na rozlaczenie (przy serii - najwyzej co 2 s, z liczba
// pominietych w "nowych="). Kody: diag::nazwaKoduRozlaczenia(); np. 201 =
// sieci centrali nie ma w eterze, 200 = centrala przestala nadawac, 8 = panel
// rozlaczyl sie sam (WiFi.disconnect() przed kolejna proba).
static void wifiZalogujRozlaczenia() {
  static uint32_t zalogowano = 0;
  static unsigned long ostatnioMs = 0;
  const uint32_t lacznie = wifiLiczbaRozlaczen;
  if (lacznie == zalogowano) return;
  if (zalogowano != 0 && millis() - ostatnioMs < 2000UL) return;
  ostatnioMs = millis();
  const uint8_t kod = wifiOstatniKodRozlaczenia;
  Serial.printf("tag=WIFI zdarzenie=rozlaczono kod=%u nazwa=%s nowych=%lu lacznie=%lu\n",
                (unsigned)kod, diag::nazwaKoduRozlaczenia(kod),
                (unsigned long)(lacznie - zalogowano), (unsigned long)lacznie);
  zalogowano = lacznie;
}

// [v1.2.1 RATUNEK] WiFi zglasza polaczenie, a centrala od 12 s nie odpowiada
// (patrz panelRatunekKrok()): rozlacz i polacz od nowa. Mozliwy powod takiego
// stanu (PRZYPUSZCZENIE, niesprawdzone na plytce): centrala zrestartowala sie
// na tyle szybko, ze panel nie zauwazyl przerwy w nadawaniu - uwaza
// polaczenie za wazne, a centrala po restarcie juz go nie zna. Niezaleznie od
// powodu: w tym stanie v1.1.0 i v1.2.0 nie probowaly niczego.
// `bezBssid`: zapomnij zapamietany nadajnik i szukaj sieci centrali od nowa.
static bool wifiWymuszenieTrwa = false;
static unsigned long wifiWymuszenieMs = 0;
static const uint32_t WIFI_WYMUSZENIE_CZAS_MS = 5000;   // tyle ma wymuszona proba, zanim wifiReconnectNonBlocking() zacznie wlasne

void wifiWymusPolaczenie(bool bezBssid, uint8_t nr) {
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  char bssid[18];
  diag::formatBssid(wifiOk ? WiFi.BSSID() : nullptr, bssid, sizeof(bssid));
  Serial.printf("tag=WIFI lvl=WARN zdarzenie=wymuszam_polaczenie nr=%u szukam=%s bssid=%s kanal=%d rssi=%d bez_danych_s=%lu "
                "msg=\"WiFi polaczone, a centrala nie odpowiada - lacze od nowa\"\n",
                (unsigned)nr, bezBssid ? "od_nowa" : "ten_sam_bssid", bssid,
                wifiOk ? (int)WiFi.channel() : 0, wifiOk ? (int)WiFi.RSSI() : 0,
                (unsigned long)(panelRatunek.bezDanychMs / 1000UL));
  if (bezBssid) rtcBssidZnany = false;
  WiFi.disconnect(false);
  WiFi.config(WIFI_IP_STATYCZNE, WIFI_GATEWAY, WIFI_MASKA);
  wifiRozpocznijLaczenie();
  wifiWymuszenieTrwa = true;
  wifiWymuszenieMs = millis();
}

void polaczWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(wifiZdarzenie);   // [v1.2.1 RATUNEK] kody rozlaczen do logu - polaczWifi() jest wolane raz, z setup()

  // [ETAP 3 KROK 2] Statyczne IP PRZED WiFi.begin() — glowna dzwignia
  // szybkosci (patrz komentarz przy definicji WIFI_IP_STATYCZNE wyzej).
  WiFi.config(WIFI_IP_STATYCZNE, WIFI_GATEWAY, WIFI_MASKA);

  tft.fillScreen(KOL_TLO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(KOL_TEKST, KOL_TLO);
  tft.setTextFont(2);

  if (!rtcBssidZnany) {
    // [v1.2.0 WIFI-WYBOR] Zimny start (pamiec RTC pusta): wlasny skan, wybor
    // najsilniejszej centrali i polaczenie z NIA po BSSID.
    tft.drawString("Szukam centrali", 240, 150);
    diag::KandydatAp ap;
    wifiCentralPrzyStarcie = wifiSkanujCentrale(ap);
    if (wifiCentralPrzyStarcie > 0) {
      memcpy(rtcBssidCentrali, ap.bssid, 6);
      rtcKanalCentrali = ap.kanal;
      rtcBssidZnany = true;
    }
    // Napis jest wysrodkowany na y=150 (MC_DATUM, font 16 px -> zajmuje
    // y=142..158), a petla ponizej czysci tylko y=150..170 - bez tego spod
    // "Laczenie z WiFi" wystawalyby resztki "Szukam centrali".
    tft.fillRect(0, 138, 480, 24, KOL_TLO);
  }

  wifiRozpocznijLaczenie();

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
    // [v1.2.0 FIX-WDT-WIFI] setup() podpina loopTask pod Task WDT (15 s,
    // panic) PRZED polaczWifi(), a ta petla czeka do 20 s i nie karmila
    // watchdoga: przy centrali wylaczonej albo poza zasiegiem panel
    // restartowalby sie co ok. 15 s w kolko, nie dochodzac do ekranu
    // (wniosek z kodu i z testu na PC - nie zaobserwowane na plytce).
    esp_task_wdt_reset();
    tft.fillRect(0, 150, 480, 20, KOL_TLO);
    tft.drawString("Laczenie z WiFi" , 240, 150);
    // [v1.2.1] Jak w petli skanu wyzej: alarm sprzed samoczynnego restartu
    // gra dalej. 8 x 50 ms zamiast jednego delay(400), bo fazy wzorca trwaja
    // po 500 ms; bez alarmu obsluzBrzeczyk() wraca od razu.
    for (int i = 0; i < 8; i++) {
      obsluzBrzeczyk();
      delay(50);
    }
  }
  esp_task_wdt_reset();

  if (WiFi.status() == WL_CONNECTED) {
    // Zapamietaj BSSID/kanal na nastepne polaczenie (np. po wybudzeniu z deep sleep).
    memcpy(rtcBssidCentrali, WiFi.BSSID(), 6);
    rtcKanalCentrali = WiFi.channel();
    rtcBssidZnany = true;
    wifiZalogujPolaczenie("polaczono");
  } else {
    // Nieudane polaczenie (np. bledny cache po zmianie kanalu AP) — wyczysc cache,
    // zeby kolejna proba nie utknela na nieaktualnych danych.
    rtcBssidZnany = false;
    Serial.println(F("tag=WIFI lvl=WARN zdarzenie=brak_polaczenia_na_starcie msg=\"ponawiam w tle, ekran pokaze --\""));
  }
}

// [ETAP 5, PIEC_ESPNOW_PLAN.md] Wywolywane w KAZDEJ iteracji loop() —
// nieblokujace (WiFi.begin() na ESP32 wraca od razu, laczenie dzieje sie w
// tle; sam sprawdzamy WiFi.status() w kolejnych iteracjach). Reconnect z
// rosnacym opoznieniem miedzy probami zamiast co iteracje — patrz komentarz
// research przy WIFI_RECONNECT_MAX_MS na gorze pliku.
void wifiReconnectNonBlocking() {
  static bool poprzednioPolaczony = true; // setup() juz polaczyl (polaczWifi())
  // [v1.2.0 WIFI-WYBOR] Ile kolejnych prob bylo "na sztywno" do zapamietanego
  // BSSID. v1.1.0 probowal tak w nieskonczonosc: gdy ta konkretna centrala
  // znikla na stale (wylaczona centrala testowa, wymieniona plytka), panel
  // nie polaczyl sie juz z zadna inna az do odlaczenia zasilania.
  static const uint8_t WIFI_PROB_Z_BSSID = 3;
  static uint8_t probyZBssid = 0;

  wifiZalogujRozlaczenia();   // [v1.2.1 RATUNEK] kod przyczyny kazdego rozlaczenia

  if (WiFi.status() == WL_CONNECTED) {
    probyZBssid = 0;
    wifiWymuszenieTrwa = false;
    if (!poprzednioPolaczony) {
      wifiZalogujPolaczenie("reconnect_ok"); // [v1.2.0] z BSSID - widac, czy to ta sama centrala co przedtem
#if LVGL_POC_TEST
      lvglPokazZnacznikCentrali();          // koncowka BSSID w naglowku mogla sie zmienic
#endif
      // Odswiez cache BSSID/kanalu na wypadek, gdyby centrala zmienila kanal
      // AP miedzy rozlaczeniem a tym polaczeniem (patrz tez polaczWifi()).
      memcpy(rtcBssidCentrali, WiFi.BSSID(), 6);
      rtcKanalCentrali = WiFi.channel();
      rtcBssidZnany = true;
    }
    wifiOpoznienieReconnectMs = 1000; // reset backoff po odzyskaniu polaczenia
    poprzednioPolaczony = true;
    return;
  }

  if (poprzednioPolaczony) {
    Serial.println(F("tag=WIFI lvl=WARN zdarzenie=utrata_polaczenia"));
    // Pierwsza proba dopiero po uplywie poczatkowego opoznienia (1s), nie
    // natychmiast — unikamy "hammering" AP przy bardzo krotkich zanikach.
    wifiOstatniaProbaReconnectMs = millis();
  }
  poprzednioPolaczony = false;

  // [v1.2.1 RATUNEK] Laczenie zlecone przed chwila przez wifiWymusPolaczenie()
  // ma WIFI_WYMUSZENIE_CZAS_MS na dokonczenie - proba ponizej zaczyna sie od
  // WiFi.disconnect(), wiec przerwalaby je w polowie.
  if (wifiWymuszenieTrwa) {
    if (millis() - wifiWymuszenieMs < WIFI_WYMUSZENIE_CZAS_MS) return;
    wifiWymuszenieTrwa = false;
  }

  if (millis() - wifiOstatniaProbaReconnectMs < wifiOpoznienieReconnectMs) return;
  wifiOstatniaProbaReconnectMs = millis();

  Serial.printf("tag=WIFI zdarzenie=reconnect_probe nastepna_proba_za_ms=%lu\n", wifiOpoznienieReconnectMs);

  // [FIX-WIFI-STATE, ten sam wzorzec co main.cpp centrali] disconnect(false)
  // PRZED begin() — bez tego powtarzane probe w petli moga trafic na
  // ESP_ERR_WIFI_STATE ("sta is connecting, cannot set config") i nie
  // wystartowac wcale. disconnect(false), NIE disconnect(true) — nie
  // zatrzymuje calego stosu WiFi, wraca w <1ms.
  WiFi.disconnect(false);
  WiFi.config(WIFI_IP_STATYCZNE, WIFI_GATEWAY, WIFI_MASKA);
  if (rtcBssidZnany && probyZBssid >= WIFI_PROB_Z_BSSID) {
    // [v1.2.0 WIFI-WYBOR] Zapamietany punkt dostepowy nie odpowiada - dalej
    // szuka juz sterownik WiFi (wszystkie kanaly, najsilniejszy sygnal -
    // patrz wifiRozpocznijLaczenie()). Bez wlasnego skanu: tu nie wolno
    // blokowac.
    rtcBssidZnany = false;
    probyZBssid = 0;
    Serial.println(F("tag=WIFI lvl=WARN zdarzenie=porzucam_bssid msg=\"zapamietana centrala nie odpowiada - szukam najsilniejszej sieci o tej nazwie\""));
  }
  if (rtcBssidZnany) probyZBssid++;
  wifiRozpocznijLaczenie();

  wifiOpoznienieReconnectMs = min(wifiOpoznienieReconnectMs * 2, WIFI_RECONNECT_MAX_MS);
}

// ----------------------------------------------------------------------------
// [ETAP 3b] ZASILANIE — deep sleep + wybudzanie przyciskiem
// ----------------------------------------------------------------------------
// Przycisk: GPIO14 -> GND (wewnetrzny pull-up, wybudzenie na stanie LOW).
// GPIO14 wybrany w poprzedniej sesji: wolny (nie koliduje z TFT/dotykiem —
// patrz platformio.ini + TOUCH_* wyzej), w domenie RTC (wymog ext0), i NIE
// jest pinem strapping (w odroznieniu od 0/2/12/15) — bezpieczny wybor.
#define WAKE_BTN_PIN GPIO_NUM_14
static const uint32_t ACTIVITY_TIMEOUT_MS = 5UL * 60UL * 1000UL; // 5 minut
unsigned long ostatniaAktywnosc = 0;

// [ETAP 3b, detekcja USB — ZDECYDOWANE: TP4056 CHRG+STDBY, nie dzielnik na
// VIN] Pierwotny plan (dzielnik napiecia z VIN/5V) zalezal od zalozenia o
// wewnetrznym okablowaniu plytki (czy bateria tez idzie przez VIN) — po
// decyzji o uzyciu TP4056 do ladowania baterii, jest prostsze i pewniejsze
// rozwiazanie niezalezne od tego zalozenia w ogole.
//
// TP4056 ma dwa wyjscia open-drain: CHRG (aktywne = ladowanie w toku) i
// STDBY (aktywne = ladowanie zakonczone) — OBA sa w stanie wysokiej
// impedancji (pływaja), gdy TP4056 NIE MA zasilania z VCC (czyli gdy USB
// odlaczone), niezaleznie od tego czy bateria jest podlaczona (research:
// datasheet TP4056 + kilka niezaleznych zrodel, potwierdzone jednoznacznie:
// "STDBY high impedance in other states", "floating pins cause false
// triggering — connect pull-up if open-drain output needed").
//
// Rozwiazanie: zewrzyj CHRG i STDBY razem (bezpieczne — oba to open-drain,
// wired-OR), JEDEN rezystor pull-up 10k z tego wspolnego wezla do 3V3
// ESP32 (NIE do VCC samego TP4056 — to VCC znika razem z USB, wiec pull-up
// do VCC nie dawalby zadnego jednoznacznego stanu przy braku USB; 3V3
// ESP32 jest zasilane z baterii przez caly czas, wiec zawsze dostepne jako
// stabilna referencja). Wspolny wezel -> GPIO35. Bez dzielnika napiecia —
// niepotrzebny, sygnal jest juz 3,3V-bezpieczny (open-drain do GND, pull-up
// do 3V3, nigdy do 5V).
//
// Logika (odwrotna wzgledem starej wersji z dzielnikiem): USB podlaczone ->
// TP4056 zasilany -> CHRG lub STDBY (lub oba) aktywne -> wezel ciagniety w
// dol -> LOW. USB odlaczone -> TP4056 bez zasilania -> oba piny plywaja ->
// pull-up wygrywa -> HIGH.
#define USB_DETECT_PIN 35

// Kilka probek zamiast jednego odczytu — tania ochrona przed pojedynczym
// zakloceniem/szumem na linii (podobny wzorzec do touchReadChannelStabilnie
// dla dotyku, ktory juz jest w tym pliku).
bool zasilanieUSB() {
  int niskie = 0;
  for (int i = 0; i < 5; i++) {
    if (digitalRead(USB_DETECT_PIN) == LOW) niskie++;
    delayMicroseconds(200);
  }
  return niskie >= 3; // wiekszosc probek LOW = TP4056 zasilany z USB (CHRG/STDBY ciagna w dol)
}

// [BATERIA-POMIAR, ta sesja] Realne napiecie ogniwa (nie tylko "czy sie
// laduje" jak zasilanieUSB() wyzej) - osobny dzielnik napiecia, osobny pin,
// bo to inny pomiar (poziom analogowy, nie stan cyfrowy open-drain).
// Dzielnik: Vbat --[R1 100k]-- BATERIA_ADC_PIN --[R2 100k]-- GND, + ~100nF
// z pinu do GND (filtr). R1=R2 -> pin ADC widzi dokladnie polowe Vbat, co
// dla ogniwa 1S (3,0-4,2V) daje 1,5-2,1V na pinie - srodek zakresu ADC
// ESP32, z dala od najmniej liniowych okolic 0V i 3,3V.
//
// Pin: musi byc ADC1 (GPIO 32-39) - ADC2 przestaje dzialac przy aktywnym
// WiFi (sprzetowe ograniczenie ESP32, WiFi u nas chodzi caly czas w galezi
// LVGL_POC_SYMULACJA=0, patrz nizej). Z wolnych ADC1 (32/34/36/39 - 33 i 35
// juz zajete przez TOUCH_MISO/USB_DETECT_PIN) wybrany GPIO34 - najczesciej
// spotykany wybor w tego typu obwodach, wejsciowy-only, nic z istniejacego
// okablowania NIE trzeba przelutowywac. Pelne uzasadnienie: PLAN_BATERIA.md.
#define BATERIA_ADC_PIN 34
static const float BATERIA_DZIELNIK = 2.0f; // R1=R2 -> odczyt z pinu razy 2 = pelne Vbat

// Usrednienie probek - ten sam motyw co w zasilanieUSB() wyzej, pojedynczy
// odczyt ADC potrafi "skakac" o kilkanascie mV. analogReadMilliVolts() (NIE
// surowy analogRead()) uzywa fabrycznej kalibracji ADC (eFuse) - wymaga
// jednorazowego analogSetAttenuation(ADC_11db) w setup() (patrz tam), bo
// analogReadMilliVolts() honoruje TYLKO globalne attenuation, ignoruje
// wersje "per-pin" (research, ta sesja - znana pulapka rdzenia arduino-esp32).
float bateriaOdczytajNapiecie() {
  const int PROBEK = 16;
  uint32_t sumaMv = 0;
  for (int i = 0; i < PROBEK; i++) {
    sumaMv += analogReadMilliVolts(BATERIA_ADC_PIN);
    delayMicroseconds(200);
  }
  return (sumaMv / (float)PROBEK) * BATERIA_DZIELNIK / 1000.0f; // mV na pinie -> pelne V baterii
}

// Napiecie LiPo NIE spada liniowo z % naladowania - 4,20 do ok. 3,80V to
// plaskie plateau (wiekszosc pojemnosci), ponizej ~3,7V krzywa gwaltownie
// przyspiesza w dol. Tabela + interpolacja liniowa MIEDZY punktami zamiast
// prostego mapowania 3,0-4,2V -> 0-100% (to zawyzaloby srodek skali).
// Orientacyjna krzywa 1S, zgodna z powszechnie publikowanymi wartosciami
// (research, ta sesja) - to wskaznik "jak w telefonie", nie licznik
// coulombow.
struct BPunkt { float v; int proc; };
static const BPunkt TABELA_BAT[] = {
  {4.20f,100},{4.15f,95},{4.11f,90},{4.08f,85},{4.02f,80},
  {3.98f,75},{3.95f,70},{3.91f,65},{3.87f,60},{3.85f,55},
  {3.84f,50},{3.82f,45},{3.80f,40},{3.79f,35},{3.77f,30},
  {3.75f,25},{3.73f,20},{3.71f,15},{3.69f,10},{3.61f, 5},
  {3.27f, 0},
};
static const int TABELA_BAT_N = sizeof(TABELA_BAT) / sizeof(TABELA_BAT[0]);

int bateriaNapiecieNaProcent(float v) {
  if (v >= TABELA_BAT[0].v) return 100;
  if (v <= TABELA_BAT[TABELA_BAT_N - 1].v) return 0;
  for (int i = 0; i < TABELA_BAT_N - 1; i++) {
    if (v <= TABELA_BAT[i].v && v >= TABELA_BAT[i + 1].v) {
      float zakresV = TABELA_BAT[i].v - TABELA_BAT[i + 1].v;
      float pozycja = (TABELA_BAT[i].v - v) / zakresV;
      return round(TABELA_BAT[i].proc - pozycja * (TABELA_BAT[i].proc - TABELA_BAT[i + 1].proc));
    }
  }
  return 0;
}

// Wywolywane przy KAZDYM wykrytym dotyku (patrz obsluzDotyk()) — resetuje
// timer nieaktywnosci.
void zarejestrujAktywnosc() {
  ostatniaAktywnosc = millis();
}

// [research, ta sesja] Znany bug/pulapka Espressif: jesli pin wybudzajacy
// jest WCIAZ wcisniety w momencie wywolania esp_deep_sleep_start(), ESP32
// natychmiast wybudza sie z powrotem — petla usypiania. Dlatego czekamy na
// PUSZCZENIE przycisku + krotki debounce TUZ PRZED wlasciwym usnieciem,
// nie tylko przy samym wybudzeniu.
void idzSpac() {
  // [OTA-PANEL, ta sesja, prompt #8] Nigdy nie usypiaj w trakcie pobierania/
  // flashowania firmware'u — przerwany zapis do flash moze zostawic panel
  // w zlym stanie (patrz BAGNO 5.1 w OTA_PANEL_CALOSC.md). Flaga jest
  // czyszczona po KAZDEJ probie pobierania (otaSprzatnijPoProbie(), [v1.4.0
  // OTA]) - udanej konczy sie restartem - wiec to tylko chwilowe opoznienie
  // usniecia, nie trwala blokada.
  if (trwaAktualizacjaOtaPanelu) {
    Serial.println(F("tag=OTA-PANEL zdarzenie=usypianie_zablokowane"));
    return;
  }

  Serial.println(F("=== USYPIANIE (timeout nieaktywnosci) ==="));

  // Wygaszenie ekranu przed snem — nie ma sensu trzymac podswietlenia
  // wlaczonego, skoro i tak zaraz nastapi twardy reset przy wybudzeniu.
  ustawJasnoscPodswietlenia(0);
  tft.fillScreen(TFT_BLACK);

  // [FIX-ILI9488-SLEEP, ta sesja] Samo wygaszenie ekranu na czarno NIE usypia
  // kontrolera ILI9488 — bez tej komendy kontroler zostaje aktywny i ciagnie
  // normalny prad roboczy przez caly czas snu ESP32, niezaleznie od tego, ze
  // nic nie widac na ekranie. Potwierdzone pomiarem multimetrem: AKTYWNY
  // (Sleep OUT) ~62mA vs SPOCZYNEK (Sleep IN) ~0.11mA.
  // Po stronie wybudzenia NIE trzeba jawnego Sleep OUT — TFT_RST=4 (patrz
  // platformio.ini) to prawdziwy pin, wiec tft.init() w setup() i tak zawsze
  // robi twardy reset przez RST, a nastepnie tabela init dla ILI9488 sama
  // wysyla Sleep OUT + delay(120) (zweryfikowane w zrodlach TFT_eSPI 2.5.43,
  // plik TFT_Drivers/ILI9488_Init.h) — dublowanie tego w setup() byloby
  // zbedne (dodatkowe 120ms przy kazdym starcie bez zadnej korzysci).
  tft.writecommand(0x10);             // ILI9488: Sleep IN

  // [FIX-TFT-BL-HOLD, ta sesja] Pin podswietlenia (TFT_BL=GPIO21) NIE MOZE
  // "wisiec" (floating) podczas deep sleep — wewnetrzne stany pinow sa
  // resetowane przy wejsciu w sen, wiec bez jawnego "hold" podswietlenie
  // moze sie z powrotem czesciowo zaswiecic (przez uplywnosc/podciaganie na
  // plytce wyswietlacza) i zjadac baterie w tle przez caly sen. GPIO21 NIE
  // jest pinem RTC (w odroznieniu od poprzedniego GPIO32), wiec wystarczy
  // uniwersalny mechanizm gpio_hold_en + globalny gpio_deep_sleep_hold_en —
  // BEZ potrzeby uzywania rtc_gpio_hold_en().
  gpio_hold_en((gpio_num_t)TFT_BL);   // zamraza biezacy stan pinu (LOW) na czas snu
  gpio_deep_sleep_hold_en();          // globalny wlacznik "trzymania" cyfrowych pinow w deep sleep

  // Czekaj na puszczenie przycisku, zeby uniknac natychmiastowego wybudzenia.
  while (digitalRead(WAKE_BTN_PIN) == LOW) {
    delay(10);
  }
  delay(50); // debounce

  // Pin wybudzajacy NIE MOZE "wisiec" (floating) podczas snu — wewnetrzne
  // pull-upy/pull-downy sa w deep sleep wylaczone, chyba ze wlaczy sie je
  // jawnie funkcjami rtc_gpio_*. Bez tego przypadkowy szum na pinie moze
  // wybudzic urzadzenie natychmiast po usnieciu.
  rtc_gpio_pullup_en(WAKE_BTN_PIN);
  rtc_gpio_pulldown_dis(WAKE_BTN_PIN);

  esp_sleep_enable_ext0_wakeup(WAKE_BTN_PIN, 0); // 0 = wybudzenie na stanie LOW
  esp_deep_sleep_start();
  // (nic dalej sie nie wykona — po wybudzeniu ESP32 startuje od nowa od setup())
}

// [OTA-ROLLBACK] Potwierdza biezaca partycje jako VALID, jesli zostala
// wgrana przez OTA i wciaz czeka na weryfikacje (PENDING_VERIFY) — patrz
// Ustalenie #6 w OTA_PANEL_CALOSC.md, mechanizm zweryfikowany
// empirycznie w projekcie testowym ota_rollback_test (main_v1.cpp, ta sama
// logika co tutaj). Wolane z KONCA setup(), PO tym jak ekran juz sie
// poprawnie narysowal i (w trybach z WiFi) pierwsza probka pobierzStatus()
// juz sie wykonala — to nasz sygnal "firmware dziala wystarczajaco dobrze,
// zeby je zatwierdzic". Bezpieczne do wywolania w kazdym trybie: gdy
// partycja NIE jest PENDING_VERIFY (np. wgranie przez USB), warunek w
// srodku po prostu nic nie robi.
// [v1.4.0 OTA] Zwraca true, gdy biezaca partycja jest zatwierdzona (albo
// zatwierdzenia nie wymagala); false tylko wtedy, gdy czekala na zatwierdzenie
// i nie udalo sie go zapisac. Wynik bierze otaPoZatwierdzeniu().
bool potwierdzPartycjeJesliCzeka() {
  const esp_partition_t *biezaca = esp_ota_get_running_partition();
  esp_ota_img_states_t stanOta;

  esp_err_t wynik = esp_ota_get_state_partition(biezaca, &stanOta);
  if (wynik != ESP_OK) {
    // Normalne przy wgraniu przez USB — otadata nie sledzi obrazow
    // wgranych przez USB, to nie blad. Patrz main_v1.cpp / Ustalenie #6.
    Serial.printf("tag=OTA-ROLLBACK zdarzenie=brak_stanu_partycji blad=%d\n", (int)wynik);
    return true;
  }

  Serial.printf("tag=OTA-ROLLBACK zdarzenie=stan_partycji partycja=%s stan=%d\n",
                biezaca->label, (int)stanOta);

  if (stanOta == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_err_t r = esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("tag=OTA-ROLLBACK zdarzenie=potwierdzono_valid wynik=%s\n",
                  esp_err_to_name(r));
    return r == ESP_OK;
  }
  return true;
}

// ============================================================================
// [v1.4.0 OTA] AKTUALIZACJA PANELU PRZEZ CENTRALE - wykonanie
// ============================================================================
// Decyzje (kiedy zaczac, kiedy odmowic, co zapamietac) sa w
// diagnostyka_panelu.h, sekcja [v1.4.0 OTA] - czysty kod testowany na PC.
// Tutaj jest to, co dotyka sprzetu: odczyt wlasnego programu, NVS, pobieranie
// (otaFetch), zapis do flash (Update), ekran i restart.
//
// Przebieg:
//   1. start panelu: otaPrzyStarcie() - identyfikator programu, z ktorego
//      panel pracuje, i to, co panel zapamietal o ostatniej ofercie,
//   2. po zatwierdzeniu partycji: otaPoZatwierdzeniu() - czy ostatnie wgranie
//      sie przyjelo, czy bootloader wrocil do poprzedniego programu,
//   3. w petli: otaKrok() - raz na sekunde decyzja (diag::otaDecyzja()), a w
//      trakcie pobierania kolejny kawalek pliku w kazdym obrocie petli.
// Do v1.3.0 (wykonajAktualizacjeOTAPanelu() + sprawdzIWykonajOtaPanelu(),
// biblioteka HTTPUpdate): panel sprawdzal oferte RAZ, na koncu setup(), i
// pobieral plik bez powrotu do petli - z pustym ekranem, bez odpytywania
// centrali (wiec i bez alarmu), z watchdogiem wypisanym na czas pobierania.
// Tego samego pliku nie odroznial od nowego, wiec przy centrali, ktora nie
// zdjela oferty, wgrywal go przy kazdym starcie.
// ----------------------------------------------------------------------------
static const char OTA_NVS_PRZESTRZEN[] = "otapanel";
static const char OTA_NVS_KLUCZ[] = "zapis";
static const uint8_t OTA_NVS_WERSJA = 1;   // pierwszy bajt zapisu; inna wartosc = zapis w nieznanej postaci, pomijany

// Caly zapis idzie do NVS jednym wpisem (putBytes): albo jest stary, albo
// nowy - nigdy polowa (np. nowy klucz oferty ze starym licznikiem prob).
static void otaZapisWczytaj() {
  memset(&otaZapis, 0, sizeof(otaZapis));
  Preferences p;
  if (!p.begin(OTA_NVS_PRZESTRZEN, true)) return;   // przestrzeni jeszcze nie ma: panel nigdy nie zaczynal aktualizacji
  uint8_t bufor[1 + sizeof(diag::OtaZapis)];
  if (p.isKey(OTA_NVS_KLUCZ) && p.getBytesLength(OTA_NVS_KLUCZ) == sizeof(bufor) &&
      p.getBytes(OTA_NVS_KLUCZ, bufor, sizeof(bufor)) == sizeof(bufor) && bufor[0] == OTA_NVS_WERSJA) {
    memcpy(&otaZapis, bufor + 1, sizeof(otaZapis));
    otaZapis.klucz[sizeof(otaZapis.klucz) - 1] = '\0';
    otaZapis.oczekiwany[sizeof(otaZapis.oczekiwany) - 1] = '\0';
    if (otaZapis.stan > diag::OTA_ZAPIS_ODMOWA) memset(&otaZapis, 0, sizeof(otaZapis));
  }
  p.end();
}

static bool otaZapisZachowaj() {
  Preferences p;
  if (!p.begin(OTA_NVS_PRZESTRZEN, false)) return false;
  uint8_t bufor[1 + sizeof(diag::OtaZapis)];
  bufor[0] = OTA_NVS_WERSJA;
  memcpy(bufor + 1, &otaZapis, sizeof(otaZapis));
  const bool ok = (p.putBytes(OTA_NVS_KLUCZ, bufor, sizeof(bufor)) == sizeof(bufor));
  p.end();
  return ok;
}

// Poczatek setup(), przed pierwszym zapytaniem o status.
// Identyfikator wlasnego programu to poczatek sumy SHA-256, ktora esptool
// dopisuje na koncu kazdego programu ESP32; esp_partition_get_sha256() dla
// partycji z programem zwraca wlasnie te dopisana sume. Ta sama suma jest na
// koncu pliku firmware.bin, z ktorego program wgrano (kablem albo przez OTA),
// wiec centrala moze policzyc ten sam identyfikator z samego pliku.
void otaPrzyStarcie() {
  const esp_partition_t *biezaca = esp_ota_get_running_partition();
  uint8_t suma[32];
  otaObrazWlasny[0] = '\0';
  if (biezaca != nullptr && esp_partition_get_sha256(biezaca, suma) == ESP_OK) diag::otaObrazZSumy(suma, otaObrazWlasny);
  // Pierwszy start po wgraniu przez OTA: partycja czeka na zatwierdzenie
  // (potwierdzPartycjeJesliCzeka() na koncu setup()). Do tej chwili panel
  // zglasza centrali "panel_obraz_proba", nie "panel_obraz".
  esp_ota_img_states_t stanOta;
  otaObrazZatwierdzony = !(biezaca != nullptr && esp_ota_get_state_partition(biezaca, &stanOta) == ESP_OK &&
                           stanOta == ESP_OTA_IMG_PENDING_VERIFY);
  otaZapisWczytaj();
  Serial.printf("tag=OTA-PANEL zdarzenie=stan_przy_starcie obraz=%s zatwierdzony=%d zapis_stan=%u zapis_prob=%u zapis_klucz=%s\n",
                otaObrazWlasny[0] != '\0' ? otaObrazWlasny : "nieznany", (int)otaObrazZatwierdzony,
                (unsigned)otaZapis.stan, (unsigned)otaZapis.prob, otaZapis.klucz[0] != '\0' ? otaZapis.klucz : "-");
  if (otaObrazWlasny[0] == '\0') {
    Serial.println(F("tag=OTA-PANEL lvl=WARN zdarzenie=brak_identyfikatora_obrazu msg=\"nie da sie odczytac sumy wlasnego programu - aktualizacja przez centrale wylaczona do restartu\""));
  }
}

// Koniec setup(), zaraz po potwierdzPartycjeJesliCzeka(). `zatwierdzona` to
// jej wynik: true = biezaca partycja jest zatwierdzona (albo nie wymagala tego).
void otaPoZatwierdzeniu(bool zatwierdzona) {
  if (!zatwierdzona) return;   // nie udalo sie zatwierdzic: panel dalej zglasza "panel_obraz_proba"
  otaObrazZatwierdzony = true;
  const int ocena = diag::otaOcenPoStarcie(otaZapis, otaObrazWlasny);
  if (ocena == 0) return;
  otaZapisZachowaj();
  if (ocena == 1) {
    Serial.printf("tag=OTA-PANEL zdarzenie=nowy_firmware_dziala obraz=%s wersja=%s\n", otaObrazWlasny, FIRMWARE_VERSION_PANEL);
  } else {
    // Panel wgral plik i zrestartowal sie, a dziala inny program niz wgrany:
    // nowy nie doszedl do zatwierdzenia i bootloader wrocil do poprzedniego.
    Serial.printf("tag=OTA-PANEL lvl=ERR zdarzenie=nowy_firmware_wycofany wgrany=%s dziala=%s\n",
                  otaZapis.oczekiwany, otaObrazWlasny[0] != '\0' ? otaObrazWlasny : "nieznany");
  }
}

// Koniec proby pobierania (udanej albo nie): gniazdo, bufor zapisu, ekran.
static void otaSprzatnijPoProbie() {
  if (otaFetch.wTrakcie()) otaFetch.przerwij();
  else otaFetch.skwituj();
  if (otaP.zapisZaczety) Update.abort();   // nic z niedokonczonego zapisu nie zostaje do uruchomienia
  otaP.zapisZaczety = false;
  otaP.trwa = false;
  trwaAktualizacjaOtaPanelu = false;
  otaBylaProba = true;
  otaKoniecProbyMs = millis();
  lvgl_dashboard_poc_ota(-1, nullptr);
}

// Proba nieudana z winy pliku, sieci albo zapisu - liczy sie do limitu prob.
static void otaProbaNieudana(uint8_t powod, bool ostateczna, const char *opis) {
  const bool koniecProb = diag::otaZapisNieudana(otaZapis, powod, ostateczna);
  const bool zapisanoStan = otaZapisZachowaj();
  logBezPamieci("tag=OTA-PANEL lvl=ERR zdarzenie=nieudana powod=%s proba=%u/%u koniec_prob=%d http=%d bajtow=%lu z=%ld etap=%c nvs=%d blad=\"%s\"\n",
                diag::otaPowodTekst(powod), (unsigned)otaZapis.prob, (unsigned)diag::OTA_MAX_PROB, (int)koniecProb,
                otaFetch.kodHttp(), (unsigned long)otaP.zapisano, otaFetch.dlugoscZapowiedziana(), otaFetch.etapBledu(),
                (int)zapisanoStan, opis != nullptr ? opis : "");
  otaSprzatnijPoProbie();
}

// Proba przerwana nie z winy pliku ani sieci (alarm, oferta wycofana) - nie liczy sie.
static void otaProbaPrzerwana(const char *dlaczego) {
  diag::otaZapisOddajProbe(otaZapis);
  otaZapisZachowaj();
  logBezPamieci("tag=OTA-PANEL lvl=WARN zdarzenie=przerwana powod=%s bajtow=%lu z=%ld msg=\"ta proba sie nie liczy\"\n",
                dlaczego, (unsigned long)otaP.zapisano, otaFetch.dlugoscZapowiedziana());
  otaSprzatnijPoProbie();
}

// Kolejny kawalek pliku od centrali (wolane z otaFetch.poll()). Nie wolno tu
// przerywac pobierania - blad jest zapamietywany, a reaguje otaKrokPobierania().
static void otaOdbiorcaBajtow(const uint8_t *dane, size_t n) {
  if (otaP.blad != diag::OTA_POWOD_BRAK) return;   // po bledzie reszta danych z tego obrotu przepada
  const size_t bylo = otaP.skan.lacznie;
  diag::otaSkanDodaj(otaP.skan, dane, n);
  if (otaP.skan.lacznie < diag::OTA_NAGLOWEK_B) return;   // naglowek pliku jeszcze niepelny - nic nie zapisujemy
  if (!otaP.zapisZaczety) {
    // Caly naglowek (24 bajty) jest: czy to w ogole program dla tego panelu?
    if (diag::otaSkanOcenNaglowek(otaP.skan) != diag::OTA_POWOD_BRAK) {
      otaP.blad = diag::OTA_POWOD_OBRAZ;
      otaP.ostateczny = true;
      otaP.opisBledu = "plik nie jest programem dla ESP32";
      return;
    }
    const long zapowiedziano = otaFetch.dlugoscZapowiedziana();
    if (zapowiedziano < (long)(diag::OTA_NAGLOWEK_B + diag::OTA_SUMA_B + 1)) {
      otaP.blad = diag::OTA_POWOD_POBIERANIE;
      otaP.opisBledu = "centrala nie podala rozmiaru pliku";
      return;
    }
    otaP.rozmiar = (uint32_t)zapowiedziano;
    if (!Update.begin((size_t)zapowiedziano)) {
      const bool zaDuzy = (Update.getError() == UPDATE_ERROR_SIZE);
      otaP.blad = zaDuzy ? diag::OTA_POWOD_ZA_DUZY : diag::OTA_POWOD_ZAPIS;
      otaP.ostateczny = zaDuzy;
      otaP.opisBledu = zaDuzy ? "plik wiekszy niz partycja panelu" : "Update.begin() nieudane";
      return;
    }
    otaP.zapisZaczety = true;
    if (Update.write(otaP.skan.naglowek, diag::OTA_NAGLOWEK_B) != diag::OTA_NAGLOWEK_B) {
      otaP.blad = diag::OTA_POWOD_ZAPIS;
      otaP.opisBledu = "zapis naglowka nieudany";
      return;
    }
    otaP.zapisano = (uint32_t)diag::OTA_NAGLOWEK_B;
    // Poczatek tego kawalka mogl nalezec do naglowka (juz zapisanego wyzej).
    const size_t zNaglowka = (bylo < diag::OTA_NAGLOWEK_B) ? (diag::OTA_NAGLOWEK_B - bylo) : 0;
    dane += zNaglowka;
    n -= zNaglowka;
  }
  if (n == 0) return;
  // Update.write() bierze wskaznik bez const, ale danych nie zmienia (kopiuje je do wlasnego bufora).
  if (Update.write(const_cast<uint8_t *>(dane), n) != n) {
    otaP.blad = diag::OTA_POWOD_ZAPIS;
    otaP.opisBledu = "zapis do flash nieudany";
    return;
  }
  otaP.zapisano += (uint32_t)n;
}

// Czy centrala nadal oferuje to samo, co panel wlasnie pobiera.
static bool otaOfertaBezZmian() {
  if (!stan.panelOtaDostepna) return false;
  char klucz[diag::OTA_KLUCZ_MAKS];
  diag::otaKlucz(klucz, sizeof(klucz), diag::otaObrazPoprawny(stan.panelOtaObraz), stan.panelOtaNr, stan.panelOtaObraz, stan.panelOtaWersja);
  return strcmp(klucz, otaP.klucz) == 0;
}

// Caly plik odebrany: sprawdzenie, uruchomienie, restart.
static void otaPoPobraniu() {
  if (!otaFetch.bylSukces()) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, otaFetch.ostatniBlad());
    return;
  }
  const int kod = otaFetch.kodHttp();
  if (kod == 503) {   // centrala 3.58.6+: trwa alarm
    otaProbaPrzerwana("alarm_centrali");
    return;
  }
  if (kod == 401 || kod == 403) {
    otaProbaNieudana(diag::OTA_POWOD_HASLO, true, "centrala odrzucila haslo OTA");
    return;
  }
  if (kod != 200) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, "centrala nie wydala pliku");
    return;
  }
  if (!otaP.zapisZaczety || !otaFetch.kompletna() || otaP.zapisano != otaP.rozmiar) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, "plik niepelny");
    return;
  }
  char obrazPliku[17];
  diag::otaSkanObraz(otaP.skan, obrazPliku);
  if (otaP.nowaCentrala && strcmp(obrazPliku, otaP.obrazOferty) != 0) {
    otaProbaNieudana(diag::OTA_POWOD_OBRAZ, true, "pobrany plik ma inny identyfikator niz oferta");
    return;
  }
  if (obrazPliku[0] != '\0' && strcmp(obrazPliku, otaObrazWlasny) == 0) {
    // Mozliwe tylko ze starsza centrala (nie podaje identyfikatora przed
    // pobraniem): to ten sam program, z ktorego panel juz pracuje. Nie ma
    // czego wgrywac ani po co sie restartowac.
    otaZapis.stan = diag::OTA_ZAPIS_DZIALA;
    otaZapis.powod = diag::OTA_POWOD_BRAK;
    otaZapisZachowaj();
    Serial.printf("tag=OTA-PANEL zdarzenie=ten_sam_plik obraz=%s msg=\"plik z centrali to program, z ktorego panel juz pracuje\"\n", obrazPliku);
    otaSprzatnijPoProbie();
    return;
  }
  lvgl_dashboard_poc_ota(100, "Sprawdzanie pliku...");
  lvgl_port_tick();
  // Update.end() dopisuje poczatek programu, sprawdza caly obraz (suma
  // kontrolna i SHA-256 - robi to ESP-IDF) i dopiero wtedy ustawia go jako
  // program startowy. Do tej chwili partycja z nowym plikiem nie da sie uruchomic.
  if (!Update.end(false)) {
    const bool zlyObraz = (Update.getError() == UPDATE_ERROR_ACTIVATE);
    otaP.zapisZaczety = false;   // Update sam porzucil zapis
    otaProbaNieudana(zlyObraz ? diag::OTA_POWOD_OBRAZ : diag::OTA_POWOD_ZAPIS, zlyObraz, Update.errorString());
    return;
  }
  otaP.zapisZaczety = false;
  diag::otaZapisWgrane(otaZapis, obrazPliku);
  const bool zapisanoStan = otaZapisZachowaj();
  Serial.printf("tag=OTA-PANEL zdarzenie=pobrano_ok obraz=%s bajtow=%lu trwalo_ms=%lu nvs=%d restart_za_chwile\n",
                obrazPliku[0] != '\0' ? obrazPliku : "-", (unsigned long)otaP.zapisano, (unsigned long)otaFetch.trwaloMs(), (int)zapisanoStan);
  otaFetch.skwituj();
  lvgl_dashboard_poc_ota(100, "Gotowe - restart panelu");
  for (int i = 0; i < 20; i++) {   // ok. 0,5 s, zeby napis zdazyl sie narysowac
    lvgl_port_tick();
    delay(25);
  }
  Serial.flush();
  ESP.restart();
}

// Trwa pobieranie: jeden obrot petli.
static void otaKrokPobierania() {
  otaFetch.poll();   // do 4 kB na obrot; kazdy kawalek od razu idzie do otaOdbiorcaBajtow()
  if (otaP.blad != diag::OTA_POWOD_BRAK) {
    otaProbaNieudana(otaP.blad, otaP.ostateczny, otaP.opisBledu);
    return;
  }
  // Alarm jest wazniejszy niz aktualizacja: ekran i brzeczyk maja byc wolne.
  if (stan.dymAlarm || stan.alarmOgrzewanie || stan.alarmPanel) {
    otaProbaPrzerwana("alarm");
    return;
  }
  if (!otaOfertaBezZmian()) {
    otaProbaPrzerwana("oferta_wycofana");
    return;
  }
  if (otaFetch.gotowyWynik()) {
    otaPoPobraniu();
    return;
  }
  if (otaP.rozmiar > 0) {
    const int procent = (int)(((uint64_t)otaP.zapisano * 100U) / otaP.rozmiar);
    if (procent != otaP.procentNaEkranie) {
      otaP.procentNaEkranie = procent;
      lvgl_dashboard_poc_ota(procent, "Pobieranie z centrali");
    }
  }
}

// Poczatek proby: zapis w NVS, potem dopiero polaczenie.
static void otaZacznij(const diag::OtaOferta &o) {
  char haslo[91];
  char sciezka[112];   // tyle miesci otaFetch (StrumienHttpGet::sciezka_)
  const bool hasloMiesciSie = diag::urlKoduj(OTA_HASLO_PANEL, haslo, sizeof(haslo));
  memset(&otaP, 0, sizeof(otaP));
  otaP.opisBledu = "";
  diag::otaKlucz(otaP.klucz, sizeof(otaP.klucz), o.nowaCentrala, o.nr, o.obraz, o.tag);
  otaP.nowaCentrala = o.nowaCentrala;
  snprintf(otaP.obrazOferty, sizeof(otaP.obrazOferty), "%s", o.nowaCentrala ? o.obraz : "");
  otaP.procentNaEkranie = -1;
  // Licznik prob rosnie i trafia do NVS ZANIM cokolwiek sie zacznie. Gdyby
  // panel zawieszal sie w trakcie pobierania, po trzech startach przestanie
  // probowac. Bez udanego zapisu nie zaczynamy - to on chroni przed petla.
  diag::otaZapisRozpocznijProbe(otaZapis, otaP.klucz);
  if (!otaZapisZachowaj()) {
    diag::otaZapisOddajProbe(otaZapis);
    otaBylaProba = true;
    otaKoniecProbyMs = millis();
    Serial.println(F("tag=OTA-PANEL lvl=ERR zdarzenie=brak_zapisu_nvs msg=\"nie udalo sie zapisac stanu aktualizacji - nie zaczynam\""));
    return;
  }
  Serial.printf("tag=OTA-PANEL zdarzenie=start wersja_docelowa=%s oferta=%s proba=%u/%u ram=%lu ram_blok=%lu\n",
                o.tag, otaP.klucz, (unsigned)otaZapis.prob, (unsigned)diag::OTA_MAX_PROB,
                (unsigned long)ramWolny(), (unsigned long)ramBlok());
  otaP.trwa = true;
  trwaAktualizacjaOtaPanelu = true;
  if (!hasloMiesciSie) {
    // Haslo OTA po zakodowaniu do adresu ma ponad 90 znakow - z tym firmware'em nie zadziala nigdy.
    otaProbaNieudana(diag::OTA_POWOD_HASLO, true, "haslo OTA za dlugie na adres zapytania");
    return;
  }
  snprintf(sciezka, sizeof(sciezka), "/panel-ota.bin?token=%s", haslo);
  lvgl_dashboard_poc_ota(0, "Laczenie z centrala...");
  if (!otaFetch.startBinarnie(sciezka, otaOdbiorcaBajtow, diag::OTA_CISZA_MS, diag::OTA_CALOSC_MS)) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, otaFetch.ostatniBlad());
  }
}

// Raz na sekunde: co zrobic z tym, co centrala podaje w /api/status.
static void otaKrokDecyzji(unsigned long teraz) {
  static const char *poprzedniPowod = "brak_oferty";   // zwykly stan po starcie - bez wpisu w logu
  diag::OtaOferta o;
  o.dostepna = stan.panelOtaDostepna;
  o.nowaCentrala = diag::otaObrazPoprawny(stan.panelOtaObraz);
  o.nr = stan.panelOtaNr;
  o.obraz = stan.panelOtaObraz;
  o.tag = stan.panelOtaWersja;

  // Najpierw bez pomiarow: jesli wynik nie zalezy od warunkow (brak oferty,
  // ten sam program, oferta juz wgrana albo odrzucona), nie ma po co mierzyc
  // baterii co sekunde - starsza centrala potrafi trzymac oferte bez konca.
  diag::OtaWarunki w;
  memset(&w, 0, sizeof(w));
  w.daneSwieze = true;
  w.wifi = true;
  w.odStartuMs = diag::OTA_PO_STARCIE_MS;
  w.bezDotykuMs = diag::OTA_BEZ_DOTYKU_MS;
  w.zasilanieUsb = true;
  w.ramWolny = diag::OTA_RAM_MIN_B;
  w.ramBlok = diag::OTA_BLOK_MIN_B;
  diag::OtaDecyzja d = diag::otaDecyzja(o, otaObrazWlasny, otaZapis, w);
  if (d.akcja == diag::OTA_AKCJA_START) {
    const float napiecie = bateriaOdczytajNapiecie();
    w.alarm = stan.dymAlarm || stan.alarmOgrzewanie || stan.alarmPanel;
    w.daneSwieze = !swiezoscStatusu.saNieaktualne(PROG_NIEAKTUALNYCH_DANYCH_MS);
    w.wifi = (WiFi.status() == WL_CONNECTED);
    w.siecZajeta = wykresyFetch.wTrakcie() || komendaFetch.wTrakcie();
    w.odStartuMs = (uint32_t)teraz;
    w.bezDotykuMs = lvgl_dashboard_poc_bez_dotyku_ms();
    w.bateriaJest = diag::bateriaObecna(napiecie);
    w.zasilanieUsb = zasilanieUSB();
    w.bateriaProcent = w.bateriaJest ? bateriaNapiecieNaProcent(napiecie) : 0;
    w.ramWolny = ramWolny();
    w.ramBlok = ramBlok();
    w.bylaProba = otaBylaProba;
    w.odOstatniejProbyMs = (uint32_t)(teraz - otaKoniecProbyMs);
    d = diag::otaDecyzja(o, otaObrazWlasny, otaZapis, w);
  }

  // To, co pojdzie do centrali w nastepnych zapytaniach o status. Tylko
  // centrala 3.58.6+ (podaje identyfikator pliku) rozumie te parametry.
  otaZglosOdmowe = (d.akcja == diag::OTA_AKCJA_ODMOW && o.nowaCentrala);
  otaOdmowaNr = o.nr;
  otaOdmowaPowod = d.powodOdmowy;
  snprintf(otaCzekaPowod, sizeof(otaCzekaPowod), "%s", (d.akcja == diag::OTA_AKCJA_CZEKAJ && o.nowaCentrala) ? d.powod : "");

  // Jedna linia logu przy kazdej ZMIANIE decyzji, nie co sekunde. Teksty
  // powodow to stale z diagnostyka_panelu.h, wiec wskaznik mozna zapamietac.
  if (strcmp(d.powod, poprzedniPowod) != 0) {
    poprzedniPowod = d.powod;
    if (d.akcja != diag::OTA_AKCJA_START) {
      Serial.printf("tag=OTA-PANEL zdarzenie=decyzja akcja=%s powod=%s oferta=%s nr=%lu ram=%lu ram_blok=%lu\n",
                    d.akcja == diag::OTA_AKCJA_NIC ? "nic" : d.akcja == diag::OTA_AKCJA_CZEKAJ ? "czekaj"
                    : d.akcja == diag::OTA_AKCJA_POMIN ? "pomin" : "odmow",
                    d.powod, o.dostepna ? o.tag : "-", (unsigned long)o.nr, (unsigned long)ramWolny(), (unsigned long)ramBlok());
    }
  }
  if (d.akcja == diag::OTA_AKCJA_START) otaZacznij(o);
}

// Wolane w kazdym obrocie loop() (galaz LVGL z centrala).
void otaKrok() {
  if (otaP.trwa) {
    otaKrokPobierania();
    return;
  }
  if (otaObrazWlasny[0] == '\0') return;   // bez identyfikatora wlasnego programu panel nie aktualizuje sie przez centrale
  const unsigned long teraz = millis();
  static unsigned long ostatniaDecyzjaMs = 0;
  if (teraz - ostatniaDecyzjaMs < 1000) return;
  ostatniaDecyzjaMs = teraz;
  otaKrokDecyzji(teraz);
}

unsigned long ostatniStatus = 0, ostatnieUstawienia = 0;

// [RESET-DIAG, ta sesja] Mapowanie esp_reset_reason_t na krotki, czytelny
// tekst. Odczytywane jako PIERWSZA rzecz w setup() (patrz nizej) — samo
// odczytanie stanu chipu nie zalezy od niczego innego, wiec robimy to
// zanim cokolwiek innego (WDT, WiFi, TFT) zdazy cokolwiek popsuc. Cel
// z TEMATU B (OTA_PANEL_CALOSC.md): dzis ESP_RST_BROWNOUT i
// ESP_RST_TASK_WDT objawiaja sie identycznie ("panel sie zresetowal"),
// a to dwie zupelnie rozne diagnozy — brownout to problem zasilania
// (kondensator/bateria), task_wdt to zawieszony task (np. podczas OTA,
// patrz Ustalenie #6). Dwa NORMALNE powody dla tego urzadzenia:
// ESP_RST_DEEPSLEEP (budzenie przyciskiem/timerem) i ESP_RST_SW
// (ESP.restart() po udanym OTA panelu, patrz otaPoPobraniu() wyzej)
// — logika alarmujaca po stronie centrali (main_centrala.cpp,
// handler /api/status) traktuje oba jako nie wymagajace alertu.
// [v1.2.1] Drugie wywolanie ESP.restart() jest w panelAutorestart() (brak
// danych z centrali) - taki restart panel zglasza centrali osobnym powodem
// "auto_...", patrz autorestartOdczytajPrzyStarcie() nizej.
const char* powodResetuTekst(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "poweron";     // pelna utrata zasilania (bateria padla / wymieniona)
    case ESP_RST_EXT:       return "ext";          // reset pinem EN
    case ESP_RST_SW:        return "sw";           // ESP.restart() z kodu (np. po udanym OTA)
    case ESP_RST_PANIC:     return "panic";        // wyjatek/crash (nie WDT)
    case ESP_RST_INT_WDT:   return "int_wdt";      // watchdog przerwan (rozne od task_wdt)
    case ESP_RST_TASK_WDT:  return "task_wdt";     // TEN watchdog skonfigurowalismy wyzej — zawieszony task
    case ESP_RST_WDT:       return "wdt";          // inny/ogolny watchdog (RTC WDT itp.)
    case ESP_RST_DEEPSLEEP: return "deepsleep";    // NORMALNE wybudzenie — jedyny oczekiwany powod
    case ESP_RST_BROWNOUT:  return "brownout";     // spadek napiecia ponizej progu — TEMAT B
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
  }
}

// [v1.2.1 RATUNEK] Wolane raz, na poczatku setup(). Sprawdza, czy ten start
// nastapil po SAMOCZYNNYM restarcie (panelAutorestart()). Jesli tak, powod
// zglaszany centrali w /api/status to nie "sw", tylko token z przyczyna, np.
// "auto_milczy_r0_n1_t45_h61_b31_el0" (diag::tokenAutorestartu()) - centrala
// wpisuje go do swojego logu (tag=RESET-DIAG zdarzenie=nietypowy_restart_panelu)
// i wysyla o nim wiadomosc, tak jak o kazdym restarcie innym niz "sw" i
// "deepsleep". Restart po udanym OTA zostaje "sw" (oczekuje == 0).
// Centrala oglasza powod przy kazdej ZMIANIE zglaszanej wartosci, dlatego:
// - token idzie tylko do pierwszej odpowiedzi centrali, potem panel wraca do
//   "sw" (obslugaStatusFetch()) - inaczej centrala po WLASNYM restarcie
//   oglaszalaby od nowa stary restart panelu;
// - nie kazdy restart z serii jest zglaszany (diag::zglosicRestart()).
void autorestartOdczytajPrzyStarcie(esp_reset_reason_t powod) {
  if (!rtcAutorestart.wazna()) {
    rtcAutorestart.wyczysc();   // po wlaczeniu zasilania w tej pamieci sa przypadkowe wartosci
    return;
  }
  if (powod == ESP_RST_SW && rtcAutorestart.oczekuje) {
    startPoAutorestarcie = true;
    char token[diag::TOKEN_AUTORESTARTU_MAKS];
    diag::tokenAutorestartu(token, sizeof(token), rtcAutorestart);
    // Nie kazdy restart z serii jest zglaszany (diag::zglosicRestart()) -
    // pominiety idzie do centrali jako zwykle "sw", ktorego ona nie oglasza.
    autorestartDoZgloszenia = diag::zglosicRestart(rtcAutorestart.seria, rtcAutorestart.zgloszonyNr);
    if (autorestartDoZgloszenia) globalnyPowodResetu = token;
    Serial.printf("tag=PANEL lvl=WARN zdarzenie=start_po_autorestarcie nr_z_rzedu=%u w_serii=%u zgloszony_nr=%u lacznie=%u przyczyna=%s bez_danych_s=%u "
                  "rozl_kod=%u rozl_nazwa=%s heap_kb=%u blok_kb=%u zglaszam_centrali=%s\n",
                  (unsigned)rtcAutorestart.kolejnych, (unsigned)rtcAutorestart.seria, (unsigned)rtcAutorestart.zgloszonyNr,
                  (unsigned)rtcAutorestart.lacznie,
                  diag::przyczynaSkrot((diag::Przyczyna)rtcAutorestart.przyczyna), (unsigned)rtcAutorestart.bezDanychS,
                  (unsigned)rtcAutorestart.kodRozlaczenia, diag::nazwaKoduRozlaczenia(rtcAutorestart.kodRozlaczenia),
                  (unsigned)rtcAutorestart.heapKb, (unsigned)rtcAutorestart.blokKb, globalnyPowodResetu.c_str());
  } else if (powod != ESP_RST_SW) {
    rtcAutorestart.kolejnych = 0;   // restart z innego powodu (przycisk, watchdog, zasilanie) - licz od nowa
  }
  rtcAutorestart.oczekuje = 0;
  rtcAutorestart.zapieczetuj();
}

// [v1.2.1 RATUNEK] Po samoczynnym restarcie: ostatni znany stan alarmow
// (zapisany przez panelAutorestart()) wraca do `stan` i do brzeczyka, razem z
// tym, czy uzytkownik go wyciszyl. Bez tego restart z braku danych w trakcie
// alarmu po cichu wylaczalby dzwiek. Ekran alarmu przywraca osobno
// lvgl_dashboard_poc_przywroc_alarm() - dopiero gdy LVGL jest juz zbudowany.
// Pierwszy poprawny status z centrali i tak ustawia wszystko od nowa.
void alarmPrzywrocPoAutorestarcie() {
  if (!startPoAutorestarcie) return;
  const uint8_t a = rtcAutorestart.alarmy;
  stan.dymAlarm = diag::alarmyDym(a);
  stan.alarmOgrzewanie = diag::alarmyOgrzewanie(a);
  stan.alarmPanel = diag::alarmyPanel(a);
  g_brzeczykWyciszony = diag::alarmyWyciszony(a);
  g_brzeczykWyciszonyTyp = (BrzeczykAlarmTyp)diag::alarmyWyciszonyTyp(a);
  if (!diag::alarmyJakikolwiek(a)) return;
  Serial.printf("tag=PANEL lvl=WARN zdarzenie=alarm_sprzed_restartu dym=%d ogrzewanie=%d panel=%d wyciszony=%d "
                "msg=\"ostatni znany stan alarmu - obowiazuje do pierwszych danych z centrali\"\n",
                (int)stan.dymAlarm, (int)stan.alarmOgrzewanie, (int)stan.alarmPanel, (int)g_brzeczykWyciszony);
  brzeczykOdswiezZAlarmow(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel);
}

void setup() {
  // [FIX, ta sesja - PISKNIECIE BRZECZYKA PRZY WLACZENIU ZASILANIA] Dopoki
  // pin nie zostanie jawnie skonfigurowany, jest floating (Hi-Z) - dziala
  // jak antena na szum elektryczny (rozruch WiFi/TFT/LVGL/reszty sprzetu),
  // ktory pasywny piezo potrafi slyszalnie "zlapac" jako trzask/pisk.
  // Docelowa PELNA inicjalizacja (ledcAttach+ledcWriteTone) jest w
  // brzeczykZainicjuj(), wolanej pozniej w tej funkcji - ALE dopiero PO
  // tft.init()/kalibracji dotyku/LVGL init, czyli pin floatowalby przez
  // caly ten czas (realnie liczony w sekundach, nie mikrosekundach - stad
  // "piszczy przez pewien moment"). Te 2 linie na samej gorze setup(),
  // PRZED wszystkim innym (nawet Serial.begin()), zamykaja pin na LOW w
  // ciagu pojedynczych mikrosekund od startu programu - to maksimum tego,
  // co da sie zrobic programowo. Fragment SPRZED setup() (bootloader ROM
  // ESP32, dziala zanim jakikolwiek kod Arduino w ogole ruszy) nie jest pod
  // kontrola tego kodu - jesli po tej zmianie pisk nadal sie sluchowo
  // slyszy (bedzie duzo krotszy), jedyny dalszy krok to zewnetrzny
  // rezystor podciagajacy w dol (10-100k, pin->GND) na linii IO buzzera,
  // fizycznie trzymajacy ja na LOW nawet zanim jakikolwiek kod wystartuje.
  pinMode(BRZECZYK_PIN, OUTPUT);
  digitalWrite(BRZECZYK_PIN, LOW);

  Serial.begin(115200);
  delay(300);
  Serial.println("=== WYSWIETLACZ CO - start ===");

  // [RESET-DIAG, ta sesja] Odczyt PRZED wszystkim innym (WDT, WiFi, TFT) —
  // to czysty odczyt stanu chipu, nic od niczego innego nie zalezy, a
  // chcemy znac powod restartu nawet gdyby cos ponizej samo padlo.
  globalnyPowodResetu = powodResetuTekst(esp_reset_reason());
  autorestartOdczytajPrzyStarcie(esp_reset_reason());   // [v1.2.1 RATUNEK] po samoczynnym restarcie: "auto_..." zamiast "sw"
  otaPrzyStarcie();   // [v1.4.0 OTA] identyfikator wlasnego programu i zapis o ostatniej ofercie - przed pierwszym zapytaniem o status
  Serial.printf("tag=RESET-DIAG zdarzenie=start powod=%s\n", globalnyPowodResetu.c_str());
  if (globalnyPowodResetu != "deepsleep") {
    // Jedyny NORMALNY powod dla tego urzadzenia to wybudzenie z deep
    // sleep — wszystko inne jest warte podniesionego glosu w logu.
    Serial.printf("tag=RESET-DIAG lvl=WARN zdarzenie=nietypowy_restart powod=%s\n", globalnyPowodResetu.c_str());
  }

  // [FIX-WDT] Task Watchdog Timer — panel dotad NIE MIAL zadnej jawnej
  // konfiguracji TWDT (w odroznieniu od main_centrala.cpp). Bez tego
  // domyslne zachowanie TWDT po timeoutcie to TYLKO log ostrzezenia, BEZ
  // paniki/restartu (patrz Ustalenie #6 w OTA_PANEL_CALOSC.md,
  // potwierdzone dokumentacja ESP-IDF: CONFIG_ESP_TASK_WDT_PANIC musi byc
  // jawnie wlaczone) — czyli realne zawieszenie panelu (np. w trakcie OTA,
  // patrz prompt #7) mogloby NIE dawac samoistnego restartu. Ten sam
  // wzorzec co w centrali: reconfigure (bo TWDT jest juz init domyslnie
  // przez framework) + add(NULL) na tasku setup()/loop() (loopTask).
  {
    esp_task_wdt_config_t wdtCfg = {
      .timeout_ms = 15000,   // ta sama wartosc co WDT_TIMEOUT_MS w centrali
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
    };
    esp_err_t wdtErr = esp_task_wdt_reconfigure(&wdtCfg);
    if (wdtErr != ESP_OK) {
      Serial.printf("tag=WDT lvl=ERR zdarzenie=reconfigure blad=%s\n", esp_err_to_name(wdtErr));
    }
    wdtErr = esp_task_wdt_add(NULL);
    if (wdtErr != ESP_OK) {
      Serial.printf("tag=WDT lvl=ERR zdarzenie=add_loopTask blad=%s\n", esp_err_to_name(wdtErr));
    }
    Serial.printf("tag=WDT zdarzenie=skonfigurowany timeout=15000ms panic=true obserwowany=loopTask\n");
  }

  // [ETAP 3b] Log powodu startu — odroznia normalny start (USB/reset) od
  // wybudzenia przyciskiem po deep sleep, przydatne przy diagnostyce.
  esp_sleep_wakeup_cause_t powod = esp_sleep_get_wakeup_cause();
  if (powod == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println(F("tag=ZASILANIE zdarzenie=wybudzenie zrodlo=przycisk"));
  } else {
    Serial.println(F("tag=ZASILANIE zdarzenie=start zrodlo=reset_lub_usb"));
  }

  pinMode(WAKE_BTN_PIN, INPUT_PULLUP); // ten sam pin uzywany do wykrywania puszczenia w idzSpac()
  pinMode(USB_DETECT_PIN, INPUT_PULLUP); // dodatkowe zabezpieczenie obok zewnetrznego 10k do 3V3 (patrz komentarz przy USB_DETECT_PIN) — nie przeszkadza, TP4056 i tak wygrywa (LOW) gdy aktywny

  // [BATERIA-POMIAR, ta sesja] Raz, globalnie - analogReadMilliVolts() w
  // bateriaOdczytajNapiecie() honoruje TYLKO to ustawienie (nie wersje
  // per-pin), patrz komentarz przy tej funkcji.
  analogSetAttenuation(ADC_11db);

  touchInicjalizujPiny();
  tft.init();
  tft.setRotation(1);   // 1 = landscape 480x320 (podmien na 3 jesli obraz odwrocony)
  tft.fillScreen(KOL_TLO);

  // [FIX-TFT-BL-HOLD, ta sesja] Odblokuj pin TFT_BL PRZED ledcAttach() -
  // po kazdym wybudzeniu z deep sleep pin nadal ma wlaczony "hold" z
  // idzSpac() (patrz gpio_hold_en() tam). Bez tego wywolania PWM nie
  // ruszyloby wcale, bo pin zostalby "zamrozony" na stale, takze po
  // kolejnych resetach. Bezpieczne do wywolania rowniez przy zwyklym
  // starcie (reset/USB), gdy hold nigdy nie byl wlaczony - wtedy to no-op.
  gpio_hold_dis((gpio_num_t)TFT_BL);

  ledcAttach(TFT_BL, 5000, 8);       // PWM 5kHz, 8-bit na podswietleniu
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Wczytaj zapisana jasnosc/
  // harmonogram PRZED pierwszym ustawJasnoscPodswietlenia() - startujemy od
  // razu z ostatnia znana jasnoscia DZIENNA usera (nie na sztywno 100%).
  // Godzina jest nieznana az do 1. pobierzStatus()/symOdswiezDoLvgl() nizej
  // (ta plytka nie ma wlasnego RTC) - wyswietlaczHarmonogramOdswiez() wolane
  // stamtad skoryguje na jasnosc NOCNA w te kilkadziesiat ms, jesli akurat
  // trwa okno przyciemnienia.
  wyswietlaczWczytajZPamieci();
  ustawJasnoscPodswietlenia(jasnoscDziennaWartosc);
  // [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Czysto sprzetowa
  // inicjalizacja (PWM na pinie), wiec JEDNO wywolanie tutaj wystarcza dla
  // obu galezi ponizej (symulacja/prawdziwa centrala) - w odroznieniu od
  // rejestracji callbackow WYSWIETLACZ, tu nie ma niczego do zarejestrowania
  // osobno w kazdej galezi.
  brzeczykZainicjuj();
  alarmPrzywrocPoAutorestarcie();   // [v1.2.1 RATUNEK] alarm trwajacy przed samoczynnym restartem gra dalej

  // [FIX - dotyk nie dzialal w LVGL_POC_TEST] Kalibracja MUSI byc zrobiona
  // przed uzyciem dotykOdczytaj() (ktorego uzywa lvgl_touch_read() w
  // lvgl_port.cpp) - inaczej kalDotyku zostaje z zerami, dotykOdczytaj()
  // widzi "zdegenerowana" kalibracje (roznica raw < 50) i ZAWSZE zwraca
  // false, wiec LVGL nigdy nie widzi zadnego dotyku. Wczesniej to wywolanie
  // bylo PO bloku #if LVGL_POC_TEST (ktory konczy sie "return;"), wiec w
  // trybie testowym nigdy sie nie wykonywalo. Przeniesione tutaj - dziala
  // teraz w obu trybach. Jesli kalibracja jest juz zapisana w Preferences
  // (namespace "dotyk"), to tylko szybki odczyt, bez interakcji.
  kalibrujDotykJesliTrzeba();

#if LVGL_POC_TEST
  // [LVGL - KROK 4/5] Sciezka testowa — STARY dashboard (rysujDashboard/
  // rysujAktualnyEkran, rysowanie recznymi pikselami) jest nadal POMINIETY,
  // zeby dwa silniki rysujace nie mieszaly sie na tym samym TFT.
#if LVGL_POC_SYMULACJA
  // [LVGL-SYMULACJA] Pracujesz sama plytka wyswietlacza, bez centrali obok -
  // zero WiFi/HTTP, dashboard dostaje dane z symOdswiezDoLvgl() (patrz wyzej).
  Serial.println(F("=== LVGL_POC_TEST=1, LVGL_POC_SYMULACJA=1: praca offline, bez centrali ==="));
  lvgl_port_init();
  lvgl_dashboard_poc_init();
  lvgl_dashboard_poc_ustaw_callback_pompy(symPompaAkcja);
  lvgl_dashboard_poc_ustaw_callback_wyboru(symWyboraAkcja);
  // [POPRAWKA, ta sesja - panel SERWO v2] Brakujaca rejestracja - bez niej
  // panel SERWO wygladalby na dzialajacy (rysuje sie), ale kliknicia w
  // AUTO/RECZNY/BEZPIECZNA i slider nic by nie robily (g_serwoTrybCb/
  // g_serwoPozycjaCb zostalyby nullptr).
  lvgl_dashboard_poc_ustaw_callback_serwo_tryb(symServoTrybAkcja);
  lvgl_dashboard_poc_ustaw_callback_serwo_pozycja(symServoPozycjaAkcja);
  lvgl_dashboard_poc_ustaw_callback_serwo_ustawienia(symServoUstawieniaAkcja);
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY]
  lvgl_dashboard_poc_ustaw_callback_pompa_ustawienia(symPompaUstawieniaAkcja);
  lvgl_dashboard_poc_ustaw_callback_pompa_antystop(symPompaAntystopAkcja);
  // [ROZSZERZENIE, ta sesja - PANEL DYM]
  lvgl_dashboard_poc_ustaw_callback_dym_prog(symDymProgAkcja);
  lvgl_dashboard_poc_ustaw_callback_dym_ustawienia(symDymUstawieniaAkcja);
  // [ROZSZERZENIE, ta sesja - EKRAN WYKRESY] Bez tej rejestracji ekran
  // Wykresy w symulacji wygladalby na dzialajacy (nawigacja/chipy/tryb), ale
  // "POKAZ WYKRES" pokazywalby tylko "Brak polaczenia z fetchem (main.cpp)"
  // (g_wykresyFetchCb zostalby nullptr) - patrz analogiczny komentarz przy
  // rejestracji SERWO wyzej.
  lvgl_dashboard_poc_ustaw_callback_wykresy_fetch(symWykresyFetchAkcja);
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] W ODROZNIENIU od reszty
  // rejestracji w tym bloku - TE SAME 3 funkcje co w galezi "prawdziwej
  // centrali" ponizej (#else), zero osobnych symXxx odpowiednikow. Podswie-
  // tlenie tej plytki jest CALKOWICIE lokalne (NVS+PWM), wiec symulacja
  // centrali nie zmienia w nim niczego - patrz obszerny komentarz przy
  // wyswietlaczJasnoscWyslij() powyzej. Wywolanie _aktualizuj_ustawienia_
  // wyswietlacza() zasila panel ostatnia wartoscia wczytana z NVS w setup()
  // (wyswietlaczWczytajZPamieci() powyzej), zeby ekran nie pokazywal samych
  // domyslnych zer/placeholderow przy pierwszym otwarciu.
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_jasnosc(wyswietlaczJasnoscWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_harmonogram(wyswietlaczHarmonogramWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_ustawienia(wyswietlaczUstawieniaWyslij);
  // [EKRAN ALARMOWY, ta sesja] Ten sam powod co WYSWIETLACZ powyzej - brzeczyk
  // jest lokalny (GPIO tej plytki), wiec dziala identycznie w obu galeziach.
  lvgl_dashboard_poc_ustaw_callback_alarm_wycisz(alarmWyciszObsluga);
  lvgl_dashboard_poc_aktualizuj_ustawienia_wyswietlacza(jasnoscDziennaWartosc, jasnoscNocnaWartosc,
                                                         godzinaStartPrzyciemnienia, godzinaKoniecPrzyciemnienia,
                                                         harmonogramWlaczonyWartosc);
  symOdswiezDoLvgl();
#else
  // WiFi + pobierzStatus() DZIALAJA - realna integracja z /api/status i
  // /api/pompa-manual centrali (patrz pompaManualWyslij() wyzej).
  Serial.println(F("=== LVGL_POC_TEST=1, LVGL_POC_SYMULACJA=0: stary dashboard TFT pominiety, WiFi/API dziala ==="));
  lvgl_port_init();
  lvgl_dashboard_poc_init();
  // [v1.2.2 PAMIEC] Ile RAM-u zostaje po zbudowaniu ekranow, zanim wystartuje
  // WiFi (ktore bierze kolejne kilkadziesiat kB).
  Serial.printf("tag=PAMIEC zdarzenie=po_ekranach ram=%lu ram_blok=%lu\n", (unsigned long)ramWolny(), (unsigned long)ramBlok());
  lvgl_dashboard_poc_ustaw_callback_pompy(pompaManualWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyboru(pompaWyborWyslij);
  // [POPRAWKA, ta sesja - panel SERWO v2] Brakujaca rejestracja (ta sama
  // przyczyna co w galezi symulacji wyzej) - podpina panel SERWO pod
  // serwoTrybWyslij()/serwoPozycjaWyslij() (patrz definicje przy
  // pompaWyborWyslij()), ktore wolaja juz istniejace
  // wyslijTrybPracy()/wyslijServoManualny().
  lvgl_dashboard_poc_ustaw_callback_serwo_tryb(serwoTrybWyslij);
  lvgl_dashboard_poc_ustaw_callback_serwo_pozycja(serwoPozycjaWyslij);
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA]
  lvgl_dashboard_poc_ustaw_callback_serwo_ustawienia(serwoUstawienieWyslij);
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY]
  lvgl_dashboard_poc_ustaw_callback_pompa_ustawienia(pompaUstawienieWyslij);
  lvgl_dashboard_poc_ustaw_callback_pompa_antystop(pompaAntystopWyslij);
  // [ROZSZERZENIE, ta sesja - PANEL DYM]
  lvgl_dashboard_poc_ustaw_callback_dym_prog(dymProgWyslij);
  lvgl_dashboard_poc_ustaw_callback_dym_ustawienia(dymUstawienieWyslij);
  // [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA]
  lvgl_dashboard_poc_ustaw_callback_alarm_temp_prog(alarmTempProgWyslij);
  // [ROZSZERZENIE, ta sesja - EKRAN WYKRESY; v1.3.0] Patrz wykresyFetchStart():
  // ekran Wykresy prosi tedy o dane, a pobieranie popycha obslugaWykresyFetch()
  // w loop().
  lvgl_dashboard_poc_ustaw_callback_wykresy_fetch(wykresyFetchStart);
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Ten sam wzorzec co galaz
  // symulacji wyzej (patrz komentarz tam) - TE SAME 3 funkcje w obu
  // galeziach, bo podswietlenie tej plytki nie zalezy od tego, czy centrala
  // jest akurat podlaczona.
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_jasnosc(wyswietlaczJasnoscWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_harmonogram(wyswietlaczHarmonogramWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_ustawienia(wyswietlaczUstawieniaWyslij);
  // [EKRAN ALARMOWY, ta sesja] Ten sam powod co WYSWIETLACZ powyzej - brzeczyk
  // jest lokalny (GPIO tej plytki), wiec dziala identycznie w obu galeziach.
  lvgl_dashboard_poc_ustaw_callback_alarm_wycisz(alarmWyciszObsluga);
  lvgl_dashboard_poc_aktualizuj_ustawienia_wyswietlacza(jasnoscDziennaWartosc, jasnoscNocnaWartosc,
                                                         godzinaStartPrzyciemnienia, godzinaKoniecPrzyciemnienia,
                                                         harmonogramWlaczonyWartosc);
  // [v1.2.1 RATUNEK] Po samoczynnym restarcie w trakcie alarmu: ekran alarmu
  // (albo dzwonek w naglowku, jesli byl zamkniety) i czerwone ramki kafelkow
  // wracaja, zanim przyjda pierwsze dane.
  if (startPoAutorestarcie && diag::alarmyJakikolwiek(rtcAutorestart.alarmy)) {
    lvgl_dashboard_poc_przywroc_alarm(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel,
                                      diag::alarmyOdrzucony(rtcAutorestart.alarmy));
  }

  polaczWifi();
  pobierzStatus();
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] W trybie LVGL nie ma
  // juz starego EKRAN_USTAWIENIA, ktory wczesniej wolal pobierzUstawienia()
  // przy starcie (patrz galaz nie-LVGL nizej) - bez tego wywolania tutaj
  // ekran USTAWIENIA SERWA pokazywalby same zera az do pierwszej recznej
  // zmiany. Wola tez lvgl_dashboard_poc_aktualizuj_ustawienia_serwa()
  // (patrz koniec obslugaUstawieniaFetch()).
  pobierzUstawienia();
  // [SIEC-NIEBLOKUJACA, ta sesja] Oba GET-y wystartowaly, ale jeszcze sie
  // nie skonczyly - patrz obszerny komentarz przy poczekajNaPierwszeDane().
  poczekajNaPierwszeDane();
  // aktualizujSezon() zostaje - `aktualnySezon` jest nadal uzywany gdzie
  // indziej w tym pliku (np. ikona slonca na starym dashboardzie), tylko
  // przestal byc przekazywany do LVGL (patrz FIX w dashboard_lvgl_poc.h -
  // podzial sezonowy kafelkow LVGL zostal usuniety).
  aktualizujSezon();
  lvgl_dashboard_poc_aktualizuj(stan.polaczono, stan.tZewn, stan.tBojler,
                                 stan.tOgrz, stan.tOgrzSr, stan.tPanel,
                                 stan.pompa, stan.pompaOverrideMin,
                                 stan.dym, stan.dymAlarm,
                                 stan.tPokoj, stan.wilgotnosc, stan.cisnienie,
                                 stan.klapa, stan.syberka, (int)trybWyboru,
                                 (int)trybSerwaWartosc,
                                 stan.alarmOgrzewanie, stan.alarmPanel,
                                 stan.godzina, stan.minuta, stan.dzien, stan.miesiac, stan.rok,
                                 stan.rtcOk,
                                 stan.rssi,
                                 stan.internet,
                                 stan.deszczStan, stan.deszczMinuty);
  // [v1.2.2 PAMIEC] Stan po starcie: WiFi dziala, pierwsze dane odebrane.
  // To jest liczba, ktora mowi, czy panel ma zapas (kilkadziesiat kB), czy
  // pracuje na styk (log z 2026-11-12: ok. 8 kB).
  logBezPamieci("tag=PAMIEC zdarzenie=po_starcie ram=%lu ram_blok=%lu ram_min=%lu\n",
                (unsigned long)ramWolny(), (unsigned long)ramBlok(), (unsigned long)ramMin());
#endif
  ostatniStatus = millis();
  // [OTA-ROLLBACK] Tutaj, nie wczesniej: ekran juz narysowany (obie galezie
  // SYMULACJA=0/1 zbiegaja sie w tym miejscu), a w galezi SYMULACJA=0
  // pierwsza probka pobierzStatus() juz sie wykonala (patrz wyzej).
  // [v1.4.0 OTA] Po zatwierdzeniu: czy ostatnie wgranie sie przyjelo, czy
  // bootloader wrocil do poprzedniego programu. Samej aktualizacji setup() juz
  // nie zaczyna (do v1.3.0: sprawdzIWykonajOtaPanelu() w tym miejscu, z pustym
  // ekranem do konca pobierania) - robi to otaKrok() w loop().
  otaPoZatwierdzeniu(potwierdzPartycjeJesliCzeka());
  return; // nie wchodz w reszte setup() (stary dashboard) w trybie testowym - kalibracja dotyku juz zrobiona wyzej
#endif

  polaczWifi();

  pobierzStatus();
  pobierzUstawienia();
  // [SIEC-NIEBLOKUJACA, ta sesja] Patrz obszerny komentarz przy
  // poczekajNaPierwszeDane() - jedyne swiadome czekanie na siec w tym pliku,
  // wylacznie tutaj, raz, na starcie.
  poczekajNaPierwszeDane();
  aktualizujSezon(); // [LCD-SEZON] przed pierwszym rysujAktualnyEkran(), zeby dashboard od razu wystartowal we wlasciwym sezonie
  rysujAktualnyEkran();
  ostatniStatus = ostatnieUstawienia = millis();
  zarejestrujAktywnosc(); // start timera 5 min od momentu, gdy ekran jest juz gotowy
  // [OTA-ROLLBACK] Analogicznie do galezi LVGL_POC_TEST=1 wyzej — ekran juz
  // narysowany, pierwsza probka pobierzStatus() juz sie wykonala.
  otaPoZatwierdzeniu(potwierdzPartycjeJesliCzeka());   // [v1.4.0 OTA] jak w galezi LVGL_POC_TEST=1 wyzej; ta galaz nie wola otaKrok()
}

void loop() {
  // [FIX-WDT] Karmienie na poczatku KAZDEJ iteracji, przed jakimkolwiek
  // rozgalezieniem — loopTask jest teraz podpiety pod TWDT (patrz setup()),
  // wiec bez tego wywolania panel resetowalby sie co 15s w calkowicie
  // normalnej pracy, nie tylko przy realnym zawieszeniu.
  esp_task_wdt_reset();
  // [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Ten sam wzorzec co
  // obsluzDotyk() nizej - musi isc co obrot petli (nie tylko co
  // STATUS_INTERWAL_MS/co 1s symulacji jak reszta w tym bloku), zeby fazy
  // wzorca (odstepy rzedu setek ms) byly odmierzane dokladnie.
  obsluzBrzeczyk();
#if LVGL_POC_TEST
  // [LVGL - KROK 4/5] W trybie testowym loop() liczy animacje LVGL.
  lvgl_port_tick();

  // [BATERIA-POMIAR, ta sesja] Wspolne miejsce dla obu podgalezi
  // (SYMULACJA i realna, patrz #if nizej) - pomiar baterii jest lokalny,
  // niezalezny od tego czy dane z centrali sa symulowane czy prawdziwe.
  // Wlasny, wolniejszy interwal (% nie zmienia sie tak szybko jak status
  // co STATUS_INTERWAL_MS). Kolor "laduje sie" bierze wprost z
  // zasilanieUSB() - dziala niezaleznie od tego, ze reszta logiki snu
  // (idzSpac()/ACTIVITY_TIMEOUT) nie jest jeszcze wpieta w ta galaz LVGL
  // (patrz PLAN_BATERIA.md, sekcja 3 - osobna, juz istniejaca luka, nie
  // zwiazana z tym pomiarem).
  static const uint32_t BATERIA_INTERWAL_MS = 30000;
  static unsigned long ostatniaBateria = 0;
  unsigned long terazBat = millis();
  if (terazBat - ostatniaBateria >= BATERIA_INTERWAL_MS) {
    ostatniaBateria = terazBat;
    float napiecieBat = bateriaOdczytajNapiecie();
    // [v1.2.0 BATERIA-BRAK] Ponizej diag::BATERIA_PROG_OBECNOSCI_V (2,5 V) na
    // dzielniku nie ma ogniwa - panel pracuje z samego USB. v1.1.0 pokazywal
    // wtedy "0%" w kolorze "laduje sie" (log z 2026-11-10:
    // "napiecie=0.28V procent=0 ladowanie=1" co 30 s). Teraz: napis "USB" na
    // wskazniku, a w logu jedna linia przy zmianie stanu zamiast co 30 s.
    static int8_t poprzednioBateriaJest = -1;   // -1 = jeszcze nie mierzono
    const bool bateriaJest = diag::bateriaObecna(napiecieBat);
    if (bateriaJest) {
      int procentBat = bateriaNapiecieNaProcent(napiecieBat);
      Serial.printf("tag=BATERIA napiecie=%.2fV procent=%d ladowanie=%d\n",
                    napiecieBat, procentBat, (int)zasilanieUSB());
      lvgl_dashboard_poc_aktualizuj_bateria(procentBat, zasilanieUSB());
    } else {
      if (poprzednioBateriaJest != 0) {
        Serial.printf("tag=BATERIA zdarzenie=brak_ogniwa napiecie=%.2fV prog=%.1fV msg=\"zasilanie z USB, wskaznik pokazuje USB\"\n",
                      napiecieBat, diag::BATERIA_PROG_OBECNOSCI_V);
      }
      lvgl_dashboard_poc_aktualizuj_bateria(-1, false);
    }
    poprzednioBateriaJest = bateriaJest ? 1 : 0;
  }

#if LVGL_POC_SYMULACJA
  // [LVGL-SYMULACJA] Brak WiFi/centrali - zamiast pobierzStatus() co
  // STATUS_INTERWAL_MS, odswiezamy z symulacji co 1s (zeby countdown/arc na
  // kafelku POMPA faktycznie sie ruszaly w trybie recznym, tak jak w realu).
  static unsigned long symOstatnieOdswiezenie = 0;
  unsigned long terazSym = millis();
  if (terazSym - symOstatnieOdswiezenie >= 1000) {
    symOstatnieOdswiezenie = terazSym;
    symOdswiezDoLvgl();
  }
#else
  // Realna integracja - pilnuje WiFi + co STATUS_INTERWAL_MS odswieza dane
  // z centrali (obsluzDotyk()/sen celowo dalej pominiete - to jeszcze nie
  // docelowy main.cpp, tylko test integracji LVGL<->API na izolowanej sciezce).
  wifiReconnectNonBlocking();

  // [SIEC-NIEBLOKUJACA, ta sesja] Musza isc CO ITERACJE (nie tylko co
  // STATUS_INTERWAL_MS/USTAWIENIA_INTERWAL_MS ponizej) - to one sprawdzaja,
  // czy zadanie wystartowane w ktorejs POPRZEDNIEJ iteracji juz sie
  // zakonczylo (statusFetch/ustawieniaFetch/komendaFetch.poll() sa tanie,
  // pojedynczy select() z zerowym czekaniem gdy bezczynne). Patrz komentarze
  // przy tych funkcjach.
  obslugaStatusFetch();
  obslugaUstawieniaFetch();
  obslugaKomendaFetch();
  obslugaWykresyFetch();   // [v1.3.0 WYKRESY] historia dla ekranu Wykresy - po kawalku na obrot, bez blokowania
  otaKrok();               // [v1.4.0 OTA] aktualizacja przez centrale: decyzja raz na sekunde, pobieranie po kawalku na obrot

  // [v1.2.0 DANE-NIEAKTUALNE] Brak poprawnie odczytanego /api/status przez
  // PROG_NIEAKTUALNYCH_DANYCH_MS -> kafelki na "--" (raz na kazdy zanik).
  if (straznikSwiezosci.wlasniePrzeterminowane(swiezoscStatusu.saNieaktualne(PROG_NIEAKTUALNYCH_DANYCH_MS))) {
    panelOznaczDaneJakoNieaktualne();
  }
  // [v1.2.1 RATUNEK] Dluzej bez danych: ponowne polaczenie z siecia centrali,
  // a gdy to nie pomaga - restart panelu. Patrz panelRatunekKrok().
  panelRatunekKrok();
  // [v1.2.0 DIAG] Jedna linia "tag=PANEL" co 30 s - patrz panelDiagOkresowo().
  panelDiagOkresowo();
  panelPamiecKrok();   // [v1.2.2 PAMIEC] ostrzezenie w logu, gdy wolnego RAM-u jest malo

  unsigned long terazLvgl = millis();
  if (terazLvgl - ostatniStatus >= STATUS_INTERWAL_MS) {
    ostatniStatus = terazLvgl;
    pobierzStatus(); // tylko startuje GET - parsowanie+odswiezenie LVGL teraz w obslugaStatusFetch() wyzej
  }
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Analogiczny okresowy
  // odczyt /api/ustawienia-get co USTAWIENIA_INTERWAL_MS - ten sam interwal
  // co stary, nie-LVGL dashboard uzywa (patrz ponizej w tym pliku), zeby
  // ekran LVGL nie polegal wylacznie na wartosci z chwili startu, gdyby ktos
  // zmienil parametr ze starego panelu WWW rownolegle.
  if (terazLvgl - ostatnieUstawienia >= USTAWIENIA_INTERWAL_MS) {
    ostatnieUstawienia = terazLvgl;
    pobierzUstawienia(); // tylko startuje GET - patrz obslugaUstawieniaFetch() wyzej
  }
#endif
  delay(5); // LVGL nie potrzebuje petli szybszej niz ~200Hz, a to odciaza CPU/WDT
  return;
#endif

  obsluzDotyk();

  // [ETAP 5] Nieblokujace pilnowanie polaczenia WiFi + reconnect z rosnacym
  // opoznieniem — patrz wifiReconnectNonBlocking() wyzej.
  wifiReconnectNonBlocking();

  // [SIEC-NIEBLOKUJACA, ta sesja] Patrz komentarz w galezi LVGL wyzej -
  // dokladnie ten sam powod, wspolny dla obu galezi (httpGet/httpPost byly
  // wspoldzielone, wiec te trzy funkcje tez sa).
  obslugaStatusFetch();
  obslugaUstawieniaFetch();
  obslugaKomendaFetch();
  // [v1.2.0 DIAG] Ta galaz (stary dashboard TFT, LVGL_POC_TEST=0) nie jest
  // obecnie budowana; dla spojnosci dostaje ten sam log co galaz LVGL wyzej.
  // Samego przelaczania kafelkow na "--" tutaj nie ma.
  if (straznikSwiezosci.wlasniePrzeterminowane(swiezoscStatusu.saNieaktualne(PROG_NIEAKTUALNYCH_DANYCH_MS))) {
    panelOznaczDaneJakoNieaktualne();
  }
  panelRatunekKrok();   // [v1.2.1 RATUNEK] jak w galezi LVGL wyzej
  panelDiagOkresowo();
  panelPamiecKrok();    // [v1.2.2 PAMIEC] jak w galezi LVGL wyzej

  // [ETAP 3b] Brak interakcji przez ACTIVITY_TIMEOUT_MS -> usypiamy, CHYBA
  // ZE wykryte jest zasilanie USB — wtedy urzadzenie ma dzialac jak
  // podlaczone na stale (timer stale resetowany, nigdy nie usypia).
  if (zasilanieUSB()) {
    zarejestrujAktywnosc();
  } else if (millis() - ostatniaAktywnosc >= ACTIVITY_TIMEOUT_MS) {
    idzSpac();
  }

  unsigned long teraz = millis();
  if (teraz - ostatniStatus >= STATUS_INTERWAL_MS) {
    ostatniStatus = teraz;
    pobierzStatus(); // tylko startuje GET - reszta (sezon/rysowanie) w obsludze wyzej
  }
  if (teraz - ostatnieUstawienia >= USTAWIENIA_INTERWAL_MS) {
    ostatnieUstawienia = teraz;
    if (aktualnyEkran != EKRAN_USTAWIENIA) pobierzUstawienia();  // nie nadpisuj ekranu w trakcie edycji
  }
}