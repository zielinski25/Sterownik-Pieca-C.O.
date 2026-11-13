#pragma once
// ============================================================================
// HISTORIA ZMIAN (ten plik) — najnowsze na gorze, kategorie jak w CHANGELOG.md
// ============================================================================
// [2026-11-12] v1.4.0 Dodano: lvgl_dashboard_poc_ota(), ..._ota_widoczna() i
//   ..._bez_dotyku_ms() - nakladka "AKTUALIZACJA PANELU" i czas od ostatniego
//   dotyku ([v1.4.0 OTA]). Pelny opis: HISTORIA ZMIAN w main.cpp.
// [2026-11-12] v1.3.0 Zmieniono: most ekranu Wykresy ([v1.3.0 WYKRESY]).
//   WykresyFetchCb dostaje zakres (LVGL_WYKRESY_TRYB_*) zamiast bool;
//   lvgl_dashboard_poc_wykresy_strumien_koniec() dostaje wynik pobierania
//   (enum LvglWykresyWynik) i zwraca to, co trafilo na ekran. Dodano:
//   ..._wykresy_postep(), ..._wykresy_czeka_na_dane(),
//   ..._wykresy_statystyka(), ..._wykresy_strumien_zredukowany(),
//   ..._wykresy_wynik_nazwa(). Pelny opis: HISTORIA ZMIAN w main.cpp.
// [2026-11-12] v1.2.1 Dodano: lvgl_dashboard_poc_alarm_odrzucony() i
//   lvgl_dashboard_poc_przywroc_alarm() - stan alarmu ma przetrwac
//   samoczynny restart panelu ([v1.2.1 RATUNEK]). Znacznik w naglowku
//   (lvgl_dashboard_poc_aktualizuj_centrale()) pokazuje tez, czego panelowi
//   brakuje, gdy nie ma danych, i jest powtorzony na ekranie alarmu.
//   Pelny opis: HISTORIA ZMIAN w main.cpp.
// [2026-11-11] v1.2.0 Dodano: lvgl_dashboard_poc_aktualizuj_centrale() -
//   znacznik "z ktora centrala rozmawia panel" na srodku naglowka
//   ([v1.2.0 CENTRALA-ID]). Zmieniono: lvgl_dashboard_poc_aktualizuj_bateria()
//   z procent < 0 pokazuje "USB" (brak ogniwa, [v1.2.0 BATERIA-BRAK]).
//   Pelny opis: HISTORIA ZMIAN w main.cpp.
// [2026-09-05] Dodano: typedef AlarmWyciszCb + setter
//   lvgl_dashboard_poc_ustaw_callback_alarm_wycisz() dla nowego EKRANU
//   ALARMOWEGO (pelnoekranowa nakladka, pokazywana automatycznie przy
//   aktywnym alarmie dym/pozar lub przegrzanie PIEC CO/PANEL) - patrz
//   PLAN_EKRAN_ALARMOWY.md. Callback dotyczy WYLACZNIE przycisku WYCISZ
//   (brzeczyk, main.cpp, w 100% lokalnie - main.cpp NIE robi tu zadnego
//   HTTP do centrali, w odroznieniu od reszty callbackow w tym pliku).
//   Drugi przycisk ekranu (ZAMKNIJ) nie ma wlasnego callbacku - to czysto
//   lokalna sprawa dashboard_lvgl_poc.cpp. Caly ekran dotyczy TYLKO
//   dashboard_lvgl_poc.h/.cpp i main.cpp (brzeczykOdswiezZAlarmow) - patrz
//   HISTORIA ZMIAN w obu tych plikach oraz CHANGELOG.md [Unreleased].
// ============================================================================
// [FIX kompilacji, PARTIA 5e] Naglowek uzywa Arduino String (parametr
// lvgl_dashboard_poc_wykresy_dane_gotowe() nizej) - w main.cpp/.cpp plikach
// tego projektu Arduino.h byl dotad dostepny "przy okazji" (przez inne
// includy), ale dashboard_lvgl_poc.h jest wlaczany jako PIERWSZY, samodzielny
// naglowek (patrz #include "dashboard_lvgl_poc.h" na gorze
// dashboard_lvgl_poc.cpp) - bez jawnego #include tutaj kompilator nie wie
// jeszcze, czym jest 'String', gdy parsuje ten plik.
#include <Arduino.h>
// [LVGL - KROK 4/5, PROOF OF CONCEPT + INTEGRACJA API] Jeden rzad kafelkow
// zbudowany jako prawdziwe obiekty LVGL. Od [LVGL-API] ten plik NIE zna
// HTTPClient/WiFi/JSON - main.cpp mu je "wstrzykuje":
//   1) po kazdym udanym pobierzStatus() woła lvgl_dashboard_poc_aktualizuj()
//      z aktualnymi wartosciami (ZEWN/BOJLER/DYM sa czystym odczytem; POMPA
//      dodatkowo dostaje pompaOverrideMin z centrali, zeby countdown na
//      kafelku byl PRAWDZIWY, nie liczony lokalnie "na oko"),
//   2) raz, w setup(), rejestruje callback przez
//      lvgl_dashboard_poc_ustaw_callback_pompy() - to jest jedyna droga,
//      ktora ten plik ma do wyslania polecenia z powrotem do centrali
//      (POST /api/pompa-manual), analogicznie do wzorca servo-manual.
void lvgl_dashboard_poc_init();

// main.cpp woła to po KAZDYM udanym pobierzStatus() (czyli co
// STATUS_INTERWAL_MS, patrz loop()). polaczono=false -> kafelki pokazuja
// "--" zamiast ostatnich (mogacych byc juz nieaktualnymi) wartosci.
// pompaOverrideMin: 0 = AUTO (automatyka centrali decyduje), >0 = tyle
// minut zostalo do cichego powrotu do AUTO - w tym stanie pompaWlaczona
// mowi, czy to reczne WL czy reczne WYL.
// [ROZSZERZENIE tej sesji] Dolaczono tOgrz/tOgrzSr - kafelek PIEC CO/SR
// (dawny podglad OGRZ_TILE_PREVIEW) jest teraz STALYM elementem siatki, wiec
// potrzebuje prawdziwych danych zamiast statycznego demo-tekstu "45.0 C".
// main.cpp juz ma te wartosci w stan.tOgrz/stan.tOgrzSr (z pol "t_ogrz"/
// "t_ogrz_sr" w /api/status) - tylko nie byly dotad przekazywane dalej.
//
// [ROZSZERZENIE, ta sesja - Partia 2] Dolaczono 5 nowych kafelkow (KLAPA,
// SYBERKA, WILGOTNOSC, CISNIENIE, POKOJ). Poczatkowo mialy byc chowane w
// GRUPIE sezonowej razem z DYM/PIEC CO/SR - patrz FIX nizej, ten pomysl
// zostal porzucony.
//
// [ROZSZERZENIE, ta sesja - Partia 5] Dolaczono `tPanel` (odczyt z pola
// "t_panel" w /api/status, tak jak stan.tPanel w main.cpp) - zasila kafelek
// PANEL (odpowiednik KAF_PANEL ze starego dashboardu).
//
// [FIX tej sesji] Parametr `zima` zostal USUNIETY - caly podzial na grupe
// sezonowa "zima" (PIEC CO/SR, KLAPA, SYBERKA, DYM) i "lato" (PANEL) zostal
// zlikwidowany, bo wszystkie 11 kafelkow miesci sie razem w siatce 4x3 na
// jednym ekranie bez przewijania. Wszystkie kafelki sa teraz ZAWSZE
// widoczne, wiec main.cpp NIE musi juz przekazywac `aktualnySezon ==
// SEZON_ZIMA` - to jedyna zmiana w sygnaturze wzgledem poprzedniej wersji.
// [ROZSZERZENIE, ta sesja - strona POMPA/tryb pracy] Dolaczono `pompaWybor`
// (1=Trociniak/cykl czasowy, 2=Kopciuch/histereza temp., 3=Automatyczny -
// dokladnie te same wartosci co "wybor" w /api/status i trybWyboru w
// main.cpp, patrz komentarz [LCD-TRYBY] tam) - zasila ikone "aktualny tryb
// pracy" na nowej stronie POMPA (patrz pompa_kafelek_klikniety_cb w
// dashboard_lvgl_poc.cpp), zeby jednym rzutem oka bylo widac, ktora
// strategia jest teraz aktywna, bez wchodzenia do zakladki wyboru.
// [POPRAWKA, ta sesja - panel SERWO v2] `klapaReczny`/`syberkaReczny` (dwa
// niezalezne bool) ZASTAPIONE przez `trybSerwa` (1=Auto, 2=Reczny,
// 3=Bezpieczna) - patrz PLAN_PANEL_SERWO_UPGRADE.md czesc 3. Powod: centrala
// (main_centrala.cpp, `TrybSerwa`) ma JEDEN WSPOLNY tryb dla obu osi, nie
// dwa niezalezne przelaczniki - poprzednia wersja tego pliku zakladala model
// niezgodny z centrala. `trybSerwa` to dokladnie ta sama wartosc co pole
// "trybSerwa" w /api/status (main.cpp juz je parsuje jako
// `trybSerwaWartosc`, patrz main.cpp linia ~842) - main.cpp tylko przekazuje
// dalej, bez zadnej wlasnej logiki. `klapa`/`syberka` to nadal FAKTYCZNA
// pozycja 0-100 (w trybie 1/3 ustawiana przez centrale, w trybie 2 - przez
// usera z panelu) - bez zmian wzgledem poprzedniej wersji.
// [ROZSZERZENIE, ta sesja - alarmy PIEC CO / PANEL] Centrala od dawna liczy
// dwa dodatkowe alarmy przegrzania (main_centrala.cpp, progAlarmowyTemperatury,
// domyslnie 80 C) - "alarm_ogrzewanie" (temp. pieca C.O. >= progu) i
// "alarm_panel" (temp. kolektora slonecznego >= progu), oba juz obecne w
// /api/status i juz parsowane po stronie main.cpp (stan.alarmOgrzewanie/
// stan.alarmPanel - patrz main.cpp ok. linii 1071-1072, do tej pory uzywane
// TYLKO przez stary, nie-LVGL rysujDashboard()/znacznik()). Te dwa bool sa
// jedynym brakujacym kawalkiem, zeby kafelki PIEC CO i PANEL w LVGL tez
// pokazywaly alarm - main.cpp tylko przekazuje dalej, bez zadnej wlasnej
// logiki progowej (tak jak dymAlarm powyzej).
// [LVGL-DATA-CZAS, ta sesja] Dolaczono godzina/minuta/dzien/miesiac/rok/rtcOk
// - zasilaja nowy kafelek DATA I CZAS (ostatni wolny slot siatki, rzad 2 obok
// SERWO/DYM). Dokladnie te same wartosci co stan.godzina/minuta/dzien/
// miesiac/rok/rtcOk w main.cpp (z pol "godzina"/"minuta"/"dzien"/"miesiac"/
// "rok"/"rtc_ok" w /api/status, centrala = RTC sprzetowy DS1307) -
// main.cpp tylko przekazuje dalej, formatowanie napisu robi
// dashboard_lvgl_poc.cpp. rtcOk=false (brak/awaria RTC na centrali) ->
// kafelek pokazuje "--:--"/"--.--.----" zamiast mylacego "00:00".
// WiFi: RSSI zmierzone przez LCD dla AP centrali; -129 rozlaczenie,
// 0/-128 brak wiarygodnego pomiaru. Globus: internet_wan z centrali,
// -1 nieznany, 0 proba WAN nieudana, 1 proba udana.
void lvgl_dashboard_poc_aktualizuj_siec(bool polaczono, int rssi, int internet);
// [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Dolaczono deszczStan/deszczMinuty
// (main.cpp: stan.deszczStan/stan.deszczMinuty, parsowane z /api/status pol
// deszcz_stan/deszcz_min wystawionych przez centrale - v2.13.2). 0=brak opadu
// w widocznym oknie (kafelek ZEWN bez zmian - temperatura+ikona wg progow
// jak dotychczas), 1=nadciaga (deszczMinuty=za ile zacznie), 2=pada teraz
// (deszczMinuty=za ile skonczy, -1=nieznane w oknie 6h) - patrz
// zewn_ikona_odswiez() w dashboard_lvgl_poc.cpp co do tego, jak te 2
// wartosci zmieniaja ikone/etykiete kafelka ZEWN.
void lvgl_dashboard_poc_aktualizuj(bool polaczono, float tZewn, float tBojler,
                                    float tOgrz, float tOgrzSr, float tPanel,
                                    bool pompaWlaczona, int pompaOverrideMin,
                                    int dym, bool dymAlarm,
                                    float tPokoj, float wilgotnosc, float cisnienie,
                                    int klapa, int syberka, int pompaWybor,
                                    int trybSerwa,
                                    bool alarmOgrzewanie, bool alarmPanel,
                                    int godzina, int minuta, int dzien, int miesiac, int rok,
                                    bool rtcOk,
                                    int rssi,
                                    int internet,
                                    int deszczStan, int deszczMinuty);

// stan przekazany do callbacku: "auto" / "wl" / "wyl" - dokladnie takie
// wartosci parametru `stan`, jakich oczekuje POST /api/pompa-manual na
// centrali. main.cpp rejestruje tu funkcje, ktora robi httpPost(...) i,
// po sukcesie, od razu odswieza dane (patrz pompaManualWyslij() w main.cpp) -
// dashboard_lvgl_poc.cpp NIE zmienia nic lokalnie "na wiare" po kliknieciu,
// tylko czeka na kolejne wywolanie *_aktualizuj() z prawdziwym stanem.
typedef void (*PompaAkcjaCb)(const char *stan);
void lvgl_dashboard_poc_ustaw_callback_pompy(PompaAkcjaCb cb);

// [ROZSZERZENIE, ta sesja] Analogiczny callback dla WYBORU STRATEGII pracy
// pompy (Trociniak/Kopciuch/Automatyczny - patrz komentarz przy `pompaWybor`
// wyzej) - NIEZALEZNY od PompaAkcjaCb powyzej (ten dotyczy tylko recznego
// override AUTO/WL/WYL). main.cpp rejestruje tu funkcje, ktora woła
// wyslijTrybPracy("wybor", wartosc) (JUZ ISTNIEJACA funkcje - patrz
// main.cpp, do tej pory uzywana tylko przez stary, nie-LVGL EKRAN_TRYBY) i,
// po sukcesie, odswieza dane - dokladnie ten sam wzorzec co PompaAkcjaCb.
// wartosc: 1=Trociniak, 2=Kopciuch, 3=Automatyczny.
typedef void (*PompaWyborCb)(int wartosc);
void lvgl_dashboard_poc_ustaw_callback_wyboru(PompaWyborCb cb);

// [POPRAWKA, ta sesja - panel SERWO v2] Poprzednia wersja mialo JEDEN
// ServoAkcjaCb(id, tryb, pozycja), co zakladalo tryb NIEZALEZNY per serwo -
// niezgodne z centrala (patrz `trybSerwa` wyzej). Teraz DWA osobne
// callbacki, dokladnie odzwierciedlajace dwa osobne endpointy centrali:
//
// 1) ServoTrybCb - zmiana WSPOLNEGO trybu (Auto/Reczny/Bezpieczna),
//    `wartosc` 1-3. main.cpp rejestruje tu funkcje wolajaca JUZ ISTNIEJACA
//    wyslijTrybPracy("trybSerwa", wartosc) (do tej pory uzywana tylko przez
//    stary, nie-LVGL ekran trybow) - dokladnie ten sam wzorzec co
//    PompaWyborCb/wyslijTrybPracy("wybor", ...).
typedef void (*ServoTrybCb)(int wartosc);
void lvgl_dashboard_poc_ustaw_callback_serwo_tryb(ServoTrybCb cb);

// 2) ServoPozycjaCb - zmiana pozycji JEDNEJ osi (`id`: 0=klapa, 1=syberka),
//    sensowna TYLKO gdy trybSerwa==2 (panel sam pilnuje, zeby slider byl
//    widoczny wylacznie wtedy - patrz serwo_panel_tryb_odswiez() w
//    dashboard_lvgl_poc.cpp). `pozycja`: 0-100. main.cpp rejestruje tu
//    funkcje ktora mapuje id->os ("klapa"/"syberek" - UWAGA: centrala
//    oczekuje "syberek", NIE "syberka", patrz main_centrala.cpp linia
//    ~4777-4779) i wola JUZ ISTNIEJACA wyslijServoManualny(os, procent).
typedef void (*ServoPozycjaCb)(int id, int pozycja);
void lvgl_dashboard_poc_ustaw_callback_serwo_pozycja(ServoPozycjaCb cb);

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Centrala ma, OSOBNO od
// trybu/pozycji powyzej, calkowicie osobna galaz automatyki - 6 parametrow
// strojenia (patrz main_centrala.cpp, ustawParametrPieca()), do tej pory
// ustawianych WYLACZNIE przez stary webowy panel /api/ustawienia-set:
//   tempZadServo (0-100 C)  - temperatura zadana, wokol ktorej pracuje auto
//   histerServo   (0-50)    - histereza wokol zadanej (pasmo bez reakcji)
//   skokKlapy     (1-100 %) - wielkosc kroku klapy przy korekcie
//   skokSyberka   (1-100 %) - jw. dla syberka
//   odchylTemp    (1-50 C)  - prog odchylenia, powyzej ktorego krok x Mnoznik
//   mnoznik       (1-10)    - mnoznik kroku przy duzym odchyleniu
// Wszystkie 6 juz przychodza w /api/status (main.cpp je parsuje, patrz
// stan.tempZadServo itd.) - ta funkcja to jedyny brakujacy kawalek: podanie
// ich dalej do LVGL. WYDZIELONA z lvgl_dashboard_poc_aktualizuj() (a nie
// dopisana jako kolejne argumenty tamtej funkcji) CELOWO - to sa ustawienia
// strojenia, zmieniane rzadko z osobnego ekranu, nie telemetria odswiezana
// co STATUS_INTERWAL_MS; woalnie jej ZAWSZE, gdy wolane jest tez
// _aktualizuj() (main.cpp robi oba w jednym miejscu), zeby ekran USTAWIENIA
// SERWA nigdy nie pokazywal nieaktualnych wartosci po powrocie z innego
// ekranu.
void lvgl_dashboard_poc_aktualizuj_ustawienia_serwa(int tempZadServo, int histerServo,
                                                     int skokKlapy, int skokSyberka,
                                                     int odchylTemp, int mnoznik);

// Generyczny callback dla WSZYSTKICH 6 pol naraz - dokladnie ten sam
// ksztalt co main.cpp juz ma w wyslijTrybPracy(pole, wartosc) (uzywanej
// przez stary EKRAN_TRYBY i przez PompaWyborCb/ServoTrybCb powyzej), wiec
// main.cpp rejestruje tu funkcje ktora PO PROSTU woła
// wyslijTrybPracy(pole, wartosc) - zero nowej logiki HTTP. `pole` to
// dokladnie klucz z /api/ustawienia-set ("tempZadServo", "histerServo",
// "skokKlapy", "skokSyberka", "odchylTemp", "mnoznik").
typedef void (*ServoUstawieniaCb)(const char *pole, long wartosc);
void lvgl_dashboard_poc_ustaw_callback_serwo_ustawienia(ServoUstawieniaCb cb);

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Dokladnie ten sam wzorzec
// co ekran USTAWIENIA SERWA powyzej, tylko dla galezi automatyki pompy -
// wejscie zebatka (lv_ikona_zebatka) na GLOWNEJ podstronie panelu POMPA,
// symetrycznie do zebatki w panelu SERWO. Centrala ma (main_centrala.cpp,
// ustawParametrPieca()) 5 pol strojenia pompy, w dwoch grupach zaleznych od
// `pompaWybor` (patrz komentarz przy lvgl_dashboard_poc_aktualizuj() wyzej)
// plus jeden niezalezny wlacznik:
//   czasOn       (1-180 min)  - Wybor=1 (Trociniak): ile pompa PRACUJE w cyklu
//   czasOff      (1-180 min)  - Wybor=1 (Trociniak): ile pompa STOI w cyklu
//   tempOn       (0-100 C)    - Wybor=2 (Kopciuch): powyzej tej temp. pieca pompa startuje
//   tempOff      (0-100 C)    - Wybor=2 (Kopciuch): ponizej tej temp. pieca pompa sie zatrzymuje
//   antystopDni  (1-45 dni)   - niezalezne od Wybor: po ilu dniach bez ruchu wymuszony puls 45s
// Wszystkie 5 juz przychodza w /api/status (main.cpp musi je dopisac do
// POLA[] analogicznie do tempZadServo itd., patrz komentarz w main.cpp) -
// ta funkcja to jedyny brakujacy kawalek: podanie ich dalej do LVGL.
void lvgl_dashboard_poc_aktualizuj_ustawienia_pompy(int czasOn, int czasOff,
                                                     int tempOn, int tempOff,
                                                     int antystopDni);

// `antystopWlaczony` jest bool (glowny wylacznik funkcji antystop), NIE
// numeryczny jak reszta 5 pol powyzej - wlasny argument zamiast szostego
// slotu w tablicy spinboxow, bo na ekranie renderuje sie jako przycisk
// WL./WYL. (lv_btn - LV_USE_SWITCH jest wylaczony w lv_conf.h tego projektu,
// patrz dashboard_lvgl_poc.cpp), nie jako wiersz +/-. Osobna funkcja (nie dopisana do
// _aktualizuj_ustawienia_pompy powyzej) CELOWO, zeby main.cpp mogl przekazac
// ja z osobnego miejsca w kodzie bez przepisywania calego wywolania - w
// praktyce woalna zawsze razem z tamta (main.cpp robi oba w jednym miejscu,
// ten sam wzorzec co reszta ekranow ustawien).
void lvgl_dashboard_poc_aktualizuj_ustawienia_pompy_antystop(bool antystopWlaczony);

// [UI-BATERIA, ta sesja] Wlasna bateria TEJ plytki (dzielnik napiecia na
// ADC + TP4056), NIEZALEZNA od /api/status - to lokalny pomiar sprzetowy,
// nie dana z centrali, dlatego osobna funkcja/timer w main.cpp (wlasny,
// wolniejszy interwal), a nie kolejny parametr _aktualizuj() powyzej.
// `ladowanie` to wprost zasilanieUSB() z main.cpp (TP4056 CHRG/STDBY).
// Patrz PLAN_BATERIA.md.
//
// [v1.2.0 BATERIA-BRAK] `procent` < 0 = ogniwa NIE MA (napiecie na dzielniku
// ponizej diag::BATERIA_PROG_OBECNOSCI_V, patrz diagnostyka_panelu.h) -
// wskaznik pokazuje wtedy pusty, wyszarzony obrys z napisem "USB" zamiast
// "0%" w kolorze ladowania (tak wygladal v1.1.0 przy pracy z samego USB,
// nie do odroznienia od rozladowanego ogniwa na ladowarce).
void lvgl_dashboard_poc_aktualizuj_bateria(int procent, bool ladowanie);

// [v1.2.0 CENTRALA-ID] Krotki znacznik na srodku naglowka dashboardu: Z KTORA
// centrala rozmawia panel i czy wartosci sa pomiarem. Pusty napis (albo
// nullptr) = nic nie pokazuj - to normalny stan przy jednej, prawdziwej
// centrali bez symulacji. main.cpp ustawia (diag::znacznikCentrali()):
//   "SYMULATOR"         - odpowiada centrala-testowa-c3 (pole
//                         "symulator":true w /api/status),
//   "SYMULACJA: zewn"   - prawdziwa centrala podaje te czujniki jako
//                         symulowane (pola "symulacja_*":true) - ich
//                         wartosci na kafelkach NIE sa pomiarem,
//   "2 CENTRALE (..)"   - przy starcie w zasiegu byly 2 (lub wiecej) sieci o
//                         nazwie centrali; panel wybral najsilniejsza,
//   albo dwa z nich naraz. Zawsze w kolorze ostrzezenia - to informacja
//   "uwazaj, co czytasz", nie dekoracja.
// [v1.2.1 RATUNEK] Gdy panel NIE MA danych (kafelki "--"), main.cpp wpisuje
// tu powod i licznik (diag::znacznikBrakuDanych()):
//   "BRAK WIFI 14 s" / "CENTRALA MILCZY 14 s" / "ZLA ODPOWIEDZ 14 s",
//   a w ostatnich 10 s przed samoczynnym restartem "RESTART ZA 7 s: BRAK WIFI".
// [v1.2.1] Ten sam tekst pojawia sie tez na ekranie alarmu (nad przyciskami
// WYCISZ / ZAMKNIJ) - ekran alarmu zaslania naglowek dashboardu.
void lvgl_dashboard_poc_aktualizuj_centrale(const char *znacznik);

// [v1.2.1 RATUNEK] Alarm ma przetrwac samoczynny restart panelu (main.cpp,
// panelAutorestart()): panel restartuje sie, gdy NIE MA danych z centrali, a
// wtedy obowiazuje zasada z alarm_ekran_odswiez() - ostatni znany stan
// alarmu zostaje. Bez tych dwoch funkcji restart po cichu gasilby ekran
// alarmu i dzwonek w naglowku.
//  - lvgl_dashboard_poc_alarm_odrzucony(): na jakim poziomie alarmu
//    uzytkownik ostatnio kliknal ZAMKNIJ (0 = nie zamykal, 1 = przegrzanie,
//    2 = pozar/dym) - do zapamietania tuz przed restartem;
//  - lvgl_dashboard_poc_przywroc_alarm(): po restarcie, ZANIM przyjda
//    pierwsze dane - pokazuje ekran alarmu / dzwonek i ramki kafelkow tak,
//    jak przed restartem. Pierwszy poprawny status z centrali i tak ustawia
//    wszystko od nowa (lvgl_dashboard_poc_aktualizuj()).
int  lvgl_dashboard_poc_alarm_odrzucony();
void lvgl_dashboard_poc_przywroc_alarm(bool dymAlarm, bool alarmOgrzewanie, bool alarmPanel, int odrzuconyPoziom);

// [v1.4.0 OTA] Nakladka "AKTUALIZACJA PANELU" na wierzchu kazdego ekranu
// (warstwa lv_layer_top()): zaslania ekran i przejmuje dotyk na czas
// pobierania nowego firmware'u z centrali. Ekrany pod spodem zostaja takie,
// jakie byly - po schowaniu nakladki panel jest tam, gdzie byl.
//   procent 0..100: pokaz albo odswiez (pasek postepu, `opis` nad nim;
//                   opis == nullptr zostawia poprzedni tekst),
//   procent < 0:    schowaj i zwolnij pamiec nakladki.
// Do v1.3.0 pobieranie zatrzymywalo petle i ekran byl przez ten czas pusty.
void lvgl_dashboard_poc_ota(int procent, const char *opis);
// [v1.4.0 OTA] true = nakladka jest teraz na ekranie.
bool lvgl_dashboard_poc_ota_widoczna();
// [v1.4.0 OTA] Ile milisekund temu ktos ostatnio dotknal ekranu (panel nie
// zaczyna aktualizacji, gdy ktos wlasnie go uzywa).
uint32_t lvgl_dashboard_poc_bez_dotyku_ms();

// Generyczny callback dla 5 pol numerycznych - dokladnie ten sam ksztalt co
// ServoUstawieniaCb powyzej (main.cpp rejestruje funkcje ktora PO PROSTU
// woła wyslijTrybPracy(pole, wartosc), zero nowej logiki HTTP). `pole` to
// dokladnie klucz z /api/ustawienia-set ("czasOn", "czasOff", "tempOn",
// "tempOff", "antystopDni"). CELOWO osobny typ/callback od
// ServoUstawieniaCb (a nie ponowne uzycie tego samego wskaznika) - inna
// domena (pompa, nie serwo), pozwala main.cpp rejestrowac/podmieniac je
// niezaleznie, tak jak PompaWyborCb jest niezalezny od ServoTrybCb.
typedef void (*PompaUstawieniaCb)(const char *pole, long wartosc);
void lvgl_dashboard_poc_ustaw_callback_pompa_ustawienia(PompaUstawieniaCb cb);

// Przelacznik antystopWlaczony (0/1) wysylany OSOBNO od 5 pol numerycznych
// powyzej, ale tym samym mechanizmem co PompaWyborCb - `wartosc` to 0 albo 1,
// main.cpp rejestruje funkcje wolajaca wyslijTrybPracy("antystopWlaczony",
// wartosc).
typedef void (*PompaAntystopCb)(int wartosc);
void lvgl_dashboard_poc_ustaw_callback_pompa_antystop(PompaAntystopCb cb);

// [ROZSZERZENIE, ta sesja - PANEL DYM] Kafelek DYM staje sie klikalny i
// otwiera wlasny, pelnoekranowy panel (ten sam wzorzec co panele
// SERWO/POMPA) z dwoma czesciami:
//   1) suwak progu alarmu (progAlarmDym, 0-4095 na centrali, ale UI uzywa
//      STALEGO zakresu 0-1500 - patrz uzasadnienie w rozmowie: pelen zakres
//      ADC sprawilby, ze sensowne wartosci (typowo <700) zajmuja ~15%
//      szerokosci paska i trudno w nie trafic palcem). Tlo suwaka to 3
//      statyczne, rownej szerokosci prostokaty (zielony/zolty/czerwony) -
//      orientacyjne pasma "spokojnie/uwaga/alarm", NIE przeliczane
//      dynamicznie (w odroznieniu od glownego menu/dashboardu, gdzie
//      trojstanowa sygnalizacja MOZE byc przeliczana w locie wzgledem progu -
//      to jest inny, poprzedni watek UI, nie ten ekran). Sam lv_slider ma
//      przezroczyste MAIN/INDICATOR, widoczna jest tylko galka (KNOB) jako
//      wskaznik pozycji progu na tle trzech pasm.
//   2) 3 wiersze "spinbox" (ten sam wzorzec co USTAWIENIA SERWA/POMPY) dla
//      dymCzasOn/dymCzasOff/dymCzasStabilizacji (sekundy) - wszystkie juz
//      wystawione przez centrale w /api/status i ustawParametrPieca().
// Wszystkie 4 wartosci na JEDNYM ekranie (nie osobny "USTAWIENIA DYM" jak
// zebatka w SERWO/POMPA) - user: "pelna strona jak POMPA", bez posredniego
// ekranu ustawien, bo DYM ma tylko 4 pola razem, nie 5-6 jak tamte.
void lvgl_dashboard_poc_aktualizuj_ustawienia_dymu(int progAlarmDym, int dymCzasOn,
                                                    int dymCzasOff, int dymCzasStabilizacji);

// Prog alarmu (suwak) - wysylany NA ZWOLNIENIE galki (ten sam debounce co
// serwoSlider), `wartosc` 0-1500 (skala UI, patrz komentarz wyzej - main.cpp
// przekazuje ja dalej 1:1 do centrali jako "progAlarmDym", centrala i tak
// waliduje pelny zakres 0-4095).
typedef void (*DymProgCb)(int wartosc);
void lvgl_dashboard_poc_ustaw_callback_dym_prog(DymProgCb cb);

// 3 pola czasowe - jeden generyczny callback, dokladnie ten sam ksztalt co
// ServoUstawieniaCb/PompaUstawieniaCb (`pole` to klucz z /api/ustawienia-set:
// "dymCzasOn", "dymCzasOff", "dymCzasStabilizacji").
typedef void (*DymUstawieniaCb)(const char *pole, long wartosc);
void lvgl_dashboard_poc_ustaw_callback_dym_ustawienia(DymUstawieniaCb cb);

// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Kafelki PIEC CO i PANEL
// staja sie klikalne i otwieraja WSPOLNY panel (jeden ekran dla obu, bo
// centrala ma JEDEN wspolny prog "progAlarmowyTemperatury" dla obu alarmow
// przegrzania - patrz main_centrala.cpp, alarmOgrzewanieWlaczone/
// alarmPanelWlaczone porownuja do TEGO SAMEGO progu). Prosty suwak 0-100 C
// (ten sam zakres co progAlarmTemp w POLA[] - min=0, max=100, patrz
// main.cpp), analogiczny wzorzec do suwaka progu DYM powyzej, ale bez 3
// stref w tle (tu nie ma osobnego progu "uwaga", tylko jeden prog alarmu -
// patrz piec_alarm_odswiez()/panel_alarm_odswiez() w dashboard_lvgl_poc.cpp,
// oba maja tylko 2 stany OK/ALARM).
void lvgl_dashboard_poc_aktualizuj_ustawienia_alarmu_temp(int progAlarmTemp);

// Wysylany NA ZWOLNIENIE galki, `wartosc` 0-100 C - main.cpp rejestruje
// funkcje ktora woła JUZ ISTNIEJACA wyslijTrybPracy("progAlarmTemp",
// wartosc) (dokladnie ten sam wzorzec co dymProgWyslij/DymProgCb powyzej).
typedef void (*AlarmTempProgCb)(int wartosc);
void lvgl_dashboard_poc_ustaw_callback_alarm_temp_prog(AlarmTempProgCb cb);

// [EKRAN ALARMOWY, ta sesja] Pelnoekranowa nakladka pokazywana AUTOMATYCZNIE
// (nie klikiem w kafelek) przy aktywnym alarmie - patrz PLAN_EKRAN_ALARMOWY.md.
// Dwa przyciski: WYCISZ i ZAMKNIJ, bez trzeciego "WYLACZ" (ustalone w
// rozmowie - dwa wystarczaja). ZAMKNIJ chowa ekran lokalnie
// (dashboard_lvgl_poc.cpp), bez udzialu main.cpp. WYCISZ dotyczy WYLACZNIE
// brzeczyka (GPIO main.cpp, BEZ HTTP do centrali) - jedyny callback w tym
// pliku, ktory main.cpp MOZE bezpiecznie odzwierciedlic od razu w UI zamiast
// czekac na kolejne _aktualizuj(), bo nie ma tu zadnego zapytania sieciowego
// w tle (efekt jest natychmiastowy i pewny, nie spekulacyjny).
typedef void (*AlarmWyciszCb)();
void lvgl_dashboard_poc_ustaw_callback_alarm_wycisz(AlarmWyciszCb cb);

// [ROZSZERZENIE - PANEL WYSWIETLACZ, uzupelnienie brakujacej implementacji]
// Jasnosc podswietlenia (dzien/noc) + okno godzinowe przyciemnienia +
// wlacznik harmonogramu - kafelek WYSWIETLACZ na drugim pulpicie otwiera
// pelnoekranowy panel z 2 suwakami jasnosci, 2 polami godzin i
// przelacznikiem WL./WYL. W ODROZNIENIU od reszty callbackow w tym pliku,
// main.cpp NIE robi tu zadnego HTTP do centrali - kazda zmiana trafia
// prosto do NVS + PWM podswietlenia lokalnie (patrz wyswietlaczJasnoscWyslij()
// i sasiednie funkcje w main.cpp), efekt jest wiec natychmiastowy.
//
// `id`: 0=jasnoscDzien, 1=jasnoscNoc - ten sam ksztalt co ServoPozycjaCb
// dla klapa/syberek (jeden callback, docelowy element wybierany przez id).
typedef void (*WyswietlaczJasnoscCb)(int id, int procent, bool koncowa);
void lvgl_dashboard_poc_ustaw_callback_wyswietlacz_jasnosc(WyswietlaczJasnoscCb cb);

// Wlacznik/wylacznik calego harmonogramu (0/1) - gdy wylaczony, plytka
// uzywa jasnosci dziennej non-stop, pola godzin ponizej sa ignorowane
// (patrz wyswietlaczHarmonogramOdswiez() w main.cpp). Ten sam wzorzec co
// PompaAntystopCb/AlarmTempProgCb - pojedynczy int jako "wartosc".
typedef void (*WyswietlaczHarmonogramCb)(int wlaczony);
void lvgl_dashboard_poc_ustaw_callback_wyswietlacz_harmonogram(WyswietlaczHarmonogramCb cb);

// Generyczny callback dla 2 pol godzinowych - dokladnie ten sam ksztalt co
// DymUstawieniaCb/PompaUstawieniaCb/ServoUstawieniaCb. `pole` to klucz z
// main.cpp: "godzStart" albo "godzKoniec", `wartosc` 0-23.
typedef void (*WyswietlaczUstawieniaCb)(const char *pole, long wartosc);
void lvgl_dashboard_poc_ustaw_callback_wyswietlacz_ustawienia(WyswietlaczUstawieniaCb cb);

// Zasila caly panel jednym wywolaniem - wolane raz w setup() main.cpp zaraz
// po wczytaniu wartosci z NVS (wyswietlaczWczytajZPamieci()), zeby ekran
// przy pierwszym otwarciu pokazywal PRAWDZIWE zapisane wartosci, nie
// domyslne zera/placeholdery. `jasnoscDzien`/`jasnoscNoc` 0-100 (%),
// `godzinaStart`/`godzinaKoniec` 0-23, `harmonogramWlaczony` bool -
// dokladnie w tej kolejnosci main.cpp je przekazuje.
void lvgl_dashboard_poc_aktualizuj_ustawienia_wyswietlacza(int jasnoscDzien, int jasnoscNoc,
                                                            int godzinaStart, int godzinaKoniec,
                                                            bool harmonogramWlaczony);

// [PARTIA 5e - MOST DANYCH DLA EKRANU WYKRESY; przebudowany w v1.3.0]
// dashboard_lvgl_poc.cpp nie wie, JAK dane trafiaja z centrali - prosi o nie
// przez ponizszy callback, a main.cpp podaje mu odpowiedz linia po linii.
// Ten sam podzial obowiazkow co reszta panelu (np. PompaManualCb).
//
// [v1.3.0 WYKRESY] Dwie zmiany wzgledem v1.2.2:
//  1. Callback dostaje TRYB (godzinny/dzienny/tygodniowy), bo o zakres pyta
//     sie teraz centrale (GET /api/history/panel?tryb=...), a nie tylko o
//     "krotki" albo "dlugi" CSV.
//  2. Callback tylko STARTUJE pobieranie i od razu wraca. Linie przychodza w
//     kolejnych obrotach loop() (main.cpp, obslugaWykresyFetch()), a ekran
//     w tym czasie dziala. Uzytkownik moze wiec wyjsc z wykresu, zanim dane
//     przyjda - main.cpp pyta o to przez _wykresy_czeka_na_dane() i wtedy
//     pobieranie porzuca.
#define LVGL_WYKRESY_TRYB_GODZINY 0   // ostatnia godzina
#define LVGL_WYKRESY_TRYB_DZIEN   1   // ostatnie 24 h
#define LVGL_WYKRESY_TRYB_TYDZIEN 2   // ostatnie 7 dni
#define LVGL_WYKRESY_PUNKTOW      96  // tyle punktow miesci wykres; o tyle panel prosi centrale (pkt=)
typedef void (*WykresyFetchCb)(int tryb);
void lvgl_dashboard_poc_ustaw_callback_wykresy_fetch(WykresyFetchCb cb);

// Wynik pobierania. main.cpp podaje to, co wie warstwa sieci (OK albo jeden z
// bledow); _strumien_koniec() zwraca to, co ostatecznie widac na ekranie -
// np. OK moze stac sie PUSTO (centrala odpowiedziala, ale nie ma danych).
enum LvglWykresyWynik {
  LVGL_WYKRESY_OK = 0,           // wykres narysowany
  LVGL_WYKRESY_NIEPELNE,         // wykres narysowany z czesci danych (starsza centrala nie zdazyla wyslac wszystkiego)
  LVGL_WYKRESY_PUSTO,            // centrala nie ma jeszcze danych z tego zakresu
  LVGL_WYKRESY_ZA_MALO,          // jest mniej niz 2 punkty - nie ma z czego narysowac linii
  LVGL_WYKRESY_BRAK_WIFI,        // panel nie jest polaczony z siecia centrali
  LVGL_WYKRESY_BRAK_POLACZENIA,  // nie udalo sie polaczyc z centrala
  LVGL_WYKRESY_LIMIT_CZASU,      // centrala nie odpowiedziala albo nie zdazyla w czasie
  LVGL_WYKRESY_HTTP,             // centrala odpowiedziala bledem HTTP (kod w `szczegol`)
  LVGL_WYKRESY_URWANA,           // odpowiedz urwala sie przed koncem
  LVGL_WYKRESY_ZLA_ODPOWIEDZ,    // to, co przyszlo, nie jest odpowiedzia centrali
  LVGL_WYKRESY_BRAK_PAMIECI,     // panelowi zabraklo pamieci
  LVGL_WYKRESY_NIEAKTUALNE       // ekran juz na te dane nie czekal - nic nie pokazano
};
// Krotka nazwa wyniku do logu ("ok", "pusto", "limit_czasu", ...).
const char *lvgl_dashboard_poc_wykresy_wynik_nazwa(int wynik);

// Kolejnosc wywolan dla KAZDEGO pobrania (takze nieudanego):
//   1) _strumien_start(dlugi)        - `dlugi`: format wierszy STAREGO CSV,
//                                      gdyby centrala nie znala nowego adresu
//                                      (true = /api/history/long)
//   2) _strumien_linia(linia)        - raz na kazda linie odpowiedzi, bez '\n'.
//                                      Pierwsza linia "#panel-hist ..." znaczy:
//                                      punkty policzyla centrala (wiersze w
//                                      formacie /api/history/long, przerwy jako
//                                      "#przerwa", na koncu "#koniec N").
//   3) _strumien_koniec(...)         - zawsze, takze przy bledzie
// W RAM jest w kazdej chwili najwyzej jedna linia odpowiedzi.
void lvgl_dashboard_poc_wykresy_strumien_start(bool dlugi);
void lvgl_dashboard_poc_wykresy_strumien_linia(const char *linia);
int  lvgl_dashboard_poc_wykresy_strumien_koniec(bool dlugi, uint32_t liczbaWierszy, int wynik, int szczegol);

// true, gdy biezaca odpowiedz zaczela sie od "#panel-hist" (centrala 3.58.4+).
bool lvgl_dashboard_poc_wykresy_strumien_zredukowany();
// true, dopoki podstrona Wykres jest na ekranie i czeka na dane.
bool lvgl_dashboard_poc_wykresy_czeka_na_dane();
// Postep pobierania do pokazania pod napisem "Ladowanie danych...".
void lvgl_dashboard_poc_wykresy_postep(uint32_t linii, uint32_t bajtow);

// Liczby z ostatniego pobrania - do linii "tag=WYKRESY" w logu.
struct LvglWykresyStatystyka {
  int  punktow;          // punkty z danymi na wykresie
  int  przerw;           // odcinki bez danych (centrala nie pracowala)
  long wierszyZrodla;    // ile wierszy historii centrala wziela pod uwage (-1 = nie podala: stary CSV)
};
void lvgl_dashboard_poc_wykresy_statystyka(LvglWykresyStatystyka *wyjscie);
