// ============================================================================
// HISTORIA ZMIAN (ten plik) — najnowsze na gorze, kategorie jak w CHANGELOG.md
// ============================================================================
// HISTORIA ZMIAN (ten plik) — najnowsze na gorze, kategorie jak w CHANGELOG.md
// ============================================================================
// [2026-11-12] v1.2.1 Dodano: lvgl_dashboard_poc_alarm_odrzucony() i
//   lvgl_dashboard_poc_przywroc_alarm() na koncu pliku - stan alarmu
//   przetrwa samoczynny restart panelu ([v1.2.1 RATUNEK], opis w
//   dashboard_lvgl_poc.h i w HISTORII ZMIAN main.cpp).
//   Dodano: etykieta alarmEkranZnacznik - znacznik z naglowka dashboardu
//   ("BRAK WIFI 14 s", "SYMULACJA: piec"...) widac tez na ekranie alarmu,
//   ktory naglowek zaslania. Reszta pliku bez zmian.
// [2026-11-11] v1.2.0 Dodano: etykieta centralaZnacznikLabel w naglowku +
//   lvgl_dashboard_poc_aktualizuj_centrale() ([v1.2.0 CENTRALA-ID]).
//   Zmieniono: wskaznik baterii pokazuje "USB" dla procent < 0
//   ([v1.2.0 BATERIA-BRAK]); przy braku danych (polaczono=false - od v1.2.0
//   takze w trakcie pracy, nie tylko na starcie) wirnik pompy staje, a jej
//   ikona jest wygaszona ([v1.2.0 DANE-NIEAKTUALNE]). Pelny opis: HISTORIA
//   ZMIAN w main.cpp.
// [2026-09-06] Naprawiono: brak drogi powrotu do WYCISZ po kliknieciu
//   ZAMKNIJ na EKRANIE ALARMOWYM (zgloszone przez uzytkownika) - ZAMKNIJ
//   celowo nie wycisza brzeczyka (patrz HISTORIA ZMIAN nizej, wpis
//   [2026-09-05]), wiec bez tej poprawki alarm mogl zostac aktywny w tle
//   (brzeczyk dalej gra) bez ZADNEJ mozliwosci dotarcia z powrotem do
//   przycisku WYCISZ, dopoki alarm sam nie eskalowal/ustapil. Dodano:
//   alarmWskaznikDash - maly dzwonek (LV_SYMBOL_BELL, wbudowany glif LVGL,
//   zero nowej bitmapy) w pasku naglowka dashboardu, tuz obok istniejacego
//   globusa/WiFi (patrz netGlobusSiatka w lvgl_dashboard_poc_init()).
//   Widoczny WYLACZNIE gdy jest aktywny alarm (pokazywany/ukrywany i
//   przebarwiany w alarm_ekran_odswiez()), tap = powrot na pelny ekran
//   (alarm_wskaznik_tap_cb() resetuje g_alarmOdrzucony i wola
//   alarm_ekran_pokaz() z zapamietanymi g_alarmOstatniOgrzewanie/Panel).
//   Dotyczy WYLACZNIE tego pliku - patrz tez CHANGELOG.md [Unreleased]
//   (zaktualizowany opis, nie nowy wpis - funkcja jeszcze nie wydana).
// ============================================================================
// [2026-09-05] Dodano: EKRAN ALARMOWY - pelnoekranowa nakladka z duza,
//   pulsujaca ikona, pokazywana AUTOMATYCZNIE (nie klikiem w kafelek) przy
//   aktywnym alarmie (dym/pozar lub przegrzanie PIEC CO/PANEL). Patrz
//   PLAN_EKRAN_ALARMOWY.md dla pelnego uzasadnienia. W skrocie:
//   - Nowy ekran (lv_obj_create(nullptr), ten sam wzorzec co pompa_panel/
//     serwo_panel/dym_panel/alarm_temp_panel) o nazwie alarmEkran - CELOWO
//     nie pod nazwa "alarm_temp_panel" (to juz zajete przez prog ustawien,
//     zupelnie inna funkcja).
//   - Dwa przyciski: WYCISZ (tylko brzeczyk, main.cpp, w 100% lokalnie -
//     patrz HISTORIA ZMIAN main.cpp) i ZAMKNIJ (chowa ekran, wraca do
//     dashboardu, calkowicie lokalnie w tym pliku) - bez trzeciego
//     "WYLACZ" (ustalone w rozmowie).
//   - BRAK idle-timeoutu (POMPA_STRONA_IDLE_MS) w odroznieniu od reszty
//     paneli - ekran znika SAM dopiero, gdy alarm faktycznie ustapi
//     (funkcja alarm_ekran_odswiez(), wolana z lvgl_dashboard_poc_aktualizuj()
//     zaraz po piec_alarm_odswiez()/panel_alarm_odswiez()), a poza tym
//     zawsze mozna go zamknac recznie wczesniej.
//   - Priorytet POZAR > OGRZEWANIE > BRAK, identyczny jak w brzeczyku
//     (main.cpp, brzeczykOdswiezZAlarmow) - eskalacja zawsze przebija juz
//     zamkniety, mniej powazny alarm; raz zamkniety alarm nie odpala sie
//     ponownie co 2s (STATUS_INTERWAL_MS), dopoki poziom sie nie zmieni.
//     Pelna tabela decyzji w komentarzu przy alarm_ekran_odswiez() nizej.
//   - Ikona: nowy bitmap IKONA_ALARM_DUZA (ikony.h/ikony_lvgl.h,
//     lv_ikona_alarm_duza) - NIE zoom istniejacego 40px trojkata (ten sam
//     powod co reszta ikon w projekcie: zoom w runtime traci ostrosc).
//     Animacja pulsowania = ten sam, juz istniejacy panel_anim_recolor_opa_cb
//     co dlon RECZNY/WYLACZ RECZNIE - zero nowego kodu animacji.
//   Dotyczy 4 plikow: ten plik (caly nowy ekran + wywolania w init()/
//   aktualizuj()), dashboard_lvgl_poc.h (typedef AlarmWyciszCb), main.cpp
//   (alarmWyciszObsluga + modyfikacja brzeczykOdswiezZAlarmow), ikony.h/
//   ikony_lvgl.h (nowa bitmapa) - patrz HISTORIA ZMIAN w main.cpp/.h oraz
//   CHANGELOG.md [Unreleased].
// ============================================================================
// [ZAKONCZONE tej sesji] Oba bloki podgladowe (ZEWN_ICON_PREVIEW,
// OGRZ_TILE_PREVIEW) zostaly USUNIETE - obie decyzje sa juz podjete i
// wpiete na stale do prawdziwej siatki:
//  - ZEWN: ikona przelaczana w runtime wg. progow z ikony.h (mroz<0st.C,
//    neutralnie 0-15st.C, cieplo >=15st.C), patrz kafelek ZEWN nizej i
//    logika w lvgl_dashboard_poc_aktualizuj().
//  - PIEC CO/SR: kafelek_ogrz_polaczony() (Wariant A, wygrany w porownaniu
//    na zywo) jest teraz stalym elementem siatki (rzad 2), zasilanym
//    prawdziwymi danymi tOgrz/tOgrzSr zamiast demo-tekstu.
// Patrz punkt 22 w STATUS_PROJEKTU_LVGL.md.

#include <lvgl.h>
#include <stdlib.h>  // malloc() dla PressFlashCtx (patrz press_flash_zainstaluj) - Serial/millis() ponizej dzialaly juz wczesniej bez jawnego #include Arduino.h (PlatformIO framework=arduino dola go automatycznie do kazdego .cpp), ale malloc wolimy miec z jawnego include zamiast liczyc na to samo dla stdlib.
#include <string.h>  // strchr/memcpy/strlen/strtof - parser CSV w sekcji [PARTIA 5b, ta sesja - DANE WYKRESOW]
#include <math.h>    // lroundf() - skalowanie float->lv_coord_t w lv_chart, patrz PARTIA 7 (wykresy_chart_skaluj)
#include "dashboard_lvgl_poc.h"
#include "ikony_lvgl.h"

// Te same wartosci hex co #define KOL_* w main.cpp centrali (skopiowane celowo,
// zeby ten plik POC dzialal niezaleznie od main.cpp - przy docelowym porcie
// przeniesiemy je do jednego wspolnego miejsca zamiast duplikowac).
#define KOL_TLO       0x0801
#define KOL_KARTA     0x10A3
#define KOL_RAMKA     0x2A8B
#define KOL_TEKST     0xC638
#define KOL_TEKST_DIM 0x6B6D
#define KOL_OGIEN     0x07FF
#define KOL_SUWAK     0xFD20
#define KOL_ZAR       0xFA80
#define KOL_CHMURKA   0x2E7F
#define KOL_ERR       0xF800
#define KOL_WARN      0xFDA0
#define KOL_KROPLA    0x4C9F
// [FIX DOTYK, ta sesja] Krzyzyk zamkniecia (LV_SYMBOL_CLOSE) na kazdym
// panelu/ekranie mial dotychczas domyslny font (14px) oraz kolor
// KOL_TEKST_DIM - taki sam przygaszony szary jak zwykly, nieaktywny
// tekst. Zgloszony problem: dotyk bywa niedokladny, wiec maly i malo
// widoczny "X" byl trudny do trafienia za pierwszym razem. KOL_ZAMKNIJ
// to ta sama nasycona czerwien co KOL_ERR (celowo - czerwony krzyzyk to
// uniwersalna konwencja "zamknij"), ale wlasna, osobno nazwana stala, bo
// znaczeniowo to NIE jest sygnalizacja bledu.
// [FIX WYGLAD, ta sesja - Partia 2] Od tej tury uzywana wewnatrz
// krzyzyk_zamkniecia_utworz() (tlo okraglego przycisku) zamiast bezposrednio
// w 12 osobnych miejscach - patrz ta funkcja (kilkaset linii nizej, obok
// panel_przycisk_ikona()) po pelny opis.
#define KOL_ZAMKNIJ 0xF800
// [Partia 3, ta sesja] CISNIENIE i POKOJ mialy KOL_TEKST_DIM (szary) - user:
// "ikonka jest szara ... powinno byc w kolorach nie szare". Wlasny kolor per
// domena, tak jak KOL_KROPLA dla wilgotnosci: fiolet dla manometru (CISNIENIE,
// zaden inny kafelek go nie uzywa) i cieply koral dla POKOJ (odrozniony od
// KOL_ZAR/amber, ktory juz oznacza BOJLER - inna domena "cieplo wody" niz
// "temperatura w pokoju").
#define KOL_CISNIENIE 0xB43F
#define KOL_POKOJ     0xFC4F
// [FIX, ta sesja - kolorowe logo POKOJ] Kolor DRUGIEJ warstwy (dach domu) -
// cieply braz/terakota, celowo odmienny od KOL_POKOJ (koralowy, uzywany
// teraz tylko na termometrze) - ten sam wzorzec co KOL_ZAR/KOL_KROPLA dla
// dwuwarstwowego BOJLERA. Patrz wywolanie kafelek() dla POKOJ nizej.
#define KOL_POKOJ_DACH 0x9266
// [Kolorystyka paneli - ta sesja] Zielony "bezpieczna/stan OK" - uzywany
// WYLACZNIE przez przycisk BEZPIECZNA w panelu SERWO (patrz
// serwo_panel_tryb_odswiez() nizej). Wczesniej ten przycisk (razem z AUTO)
// pozyczal KOL_OGIEN - kolor zarezerwowany na kafelek POMPA w glownej
// siatce - co sprawialo, ze panel SERWO "przeskakiwal" na cudzy akcent
// zamiast kontynuowac wlasna, pomaranczowa tozsamosc (KOL_SUWAK) znana z
// kafelkow KLAPA/SYBERKA. Zielony to tu NOWA, wolna domena (podobnie jak
// KOL_CISNIENIE/KOL_POKOJ wyzej) - uniwersalna konwencja "stan bezpieczny/
// OK", nieuzywana nigdzie indziej w UI.
#define KOL_BEZPIECZNA 0x3E4F
// [Partia 4, ta sesja] User na zywym sprzecie: "chmurka ma byc
// niebiesko-biala" - neutralny stan ZEWN (0-15 st.C) mial KOL_TEKST_DIM,
// czyli zwykly szary tekst-placeholder (patrz zewn_ikona_odswiez()), NIE
// jakikolwiek odcien niebieskiego - stad wrazenie "szarej chmurki" na LCD.
// [Partia 5, ta sesja] Pierwsza poprawka (0xD73F, ~(213,230,255)) byla tak
// jasna, ze na LCD dalej czytala sie prawie jak biel - user powtorzyl
// prosbe "biala niebieska". Format ikony (LV_IMG_CF_ALPHA_1BIT) pozwala
// tylko na JEDEN kolor recolor na obrazek (patrz obszerny komentarz przy
// kafelek() nizej co do dwuwarstwowego triku uzytego dla BOJLERA) - user
// potwierdzil, ze woli szybsza poprawke jednego odcienia zamiast nowej
// dwuwarstwowej bitmapy jak BOJLER. Nowy odcien (0x963F, ~(150,200,255)) -
// wyrazniej blekitny, ale wciaz jasny/pastelowy ("bialo-niebieski", nie
// nasycony niebieski jak KOL_KROPLA).
#define KOL_ZEWN_NEUTRALNY 0x963F
// [UI-PANEL-KOLOR, ta sesja] User: "chce aby ikona panelu byla w ruchnych
// kolorach np slonce zulto reszta jak jest" - staly, nasycony zolty
// (0xFFE0 RGB565 = czysta zoltiec) WYLACZNIE dla warstwy slonca ikony PANEL
// (lv_ikona_panel_slonce). Nowa, wolna domena - nieuzywana nigdzie indziej,
// wiec nie koliduje z KOL_SUWAK/KOL_ZAR (oba bardziej pomaranczowe). Reszta
// ikony (kolektor) zostaje przy dotychczasowym KOL_ZAR/KOL_ERR - patrz
// wywolanie kafelek() dla PANEL nizej i panel_alarm_odswiez().
#define KOL_SLONCE 0xFFE0
// [UI-INTERNET, ta sesja] Kolory stalych warstw globusu (fbAppReady) - ten
// sam dwubarwny wzorzec co PANEL (slonce+kolektor)/BOJLER (kontur+woda).
// Niebieski dla "szkieletu" kuli (siatka poludnikow/rownoleznika) + zielony
// dla ladu (kontynenty) - klasyczne barwy globusu, widoczne tylko gdy
// internet==true. Przy internet==false OBIE warstwy przechodza na
// KOL_TEKST_DIM (patrz internet_ikona_odswiez()), wiec te stale nie sa
// uzywane w stanie "brak internetu".
#define KOL_GLOBUS_SIATKA 0x451F
#define KOL_GLOBUS_LADY   0x1E43
// [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Dwa nowe, wolne kolory -
// cieply bursztyn dla suwaka jasnosci DZIENNEJ, chlodny indygo dla suwaka
// jasnosci NOCNEJ (skojarzenie dzien/cieplo vs noc/chlod, ten sam "jeden
// kolor per domena" wzorzec co KOL_CISNIENIE/KOL_POKOJ wyzej - celowo NIE
// pozyczone od KOL_SLONCE, ktory komentarz przy nim zastrzega WYLACZNIE dla
// warstwy slonca ikony PANEL).
#define KOL_WYSWIETLACZ_DZIEN 0xFE00
#define KOL_WYSWIETLACZ_NOC   0x633E

// LVGL v8 z LV_COLOR_DEPTH=16 -> lv_color_t.full to surowy RGB565. Nasze
// KOL_* SA JUZ RGB565, wiec NIE wolno ich przepuszczac przez lv_color_hex()
// (ta oczekuje 24-bit 0xRRGGBB i sama kwantyzuje - dla naszych wartosci
// dalaby zly kolor). Wpisujemy bity wprost.
static inline lv_color_t rgb565(uint16_t v) {
  lv_color_t c;
  c.full = v;
  return c;
}

// ============================================================================
// [LVGL-API] Ten plik NIE zna HTTPClient/WiFi - wszystkie dane przychodza z
// main.cpp przez lvgl_dashboard_poc_aktualizuj(), a jedyna droga "z powrotem"
// do centrali to zarejestrowany callback (POST /api/pompa-manual). Zero stanu
// symulowanego lokalnie - to co widac na ekranie to zawsze ostatnia wartosc
// realnie odczytana z /api/status.
// ============================================================================
static PompaAkcjaCb g_pompaCb = nullptr;
// [ROZSZERZENIE, ta sesja] Callback wyboru strategii (Trociniak/Kopciuch/
// Automatyczny) - patrz PompaWyborCb w dashboard_lvgl_poc.h. Niezalezny od
// g_pompaCb (ten dotyczy tylko recznego override AUTO/WL/WYL).
static PompaWyborCb g_pompaWyborCb = nullptr;
// [POPRAWKA, ta sesja - panel SERWO v2] Dwa osobne callbacki zamiast jednego
// ServoAkcjaCb - patrz ServoTrybCb/ServoPozycjaCb w dashboard_lvgl_poc.h i
// uzasadnienie tam (centrala ma WSPOLNY tryb dla obu serw, nie niezalezny
// per sztuka).
static ServoTrybCb    g_serwoTrybCb    = nullptr;
static ServoPozycjaCb g_serwoPozycjaCb = nullptr;
// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Jeden generyczny
// callback dla wszystkich 6 pol - patrz ServoUstawieniaCb w
// dashboard_lvgl_poc.h.
static ServoUstawieniaCb g_serwoUstawieniaCb = nullptr;
// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Ten sam wzorzec co
// g_serwoUstawieniaCb powyzej, tylko dla galezi automatyki pompy - patrz
// PompaUstawieniaCb/PompaAntystopCb w dashboard_lvgl_poc.h. Dwa osobne
// callbacki (nie jeden), bo antystopWlaczony to bool/przelacznik, nie kolejny
// numeryczny slot - dokladnie ten sam podzial co PompaWyborCb vs PompaAkcjaCb.
static PompaUstawieniaCb g_pompaUstawieniaCb = nullptr;
static PompaAntystopCb   g_pompaAntystopCb   = nullptr;

// [ROZSZERZENIE, ta sesja - PANEL DYM] Ten sam wzorzec co pary callbackow
// SERWO/POMPA powyzej - jeden dla suwaka progu (DymProgCb), jeden generyczny
// dla 3 pol czasowych (DymUstawieniaCb).
static DymProgCb        g_dymProgCb        = nullptr;
static DymUstawieniaCb  g_dymUstawieniaCb  = nullptr;
// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Ten sam wzorzec co
// g_dymProgCb - callback wysylajacy nowy prog do centrali po zwolnieniu
// galki suwaka.
static AlarmTempProgCb  g_alarmTempProgCb  = nullptr;
// [EKRAN ALARMOWY, ta sesja] Patrz AlarmWyciszCb w dashboard_lvgl_poc.h -
// dotyczy WYLACZNIE przycisku WYCISZ na nowym ekranie alarmowym (nizej,
// sekcja "EKRAN ALARMOWY"). Przycisk ZAMKNIJ tego ekranu NIE ma
// odpowiednika - to lokalna sprawa tego pliku.
static AlarmWyciszCb    g_alarmWyciszCb    = nullptr;
// [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Ten sam wzorzec co pary
// callbackow wyzej, mimo ze "druga strona" w main.cpp NIE robi HTTP (patrz
// obszerny komentarz w dashboard_lvgl_poc.h) - ksztalt callbacku zostaje
// identyczny, zmienia sie tylko implementacja poza tym plikiem.
static WyswietlaczJasnoscCb     g_wyswietlaczJasnoscCb     = nullptr;
static WyswietlaczUstawieniaCb  g_wyswietlaczUstawieniaCb  = nullptr;
static WyswietlaczHarmonogramCb g_wyswietlaczHarmonogramCb = nullptr;

static bool  g_polaczono        = false;
static bool  g_pompaWlaczona    = false;
static int   g_pompaOverrideMin = 0;   // 0 = AUTO, >0 = minuty do powrotu do AUTO
// [ROZSZERZENIE, ta sesja] 1=Trociniak, 2=Kopciuch, 3=Automatyczny - patrz
// komentarz przy `pompaWybor` w dashboard_lvgl_poc.h. Domyslnie 1, zeby
// ikona "tryb pracy" mials sensowna wartosc jeszcze PRZED pierwszym
// pobierzStatus() (main.cpp: trybWyboru tez domyslnie = 1).
static int   g_pompaWybor       = 1;

// Musi byc zgodne z POMPA_OVERRIDE_MS w main_centrala.cpp (obecnie 60 min) -
// uzywane WYLACZNIE do narysowania % luku odliczania, centrala i tak jest
// jedynym zrodlem prawdy co do samych minut (g_pompaOverrideMin).
static const int POMPA_OVERRIDE_CALKOWITE_MIN = 60;

// [POPRAWKA, ta sesja - panel SERWO v2] `g_trybSerwa` (1=Auto/2=Reczny/
// 3=Bezpieczna) ZASTEPUJE dwie niezalezne flagi g_klapaReczny/g_syberkaReczny
// - zrodlo prawdy z centrali (pole "trybSerwa" w /api/status, main.cpp
// przekazuje bez zmian), WSPOLNE dla obu osi (patrz main_centrala.cpp
// `TrybSerwa`). `Pozycja` to nadal KAZDORAZOWO ostatnia znana pozycja 0-100
// (w trybie 1/3 tyle, ile ustawila automatyka/pozycja bezpieczna; w trybie 2
// - tyle, ile ustawil user) - cache potrzebny, zeby przy otwarciu panelu
// slider startowal od PRAWDZIWEJ aktualnej wartosci, nie od 0.
static int g_trybSerwa = 1; // 1=Auto, 2=Reczny, 3=Bezpieczna - domyslnie Auto do pierwszego pobierzStatus()
static int g_klapaPozycja   = 0;
static int g_syberkaPozycja = 0;
// [USUNIETE, ta sesja - panel SERWO v3] g_serwoAktywnyId (0=klapa, 1=syberka)
// zniknal - panel juz nie "przelacza sie tresciowo" na jedna aktywna os,
// pokazuje OBIE naraz (patrz serwo_panel_zbuduj()/serwo_panel_otworz() nizej),
// wiec nie ma czego pamietac miedzy otwarciami z kafelka KLAPA vs SYBERKA.

// [ROZBUDOWA, ta sesja] Idle-timeout na CALEJ stronie POMPA - user
// pierwotnie chcial 3s dla malego popupu, ale strona teraz jest pelnowymiarowa
// i ma 2 podstrony (wiecej do zobaczenia/dotkniecia), wiec wydluzone do 9s
// (wlasna propozycja, zaakceptowana przez user "Ok zrob tak"). Uzyte w
// pompa_strona_otworz() oraz przy kazdej nawigacji miedzy podstronami
// (patrz pompa_idle_timer_resetuj()), zeby przegladanie zakladki wyboru nie
// zostalo "ucinane w polowie".
// [ROZSZERZENIE, ta sesja] User: "powrot jest za szybki, zwieksz wszedzie o
// 1/2 czasu" - z 9000ms na 9000*1.5=13500ms. Jedna stala, uzywana przez
// WSZYSTKIE panele (POMPA, SERWO, USTAWIENIA SERWA/POMPY, WYBOR SERWA, DYM),
// wiec ta jedna zmiana podnosi czas bezczynnosci wszedzie naraz.
static const uint32_t POMPA_STRONA_IDLE_MS = 13500;

// [FIX, ta sesja - auto-powrot z podstrony Wykres] Podstrona Wykres
// (wykresyWykresCont) to jedyne miejsce w calym UI, gdzie user typowo NIC
// nie klika przez dluzszy czas - po prostu patrzy na wykres. Wspolny
// POMPA_STRONA_IDLE_MS (13.5s) zaklada wzorzec "klik co pare sekund" jak w
// pozostalych panelach, wiec tu byl zbyt agresywny. Zamiast globalnie
// wydluzac WSZYSTKIE panele (co znowu rozjechaloby "krotki idle" tam, gdzie
// jest pozadany), dostaje wlasna, dluzsza stala - patrz uzycie w
// wykresy_pokaz_wykres() ponizej.
//
// [ZMIANA, ta sesja] Dwa niezalezne mechanizmy powrotu naraz zamiast
// "resetuj timer przy kazdym dotyku": (1) DOWOLNY dotyk na podstronie
// Wykres (tlo, sam wykres) natychmiast wraca do Selekcji - patrz
// wykresy_wykres_dotyk_cb() nizej, ktora teraz NIE resetuje juz timera,
// tylko od razu wola wykresy_pokaz_selekcje(); (2) jesli user w ogole nie
// dotknie ekranu, auto-powrot i tak nastapi po WYKRESY_WYKRES_IDLE_MS od
// wejscia na podstrone (ustawiane raz w wykresy_pokaz_wykres()). Skrocone
// z 45s na 30s, bo przy nowym schemacie to juz tylko czysta bezczynnosc
// (brak jakiegokolwiek dotyku), a nie margines "user cos klika, ale rzadko".
static const uint32_t WYKRESY_WYKRES_IDLE_MS = 30000;

// Uchwyty do podelementow kafelka POMPA - potrzebne, zeby aktualizowac
// tekst/kolor/luk bez przebudowywania calego kafelka od zera.
static lv_obj_t *pompaValLabel = nullptr;
static lv_obj_t *pompaIcon = nullptr;      // jedna animowana ikona (32x32), patrz komentarz przy wyborze wariantu w ikony.h
static lv_obj_t *pompaArc = nullptr;
static lv_obj_t *pompaPanel = nullptr;  // [ROZBUDOWA, ta sesja] To jest teraz CALY DRUGI EKRAN (lv_scr, pelny 480x320), nie maly overlay - user: "cala nowa strona". Nazwa zostaje (mniej zmian w reszcie pliku), ale patrz pompa_panel_zbuduj() nizej.
// [UI-RECZNIE] Maly znaczek dloni w rogu etykiety wartosci - widoczny TYLKO
// gdy tryb reczny aktywny (override != AUTO). W AUTO znaczek jest ukryty, a
// etykieta wartosci jest pusta - brak dloni = "automatyka decyduje", jego
// obecnosc = "ktos recznie nadpisal", bez potrzeby czytania slowa "AUTO".
static lv_obj_t *pompaRecznieIcon = nullptr;

// [UI-POMPA-GLOWNY-KAFELEK, ta sesja] Znaczek AKTYWNEJ STRATEGII (Czasowy/
// Temp/Automatyczny wg g_pompaWybor) - zawsze widoczny, prawy gorny róg
// karty. Patrz pompa_tile_odswiez() (dobor bitmapy) i kafelek_pompa()
// (budowa/pozycja).
static lv_obj_t *pompaStrategiaIcon = nullptr;

// [UI-POMPA-OVERRIDE, ta sesja] Ukosna kreska nad ikona pompy - widoczna
// TYLKO gdy override wymusza WYLACZENIE (override aktywny I
// !g_pompaWlaczona). Bez tego brak animacji wirnika (juz istniejace
// zachowanie w pompa_wirnik_timer_cb()) wyglada identycznie w AUTO, gdy
// pompa akurat nie pracuje, jak w wymuszonym WYLACZ - user chcial, zeby
// wymuszone WYLACZ bylo jednoznacznie widoczne, nie tylko "brak ruchu".
// Zwykly lv_obj (belka) obrocona stylem transform_angle, NIE lv_line/
// lv_img - zero zaleznosci od LV_USE_LINE (nieznany stan w lv_conf.h tego
// projektu) i zero problemu z transformacjami obrazow ALPHA_1BIT (patrz
// komentarz w ikony.h czemu ikony NIE uzywaja lv_img_set_zoom/angle).
static lv_obj_t *pompaPrzekreslenie = nullptr;

// [UI-POMPA-PANEL, ta sesja] Uchwyty do 3 ikon-przyciskow trybu recznego
// (AUTO/WLACZ/WYLACZ) na GLOWNEJ podstronie strony POMPA (patrz
// pompa_strona_zbuduj()) - user: "nie chce napisow tylko animowane ikonki".
// Kazdy z nich dostaje WLASNA, ciagla animacje (dziala TYLKO gdy strona jest
// widoczna, patrz pompa_strona_otworz/zamknij) zamiast statycznego tekstu,
// zeby cala trojka byla "zywa" tak jak animowana ikona pompy na glownym
// kafelku.
static lv_obj_t *pompaPanelAutoIcon = nullptr;   // petla/cykl, 8 klatek obrotu
static lv_obj_t *pompaPanelWlIcon = nullptr;     // dlon, delikatny "bounce" gora-dol
static lv_obj_t *pompaPanelWylIcon = nullptr;    // przekreslona dlon, pulsujaca przezroczystosc
static lv_timer_t *g_pompaPanelAutoTimer = nullptr; // timer klatek AUTO - zyje TYLKO gdy strona otwarta
static uint8_t g_pompaPanelAutoKlatka = 0;
static lv_timer_t *g_pompaPanelIdleTimer = nullptr; // [IDLE-TIMEOUT] bez klikniecia -> auto-powrot do dashboardu

// [ROZBUDOWA, ta sesja - strona POMPA w 2 podstronach] User: "klikam na
// pompe to pojawia sie cala nowa strona ... klikne na odpowiednia ikone to
// pojawia sie dodatkowa zakladka wyboru". Zamiast lv_tabview (niepewne API
// miedzy wersjami LVGL 8.x, ktorych tu nie mozemy skompilowac/przetestowac)
// - DWA pelnowymiarowe kontenery przelaczane recznie flaga HIDDEN, dokladnie
// tym samym, juz sprawdzonym wzorcem co reszta tego pliku (pompaArc,
// pompaPanel itself). Prostsze, zero nowej powierzchni API do zgadywania.
static lv_obj_t *dashboardScr = nullptr;      // oryginalny lv_scr_act() z lvgl_dashboard_poc_init() - zapamietany, zeby wracac
static lv_obj_t *pompaStronaGlowna = nullptr; // JEDYNA podstrona panelu POMPA (patrz FIX ponizej - podstrona "wybor" zlikwidowana)
// [FIX UX, ta sesja - druga tura] Wczesniejsza wersja tej sesji mialo tu
// dodatkowo `pompaStronaWybor` (osobna podstrona Trociniak/Kopciuch/
// Automatyczny, wchodzona przez posredni przycisk "TRYB PRACY") oraz
// `pompaTrybIkona`/`pompaTrybLabel` (jedna ikona+podpis na stronie glownej
// pokazujaca AKTUALNA strategie). User doprecyzowal po zobaczeniu
// poprzedniej poprawki: "nie o to chodzilo - tam gdzie AUTO/RECZNIE/WLACZ/
// WYLACZ, nizej powinno byc [wprost, bez posrednika] tryb pracy:
// czasowe/temperaturowe/auto, a klikniecie w dany tryb ma OD RAZU pokazac
// jego parametry (np. dla temp: zal/wyl i inne)". Cala podstrona wyboru
// (i posredni przycisk) zostaly wiec USUNIETE - 3 przyciski strategii sa
// teraz WPROST na stronie glownej (patrz pompa_panel_zbuduj() nizej) i
// kazdy z nich, po kliknieciu, jednoczesnie (1) wysyla wybor strategii
// (bez zmian - g_pompaWyborCb) i (2) OD RAZU otwiera juz istniejacy ekran
// USTAWIENIA POMPY (pompa_ustawienia_otworz(), zebatka) zamiast zostawac
// na stronie wyboru - ten ekran juz podswietla/przygasza pary pol wg
// aktywnej strategii (patrz pompa_ust_aktywnosc_odswiez()), wiec "i inne"
// (np. antystop) sa tam od razu widoczne przy okazji, bez dodatkowej
// nawigacji.
// [FIX UX, ta sesja] Uchwyty do 3 przyciskow AUTO/WLACZ/WYLACZ na stronie
// glownej POMPY - byly TYLKO lokalnymi zmiennymi w pompa_panel_zbuduj(),
// wiec nic nigdy nie podswietlalo aktywnego z nich (w odroznieniu od
// panelu SERWO, patrz serwo_panel_tryb_odswiez() i serwoAutoBtn/
// serwoRecznyBtn/serwoBezpiecznaBtn nizej - ten sam wzorzec, przeniesiony
// tutaj). Teraz statyczne uchwyty pliku, zeby pompa_panel_glowna_odswiez()
// (nizej) moglo do nich siegnac z dowolnego miejsca.
static lv_obj_t *pompaAutoBtn = nullptr;
static lv_obj_t *pompaWlBtn = nullptr;
static lv_obj_t *pompaWylBtn = nullptr;
// [FIX UX, ta sesja - druga tura] Analogiczne uchwyty dla 3 przyciskow
// strategii (CZASOWY=Trociniak/TEMP=Kopciuch/AUTO=Automatyczny), teraz
// bezposrednio na stronie glownej - patrz pompa_tryb_odswiez() nizej.
static lv_obj_t *pompaTrybCzasowyBtn = nullptr;
static lv_obj_t *pompaTrybTempBtn = nullptr;
static lv_obj_t *pompaTrybAutoBtn = nullptr;


// Uchwyty do kafelkow ktore dostaja realne dane z lvgl_dashboard_poc_aktualizuj().
static lv_obj_t *zewnValLabel = nullptr;
static lv_obj_t *zewnIcon = nullptr;   // przelaczana bitmapa (mroz/neutralnie/cieplo) - patrz aktualizuj()
static lv_obj_t *bojlerValLabel = nullptr;
static lv_obj_t *dymValLabel = nullptr;
static lv_obj_t *dymIcon = nullptr;
static lv_obj_t *dymCard = nullptr;  // rodzic dymValLabel - do zmiany koloru ramki przy alarmie (patrz aktualizuj())
// [ROZSZERZENIE, ta sesja] Pasek dynamiczny pod wartoscia - patrz
// kafelek_dym()/dym_ikona_odswiez() nizej.
static lv_obj_t *dymBar = nullptr;
// [FIX tej sesji] Szerokosc paska stref DYM w px - zapamietana przy budowie
// kafelka (kafelek_dym()), zeby dym_ikona_odswiez() mogl przeliczyc pozycje
// X wskaznika (0..dymBarSzer-3, patrz komentarz przy wskazniku) bez
// odczytywania jej z LVGL w kazdej klatce.
static int dymBarSzer = 0;
// [UI-WIFI-SYGNAL, ta sesja] Wskaznik sily sygnalu WiFi w naglowku (zastepuje
// staly napis "PIEC CO - LVGL proof of concept") - 7 pionowych kresek
// rosnacej wysokosci (jak w telefonie) + osobny label "X" wyswietlany
// zamiast/nad nimi, gdy polaczono==false. Uchwyty zapamietane, zeby
// wifi_ikona_odswiez() (wolane z aktualizuj()) moglo zmieniac tylko kolory
// kresek i widocznosc X, bez przebudowy calego widgetu przy kazdym
// odswiezeniu - ten sam wzorzec co dymBar/dymIcon powyzej.
static lv_obj_t *wifiPasek[7] = { nullptr };
static lv_obj_t *wifiX = nullptr;
// [UI-INTERNET, ta sesja] Wskaznik dostepnosci internetu (fbAppReady z
// main_centrala.cpp) - globus w naglowku, tuz obok wskaznika sygnalu WiFi.
// Dwie rozlaczne warstwy (siatka+lad, ten sam wzorzec co bojlerKontur/
// bojlerWoda), przebarwiane RAZEM w internet_ikona_odswiez() + osobny
// label "X" na wierzchu, dokladnie ten sam mechanizm co wifiX powyzej.
static lv_obj_t *netGlobusSiatka = nullptr;
static lv_obj_t *netGlobusLady = nullptr;
static lv_obj_t *netX = nullptr;
// [UI-BATERIA, ta sesja] Wskaznik wlasnej baterii tej plytki w naglowku -
// lustrzane odbicie wskaznika WiFi (WiFi po prawej TOP_RIGHT -8,6, bateria
// po lewej TOP_LEFT 8,6). Ksztalt czysto wektorowy jak WiFi (obrys+wypelnienie
// +"nib", zaden bitmap z ikony.h) - patrz PLAN_BATERIA.md. Nib bez wlasnego
// uchwytu (tworzony i zapominany w init - nigdy pozniej nie aktualizowany,
// ten sam motyw co lokalna zmienna "kreska" w petli WiFi powyzej).
static const int BAT_SZER = 22, BAT_WYS = 12, BAT_RAMKA = 2;
static lv_obj_t *bateriaWypelnienie = nullptr;
static lv_obj_t *bateriaEtykieta = nullptr;
// [v1.2.0 CENTRALA-ID] Znacznik "z ktora centrala rozmawiam" na srodku
// naglowka - patrz lvgl_dashboard_poc_aktualizuj_centrale().
static lv_obj_t *centralaZnacznikLabel = nullptr;
// [PIEC CO/SR - byl OGRZ_TILE_PREVIEW] Teraz staly kafelek w siatce, patrz
// kafelek_ogrz_polaczony() i wywolanie w lvgl_dashboard_poc_init().
static lv_obj_t *ogrzGlownaLabel = nullptr;
static lv_obj_t *ogrzSredniaLabel = nullptr;
// [ROZSZERZENIE, ta sesja - alarmy PIEC CO/PANEL] Uchwyty karty+ikony, ten
// sam wzorzec co dymCard/dymIcon - potrzebne, zeby piec_alarm_odswiez()/
// panel_alarm_odswiez() mogly zmienic kolor ramki/ikony przy alarmie
// przegrzania (alarm_ogrzewanie/alarm_panel z centrali).
static lv_obj_t *ogrzCard = nullptr;
static lv_obj_t *ogrzIcon = nullptr;
static lv_obj_t *panelCard = nullptr;
static lv_obj_t *panelIcon = nullptr;

// [Partia 2, ta sesja] KLAPA/SYBERKA/WILGOTNOSC/CISNIENIE/POKOJ - patrz
// STATUS_PROJEKTU_LVGL.md pkt 30.
static lv_obj_t *pokojValLabel = nullptr;
static lv_obj_t *wilgotnoscValLabel = nullptr;
static lv_obj_t *cisnienieValLabel = nullptr;
static lv_obj_t *klapaValLabel = nullptr;
static lv_obj_t *syberkaValLabel = nullptr;
// [LVGL-DATA-CZAS, ta sesja] Kafelek DATA I CZAS - ostatni wolny slot siatki
// (rzad 2, obok SERWO/DYM). Dwa uchwyty: godzina:minuta jako GLOWNA wartosc
// (font 28, ten sam wzorzec co reszta kafelkow - najbardziej "rzucajaca sie
// w oczy" liczba), dzien.miesiac.rok jako maly wiersz na dole (font 14,
// dokladnie ten sam uklad co "SR --" w kafelek_ogrz_polaczony()).
static lv_obj_t *dataCzasCzasLabel = nullptr;
static lv_obj_t *dataCzasDataLabel = nullptr;
// [ROZSZERZENIE, ta sesja - kafelek wejsciowy SERWO x2] Uchwyt do podpisu
// wewnatrz kafelek_serwo_wejscie() (dashboard glowny) - user: "ikona serwo
// ma miec wielkosc dwoch ikonek, bo teraz nie ma wartosci a powinny byc na
// pierwszym ekranie". Poprzednio kafelek_serwo_wejscie() pokazywal tylko
// staly tekst "Klapa\nSyberka" bez liczb (patrz komentarz przy funkcji) -
// teraz ten sam podpis dostaje zywe wartosci (aktualizowane w
// lvgl_dashboard_poc_aktualizuj(), ten sam wzorzec co klapaValLabel/
// syberkaValLabel wyzej dla ekranu podmenu). Osobny wskaznik od
// klapaValLabel/syberkaValLabel powyzej, bo to inny obiekt lv_label (inny
// rodzic/ekran).
static lv_obj_t *serwoWejsciePodpis = nullptr;
// [POPRAWKA, ta sesja] Uchwyt do "malego znaczka trybu" na GLOWNYM kafelku
// SERWO (kafelek_serwo_wejscie() - jedyny kafelek SERWO widoczny wprost na
// dashboardzie; klapaCard/syberkaCard ponizej sa na OSOBNYM, posrednim
// ekranie serwoWyborScr, wiec zmiana TYLKO tam nie byla widoczna na
// glownym panelu - to byl blad poprzedniej poprawki). Podmieniany w
// serwo_tile_odswiez() dokladnie jak klapaRecznieIcon/syberkaRecznieIcon.
static lv_obj_t *serwoWejscieTrybIcon = nullptr;
// [ROZSZERZENIE, ta sesja - panel SERWO] Uchwyty do KART (nie tylko wartosci)
// KLAPY/SYBERKI - potrzebne, zeby podswietlac ramke w trybie recznym, tak
// jak main.cpp centrali robi to dla starego, recznie rysowanego UI (patrz
// isManualLike() w piec_co_ui_koncept.html - ten sam pomysl, teraz w LVGL).
static lv_obj_t *klapaCard = nullptr;
static lv_obj_t *syberkaCard = nullptr;
// Znaczek dloni w rogu wartosci - identyczny wzorzec co pompaRecznieIcon,
// jeden na kafelek, widoczny TYLKO w trybie recznym.
static lv_obj_t *klapaRecznieIcon = nullptr;
static lv_obj_t *syberkaRecznieIcon = nullptr;

// --- Ekran WYBOR SERWA (miedzyekran) ---
// [ROZSZERZENIE, ta sesja - jedna ikona SERWO na glownym menu] User: "na
// glownym menu jedna ikona do serwa, dopiero w podmenu po kliknieciu
// pojawiaja sie opcje dla danego". Research (HMI/touchscreen menu best
// practice - Toptal/ISA-101): trzymac hierarchie DWUPOZIOMOWA (nie glebiej),
// rodzic ma jasny wizualny sygnal ze prowadzi do podmenu (ikona/strzalka), a
// "wstecz" wraca tam, skad user przyszedl - nie zawsze na sam dashboard.
// Dlatego ten ekran jest ROWNORZEDNY z serwoPanel/dashboardScr (wlasny
// lv_obj_t), NIE modyfikuje kafelek_serwo()/serwo_tile_odswiez() ponizej -
// obie karty KLAPA/SYBERKA (klapaCard/syberkaCard itd. wyzej) po prostu
// zmieniaja RODZICA (budowane w serwo_wybor_zbuduj() zamiast bezposrednio
// na dashboardScr) - CALA reszta logiki (aktualizacja wartosci,
// podswietlenie trybu recznego) zostaje identyczna, zero duplikacji.
static lv_obj_t *serwoWyborScr = nullptr;
static lv_timer_t *g_serwoWyborIdleTimer = nullptr;

// --- Panel SERWO (jeden wspoldzielony ekran dla KLAPY i SYBERKI) ---
// [POPRAWKA, ta sesja - panel SERWO v3] User na zywym sprzecie: klikanie
// kafelka KLAPA albo SYBERKA otwieralo panel z JEDNYM suwakiem "aktywnej"
// osi (g_serwoAktywnyId) - ale tryb (Auto/Reczny/Bezpieczna) jest i tak
// WSPOLNY dla obu (patrz TrybSerwa w main_centrala.cpp), wiec ten podzial na
// dwa "osobne" ekrany sugerowal, ze user wybiera tryb per-os, podczas gdy w
// rzeczywistosci przelaczenie z ekranu KLAPA po cichu zmienialo tez SYBERKE.
// Sprawdzony webowy panel centrali (patrz `git log`/dashboard HTML) rozwiazuje
// to dokladnie tak: JEDEN wspolny segmented control trybu + OBA suwaki
// (klapa+syberek) pokazane naraz pod spodem, kazdy niezaleznie przeciagalny,
// tylko gdy tryb==Reczny. To samo potwierdza literatura UX multi-zone
// (np. termostaty wielostrefowe: jeden wspolny tryb pracy, niezalezne
// nastawy per strefa) - rozdzielanie "co jest wspolne" od "co jest osobne"
// w jednym ekranie, zamiast dwoch pozornie niezaleznych ekranow.
static lv_obj_t *serwoPanel = nullptr;        // drugi ekran (jak pompaPanel), pelny 480x320
static lv_obj_t *serwoTytulLabel = nullptr;   // stały tytul "STEROWANIE SERWAMI" (juz NIE per-os, patrz wyzej)
static lv_obj_t *serwoOpisLabel = nullptr;    // jednozdaniowy opis zalezny WYLACZNIE od trybu - patrz SERWO_TRYB_OPIS[] nizej
static lv_obj_t *serwoAutoBtn = nullptr;      // podswietlany wg aktywnego trybu
static lv_obj_t *serwoRecznyBtn = nullptr;
// [POPRAWKA, ta sesja - panel SERWO v2] Trzeci przycisk - centrala ma 3
// tryby (Auto/Reczny/Bezpieczna), nie 2 - patrz g_trybSerwa.
static lv_obj_t *serwoBezpiecznaBtn = nullptr;
// [ROZSZERZENIE, ta sesja - Czesc 1 planu] Uchwyty do samych OBRAZKOW (nie
// przyciskow) wewnatrz trzech przyciskow wyzej - potrzebne do animacji,
// dokladnie ten sam wzorzec co pompaPanelAutoIcon/pompaPanelWlIcon/
// pompaPanelWylIcon w panelu POMPA. AUTO wiruje (podmiana 8 klatek,
// wspoldzielone PANEL_AUTO_KLATKI[] z panelem POMPA), RECZNY pulsuje
// przezroczystoscia (ta sama funkcja co WYLACZ RECZNIE w POMPIE -
// panel_anim_recolor_opa_cb() jest generyczna, bierze dowolny lv_obj_t*).
// BEZPIECZNA celowo BEZ animacji - plan (PLAN_PANEL_SERWO_UPGRADE.md czesc
// 1) okresla dla niej tylko "nowa ikona", nie animacje; stala tarcza jest
// tez semantycznie spojna z "wymuszony/zablokowany stan", w odroznieniu od
// "w toku" (AUTO) i "aktywna interakcja" (RECZNY).
static lv_obj_t *serwoAutoIcon = nullptr;
static lv_obj_t *serwoRecznyIcon = nullptr;
static lv_timer_t *g_serwoPanelAutoTimer = nullptr; // timer klatek AUTO - zyje TYLKO gdy panel SERWO otwarty
static uint8_t g_serwoPanelAutoKlatka = 0;
static lv_obj_t *serwoRecznyWiersz = nullptr; // kontener DWOCH wierszy (klapa+syberek) - HIDDEN poza trybem Reczny (2)
// [POPRAWKA, ta sesja - panel SERWO v3] Jeden serwoSlider/serwoPozycjaLabel
// (dla "aktywnej" osi) zastapiony DWOMA niezaleznymi parami - obie osie
// widoczne i przeciagalne naraz, zaden suwak nie "znika" pod drugim.
static lv_obj_t *serwoSliderKlapa = nullptr;
static lv_obj_t *serwoSliderSyberka = nullptr;
static lv_obj_t *serwoPozycjaLabelKlapa = nullptr;   // "62%" nad wierszem KLAPA, zywy podczas przeciagania
static lv_obj_t *serwoPozycjaLabelSyberka = nullptr; // to samo dla SYBERKI
// [ROZSZERZENIE, ta sesja - Panel SERWO Czesc 2] Luk pozycji per os, na wzor
// pompaArc (patrz deklaracja/budowa tamtego wyzej) - czysto WIZUALNY wskaznik
// 0-100%, NIE klikalny (jak pompaArc). W odroznieniu od pompaArc (ktory
// pokazuje jedna wartosc odczytywana wylacznie z centrali - "ile minut
// zostalo"), tu wartosc 0-100% jest RAZEM zrodlem interakcji (przeciagniecie
// suwaka - luk odswiezany na biezaco w serwo_slider_event_cb, to samo
// zdarzenie co juz odswieza etykiete procentowa) I odczytem z centrali
// (serwo_panel_otworz() przy wejsciu na panel + lvgl_dashboard_poc_aktualizuj()
// w tle, oba juz aktualizuja g_klapaPozycja/g_syberkaPozycja - luk podlacza
// sie do tych samych dwoch miejsc, zero nowego zrodla prawdy).
static lv_obj_t *serwoArcKlapa = nullptr;
static lv_obj_t *serwoArcSyberka = nullptr;
static lv_timer_t *g_serwoPanelIdleTimer = nullptr;

// [POPRAWKA, ta sesja - panel SERWO v3] SERWO_OPIS[2] (opis per-os) +
// SERWO_TRYB_SUFIKS[3] (sufiks per-tryb), sklejane w serwo_panel_tryb_odswiez()
// w jeden zdaniowy opis pod tytulem - zastapione JEDNA tablica SERWO_TRYB_OPIS[3],
// bo opis panelu jest teraz WYLACZNIE o trybie (wspolnym dla obu osi), nie o
// pojedynczej "aktywnej" osi. Fizyczna funkcja kazdej osi (dawne SERWO_OPIS[])
// przeniesiona do etykiet wierszy nizej (patrz SERWO_NAZWA[]/serwo_panel_zbuduj()).
static const char *SERWO_NAZWA[2] = { "KLAPA", "SYBERKA" };
// Indeks 0..2 odpowiada trybSerwa 1..3 (Auto/Reczny/Bezpieczna). Opisuje
// WSPOLNE zachowanie obu osi w danym trybie - jeden tekst wystarczy, bo obie
// osie zawsze sa w tym samym trybie naraz.
static const char *SERWO_TRYB_OPIS[3] = {
  "Automatyka centrali steruje obiema osiami (klapa i syberek).",
  "Ustaw pozycje klapy i syberka nizej - kazda niezaleznie.",
  "Wymuszona pozycja bezpieczna: syberek 90%, klapa 0%.",
};
// [Partia 3, ta sesja] Uchwyty do ikon KROPLA/CISNIENIE - potrzebne PO
// stworzeniu kafelka, zeby doregulowac zoom (kropla mniejsza) wzgledem
// pozycje (cisnienie nizej), bo kafelek() sam nie przyjmuje takich
// parametrow (patrz uzycie w lvgl_dashboard_poc_init()).
static lv_obj_t *wilgotnoscIkonaUchwyt = nullptr;
static lv_obj_t *cisnienieIkonaUchwyt = nullptr;

// --- Ekran USTAWIENIA SERWA (6 parametrow automatyki klapy/syberka) ---
// [ROZSZERZENIE, ta sesja] Osobny, WLASNY pelnoekranowy lv_obj_t (nie
// podstrona wewnatrz serwoPanel jak pompaStronaWybor) - otwierany PRZYCISKIEM
// ZEBATKI z panelu SERWO, ale koncepcyjnie dotyczy OBU osi naraz (nie jednej
// aktywnej g_serwoAktywnyId), wiec zyje jako rownorzedny ekran, nie
// pod-widok jednej osi. Wraca zawsze do serwoPanel (nie do dashboardScr).
static lv_obj_t *serwoUstawieniaScr = nullptr;
// Kazdy z 6 wierszy = etykieta + duza wartosc na srodku + -/+ po bokach,
// dokladnie wzorzec "spinbox" z researchu (duze przyciski, latwe trafienie
// dotykiem rezystancyjnym, bez ciezkiej klawiatury ekranowej). Wartosc i
// przyciski osobno per wiersz - potrzebujemy uchwytu do labelki wartosci,
// zeby ja odswiezac po +/- bez przebudowy calego ekranu.
static lv_obj_t *serwoUstWartoscLabel[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

// Opis kazdego z 6 pol - klucz DOKLADNIE jak w /api/ustawienia-set i
// ustawParametrPieca() (main_centrala.cpp), zakres/krok jako lokalny "klips"
// (centrala i tak waliduje ostatecznie - ten sam wzorzec co PoleUstawien w
// main.cpp) - zeby uzytkownik nie mogl nawet lokalnie wyjsc poza sensowny
// zakres zanim POST dojdzie do centrali.
struct PoleServo {
  const char *klucz;
  const char *etykieta;
  int krok, minZakres, maxZakres;
};
static const PoleServo SERWO_USTAWIENIA_POLA[6] = {
  { "tempZadServo", "Temp. zadana",     1, 0, 100 },
  { "histerServo",  "Histereza",        1, 0,  50 },
  { "skokKlapy",    "Skok klapy %",     1, 1, 100 },
  { "skokSyberka",  "Skok syberka %",   1, 1, 100 },
  { "odchylTemp",   "Prog odchylenia",  1, 1,  50 },
  { "mnoznik",      "Mnoznik korekty",  1, 1,  10 },
};
// Ostatnie znane wartosci - wypelniane przez
// lvgl_dashboard_poc_aktualizuj_ustawienia_serwa() (patrz main.cpp,
// stan.tempZadServo itd. z /api/status), zeby ekran zawsze pokazywal
// PRAWDZIWY stan centrali, nie tylko to co user klepnal lokalnie (ten sam
// "nie klam po nieudanym poleceniu" wzorzec co pompaManualWyslij()).
static int g_serwoUstWartosc[6] = { 0, 0, 0, 0, 0, 0 };
static lv_timer_t *g_serwoUstIdleTimer = nullptr;

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Dokladnie ten sam wzorzec
// co blok SERWO powyzej, 5 pol zamiast 6 (patrz PolePompa/POMPA_USTAWIENIA_POLA
// nizej) + jeden osobny przelacznik antystopWlaczony (nie pasuje do wzorca
// spinbox +/-, patrz pompaAntystopBtn nizej).
static lv_obj_t *pompaUstawieniaScr = nullptr;
static lv_obj_t *pompaUstWartoscLabel[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

struct PolePompa {
  const char *klucz;
  const char *etykieta;
  int krok, minZakres, maxZakres;
};
// [Grupowanie - ta sesja] Kolejnosc pol odzwierciedla dwie NIEZALEZNE
// strategie pompy (patrz komentarz przy `pompaWybor` w dashboard_lvgl_poc.h)
// - czasOn/czasOff dzialaja tylko w Wybor=1 (Trociniak), tempOn/tempOff tylko
// w Wybor=2 (Kopciuch), antystopDni niezaleznie od obu. Ekran (patrz
// pompa_ustawienia_zbuduj() nizej) grupuje je wizualnie w tej samej
// kolejnosci, z akcentem koloru dopasowanym do przycisku danej strategii na
// podstronie WYBOR (KOL_ZAR=Trociniak, KOL_KROPLA=Kopciuch), zeby user od
// razu widzial, ktore pole nalezy do ktorej strategii bez czytania opisow.
static const PolePompa POMPA_USTAWIENIA_POLA[5] = {
  { "czasOn",      "Czas ON [min]",   1, 1, 180 },
  { "czasOff",     "Czas OFF [min]",  1, 1, 180 },
  { "tempOn",      "Zal. pow. [C]",   1, 0, 100 },
  { "tempOff",     "Wyl. pon. [C]",   1, 0, 100 },
  { "antystopDni", "Antystop co [dni]", 1, 1, 45 },
};
static int g_pompaUstWartosc[5] = { 0, 0, 0, 0, 0 };
// [ROZSZERZENIE, ta sesja] User: "male malo intuicyjne te kolory, nie widac
// po nich czy i co jest aktywne" - kolorowe paski dotad pokazywaly TYLKO
// przynaleznosc pola do strategii (grupowanie), nie to, KTORA strategia jest
// FAKTYCZNIE aktywna teraz (g_pompaWybor). POMPA_UST_GRUPA mapuje idx pola
// (ten sam idx co POMPA_USTAWIENIA_POLA[]/g_pompaUstWartosc[] wyzej) na
// strategie: 1=Trociniak, 2=Kopciuch, 0=niezalezne od strategii (antystop).
// pompaUstPasek[]/pompaUstEtykieta[]/pompaUstAkcent[] to uchwyty do
// widgetow kazdego wiersza (wypelniane w pompa_ust_wiersz_zbuduj()),
// potrzebne zeby pompa_ust_aktywnosc_odswiez() (patrz nizej) mogl przygasic
// pary NIEaktywnej strategii.
static const int POMPA_UST_GRUPA[5] = { 1, 1, 2, 2, 0 };
static lv_obj_t *pompaUstPasek[5]    = { nullptr, nullptr, nullptr, nullptr, nullptr };
static lv_obj_t *pompaUstEtykieta[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
static uint16_t  pompaUstAkcent[5]   = { 0, 0, 0, 0, 0 };
// Uchwyt do plomki "aktywny tryb" w naglowku ekranu USTAWIENIA POMPY - patrz
// pompa_ust_aktywnosc_odswiez().
static lv_obj_t *pompaUstTrybPlomka = nullptr;
static lv_obj_t *pompaUstTrybPlomkaLbl = nullptr;
static lv_timer_t *g_pompaUstIdleTimer = nullptr;

// [ROZSZERZENIE, ta sesja - PANEL DYM] Jeden ekran laczacy suwak progu +
// 3 pola czasowe (patrz uzasadnienie w dashboard_lvgl_poc.h). Zakres suwaka
// STALY 0-1500 (skala UI, nie ADC 0-4095 - patrz komentarz przy
// DYM_PROG_ZAKRES_MAX nizej).
static lv_obj_t *dymPanel = nullptr;
static lv_obj_t *dymProgLabel = nullptr;   // "PROG: NNNN" nad suwakiem
static lv_obj_t *dymSlider = nullptr;
// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Ten sam wzorzec co
// dymPanel/dymProgLabel/dymSlider powyzej - jeden WSPOLNY panel dla kafelkow
// PIEC CO i PANEL (oba alarmuja wzgledem tego samego progu na centrali).
static lv_obj_t *alarmTempPanel = nullptr;
static lv_obj_t *alarmTempProgLabel = nullptr;
static lv_obj_t *alarmTempSlider = nullptr;
static lv_timer_t *g_alarmTempPanelIdleTimer = nullptr;

// [EKRAN ALARMOWY, ta sesja] Pelnoekranowa nakladka pokazywana AUTOMATYCZNIE
// (nie klikiem w kafelek) przy aktywnym alarmie (dym/pozar lub przegrzanie
// PIEC CO/PANEL) - patrz PLAN_EKRAN_ALARMOWY.md. W skrocie: 2 przyciski
// (WYCISZ, ZAMKNIJ, bez trzeciego "WYLACZ"), priorytet POZAR > OGRZEWANIE >
// BRAK identyczny jak w brzeczyku (main.cpp, brzeczykOdswiezZAlarmow), i
// CELOWO bez wlasnego g_..IdleTimer - w odroznieniu od kazdego innego
// ekranu w tym pliku ten NIE ma timera bezczynnosci (patrz
// alarm_ekran_odswiez() nizej - znika sam dopiero gdy alarm faktycznie
// ustapi).
enum class AlarmPoziom : uint8_t { BRAK = 0, OGRZEWANIE = 1, POZAR = 2 };
static lv_obj_t *alarmEkran = nullptr;
static lv_obj_t *alarmEkranTytul = nullptr;
static lv_obj_t *alarmEkranPodtytul = nullptr;
static lv_obj_t *alarmEkranIkona = nullptr;
static lv_obj_t *alarmEkranWyciszLabel = nullptr;
// [v1.2.1] Ten sam tekst co znacznik w naglowku dashboardu (ekran alarmu
// zaslania naglowek) - patrz lvgl_dashboard_poc_aktualizuj_centrale().
static lv_obj_t *alarmEkranZnacznik = nullptr;
// g_alarmPokazywany: co ekran TERAZ faktycznie pokazuje (BRAK = ukryty).
// g_alarmOdrzucony: na jakim poziomie user ostatnio kliknal ZAMKNIJ - dopoki
// biezacy poziom alarmu nie przekroczy TEJ wartosci, ekran ma sie NIE
// pokazac ponownie. Pelna tabela decyzji przy alarm_ekran_odswiez() nizej.
static AlarmPoziom g_alarmPokazywany = AlarmPoziom::BRAK;
static AlarmPoziom g_alarmOdrzucony  = AlarmPoziom::BRAK;
// [WSKAZNIK POWROTU, ta sesja - zglaszony problem] Po ZAMKNIJ (bez WYCISZ)
// alarm zostaje aktywny w tle, ale bez tego user nie mialby ZADNEJ drogi
// powrotu do WYCISZ, dopoki alarm sam nie eskaluje/ustapi. Maly dzwonek w
// pasku naglowka (patrz koniec alarm_ekran_zbuduj()), widoczny wylacznie
// gdy alarm jest aktywny, tap = powrot na pelny ekran. g_alarmOstatni* to
// ostatnie widziane wartosci alarmOgrzewanie/alarmPanel (aktualizowane w
// alarm_ekran_odswiez() na kazdym cyklu), zeby recznie otwarty ekran mial
// tak samo trafny podtytul jak przy automatycznym pokazaniu.
static lv_obj_t *alarmWskaznikDash = nullptr;
static bool g_alarmOstatniOgrzewanie = false;
static bool g_alarmOstatniPanel = false;

// [PARTIA 5a, ta sesja - EKRAN WYKRESY] Szkielet ekranu historii pomiarow -
// patrz PLAN_WYKRESY_LVGL.md/STATUS_PROJEKTU_LVGL.md. W odroznieniu od
// WSZYSTKICH innych paneli w tym pliku, wejscie NIE jest przez kafelek w
// siatce 4x3 (siatka jest juz pelna, 12/12 - patrz decyzja w
// STATUS_PROJEKTU_LVGL.md), tylko przez GEST przeciagniecia palcem w
// poziomie na dashboardzie - pierwszy taki przypadek w tym pliku (reszta
// mechanizmu panelu - pelnoekranowy lv_obj_create(nullptr), LV_SYMBOL_CLOSE
// jako fallback, idle-timeout POMPA_STRONA_IDLE_MS, powrot przez
// lv_scr_load_anim(dashboardScr,...) - bez zmian wzgledem innych paneli).
static lv_obj_t *wykresyPanel = nullptr;
static lv_timer_t *g_wykresyPanelIdleTimer = nullptr;
// [DRUGI PULPIT, ta sesja] Posredni ekran-menu miedzy dashboardem a
// Wykresami/przyszlymi funkcjami - patrz obszerny komentarz przy
// drugi_pulpit_zbuduj() nizej co do decyzji "gest -> siatka, nie gest ->
// wprost ekran".
static lv_obj_t *drugiPulpitScr = nullptr;
static lv_timer_t *g_drugiPulpitIdleTimer = nullptr;
static const int DYM_PROG_ZAKRES_MAX = 1500;
static int g_dymProg = 500; // ostatnia znana wartosc z centrali (progAlarmDym), domyslnie jak main_centrala.cpp
// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Ostatnia znana wartosc
// progAlarmTemp z centrali, domyslnie 80 jak main_centrala.cpp
// (progAlarmowyTemperatury).
static int g_alarmTempProg = 80;

// [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Ostatnie znane wartosci -
// wypelniane przez lvgl_dashboard_poc_aktualizuj_ustawienia_wyswietlacza()
// (main.cpp, zaraz po wczytaniu z NVS przy starcie - patrz komentarz w
// dashboard_lvgl_poc.h), zeby panel przy KAZDYM otwarciu pokazywal PRAWDZIWY,
// ostatnio zapisany stan, ten sam wzorzec co g_dymProg/g_alarmTempProg wyzej.
static int  g_wyswietlaczJasnoscDzien = 100;
static int  g_wyswietlaczJasnoscNoc   = 30;
static bool g_wyswietlaczHarmonogramWlaczony = true;

struct PoleWyswietlacz {
  const char *klucz;
  const char *etykieta;
};
// Godzina 0-23, BEZ krok/minZakres/maxZakres jak PoleDym/PolePompa/PoleServo
// wyzej - te dwa pola ZAWIJAJA SIE na granicach doby (23+1->0, 0-1->23,
// patrz wyswietlacz_ust_minus_cb/plus_cb nizej) zamiast sie klipsowac, bo
// godzina jest z natury cykliczna, a najbardziej typowe okno (np. 22..6)
// wlasnie PRZECHODZI przez ta granice - klips zostawilby usera "utknietego"
// na 23 albo 0, nie mogac dojsc do drugiej strony przyciskiem +/-.
static const PoleWyswietlacz WYSWIETLACZ_USTAWIENIA_POLA[2] = {
  { "godzStart",  "Przyciemnij od godz." },
  { "godzKoniec", "Az do godz." },
};
static int g_wyswietlaczGodz[2] = { 22, 6 }; // [0]=godzStart, [1]=godzKoniec
static lv_obj_t *wyswietlaczGodzLabel[2] = { nullptr, nullptr };

static lv_obj_t *wyswietlaczPanel = nullptr;
static lv_obj_t *wyswietlaczSliderDzien = nullptr;
static lv_obj_t *wyswietlaczSliderNoc   = nullptr;
static lv_obj_t *wyswietlaczLabelDzien  = nullptr;
static lv_obj_t *wyswietlaczLabelNoc    = nullptr;
static lv_obj_t *wyswietlaczHarmonogramBtn      = nullptr;
static lv_obj_t *wyswietlaczHarmonogramBtnLabel = nullptr;
static lv_timer_t *g_wyswietlaczPanelIdleTimer  = nullptr;

struct PoleDym {
  const char *klucz;
  const char *etykieta;
  int krok, minZakres, maxZakres;
};
// Zakresy/krok 1:1 z main_centrala.cpp ustawParametrPieca() - krok 5 (nie 1)
// bo zakresy siegaja 600s, klikanie po 1 byloby meczace.
static const PoleDym DYM_USTAWIENIA_POLA[3] = {
  { "dymCzasOn",           "Czas WL. [s]",     5, 20, 600 },
  { "dymCzasOff",          "Czas WYL. [s]",    5, 10, 600 },
  { "dymCzasStabilizacji", "Stabilizacja [s]", 5,  5, 590 },
};
static lv_obj_t *dymUstWartoscLabel[3] = { nullptr, nullptr, nullptr };
static int g_dymUstWartosc[3] = { 130, 60, 90 }; // te same domyslne co main_centrala.cpp
static lv_timer_t *g_dymPanelIdleTimer = nullptr;

// Przelacznik antystopWlaczony - osobny stan bool (nie w g_pompaUstWartosc[])
// bo renderuje sie jako przycisk WL./WYL. (lv_btn + lv_label), nie jako
// wiersz +/-. [FIX] Pierwotnie lv_switch_create() - ZAMIENIONE, bo
// LV_USE_SWITCH jest wylaczony w lv_conf.h tego projektu (ten sam problem
// juz raz wywalil build przy panelu SERWO) - lv_btn jest juz uzywany wszedzie
// indziej w tym pliku, wiec zero nowej zaleznosci LVGL. `pompaAntystopBtn`/
// `pompaAntystopBtnLabel` to uchwyty do widgetu, zeby pompa_ustawienia_otworz()
// mogl ustawic jego wyglad (kolor tla + tekst) na ostatnia znana wartosc z
// centrali przy kazdym otwarciu ekranu - ten sam "nie klam" wzorzec co
// spinboxy powyzej.
static bool g_pompaAntystopWlaczony = true;
static lv_obj_t *pompaAntystopBtn = nullptr;
static lv_obj_t *pompaAntystopBtnLabel = nullptr;

// [Partia 5, ta sesja] PANEL (t_panel) - kafelek letni, odpowiednik KAF_PANEL
// ze starego main.cpp. Byl juz zaplanowany w STATUS_PROJEKTU_LVGL.md (pkt 19,
// "PANEL w lecie") ale nigdy nie zbudowany w LVGL - user poprosil o niego
// wprost przy okazji przeukladania siatki. Uzywa tego samego wzorca co
// BOJLER (etykieta + plomien KOL_ZAR + wartosc, kafelek() z jedna ikona).
// [FIX tej sesji] Podzial sezonowy zima/lato zostal calkowicie usuniety -
// wszystkie kafelki (w tym PANEL) sa teraz ZAWSZE widoczne, bo caly uklad
// (11 kafelkow, siatka 4x3) miesci sie na jednym ekranie bez przewijania.
// Wczesniej PANEL byl chowany gdy `zima == true`, co zostawialo pusta
// dziure w kafelku miedzy BOJLER i PIEC CO/SR w zimie - stad ta zmiana.
static lv_obj_t *panelValLabel = nullptr;

// ============================================================================
// [UI-POMPA] Animacja obrotu calej ikony pompy - patrz obszerny komentarz
// przy wyborze wariantu (styl "wentylator") w ikony.h co do TEGO, dlaczego
// to podmiana calej bitmapy klatka-po-klatce, a nie lv_img_set_angle() (ten
// ostatni po cichu nie dziala na formacie 1-bit, ktorego uzywamy).
//
// [ZMIANA tej sesji] Poprzednio byly to 4 klatki (90 st.) TYLKO lopatek,
// rysowane NA WIERZCHU osobnej, statycznej obudowy (dwa obiekty lv_img).
// Wybrany wariant nie ma osobnej obudowy (patrz komentarz w ikony.h), wiec
// zostaje JEDEN obiekt (pompaIcon) i 8 klatek (45 st.) calej ikony.
//
// Timer chodzi caly czas w tle (raz utworzony w kafelek_pompa()), ale
// FAKTYCZNIE zmienia klatke tylko gdy pompa naprawde pracuje - w przeciwnym
// razie ikona stoi na klatce 0 (spoczynek), bez zbednego przerysowywania
// ekranu.
// ============================================================================
static const lv_img_dsc_t *POMPA_KLATKI[8] = {
  &lv_ikona_pompa_0, &lv_ikona_pompa_1, &lv_ikona_pompa_2, &lv_ikona_pompa_3,
  &lv_ikona_pompa_4, &lv_ikona_pompa_5, &lv_ikona_pompa_6, &lv_ikona_pompa_7,
};
static uint8_t g_pompaKlatka = 0;

static void pompa_wirnik_timer_cb(lv_timer_t *t) {
  (void)t;
  if (pompaIcon == nullptr) return;
  // [v1.2.0 DANE-NIEAKTUALNE] `!g_polaczono`: od v1.2.0 kafelki przechodza na
  // "--" takze W TRAKCIE pracy (dane starsze niz PROG_NIEAKTUALNYCH_DANYCH_MS
  // w main.cpp), nie tylko na starcie. Wirnik krecacy sie dalej obok "--"
  // twierdzilby, ze pompa pracuje, choc panel tego juz nie wie.
  if (!g_pompaWlaczona || !g_polaczono) {
    if (g_pompaKlatka != 0) {
      g_pompaKlatka = 0;
      lv_img_set_src(pompaIcon, POMPA_KLATKI[0]);
    }
    return;
  }
  g_pompaKlatka = (uint8_t)((g_pompaKlatka + 1) % 8);
  lv_img_set_src(pompaIcon, POMPA_KLATKI[g_pompaKlatka]);
}

// ============================================================================
// [UI-POMPA-PANEL, ta sesja] Animacje 3 ikon w panelu wyboru trybu - dzialaja
// WYLACZNIE gdy panel jest widoczny (uruchamiane w pompa_panel_otworz(),
// sprzatane w pompa_panel_zamknij()), zeby nie zuzywac CPU/zeby nie migac w
// tle, gdy uzytkownik patrzy na glowna siatke kafelkow.
// ============================================================================
static const lv_img_dsc_t *PANEL_AUTO_KLATKI[8] = {
  &lv_ikona_auto_0, &lv_ikona_auto_1, &lv_ikona_auto_2, &lv_ikona_auto_3,
  &lv_ikona_auto_4, &lv_ikona_auto_5, &lv_ikona_auto_6, &lv_ikona_auto_7,
};

// AUTO - dokladnie ten sam wzorzec "podmiana calej bitmapy" co wirnik pompy
// (format 1-bit nie wspiera lv_img_set_angle(), patrz komentarz wyzej).
static void pompa_panel_auto_timer_cb(lv_timer_t *t) {
  (void)t;
  if (pompaPanelAutoIcon == nullptr) return;
  g_pompaPanelAutoKlatka = (uint8_t)((g_pompaPanelAutoKlatka + 1) % 8);
  lv_img_set_src(pompaPanelAutoIcon, PANEL_AUTO_KLATKI[g_pompaPanelAutoKlatka]);
}

// WLACZ RECZNIE - delikatny "bounce" (obiekt przesuwa sie o +-3px w pionie).
// translate_y jest zwykla wlasciwoscia stylu (nie transformacja bitmapy),
// wiec dziala bez ograniczen formatu 1-bit, ktore blokuja obrot/zoom.
static void panel_anim_translate_y_cb(void *obj, int32_t v) {
  lv_obj_set_style_translate_y((lv_obj_t *)obj, (lv_coord_t)v, 0);
}

// WYLACZ RECZNIE - pulsujaca przezroczystosc (oddychanie miedzy przygaszeniem
// a pelnym kryciem) - odrozniona od "zywego" ruchu WLACZ, celowo czytelna
// jako "przygaszanie/wygaszanie", spojne z semantyka wylaczenia.
static void panel_anim_recolor_opa_cb(void *obj, int32_t v) {
  lv_obj_set_style_img_recolor_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void pompa_panel_zamknij(); // forward - definicja nizej, potrzebna juz w idle-timeout callbacku ponizej
static void pompa_panel_idle_timeout_cb(lv_timer_t *t); // forward - potrzebny juz w pompa_idle_timer_resetuj() ponizej
// [ROZSZERZENIE, ta sesja] forward - zebatka USTAWIENIA POMPY jest budowana w
// pompa_panel_zbuduj() (nizej), ale sam callback/ekran ustawien jest
// zdefiniowany dalej w pliku (obok bliźniaczego bloku USTAWIENIA SERWA).
static void pompa_btn_ustawienia_cb(lv_event_t *e); // forward - definicja nizej
// [FIX UX, ta sesja] forward - potrzebny wczesniej niz jego pelna definicja,
// bo teraz kliknięcie trybu (CZASOWY/TEMP/AUTO) na stronie glownej od razu
// otwiera ekran USTAWIENIA POMPY (patrz nizej), zamiast wracac na strone
// glowna jak poprzednio.
static void pompa_ustawienia_otworz(); // forward - pelna definicja dalej w pliku

// [ROZBUDOWA, ta sesja] Reset/(re)start idle-timera - wywolywany zarowno przy
// otwarciu calej strony, jak i przy KAZDEJ nawigacji miedzy podstronami
// (glowna <-> wybor), zeby przegladanie zakladki wyboru nie zostalo ucinane
// w polowie przez timer odliczany jeszcze od momentu otwarcia strony.
static void pompa_idle_timer_resetuj() {
  if (g_pompaPanelIdleTimer != nullptr) {
    lv_timer_del(g_pompaPanelIdleTimer);
    g_pompaPanelIdleTimer = nullptr;
  }
  g_pompaPanelIdleTimer = lv_timer_create(pompa_panel_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_pompaPanelIdleTimer, 1);
}

static void pompa_panel_animacje_start() {
  // AUTO - timer klatek, startuje od 0 przy kazdym otwarciu (przewidywalne).
  g_pompaPanelAutoKlatka = 0;
  lv_img_set_src(pompaPanelAutoIcon, PANEL_AUTO_KLATKI[0]);
  if (g_pompaPanelAutoTimer == nullptr) {
    g_pompaPanelAutoTimer = lv_timer_create(pompa_panel_auto_timer_cb, 150, nullptr);
  }

  // WLACZ RECZNIE - bounce w nieskonczonosc (playback = wraca do wartosci
  // poczatkowej tym samym torem, wiec calosc jest plynnym "gora-dol-gora").
  lv_anim_t aBounce;
  lv_anim_init(&aBounce);
  lv_anim_set_var(&aBounce, pompaPanelWlIcon);
  lv_anim_set_exec_cb(&aBounce, panel_anim_translate_y_cb);
  lv_anim_set_values(&aBounce, -3, 3);
  lv_anim_set_time(&aBounce, 900);
  lv_anim_set_playback_time(&aBounce, 900);
  lv_anim_set_repeat_count(&aBounce, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&aBounce, lv_anim_path_ease_in_out);
  lv_anim_start(&aBounce);

  // WYLACZ RECZNIE - oddychanie przezroczystoscia w nieskonczonosc.
  lv_anim_t aPulse;
  lv_anim_init(&aPulse);
  lv_anim_set_var(&aPulse, pompaPanelWylIcon);
  lv_anim_set_exec_cb(&aPulse, panel_anim_recolor_opa_cb);
  lv_anim_set_values(&aPulse, LV_OPA_50, LV_OPA_COVER);
  lv_anim_set_time(&aPulse, 700);
  lv_anim_set_playback_time(&aPulse, 700);
  lv_anim_set_repeat_count(&aPulse, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&aPulse, lv_anim_path_ease_in_out);
  lv_anim_start(&aPulse);
}

static void pompa_panel_animacje_stop() {
  if (g_pompaPanelAutoTimer != nullptr) {
    lv_timer_del(g_pompaPanelAutoTimer);
    g_pompaPanelAutoTimer = nullptr;
  }
  // lv_anim_del kasuje kazda animacje przypieta do danej pary (var, exec_cb) -
  // dokladnie te utworzone w pompa_panel_animacje_start() wyzej.
  lv_anim_del(pompaPanelWlIcon, panel_anim_translate_y_cb);
  lv_anim_del(pompaPanelWylIcon, panel_anim_recolor_opa_cb);
  // Reset do stanu spoczynku, zeby kolejne otwarcie panelu zaczynalo czysto
  // (bez "zamrozonej" wartosci w polowie animacji z poprzedniego razu).
  if (pompaPanelWlIcon)  lv_obj_set_style_translate_y(pompaPanelWlIcon, 0, 0);
  if (pompaPanelWylIcon) lv_obj_set_style_img_recolor_opa(pompaPanelWylIcon, LV_OPA_COVER, 0);
}

// [IDLE-TIMEOUT] User: "wymyslk jak powrocic do menu np auto powrot ...w
// ciagu 3s" -> potwierdzone w rozmowie: NIEZALEZNY idle-timeout OPROCZ (nie
// zamiast) istniejacego natychmiastowego zamkniecia po wybraniu trybu.
// Timer (re)startowany przez pompa_idle_timer_resetuj() - przy otwarciu
// strony ORAZ przy kazdej nawigacji miedzy podstronami. Jesli user zdazy
// kliknac ktorykolwiek przycisk, pompa_panel_zamknij() i tak go skasuje
// (patrz nizej) - wiec nie ma wyscigu ani podwojnego zamkniecia.
static void pompa_panel_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  Serial.println("[LVGL-DIAG] pompa strona: brak aktywnosci, auto-powrot do dashboardu");
  g_pompaPanelIdleTimer = nullptr; // timer sam sie usuwa (repeat_count=1), nie wolno go tu jeszcze raz kasowac
  pompa_panel_zamknij();
}

// [FIX UX, ta sesja - druga tura] Poprzednia wersja tej funkcji (ta sesja,
// pierwsza tura) podmieniala ikone+podpis JEDNEGO przycisku "TRYB PRACY".
// User doprecyzowal, ze chce 3 przyciski strategii WPROST widoczne (patrz
// obszerny komentarz przy deklaracji pompaTrybCzasowyBtn/pompaTrybTempBtn/
// pompaTrybAutoBtn wyzej), wiec ta funkcja teraz PODSWIETLA TLO aktywnego
// z tych 3 przyciskow - dokladnie ten sam wzorzec co
// pompa_panel_glowna_odswiez() (AUTO/WLACZ/WYLACZ) i serwo_panel_tryb_
// odswiez() (panel SERWO). Kolory = te same, ktorych ta strategia uzywala
// wczesniej na (juz usunietej) podstronie wyboru i na ekranie USTAWIENIA
// POMPY (patrz pompa_tryb_kolor()) - user nie musi uczyc sie nowego
// kodowania kolorow.
static void pompa_tryb_odswiez() {
  if (pompaTrybCzasowyBtn == nullptr || pompaTrybTempBtn == nullptr || pompaTrybAutoBtn == nullptr) return;
  lv_obj_set_style_bg_color(pompaTrybCzasowyBtn, rgb565(g_pompaWybor == 1 ? KOL_ZAR : KOL_KARTA), 0);
  lv_obj_set_style_bg_color(pompaTrybTempBtn,    rgb565(g_pompaWybor == 2 ? KOL_KROPLA : KOL_KARTA), 0);
  lv_obj_set_style_bg_color(pompaTrybAutoBtn,    rgb565(g_pompaWybor == 3 ? KOL_OGIEN : KOL_KARTA), 0);
}

// [FIX UX, ta sesja - "nie wiem co jest aktywne"] Podswietla TLO aktywnego
// przycisku AUTO/WLACZ/WYLACZ na stronie glownej POMPY, dokladnie tym samym
// wzorcem co serwo_panel_tryb_odswiez() dla panelu SERWO (patrz tam) - do
// tej pory te 3 przyciski mialy zawsze to samo, statyczne tlo (KOL_KARTA z
// domyslnego motywu lv_btn), wiec z samego wygladu NIE dalo sie poznac,
// ktory stan jest aktualnie aktywny (widac to bylo tylko posrednio, po
// animacji ikony - latwe do przeoczenia, stad zdjecie ze sprzetu "nie wiem
// co tu jest aktywne co nie"). Stan wyznaczany dokladnie tak samo jak w
// pompa_tile_odswiez() nizej: g_pompaOverrideMin<=0 => AUTO; w przeciwnym
// razie g_pompaWlaczona rozstrzyga WLACZ/WYLACZ.
static void pompa_panel_glowna_odswiez() {
  if (pompaAutoBtn == nullptr || pompaWlBtn == nullptr || pompaWylBtn == nullptr) return;
  bool autoAktywne = (g_pompaOverrideMin <= 0);
  bool wlAktywne = (!autoAktywne && g_pompaWlaczona);
  bool wylAktywne = (!autoAktywne && !g_pompaWlaczona);
  lv_obj_set_style_bg_color(pompaAutoBtn, rgb565(autoAktywne ? KOL_OGIEN : KOL_KARTA), 0);
  lv_obj_set_style_bg_color(pompaWlBtn,   rgb565(wlAktywne   ? KOL_KROPLA : KOL_KARTA), 0);
  lv_obj_set_style_bg_color(pompaWylBtn,  rgb565(wylAktywne  ? KOL_TEKST_DIM : KOL_KARTA), 0);
}

// Odswieza WYGLAD kafelka POMPA (tekst wartosci, kolor ikony, luk odliczania)
// pod aktualne g_pompaWlaczona/g_pompaOverrideMin - wolane wylacznie z
// lvgl_dashboard_poc_aktualizuj(), czyli po kazdym prawdziwym pobierzStatus().
static void pompa_tile_odswiez() {
  if (pompaValLabel == nullptr) return; // kafelek jeszcze nie istnieje

  uint16_t kolorAktywny = g_pompaWlaczona ? KOL_KROPLA : KOL_TEKST_DIM;
  lv_obj_set_style_img_recolor(pompaIcon, rgb565(kolorAktywny), 0);

  // [UI-POMPA-GLOWNY-KAFELEK, ta sesja] Znaczek strategii - niezalezny od
  // override'u nizej, wiec aktualizowany zawsze, bez wzgledu na
  // g_pompaOverrideMin. g_pompaWybor: 1=Czasowy, 2=Temp, 3=Automatyczny
  // (patrz komentarz przy `pompaWybor` w dashboard_lvgl_poc.h). Kolor
  // rowny tym z przyciskow wyboru strategii (patrz komentarz przy
  // pompaStrategiaIcon w kafelek_pompa()) - user: "ikony trybu powinny byc
  // w kolorze [...] kilku kolorach", zamiast jednego stalego szarego.
  if (pompaStrategiaIcon != nullptr) {
    const lv_img_dsc_t *strategiaSrc;
    uint16_t strategiaKolor;
    if (g_pompaWybor == 1) {
      strategiaSrc = &lv_ikona_trociniak_mala;
      strategiaKolor = KOL_ZAR;
    } else if (g_pompaWybor == 2) {
      strategiaSrc = &lv_ikona_kopciuch_mala;
      strategiaKolor = KOL_KROPLA;
    } else {
      strategiaSrc = &lv_ikona_automatyczny_mala;
      strategiaKolor = KOL_OGIEN;
    }
    lv_img_set_src(pompaStrategiaIcon, strategiaSrc);
    lv_obj_set_style_img_recolor(pompaStrategiaIcon, rgb565(strategiaKolor), 0);
  }

  // [UI-POMPA-OVERRIDE, ta sesja] Dlon: KOLOR + OPACITY razem (nie sama
  // opacity jak poprzednio - user: "malo roznica sie kolor reki") - w AUTO
  // (bez nadpisania) dlon jest jednoczesnie PRZYGASZONA i WYSZARZONA
  // (KOL_TEKST_DIM), przy aktywnym override pelna i w kolorze KOL_KROPLA -
  // dwie zmienne naraz daja wyrazny kontrast miedzy stanami zamiast tylko
  // subtelnej roznicy przezroczystosci tego samego koloru.
  bool nadpisane = (g_pompaOverrideMin > 0);
  if (nadpisane) {
    lv_obj_set_style_img_opa(pompaRecznieIcon, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(pompaRecznieIcon, rgb565(KOL_KROPLA), 0);
  } else {
    lv_obj_set_style_img_opa(pompaRecznieIcon, LV_OPA_40, 0);
    lv_obj_set_style_img_recolor(pompaRecznieIcon, rgb565(KOL_TEKST_DIM), 0);
  }

  // [UI-POMPA-OVERRIDE, ta sesja] Przekreslenie pompy - widoczne WYLACZNIE
  // gdy override wymusza WYLACZENIE (nadpisane && !g_pompaWlaczona). W
  // wymuszonym WLACZ (nadpisane && g_pompaWlaczona) pompa juz sama sie
  // krec (patrz pompa_wirnik_timer_cb()) - nie trzeba dodatkowego znaczka,
  // ruch mowi sam za siebie. W AUTO (bez nadpisania) tez brak przekreslenia
  // - to nie jest "wymuszone", tylko normalny odczyt biezacego stanu
  // automatyki (animacja/jej brak juz to pokazuje).
  if (pompaPrzekreslenie != nullptr) {
    if (nadpisane && !g_pompaWlaczona) {
      lv_obj_clear_flag(pompaPrzekreslenie, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(pompaPrzekreslenie, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (g_pompaOverrideMin <= 0) {
    // [UI-RECZNIE] AUTO = pusta etykieta (dlon/przekreslenie obsluzone
    // wyzej, wspolnie dla obu galezi).
    lv_label_set_text(pompaValLabel, "");
    lv_obj_add_flag(pompaArc, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  // Tryb reczny (override != 0, wg centrali) - minuty juz policzone PRZEZ
  // CENTRALE (pompaManualOverrideUntil), tu tylko wyswietlamy.
  // [FIX-NACHODZENIE, ta sesja] Bez slowa WL/WYL - ta informacja i tak jest
  // widoczna z koloru/animacji ikony pompy (zielona+krecacy sie wirnik =
  // pracuje, wygaszona+nieruchomy = nie pracuje) i z obecnosci znaczka dloni
  // (= tryb reczny aktywny). Sam tekst "WL 45m"/"WYL 45m" byl za dlugi na
  // 114px karte przy foncie czytelnym z dystansu (patrz geometria w
  // STATUS_PROJEKTU_LVGL.md) - skrocenie do samych minut usuwa problem u
  // zrodla zamiast kolejny raz kombinowac z rozmiarem fontu.
  char bufor[16];
  snprintf(bufor, sizeof(bufor), "%dm", g_pompaOverrideMin);
  lv_label_set_text(pompaValLabel, bufor);

  lv_obj_clear_flag(pompaArc, LV_OBJ_FLAG_HIDDEN);
  int procentZostalo = (g_pompaOverrideMin * 100) / POMPA_OVERRIDE_CALKOWITE_MIN;
  if (procentZostalo > 100) procentZostalo = 100;
  lv_arc_set_value(pompaArc, procentZostalo);
}

// --- panel wyboru trybu (AUTO / RECZNIE WL / RECZNIE WYL) ---

static void pompa_panel_zamknij() {
  Serial.println("[LVGL-DIAG] pompa_panel_zamknij() wywolane - powrot do dashboardu");
  // [ROZBUDOWA, ta sesja] Byla to zmiana widocznosci malego overlay -
  // teraz to nawigacja miedzy DWOMA PELNYMI EKRANAMI.
  // [FIX tej sesji - LAGI] User zglosil "dwa przyciete ruchy" przy
  // otwieraniu/zamykaniu. Research (forum LVGL + dok. wydajnosci Espressif)
  // potwierdza: to znany problem na ESP32 BEZ PSRAM z malym/pojedynczym
  // buforem rysowania (patrz lvgl_port.cpp, LVGL_BUF_WIERSZE=40, buf1
  // pojedynczy, BEZ podwojnego buforowania) - animowane przejscie MOVE
  // wymaga przerysowania CALYCH DWOCH ekranow klatka po klatce przez SPI,
  // co na takim buforze daje realnie kilka FPS (dokladnie "poszarpane
  // ruchy" - identyczny przypadek opisany na forum LVGL: "ESP32 320x480
  // low FPS animating"). Standardowa poprawka dla tego typu sprzetu:
  // LV_SCR_LOAD_ANIM_NONE - natychmiastowe przelaczenie, JEDNO rysowanie
  // (tyle samo co normalny ekran), zero klatek posrednich do "gubienia".
  // auto_del=false zostaje (patrz tam) - dashboardScr NIE moze zostac
  // skasowany, bo to jedyny, stale zyjacy ekran glowny.
  if (dashboardScr != nullptr) {
    lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  }
  pompa_panel_animacje_stop();
  // [IDLE-TIMEOUT] Zamkniecie strony - niezaleznie CZYM wywolane (klik w
  // tryb, klik w X, sam idle-timeout, czy wybor strategii) - zawsze kasuje
  // ewentualny oczekujacy idle-timer, zeby nie odpalil sie pozniej "w
  // powietrze" na juz zamknietej/nastepnym otwarciu stronie.
  if (g_pompaPanelIdleTimer != nullptr) {
    lv_timer_del(g_pompaPanelIdleTimer);
    g_pompaPanelIdleTimer = nullptr;
  }
}

// [LVGL-API] Przyciski NIE zmieniaja juz nic lokalnie - tylko wolaja
// zarejestrowany callback (main.cpp: POST /api/pompa-manual + natychmiastowy
// pobierzStatus()). Kafelek pokaze nowy stan dopiero jak main.cpp wywola
// lvgl_dashboard_poc_aktualizuj() z prawdziwa odpowiedzia centrali - jesli
// POST sie nie uda, kafelek po prostu zostaje taki jaki byl (nie "klamie").
static void pompa_btn_auto_cb(lv_event_t *e) { (void)e; Serial.println("[LVGL-DIAG] btn AUTO klik"); if (g_pompaCb) g_pompaCb("auto"); pompa_panel_zamknij(); }
static void pompa_btn_wl_cb(lv_event_t *e)   { (void)e; Serial.println("[LVGL-DIAG] btn WL klik");   if (g_pompaCb) g_pompaCb("wl");   pompa_panel_zamknij(); }
static void pompa_btn_wyl_cb(lv_event_t *e)  { (void)e; Serial.println("[LVGL-DIAG] btn WYL klik");  if (g_pompaCb) g_pompaCb("wyl");  pompa_panel_zamknij(); }

// [FIX UX, ta sesja - druga tura] 3 przyciski strategii sa teraz WPROST na
// stronie glownej (patrz obszerny komentarz przy pompaTrybCzasowyBtn wyzej -
// posredni przycisk "TRYB PRACY" i cala podstrona wyboru zostaly USUNIETE).
// Klikniecie: (1) wysyla wybor strategii przez g_pompaWyborCb (bez zmian -
// main.cpp: wyslijTrybPracy("wybor", N) + pobierzStatus()), (2) OD RAZU
// otwiera ekran USTAWIENIA POMPY zamiast wracac na strone glowna - user:
// "jak klikne np w temp to mam opcje temp zal wyl i inne". Ten ekran juz
// istnieje (zebatka) i juz podswietla/przygasza pary pol wg aktywnej
// strategii (patrz pompa_ust_aktywnosc_odswiez() dalej w pliku) - zero
// nowej logiki filtrowania potrzebne. Kafelek/przyciski na stronie glownej
// pokaza nowy stan dopiero gdy main.cpp wywola aktualizuj() z prawdziwa
// odpowiedzia centrali (ten sam kontrakt "nie klam" co reszta).
static void pompa_btn_trociniak_cb(lv_event_t *e) { (void)e; Serial.println("[LVGL-DIAG] btn CZASOWY(TROCINIAK) klik"); if (g_pompaWyborCb) g_pompaWyborCb(1); pompa_ustawienia_otworz(); }
static void pompa_btn_kopciuch_cb(lv_event_t *e)  { (void)e; Serial.println("[LVGL-DIAG] btn TEMP(KOPCIUCH) klik");  if (g_pompaWyborCb) g_pompaWyborCb(2); pompa_ustawienia_otworz(); }
static void pompa_btn_automatyczny_cb(lv_event_t *e) { (void)e; Serial.println("[LVGL-DIAG] btn AUTOMATYCZNY klik"); if (g_pompaWyborCb) g_pompaWyborCb(3); pompa_ustawienia_otworz(); }
static void pompa_btn_zamknij_cb(lv_event_t *e) { (void)e; Serial.println("[LVGL-DIAG] btn X klik"); pompa_panel_zamknij(); }


// ============================================================================
// [UI-3D, PRZEROBIONE tej sesji] Poprzednia wersja wiazala caly efekt
// wcisniecia z LV_STATE_PRESSED (styl-na-stan + transition_dsc). W teorii
// powinno dzialac, ale na sprzecie efekt byl "zerowy". Najbardziej
// prawdopodobna przyczyna: main.cpp w loop() woła blokujace zapytania HTTP
// (status co STATUS_INTERWAL_MS) MIEDZY kolejnymi wywolaniami
// lvgl_port_tick(); jesli caly cykl "dotkniecie -> puszczenie" fizycznie
// zdazy sie zmiescic pomiedzy dwoma takimi tickami, LVGL przetwarza oba
// zdarzenia na raz i stan PRESSED nigdy nie trafia na ekran jako osobna,
// widoczna klatka - styl zmienia sie i wraca zanim cokolwiek zostanie
// wyslane po SPI do wyswietlacza.
//
// Zamiast wiec polegac na tym, jak dlugo faktycznie trwa fizyczny dotyk,
// wymuszamy WLASNY, gwarantowany minimalny czas trwania efektu
// (PRESS_FLASH_MIN_MS) niezaleznie od tego kiedy nadejdzie puszczenie:
// LV_EVENT_PRESSED ustawia "wcisniety" wyglad NATYCHMIAST i zapamietuje czas
// startu; LV_EVENT_RELEASED/PRESS_LOST cofa go od razu TYLKO jesli minelo
// juz >= PRESS_FLASH_MIN_MS, w przeciwnym razie planuje cofniecie na
// pozniej przez jednorazowy lv_timer. To gwarantuje co najmniej jedno
// pelne odswiezenie ekranu z widocznym efektem, cokolwiek by nie robil
// reszta loop() w miedzyczasie.
// ============================================================================
#define PRESS_FLASH_MIN_MS 120

typedef struct {
  uint16_t kolorNormalny;          // RGB565 tla w stanie spoczynku (rozne dla kazdego przycisku)
  uint8_t  cienSzerokoscSpoczynek; // [FIX-NACHODZENIE-CIENIA] rozne dla przyciskow panelu (6px, duzo
                                    // wolnego miejsca dookola) i kafelka POMPA w gesto upakowanym rzedzie
                                    // (3px - odstep miedzy kafelkami to tylko 4px, grubszy cien "wyciekalby"
                                    // na sasiedni kafelek juz w stanie spoczynku)
  uint32_t pressStartMs;           // millis() z chwili LV_EVENT_PRESSED
} PressFlashCtx;

static void press_flash_zastosuj(lv_obj_t *obj, bool wcisniety) {
  PressFlashCtx *ctx = (PressFlashCtx *)lv_obj_get_user_data(obj);
  if (wcisniety) {
    lv_obj_set_style_bg_color(obj, lv_color_darken(rgb565(ctx->kolorNormalny), 70), 0);
    lv_obj_set_style_shadow_width(obj, 1, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 0, 0);
    lv_obj_set_style_translate_y(obj, 3, 0);
  } else {
    lv_obj_set_style_bg_color(obj, rgb565(ctx->kolorNormalny), 0);
    lv_obj_set_style_shadow_width(obj, ctx->cienSzerokoscSpoczynek, 0);
    lv_obj_set_style_shadow_ofs_y(obj, ctx->cienSzerokoscSpoczynek >= 6 ? 3 : 2, 0);
    lv_obj_set_style_translate_y(obj, 0, 0);
  }
}

static void press_flash_revert_timer_cb(lv_timer_t *t) {
  lv_obj_t *obj = (lv_obj_t *)t->user_data; // bezposredni dostep do pola, jak reszta kodu (np. dispDrv.hor_res w lvgl_port.cpp)
  press_flash_zastosuj(obj, false);
}

static void press_flash_event_cb(lv_event_t *e) {
  lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
  PressFlashCtx *ctx = (PressFlashCtx *)lv_obj_get_user_data(obj);
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED) {
    ctx->pressStartMs = millis();
    press_flash_zastosuj(obj, true);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    uint32_t trwaloMs = millis() - ctx->pressStartMs;
    if (trwaloMs >= PRESS_FLASH_MIN_MS) {
      press_flash_zastosuj(obj, false);
    } else {
      lv_timer_t *t = lv_timer_create(press_flash_revert_timer_cb, PRESS_FLASH_MIN_MS - trwaloMs, obj);
      lv_timer_set_repeat_count(t, 1);
    }
  }
}

// Podpina efekt "wypuklego, wciskanego" przycisku pod dowolny klikalny
// obj - uzywane zarowno dla przyciskow panelu, jak i calego kafelka POMPA.
static void press_flash_zainstaluj(lv_obj_t *obj, uint16_t kolorNormalny, uint8_t cienSzerokoscSpoczynek) {
  PressFlashCtx *ctx = (PressFlashCtx *)malloc(sizeof(PressFlashCtx));
  ctx->kolorNormalny = kolorNormalny;
  ctx->cienSzerokoscSpoczynek = cienSzerokoscSpoczynek;
  ctx->pressStartMs = 0;
  lv_obj_set_user_data(obj, ctx);

  lv_obj_set_style_shadow_color(obj, rgb565(0x0000), 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_60, 0);
  press_flash_zastosuj(obj, false); // ustawia startowy (niewcisniety) cien/kolor/przesuniecie

  lv_obj_add_event_cb(obj, press_flash_event_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(obj, press_flash_event_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(obj, press_flash_event_cb, LV_EVENT_PRESS_LOST, nullptr);
}

// [UI-POMPA-PANEL] Przycisk panelu "ikona + podpis" - user pierwotnie chcial
// "tylko animowane ikonki" (bez napisow), ale po zobaczeniu na sprzecie
// zmienil zdanie: "powinny byc napisy a nie same ikony" - ROZUMIEMY jako
// "same ikony sa niejednoznaczne, potrzebny tekst pod spodem". Animowana
// ikona ZOSTAJE (translate_y/recolor_opa/podmiana klatek nadal dziala na
// samym obiekcie lv_img - patrz pompa_panel_animacje_start()), dochodzi
// tylko maly lv_label pod nia, ulozony w kolumnie (LV_FLEX_FLOW_COLUMN).
// `podpis == nullptr` pomija etykiete (zostawia czysta ikone, na wypadek
// gdyby jednak gdzies byla potrzebna) - w praktyce dziś wszystkie wywolania
// podaja tekst. Zwraca WSKAZNIK DO IKONY (nie do przycisku) przez
// ikonaWyjscie - pompa_panel_animacje_start() potrzebuje bezposredniego
// uchwytu do obiektu obrazka.
// [FIX UX, ta sesja] Dolaczony 6. parametr `etykietaWyjscie` (domyslnie
// nullptr, wiec wszystkie ISTNIEJACE wywolania tej funkcji w pliku
// dzialaja bez zmian) - pozwala wywolujacemu zapamietac uchwyt do LABELA
// z podpisem, zeby moc go pozniej podmienic w runtime. Uzyty w pierwszej
// turze tej sesji dla pojedynczego przycisku "TRYB PRACY" (usunietego w
// drugiej turze - patrz komentarz przy pompaTrybCzasowyBtn); zostaje w
// sygnaturze jako ogolny mechanizm, na wypadek gdyby inny przycisk
// potrzebowal podmiennego podpisu w przyszlosci. Wczesniej etykieta byla
// tworzona i od razu "gubiona" - nie dalo sie jej zmienic bez tego wyjscia.
static lv_obj_t *panel_przycisk_ikona(lv_obj_t *parent, const lv_img_dsc_t *ikona,
                                       uint16_t kolor, lv_event_cb_t cb,
                                       lv_obj_t **ikonaWyjscie,
                                       const char *podpis = nullptr,
                                       lv_obj_t **etykietaWyjscie = nullptr) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 74, 78);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  // [FIX - "AUTOMATYCZNY nadal sie nie miesci"] Domyslny motyw LVGL daje
  // przyciskowi spory wewnetrzny padding (ok. 10px z kazdej strony) plus
  // odstep miedzy dziecmi flex (ikona/podpis) - to razem zjada ok. 20-25px
  // szerokosci, ktore normalnie poszlyby na tekst. Dla krotkich podpisow
  // (AUTO, RECZNY) nie bylo tego widac, ale nawet po poszerzeniu przyciskow
  // do 138-148px "AUTOMATYCZNY"/"BEZPIECZNA" wciaz sie lamaly/przycinaly, bo
  // caly przyrost szerokosci szedl w padding, nie w tekst. Zmniejszamy
  // padding/odstep do minimum, zeby oddac to miejsce z powrotem etykiecie.
  lv_obj_set_style_pad_all(btn, 3, 0);
  lv_obj_set_style_pad_row(btn, 2, 0);

  press_flash_zainstaluj(btn, kolor, 5);

  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *img = lv_img_create(btn);
  lv_img_set_src(img, ikona);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(img, rgb565(KOL_TEKST), 0);

  if (podpis != nullptr) {
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, podpis);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    // [FIX] Odrobine ciasniejszy tracking - dla "AUTOMATYCZNY"/"BEZPIECZNA"
    // (najdluzsze podpisy w projekcie) to kilka dodatkowych pikseli, ktore
    // czesto decyduja, czy slowo zmiesci sie w jednej linii, czy zawinie sie
    // na druga (a przy 92px wysokosci przycisku druga linia juz koliduje z
    // dolna krawedzia).
    lv_obj_set_style_text_letter_space(lbl, -1, 0);
    // [FIX] Dluzsze podpisy (np. "AUTOMATYCZNY", "TRYB PRACY") nie miesza
    // sie na jednej linii w waskim przycisku (74px) - zawijanie zamiast
    // przycinania/wychodzenia poza przycisk.
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, LV_PCT(100));
    if (etykietaWyjscie != nullptr) *etykietaWyjscie = lbl;
  }

  if (ikonaWyjscie != nullptr) *ikonaWyjscie = img;
  return btn;
}

// [FIX DOTYK/WYGLAD, ta sesja - Partia 2] Krzyzyk zamkniecia jako
// PRAWDZIWY okragly przycisk (czerwone kolko + biala obwodka + bialy "X"),
// zamiast samego kolorowego tekstu z poprzedniej tury tej sesji. User
// pokazal referencyjna grafike (czerwone/zielone "znaczki" - kolko z
// obwodka i symbolem w srodku, taki sam ksztalt jak systemowe ikony
// zatwierdz/anuluj) i poprosil o dokladnie taki wyglad.
//
// Zbudowany na TYM SAMYM lv_btn_create()+press_flash_zainstaluj() co KAZDY
// inny przycisk w tym pliku (patrz panel_przycisk_ikona() wyzej) - wiec ma
// dokladnie to samo "wcisniecie"/cien co reszta UI, nie jest osobnym,
// niespojnym stylem. press_flash_zainstaluj() SAM ustawia bg_color
// (parametr kolorNormalny) - nie trzeba tego robic osobno, patrz jej
// definicja kilkadziesiat linii wyzej.
//
// LV_RADIUS_CIRCLE = pelne zaokraglenie niezaleznie od rozmiaru obiektu
// (nie trzeba przeliczac promienia recznie przy zmianie rozmiaru kolka).
// lv_obj_set_ext_click_area (12px) - dodatkowo do samego powiekszenia
// wizualnego, ten sam mechanizm co juz uzyty przy alarmWskaznikDash.
//
// Zastepuje 12 osobnych kopii "kolorowy tekst + ext_click_area" (jedna na
// kazdym panelu/ekranie) jedna wspolna funkcja - latwiej dalej dostosowac
// wyglad w JEDNYM miejscu zamiast 12 razy z rzedu.
static lv_obj_t *krzyzyk_zamkniecia_utworz(lv_obj_t *parent, lv_event_cb_t cb,
                                            int16_t offX = -10, int16_t offY = 10) {
  lv_obj_t *kolko = lv_btn_create(parent);
  lv_obj_set_size(kolko, 34, 34);
  lv_obj_set_style_radius(kolko, LV_RADIUS_CIRCLE, 0);
  lv_obj_clear_flag(kolko, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_color(kolko, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_border_width(kolko, 2, 0);
  lv_obj_set_style_border_opa(kolko, LV_OPA_COVER, 0);
  lv_obj_align(kolko, LV_ALIGN_TOP_RIGHT, offX, offY);
  press_flash_zainstaluj(kolko, KOL_ZAMKNIJ, 4);
  lv_obj_add_event_cb(kolko, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_ext_click_area(kolko, 12);

  lv_obj_t *x = lv_label_create(kolko);
  lv_label_set_text(x, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_font(x, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(x, rgb565(KOL_TEKST), 0);
  lv_obj_center(x);

  return kolko;
}


static void pompa_panel_otworz() {
  Serial.println("[LVGL-DIAG] pompa_panel_otworz() wywolane (kafelek dotkniety) - otwarcie calej strony POMPA");
  // [ROZBUDOWA, ta sesja] Byla to zmiana widocznosci malego overlay na tym
  // samym ekranie - teraz to prawdziwa nawigacja na DRUGI EKRAN (user: "cala
  // nowa strona"). auto_del=false - pompaPanel (drugi ekran) NIE moze zostac
  // skasowany po zejsciu z niego, bo trzyma wszystkie uchwyty/animacje na
  // stale, wielokrotnego uzytku.
  // [FIX tej sesji - LAGI] Bylo LV_SCR_LOAD_ANIM_MOVE_LEFT (wjazd z prawej)
  // - user zglosil "dwa przyciete ruchy". Patrz obszerny komentarz przy
  // LV_SCR_LOAD_ANIM_NONE w pompa_panel_zamknij() ponizej co do przyczyny
  // (SPI + pojedynczy 40-wierszowy bufor, brak PSRAM - animowane MOVE
  // przerysowuje CALE DWA ekrany klatka po klatce, realnie kilka FPS).
  // Ta sama poprawka tutaj, dla symetrii otwarcie/zamkniecie.
  // [FIX UX, ta sesja - druga tura] pompaStronaWybor zlikwidowana (patrz
  // komentarz przy deklaracji pompaTrybCzasowyBtn) - pompaStronaGlowna to
  // teraz JEDYNA podstrona panelu POMPA, wiec nie ma juz czego chowac.
  lv_obj_clear_flag(pompaStronaGlowna, LV_OBJ_FLAG_HIDDEN);
  pompa_tryb_odswiez(); // upewnij sie, ze podswietlenie CZASOWY/TEMP/AUTO pokazuje NAJSWIEZSZA wartosc przed pokazaniem strony
  pompa_panel_glowna_odswiez(); // [FIX UX, ta sesja] jw. dla podswietlenia AUTO/WLACZ/WYLACZ
  lv_scr_load_anim(pompaPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  pompa_panel_animacje_start();
  // [IDLE-TIMEOUT] Patrz pompa_idle_timer_resetuj() - startuje odliczanie
  // POMPA_STRONA_IDLE_MS od momentu otwarcia strony.
  pompa_idle_timer_resetuj();
}

static void pompa_kafelek_klikniety_cb(lv_event_t *e) {
  (void)e;
  pompa_panel_otworz();
}

// ============================================================================

// Jeden kafelek "podglad" (bez interakcji) - ZEWN/BOJLER/DYM w tym POC.
// Zwraca uchwyt do labelki WARTOSCI, zeby wolajacy mogl ja pozniej
// aktualizowac realnymi danymi (lvgl_dashboard_poc_aktualizuj()).
// [UI-WIELOKOLOR] ikona2/kolorIkony2 (domyslnie brak) - druga warstwa
// nalozona DOKLADNIE na ta sama pozycje co ikona/kolorIkony, przebarwiana
// osobno. Format LV_IMG_CF_ALPHA_1BIT sam w sobie jest jednokolorowy
// (recolor to JEDEN kolor na caly obrazek), wiec "prawdziwy" wielokolorowy
// symbol = dwa obrazki 1-bit tego samego rozmiaru nalozone na siebie, kazdy
// w innym kolorze - dokladnie ten sam trik co wczesniejsza (juz usunieta)
// wersja ikony POMPA z osobna obudowa+wirnikiem. Uzycie: BOJLER (kontur
// pomaranczowy + woda niebieska), patrz STATUS_PROJEKTU_LVGL.md pkt 24.
static lv_obj_t *kafelek(lv_obj_t *parent, int x, int y, int w, int h,
                          const char *etykieta, const char *wartosc,
                          uint16_t kolorRamki, const lv_img_dsc_t *ikona, uint16_t kolorIkony,
                          lv_obj_t **ikonaUchwytWyjscie,
                          const lv_img_dsc_t *ikona2 = nullptr, uint16_t kolorIkony2 = 0,
                          lv_obj_t **kartaUchwytWyjscie = nullptr) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, rgb565(KOL_KARTA), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_color(card, rgb565(kolorRamki), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
  // [ROZSZERZENIE, ta sesja - alarm PANEL] Opcjonalny uchwyt do karty, ten
  // sam wzorzec co ikonaUchwytWyjscie - domyslnie nullptr, wiec ZERO wplywu
  // na dotychczasowych wywolujacych (BOJLER, ZEWN, POKOJ, itd.), ktorzy go
  // nie podaja. Uzywany na razie tylko przez kafelek PANEL (patrz
  // panel_alarm_odswiez()), zeby moc przebarwic ramke przy alarmie
  // przegrzania z centrali.
  if (kartaUchwytWyjscie != nullptr) *kartaUchwytWyjscie = card;

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, etykieta);
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 6);

  // [FIX tej sesji, PRZELICZONE - nie "na oko"] Poprzednia wersja (wartosc
  // zaraz pod etykieta, y=30) usuwala pusta dziure w SRODKU karty, ale
  // zostawiala ikone (wtedy 24px) samotna w gornym rogu z duzo wolnej
  // przestrzeni kolo niej, a wartosc "przyklejona" do etykiety bez oddechu -
  // user: "ikona ma spora przestrzen, sformatuj inaczej". Zamiast zgadywac
  // kolejny offset, policzone z realnych danych:
  //  - lv_font_montserrat_28.c (zrodlo LVGL, sekcja FONT DESCRIPTOR):
  //    .line_height = 30, .base_line = 5 -> etykieta wartosci zajmuje
  //    PELNE 30px wiersza niezaleznie od tresci (cyfry siegaja od y+5 do
  //    y+25 w tym wierszu, .base_line=5 to zapas pod spodem na "ogonki" jak
  //    g/y, nieuzywany w wartosciach liczbowych, ale zarezerwowany).
  //  - ikona teraz 32x32 (patrz IKONA_ROZMIAR w ikony.h) zaczyna sie w tym
  //    samym y co etykieta (6) -> dol ikony = 6+32 = 38.
  //  - wartosc startuje 14px PONIZEJ dolu ikony (odstep czytelny na oko na
  //    fizycznym LCD, nie "0" i nie przypadkowy) = 38+14 = 52, konczy sie
  //    na 52+30 = 82.
  //  - margines pod wartoscia do dolu karty (kh=90) = 90-82 = 8px - maly,
  //    celowy, NIE kolejna czarna dziura jak poprzednio (tam bylo 26-27px).
  //  Efekt: ikona i wartosc siedza w oddzielnych pionach (dol ikony 38 <
  //  gora wartosci 52), wiec NIE MOGA nachodzic na siebie bez wzgledu na
  //  szerokosc tekstu wartosci - sprawdzone na najgorszym przypadku BOJLERA
  //  "100.0 C" (policzone z realnych adv_w glifow w lv_font_montserrat_28.c:
  //  ~101px, mieisci sie w kw=114 z 8px marginesem lewym).
  lv_obj_t *val = lv_label_create(card);
  lv_label_set_text(val, wartosc);
  lv_obj_set_style_text_color(val, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
  lv_obj_align(val, LV_ALIGN_TOP_LEFT, 8, 52);

  if (ikona != nullptr) {
    lv_obj_t *img = lv_img_create(card);
    lv_img_set_src(img, ikona);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img, rgb565(kolorIkony), 0);
    lv_obj_align(img, LV_ALIGN_TOP_RIGHT, -4, 6);
    if (ikonaUchwytWyjscie != nullptr) *ikonaUchwytWyjscie = img;

    // [UI-WIELOKOLOR] Druga warstwa, identyczne wyrownanie -> nakladka
    // idealnie pokrywa sie z pierwsza (obie ikony musza miec ten sam
    // rozmiar zrodlowy, patrz IKONA_BOJLER_ROZMIAR).
    if (ikona2 != nullptr) {
      lv_obj_t *img2 = lv_img_create(card);
      lv_img_set_src(img2, ikona2);
      lv_obj_set_style_img_recolor_opa(img2, LV_OPA_COVER, 0);
      lv_obj_set_style_img_recolor(img2, rgb565(kolorIkony2), 0);
      lv_obj_align(img2, LV_ALIGN_TOP_RIGHT, -4, 6);
    }
  }

  return val;
}

// Kafelek POMPA - interaktywny: dotkniecie otwiera panel AUTO/RECZNIE.
static void kafelek_pompa(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_color(card, rgb565(KOL_OGIEN), 0); // sterowalny -> ramka akcentowa, jak KLAPA/SYBERKA w main.cpp centrali
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);

  // [UI-3D] Ten sam gwarantowany-minimalny-czas efekt "wcisniecia" co
  // przyciski panelu (patrz press_flash_zainstaluj wyzej) - kafelek
  // ciemnieje, "wpada" w dol o 3px i traci wiekszosc cienia na czas dotyku,
  // z gwarancja co najmniej jednej widocznej klatki na ekranie. Cien
  // spoczynkowy celowo cieńszy (3px) niz przy przyciskach panelu (6px) -
  // kafelki stoja w rzedzie z tylko 4px odstepu, grubszy cien wyciekalby
  // na sasiedni kafelek.
  press_flash_zainstaluj(card, KOL_KARTA, 3);

  lv_obj_add_event_cb(card, pompa_kafelek_klikniety_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, "POMPA");
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 4);

  // [FIX-NACHODZENIE, ta sesja] Wartosc kafelka POMPA to TEKST ("30m"), nie
  // krotka liczba jak w innych kafelkach. Font 14 (nie 28) dla tej JEDNEJ
  // etykiety - patrz historia w STATUS_PROJEKTU_LVGL.md (punkt 9).
  // Offset x=36 (bylo 28) - dlon obok urosla z 22px na 30px (user: "reka sie
  // zlewa, zrob lepsza"), wiec stary offset 28 nachodzil na dlon (2 do 32px
  // od lewej), zostawiajac -4px "zapasu" zamiast przerwy. 36 = 2 (start
  // dloni) + 30 (jej szerokosc) + 4px bezpiecznej przerwy, dokladnie jak
  // przy poprzednim rozmiarze.
  pompaValLabel = lv_label_create(card);
  lv_obj_set_style_text_color(pompaValLabel, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_text_font(pompaValLabel, &lv_font_montserrat_14, 0);
  // [Partia 4, ta sesja] User na zywym sprzecie: "reka i napis minut lekko
  // w prawo" - x=36 -> 40 (ten sam +4px zapas przeniesiony w prawo razem z
  // dlonia ponizej, zeby odstep miedzy nimi zostal identyczny).
  lv_obj_align(pompaValLabel, LV_ALIGN_BOTTOM_LEFT, 40, -6);

  // [UI-RECZNIE, FIX-NACHODZENIE ta sesja] Znaczek dloni po lewej od tekstu
  // wartosci - widoczny tylko w trybie recznym, patrz pompa_tile_odswiez().
  // 30x30 (IKONA_RECZNIE_ROZMIAR w ikony.h) - kafelek ma kh=90 (nie 46 jak
  // mowil stary, juz nieaktualny komentarz sprzed powiekszenia siatki), wiec
  // pionowo jest mnostwo zapasu; jedyny realny problem byl POZIOMY, patrz
  // offset pompaValLabel wyzej. NIE uzywamy lv_img_set_zoom() - format 1-bit
  // nie wspiera transformacji w LVGL v8 (patrz "Wazne ograniczenie LVGL v8"
  // w STATUS_PROJEKTU_LVGL.md).
  pompaRecznieIcon = lv_img_create(card);
  lv_img_set_src(pompaRecznieIcon, &lv_ikona_recznie);
  lv_obj_set_style_img_recolor_opa(pompaRecznieIcon, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(pompaRecznieIcon, rgb565(KOL_KROPLA), 0);
  // [Partia 4, ta sesja] +4px w prawo, razem z pompaValLabel wyzej (ten sam
  // powod - user: "reka i napis minut lekko w prawo").
  lv_obj_align(pompaRecznieIcon, LV_ALIGN_BOTTOM_LEFT, 6, -1);
  // [UI-POMPA-OVERRIDE, ta sesja] ZAWSZE widoczna (nie HIDDEN/CLEAR jak
  // poprzednio) - zamiast znikac calkowicie w AUTO, jest tylko PRZYGASZONA
  // (niska img_opa), a w trybie recznego override pelna. User widzi wprost
  // "czy ktos to teraz recznie nadpisal" zamiast zgadywac czy ikony tam po
  // prostu nie ma. Faktyczny poziom opa ustawia pompa_tile_odswiez().
  lv_obj_set_style_img_opa(pompaRecznieIcon, LV_OPA_30, 0); // domyslnie AUTO -> przygaszona

  // [UI-POMPA-GLOWNY-KAFELEK, ta sesja] Znaczek strategii (Czasowy/Temp/
  // Automatyczny) - zawsze widoczny. [FIX-NACHODZENIE, ta sesja] TOP_RIGHT
  // nachodzil na pompaArc (48x48 przy RIGHT_MID siega az do y=17 od gory) -
  // user: "nachodzi na tryb pracy, przesun pod napisy POMPA a nad reka".
  // Przeliczone: etykieta "POMPA" (TOP_LEFT 8,4, font 14) konczy sie
  // ok. y=22; dlon (BOTTOM_LEFT 6,-1, 30px, patrz nizej) zaczyna sie od
  // y=59 (kh=90 - 1 - 30). Miedzy nimi ~37px wolnej przestrzeni po lewej
  // (przed poczatkiem pompaIcon/pompaArc na x~65) - 20px znaczek miesci sie
  // z zapasem. TOP_LEFT 6,24 (2px odstepu od etykiety). Kolor zalezny od
  // trybu (ustawiany w pompa_tile_odswiez()) - te same 3 kolory co na
  // przyciskach wyboru strategii (pompaTrybCzasowyBtn/pompaTrybTempBtn/
  // pompaTrybAutoBtn, patrz pompa_panel_tryb_odswiez()): KOL_ZAR=Czasowy,
  // KOL_KROPLA=Temp, KOL_OGIEN=Automatyczny.
  pompaStrategiaIcon = lv_img_create(card);
  lv_img_set_src(pompaStrategiaIcon, &lv_ikona_automatyczny_mala);
  lv_obj_set_style_img_recolor_opa(pompaStrategiaIcon, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(pompaStrategiaIcon, rgb565(KOL_OGIEN), 0);
  lv_obj_align(pompaStrategiaIcon, LV_ALIGN_TOP_LEFT, 6, 24);

  // [UI-POMPA] Ikona pompy (32x32) - wybrany wariant "wentylator" (4
  // lopatki, bez obudowy) po porownaniu 5 kandydatow na zywo na LCD, patrz
  // historia w STATUS_PROJEKTU_LVGL.md (punkty 15-16). JEDEN obiekt lv_img,
  // cala bitmapa cyklicznie podmieniana miedzy 8 klatkami przez
  // pompa_wirnik_timer_cb() gdy pompa pracuje (POPRZEDNIO byly to dwa
  // obiekty - statyczna obudowa + osobny maly wirnik lopatek na wierzchu;
  // ten ksztalt nie ma obudowy, wiec podzial na warstwy juz niepotrzebny).
  //
  // [FIX-ROZKLAD ta sesja, user: "pompa moze nizej"] Wyrownanie zmienione z
  // TOP_RIGHT na RIGHT_MID (pionowy srodek karty) - przy kh=90 stary TOP_RIGHT
  // zostawial ikone przyklejona do gornej krawedzi z duza, pusta przestrzenia
  // pod nia w trybie AUTO (etykieta wartosci wtedy pusta, patrz
  // pompa_tile_odswiez()). Wyrownanie do pionowego srodka rozklada karte
  // rownomiernie. Poziomo bez zmian (x=-6, prawa krawedz) - nie koliduje z
  // dlonia/tekstem po lewej stronie (rozne kolumny X).
  pompaIcon = lv_img_create(card);
  lv_img_set_src(pompaIcon, POMPA_KLATKI[0]);
  lv_obj_set_style_img_recolor_opa(pompaIcon, LV_OPA_COVER, 0);
  // [Partia 4, ta sesja] User na zywym sprzecie: "pompa ciut wieksza lekko
  // w gore". Rozmiar: nowa bitmapa 38px (bylo 32, patrz
  // IKONA_POMPA_ROZMIAR w ikony.h - wszystkie 8 klatek przeskalowane).
  // Pozycja: y=-4 (bylo 0) wzgledem RIGHT_MID - kilka px w gore.
  lv_obj_align(pompaIcon, LV_ALIGN_RIGHT_MID, -6, -4);

  // Luk odliczania wokol ikony (5px marginesu z kazdej strony wokol ikony
  // POMPA, jak poprzednio) - wyrownanie RIGHT_MID zgodne z pompaIcon wyzej
  // (ten sam pionowy srodek/przesuniecie, x=-1 zamiast -6 bo luk jest
  // szerszy o te same 5px z kazdej strony). Rozmiar 48x48 = nowe
  // IKONA_POMPA_ROZMIAR (38) + 2*5 marginesu (bylo 42x42 przy 32px ikonie).
  pompaArc = lv_arc_create(card);
  lv_obj_set_size(pompaArc, 48, 48);
  lv_obj_align(pompaArc, LV_ALIGN_RIGHT_MID, -1, -4);
  lv_arc_set_bg_angles(pompaArc, 0, 360);
  lv_arc_set_rotation(pompaArc, 270);
  lv_arc_set_range(pompaArc, 0, 100);
  lv_obj_remove_style(pompaArc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(pompaArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(pompaArc, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_width(pompaArc, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(pompaArc, rgb565(KOL_RAMKA), LV_PART_MAIN);
  lv_obj_set_style_arc_color(pompaArc, rgb565(KOL_OGIEN), LV_PART_INDICATOR);
  lv_obj_add_flag(pompaArc, LV_OBJ_FLAG_HIDDEN);

  // [UI-POMPA-OVERRIDE, ta sesja] Ukosna kreska "wylaczone na sile" nad
  // ikona pompy - patrz obszerny komentarz przy deklaracji pompaPrzekreslenie.
  // Zwykla belka (46x4) wysrodkowana na tym samym punkcie co pompaIcon/
  // pompaArc (RIGHT_MID, -6/-4), obrocona o 45 st. stylem transform_angle
  // (jednostki LVGL to dziesiate stopnia, wiec 450 = 45.0 st.). Pivot
  // ustawiony jawnie na srodek belki, zeby obrot byl wokol jej wlasnego
  // srodka, nie (0,0).
  pompaPrzekreslenie = lv_obj_create(card);
  lv_obj_remove_style_all(pompaPrzekreslenie);
  lv_obj_set_size(pompaPrzekreslenie, 46, 4);
  lv_obj_set_style_bg_color(pompaPrzekreslenie, rgb565(KOL_ERR), 0);
  lv_obj_set_style_bg_opa(pompaPrzekreslenie, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(pompaPrzekreslenie, 2, 0);
  lv_obj_clear_flag(pompaPrzekreslenie, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(pompaPrzekreslenie, LV_ALIGN_RIGHT_MID, -6, -4);
  lv_obj_set_style_transform_pivot_x(pompaPrzekreslenie, 23, 0);
  lv_obj_set_style_transform_pivot_y(pompaPrzekreslenie, 2, 0);
  lv_obj_set_style_transform_angle(pompaPrzekreslenie, 450, 0);
  lv_obj_add_flag(pompaPrzekreslenie, LV_OBJ_FLAG_HIDDEN); // domyslnie brak nadpisania -> ukryta

  lv_label_set_text(pompaValLabel, "--"); // do pierwszego pobierzStatus()

  // Timer animacji ikony - jeden na caly kafelek, chodzi caly czas w tle,
  // ale realnie przerysowuje tylko gdy pompa pracuje (patrz funkcja). 8
  // klatek co 150ms = pelny obrot co 1200ms (bylo 4 klatki -> 600ms; wolniej,
  // ale plynniej dzieki mniejszemu krokowi katowemu 45 st. zamiast 90 st.).
  lv_timer_create(pompa_wirnik_timer_cb, 150, nullptr);
}

static void pompa_panel_zbuduj(lv_obj_t *scr) {
  // [ROZBUDOWA, ta sesja] scr tu = GLOWNY dashboard, zapamietywany do
  // powrotu (patrz pompa_panel_zamknij). pompaPanel to teraz PRAWDZIWY
  // DRUGI EKRAN - lv_obj_create(NULL) tworzy nowy, niezalezny ekran (nie
  // dziecko scr), dokladnie tak jak robi to LVGL wewnetrznie przy starcie
  // dla domyslnego lv_scr_act(). Pelny rozmiar 480x320 (bez set_size -
  // nowy ekran domyslnie dziedziczy rozdzielczosc wyswietlacza).
  dashboardScr = scr;
  pompaPanel = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(pompaPanel, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(pompaPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(pompaPanel, LV_OBJ_FLAG_SCROLLABLE);

  // --- Jedyna podstrona: GLOWNA (AUTO/WL/WYL + strategia pracy WPROST) ---
  pompaStronaGlowna = lv_obj_create(pompaPanel);
  lv_obj_remove_style_all(pompaStronaGlowna);
  lv_obj_set_size(pompaStronaGlowna, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(pompaStronaGlowna, LV_OBJ_FLAG_SCROLLABLE);

  // [UI-POMPA-STRONA] User pierwotnie chcial "nie chce napisow tylko
  // animowane ikonki", ale po zobaczeniu na sprzecie zmienil zdanie:
  // "powinny byc napisy a nie same ikony" - kazdy przycisk ma teraz maly
  // podpis pod ikona (patrz panel_przycisk_ikona()), animacja ikony
  // zostaje bez zmian. Rzad 1: reczny override (3 ikony+podpisy). Rzad 2:
  // JEDNA ikona "aktualny tryb pracy automatyki", wizualnie oddzielona
  // (patrz separator nizej) bo to inna os ustawien.
  // [FIX tej sesji - "nie miesci sie"] Tak jak w panelu SERWO (patrz
  // rzadTryb w serwo_panel_zbuduj() nizej), domyslny 74px przycisk z
  // panel_przycisk_ikona() jest za waski dla podpisow "WLACZ"/"WYLACZ" -
  // font montserrat_14 lamal je w polowie slowa. Rzad poszerzony 280->340,
  // kazdy przycisk poszerzony 74->104 (nizej, ten sam wzorzec co servoAutoBtn
  // itd. - lv_obj_set_size PO stworzeniu).
  // [FIX UX, ta sesja - kolizja nazw "AUTO"/"AUTO"] Rzad poszerzony
  // 340->372 zeby zmiescic dluzszy podpis pompaAutoBtn (patrz nizej,
  // "AUTOMATYKA" zamiast "AUTO") bez zgniatania WLACZ/WYLACZ.
  lv_obj_t *rzad = lv_obj_create(pompaStronaGlowna);
  lv_obj_remove_style_all(rzad);
  lv_obj_set_size(rzad, 372, 90);
  lv_obj_align(rzad, LV_ALIGN_CENTER, 0, -55);
  lv_obj_set_flex_flow(rzad, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rzad, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(rzad, LV_OBJ_FLAG_SCROLLABLE);

  // [Kolorystyka - ta sesja] KOL_OGIEN zamiast neutralnego KOL_RAMKA - to
  // dokladnie ten sam turkus, ktorym oznaczona jest ramka/luk kafelka POMPA
  // na glownej siatce (patrz kafelek_pompa()), wiec panel od razu czuje sie
  // jak kontynuacja tego samego kafelka, nie osobny, "odbarwiony" ekran.
  // [FIX UX, ta sesja] Przypisanie do statycznych uchwytow pliku (byly
  // lokalne) - patrz komentarz przy deklaracji pompaAutoBtn/pompaWlBtn/
  // pompaWylBtn wyzej i pompa_panel_glowna_odswiez() nizej.
  // [FIX UX, ta sesja - kolizja nazw "AUTO"/"AUTO"] User: "jak wejde w
  // pompe to mam u gury auto i na dole tez auto ... mylace". Research
  // (patrz notatka w rozmowie): zaden termostat z ktorego wzorcow
  // korzystalismy nie powtarza tego samego slowa na dwoch poziomach
  // hierarchii - gorny poziom to reczny override/HOLD ("Cancel Hold"
  // odpowiada dokladnie pompaManualOverrideUntil=0 w main_centrala.cpp,
  // patrz komentarz tam), dolny poziom (rzadTrybPracy nizej) to trwala
  // STRATEGIA automatyki (Wybor 1/2/3 w main_centrala.cpp - tam tez
  // podpisany "Automatyczny", zostaje bez zmian, patrz web panel centrali
  // linia ok. 4169). Zmieniamy wiec TYLKO gorny, zeby nie bylo dwoch
  // identycznych podpisow "AUTO" obok siebie na jednym ekranie.
  pompaAutoBtn = panel_przycisk_ikona(rzad, &lv_ikona_auto_0, KOL_OGIEN, pompa_btn_auto_cb, &pompaPanelAutoIcon, "AUTOMATYKA");
  lv_obj_set_size(pompaAutoBtn, 130, 78);
  pompaWlBtn = panel_przycisk_ikona(rzad, &lv_ikona_recznie, KOL_KROPLA, pompa_btn_wl_cb, &pompaPanelWlIcon, "WLACZ");
  lv_obj_set_size(pompaWlBtn, 104, 78);
  pompaWylBtn = panel_przycisk_ikona(rzad, &lv_ikona_recznie_przekreslona, KOL_TEKST_DIM, pompa_btn_wyl_cb, &pompaPanelWylIcon, "WYLACZ");
  lv_obj_set_size(pompaWylBtn, 104, 78);

  // Cienka pozioma linia-separator - czysto wizualny podzial "reczny
  // override" / "strategia automatyki", bez ani jednego slowa opisu.
  lv_obj_t *separator = lv_obj_create(pompaStronaGlowna);
  lv_obj_remove_style_all(separator);
  lv_obj_set_size(separator, 200, 1);
  lv_obj_align(separator, LV_ALIGN_CENTER, 0, 5);
  lv_obj_set_style_bg_color(separator, rgb565(KOL_RAMKA), 0);
  lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, 0);

  // [FIX UX, ta sesja - druga tura] Rzad 2: TRZY przyciski strategii WPROST
  // na stronie glownej (CZASOWY=Trociniak/TEMP=Kopciuch/AUTO=Automatyczny),
  // zamiast jednego posredniego "TRYB PRACY" - user: "tam gdzie AUTO/RECZNIE/
  // WLACZ/WYLACZ, nizej powinno byc [wprost] tryb pracy: czasowe/
  // temperaturowe/auto, a klikniecie ma OD RAZU pokazac jego parametry".
  // Ten sam wzorzec ukladu (kontener 460 szer. x100 wys., SPACE_EVENLY,
  // przyciski 140/140/150px) co poprzednio mialo miejsce na oddzielnej
  // podstronie WYBOR - przeniesiony bez zmian wymiarow (juz sprawdzony na
  // sprzecie w tamtym kontekscie, patrz PLAN_PANEL_SERWO_UPGRADE/HANDOFF).
  lv_obj_t *rzadTrybPracy = lv_obj_create(pompaStronaGlowna);
  lv_obj_remove_style_all(rzadTrybPracy);
  lv_obj_set_size(rzadTrybPracy, 460, 100);
  lv_obj_align(rzadTrybPracy, LV_ALIGN_CENTER, 0, 62);
  lv_obj_set_flex_flow(rzadTrybPracy, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rzadTrybPracy, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(rzadTrybPracy, LV_OBJ_FLAG_SCROLLABLE);

  // [Kolorystyka - bez zmian wzgledem poprzedniej podstrony WYBOR] KOL_ZAR
  // dla CZASOWY (Trociniak - "reczne" opalanie trocinami), KOL_KROPLA dla
  // TEMP (Kopciuch - histereza temperaturowa), KOL_OGIEN dla AUTO
  // (Automatyczny - ten sam turkus co kafelek POMPA i akcent AUTO w rzedzie
  // 1 wyzej).
  pompaTrybCzasowyBtn = panel_przycisk_ikona(rzadTrybPracy, &lv_ikona_trociniak, KOL_ZAR, pompa_btn_trociniak_cb, nullptr, "CZASOWY");
  lv_obj_set_size(pompaTrybCzasowyBtn, 140, 92);
  pompaTrybTempBtn = panel_przycisk_ikona(rzadTrybPracy, &lv_ikona_kopciuch, KOL_KROPLA, pompa_btn_kopciuch_cb, nullptr, "TEMP");
  lv_obj_set_size(pompaTrybTempBtn, 140, 92);
  pompaTrybAutoBtn = panel_przycisk_ikona(rzadTrybPracy, &lv_ikona_automatyczny, KOL_OGIEN, pompa_btn_automatyczny_cb, nullptr, "AUTO");
  lv_obj_set_size(pompaTrybAutoBtn, 150, 92);

  // [FIX WYGLAD, ta sesja - Partia 2] Okragly kolorowy krzyzyk (user
  // pokazal referencyjna grafike) - patrz krzyzyk_zamkniecia_utworz() i
  // pelny komentarz przy niej, kilkadziesiat linii wyzej.
  krzyzyk_zamkniecia_utworz(pompaStronaGlowna, pompa_btn_zamknij_cb);

  // [ROZSZERZENIE, ta sesja] Zebatka w przeciwnym rogu (gora-lewo, symetrycznie
  // do X gora-prawo) - wejscie na ekran USTAWIENIA POMPY (5 parametrow
  // strojenia + antystop, patrz pompa_ustawienia_zbuduj() nizej) BEZ zmiany
  // strategii - zostaje jako szybki skrot "pokaz wszystkie parametry, jakie
  // by nie byly" niezaleznie od przyciskow CZASOWY/TEMP/AUTO powyzej.
  lv_obj_t *pompaZebatka = lv_img_create(pompaStronaGlowna);
  lv_img_set_src(pompaZebatka, &lv_ikona_zebatka);
  lv_obj_set_style_img_recolor(pompaZebatka, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_img_recolor_opa(pompaZebatka, LV_OPA_COVER, 0);
  lv_obj_align(pompaZebatka, LV_ALIGN_TOP_LEFT, 6, 4);
  lv_obj_add_flag(pompaZebatka, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pompaZebatka, pompa_btn_ustawienia_cb, LV_EVENT_CLICKED, nullptr);
}

// ============================================================================
// [ROZSZERZENIE, ta sesja - panel SERWO] KLAPA i SYBERKA dostaja panel
// AUTO/RECZNIE + slider pozycji, na wzor panelu POMPA, ale PROSCIEJ:
//  - JEDNA podstrona (nie dwie jak POMPA) - nie ma tu odpowiednika "wyboru
//    strategii", wiec nie ma po co dzielic na zakladki.
//  - Tryb reczny jest TRWALY (user: "jak w koncepcie HTML") - brak arc/
//    hold-timera/odliczania minut, ktore ma kafelek POMPA.
//  - Panel jest WSPOLDZIELONY miedzy KLAPA i SYBERKA - dwa kafelki, jeden
//    ekran, tak jak jeden pompaPanel obsluguje jedna pompe. [POPRAWKA, ta
//    sesja - panel SERWO v3] Wczesniej panel pokazywal tresci "aktywnej"
//    osi (ktora zostala kliknieta) po kolei; teraz pokazuje OBIE osie
//    naraz (wspolny tryb u gory, dwa niezalezne suwaki nizej) - patrz
//    komentarz przy deklaracji serwoPanel wyzej co do uzasadnienia.
// ============================================================================

// [POPRAWKA, ta sesja - panel SERWO v2] Dwie osobne funkcje wysylki zamiast
// jednej serwo_wyslij(id, reczny, pozycja) - odzwierciedlaja dwa osobne
// endpointy/callbacki centrali (patrz ServoTrybCb/ServoPozycjaCb w .h).
// `serwo_tryb_wyslij` zmienia WSPOLNY tryb dla obu osi na raz.
static void serwo_tryb_wyslij(int trybSerwa) {
  if (g_serwoTrybCb == nullptr) return;
  g_serwoTrybCb(trybSerwa);
  // [UI] Zero zmiany stanu "na wiare" tutaj - main.cpp po udanym POST wywola
  // kolejny pobierzStatus() -> lvgl_dashboard_poc_aktualizuj(), ktory dopiero
  // wtedy naprawde odswiezy kafelki/panel prawdziwa wartoscia z centrali.
}
// [POPRAWKA, ta sesja - panel SERWO v3] `id` teraz jawnym parametrem (0=klapa,
// 1=syberka) zamiast czytania z usunietego g_serwoAktywnyId - kazdy z dwoch
// niezaleznych suwakow (patrz serwo_panel_zbuduj()) przekazuje WLASNE id
// przez user_data callbacku, wiec funkcja juz nie musi zgadywac "ktora os
// jest aktywna" - obie sa "aktywne" naraz. Sensowne wylacznie gdy
// g_trybSerwa==2, wywolujacy (suwaki) sam o to dba (nieaktywne/niewidoczne
// poza trybem 2).
static void serwo_pozycja_wyslij(int id, int pozycja) {
  if (g_serwoPozycjaCb == nullptr) return;
  g_serwoPozycjaCb(id, pozycja);
}

// Podswietla aktywny przycisk trybu (AUTO/RECZNY/BEZPIECZNA), pokazuje/chowa
// wiersz slidera (tylko tryb 2) i podmienia sufiks opisu - wolane przy
// otwarciu panelu ORAZ po kazdym kliknieciu trybu.
static void serwo_panel_tryb_odswiez() {
  // [Kolorystyka - ta sesja] Trzy rozne kolory podswietlenia zamiast
  // wczesniejszego KOL_OGIEN/KOL_OGIEN/KOL_SUWAK (dwa z trzech przyciskow
  // pozyczaly turkus z kafelka POMPA) - teraz kazdy stan ma wlasny, sensowny
  // akcent: AUTO = neutralna szarosc kafelka w trybie auto (KOL_RAMKA),
  // RECZNY = ta sama pomarancz co ikona "recznie" na kafelkach KLAPA/
  // SYBERKA (KOL_SUWAK, bez zmian), BEZPIECZNA = nowy zielony KOL_BEZPIECZNA.
  lv_obj_set_style_bg_color(serwoAutoBtn,       rgb565(g_trybSerwa == 1 ? KOL_RAMKA : KOL_KARTA), 0);
  lv_obj_set_style_bg_color(serwoRecznyBtn,     rgb565(g_trybSerwa == 2 ? KOL_SUWAK : KOL_KARTA), 0);
  lv_obj_set_style_bg_color(serwoBezpiecznaBtn, rgb565(g_trybSerwa == 3 ? KOL_BEZPIECZNA : KOL_KARTA), 0);

  // [POPRAWKA, ta sesja] Wiersz z pozycja (suwak+etykieta+luk) byl wczesniej
  // CALKOWICIE ukrywany poza trybem Reczny (2) - user nie widzial nastaw w
  // AUTO/BEZPIECZNA. Teraz wiersz jest widoczny ZAWSZE (podglad aktualnej
  // pozycji we wszystkich trybach), a jedynie SAM SUWAK jest blokowany
  // (LV_OBJ_STATE_DISABLED) poza trybem 2 - w AUTO/BEZPIECZNA to juz nie
  // user decyduje o pozycji (steruje automatyka/bezpieczna wartosc), wiec
  // przeciaganie palcem nie powinno tam nic wysylac; wartosc jest i tak na
  // biezaco nadpisywana prawdziwymi danymi z centrali w innym miejscu
  // (aktualizacji stanu), suwak w trybach innych niz Reczny sluzy juz tylko
  // do podgladu.
  lv_obj_clear_flag(serwoRecznyWiersz, LV_OBJ_FLAG_HIDDEN);
  if (g_trybSerwa == 2) {
    lv_obj_clear_state(serwoSliderKlapa,   LV_STATE_DISABLED);
    lv_obj_clear_state(serwoSliderSyberka, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(serwoSliderKlapa,   LV_STATE_DISABLED);
    lv_obj_add_state(serwoSliderSyberka, LV_STATE_DISABLED);
  }

  // [POPRAWKA, ta sesja - panel SERWO v3] Opis juz nie sklejany z 2 czesci
  // (baza per-os + sufiks per-tryb) - jest teraz WYLACZNIE o trybie
  // (SERWO_TRYB_OPIS[]), bo obie osie zawsze dziela ten sam tryb. Prostsze:
  // brak bufora/snprintf, string jest juz gotowy.
  int trybIdx = g_trybSerwa - 1;
  if (trybIdx < 0 || trybIdx > 2) trybIdx = 0; // zabezpieczenie przed nieoczekiwana wartoscia z /api/status
  lv_label_set_text(serwoOpisLabel, SERWO_TRYB_OPIS[trybIdx]);
}

// ============================================================================
// [UI-SERWO-PANEL, Czesc 1 planu] Animacje 2 z 3 ikon trybu - dokladnie ten
// sam wzorzec i te same pomocnicze funkcje co panel POMPA
// (pompa_panel_animacje_start/stop() wyzej): dzialaja WYLACZNIE gdy panel
// SERWO jest widoczny (start w serwo_panel_otworz(), stop w
// serwo_panel_zamknij()), zeby nie zuzywac CPU/nie migac w tle.
// ============================================================================

// AUTO - podmiana calej bitmapy klatka-po-klatce (format 1-bit nie wspiera
// lv_img_set_angle(), patrz obszerny komentarz przy POMPA_KLATKI wyzej).
// Wspoldzieli PANEL_AUTO_KLATKI[8] z panelem POMPA (te same 8 obrazkow
// lv_ikona_auto_0..7) - wlasny licznik klatki i wlasny timer, bo to
// niezalezny obiekt lv_img na innym ekranie.
static void serwo_panel_auto_timer_cb(lv_timer_t *t) {
  (void)t;
  if (serwoAutoIcon == nullptr) return;
  g_serwoPanelAutoKlatka = (uint8_t)((g_serwoPanelAutoKlatka + 1) % 8);
  lv_img_set_src(serwoAutoIcon, PANEL_AUTO_KLATKI[g_serwoPanelAutoKlatka]);
}

static void serwo_panel_animacje_start() {
  // AUTO - timer klatek, startuje od 0 przy kazdym otwarciu (przewidywalne).
  g_serwoPanelAutoKlatka = 0;
  lv_img_set_src(serwoAutoIcon, PANEL_AUTO_KLATKI[0]);
  if (g_serwoPanelAutoTimer == nullptr) {
    g_serwoPanelAutoTimer = lv_timer_create(serwo_panel_auto_timer_cb, 150, nullptr);
  }

  // RECZNY - pulsujaca dlon (oddychanie przezroczystoscia w nieskonczonosc)
  // - user (handoff/plan): "RECZNY pulsujaca dlon". Ta sama funkcja
  // panel_anim_recolor_opa_cb() co WYLACZ RECZNIE w panelu POMPA - jest
  // generyczna (dziala na dowolnym lv_obj_t*), zero duplikacji kodu animacji.
  lv_anim_t aPulse;
  lv_anim_init(&aPulse);
  lv_anim_set_var(&aPulse, serwoRecznyIcon);
  lv_anim_set_exec_cb(&aPulse, panel_anim_recolor_opa_cb);
  lv_anim_set_values(&aPulse, LV_OPA_50, LV_OPA_COVER);
  lv_anim_set_time(&aPulse, 700);
  lv_anim_set_playback_time(&aPulse, 700);
  lv_anim_set_repeat_count(&aPulse, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&aPulse, lv_anim_path_ease_in_out);
  lv_anim_start(&aPulse);
}

static void serwo_panel_animacje_stop() {
  if (g_serwoPanelAutoTimer != nullptr) {
    lv_timer_del(g_serwoPanelAutoTimer);
    g_serwoPanelAutoTimer = nullptr;
  }
  // lv_anim_del kasuje kazda animacje przypieta do tej pary (var, exec_cb) -
  // dokladnie ta utworzona w serwo_panel_animacje_start() wyzej.
  lv_anim_del(serwoRecznyIcon, panel_anim_recolor_opa_cb);
  // Reset do stanu spoczynku, zeby kolejne otwarcie panelu zaczynalo czysto.
  if (serwoRecznyIcon) lv_obj_set_style_img_recolor_opa(serwoRecznyIcon, LV_OPA_COVER, 0);
}

static void serwo_panel_zamknij(); // forward - definicja kilka linii nizej, potrzebna juz w idle-timeout callbacku ponizej

static void serwo_panel_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_serwoPanelIdleTimer = nullptr; // timer sam sie usuwa (repeat_count=1), nie wolno go tu jeszcze raz kasowac
  serwo_panel_zamknij();
}

static void serwo_idle_timer_resetuj() {
  if (g_serwoPanelIdleTimer != nullptr) {
    lv_timer_del(g_serwoPanelIdleTimer);
    g_serwoPanelIdleTimer = nullptr;
  }
  g_serwoPanelIdleTimer = lv_timer_create(serwo_panel_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_serwoPanelIdleTimer, 1);
}

static void serwo_panel_zamknij() {
  // [FIX, ta sesja] Wraca PROSTO do dashboardScr - posredni ekran
  // "SERWO - WYBIERZ OS" (serwoWyborScr) zostal usuniety z nawigacji
  // (kafelek_serwo_wejscie() na dashboardzie otwiera teraz serwoPanel
  // bezposrednio, patrz callback ponizej przy kafelek_serwo_wejscie()),
  // wiec nie ma juz posredniego ekranu, do ktorego mozna by wracac.
  if (dashboardScr != nullptr) {
    lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  }
  if (g_serwoPanelIdleTimer != nullptr) {
    lv_timer_del(g_serwoPanelIdleTimer);
    g_serwoPanelIdleTimer = nullptr;
  }
  // [ROZSZERZENIE, ta sesja - Czesc 1 planu] Zatrzymaj animacje ikon trybu -
  // ten sam wzorzec co pompa_panel_zamknij()/pompa_panel_animacje_stop().
  serwo_panel_animacje_stop();
}

// [POPRAWKA, ta sesja - panel SERWO v3] Bez parametru `id` - panel juz nie
// pokazuje "aktywnej" osi, pokazuje OBIE naraz, wiec otwarcie inicjuje OBA
// suwaki od razu z prawdziwych ostatnich wartosci (g_klapaPozycja/
// g_syberkaPozycja), niezaleznie od tego, ktory kafelek user dotknal.
// serwoTytulLabel jest teraz stalym tekstem ustawionym raz w
// serwo_panel_zbuduj() - nie trzeba go tu odswiezac.
static void serwo_panel_otworz() {
  char bufor[8];
  lv_slider_set_value(serwoSliderKlapa, g_klapaPozycja, LV_ANIM_OFF); // start od PRAWDZIWEJ ostatniej wartosci, bez animacji przy otwarciu
  snprintf(bufor, sizeof(bufor), "%d%%", g_klapaPozycja);
  lv_label_set_text(serwoPozycjaLabelKlapa, bufor);
  if (serwoArcKlapa) lv_arc_set_value(serwoArcKlapa, g_klapaPozycja); // [Panel SERWO Czesc 2] luk startuje zgrany ze suwakiem/etykieta

  lv_slider_set_value(serwoSliderSyberka, g_syberkaPozycja, LV_ANIM_OFF);
  snprintf(bufor, sizeof(bufor), "%d%%", g_syberkaPozycja);
  lv_label_set_text(serwoPozycjaLabelSyberka, bufor);
  if (serwoArcSyberka) lv_arc_set_value(serwoArcSyberka, g_syberkaPozycja);

  serwo_panel_tryb_odswiez();
  lv_scr_load_anim(serwoPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  // [ROZSZERZENIE, ta sesja - Czesc 1 planu] Uruchom animacje ikon trybu -
  // ten sam wzorzec co pompa_panel_otworz()/pompa_panel_animacje_start().
  serwo_panel_animacje_start();
  serwo_idle_timer_resetuj();
}

// Oba kafelki otwieraja TEN SAM widok (obie osie naraz) - patrz komentarz
// przy serwo_panel_otworz() wyzej co do tego, dlaczego nie ma juz parametru id.
static void klapa_kafelek_klikniety_cb(lv_event_t *e) { (void)e; serwo_panel_otworz(); }
static void syberka_kafelek_klikniety_cb(lv_event_t *e) { (void)e; serwo_panel_otworz(); }

// [POPRAWKA, ta sesja - panel SERWO v2] Trzy callbacki trybu (nie dwa) -
// kazdy wysyla WSPOLNY trybSerwa (1/2/3), nie dotyka pozycji. Optymistyczne
// podswietlenie od razu (feedback natychmiast na dotyk, nim przyjdzie
// potwierdzenie z sieci - zasada microinteractions), prawdziwa wartosc i
// tak przyjdzie kolejnym _aktualizuj().
static void serwo_btn_auto_cb(lv_event_t *e) {
  (void)e;
  serwo_tryb_wyslij(1);
  serwo_idle_timer_resetuj();
  g_trybSerwa = 1;
  serwo_panel_tryb_odswiez();
}

static void serwo_btn_reczny_cb(lv_event_t *e) {
  (void)e;
  serwo_tryb_wyslij(2);
  serwo_idle_timer_resetuj();
  g_trybSerwa = 2;
  serwo_panel_tryb_odswiez();
}

static void serwo_btn_bezpieczna_cb(lv_event_t *e) {
  (void)e;
  serwo_tryb_wyslij(3);
  serwo_idle_timer_resetuj();
  g_trybSerwa = 3;
  serwo_panel_tryb_odswiez();
}

// [POPRAWKA, ta sesja - panel SERWO v3] Jeden wspolny callback dla OBU
// suwakow - `id` (0=klapa, 1=syberka) przychodzi przez user_data przy
// rejestracji (patrz serwo_panel_zbuduj()), wiec funkcja sama rozpoznaje,
// ktory suwak/etykiete ma zaktualizowac, zamiast czytac usuniety
// g_serwoAktywnyId. lv_event_get_target() daje wskaznik na suwak, ktory
// faktycznie wywolal zdarzenie (dziala tak samo dla obu, bez rozgalezien).
// Wysylka NA ZWOLNIENIE gałki (nie na kazdy pixel przeciagniecia) - unika
// zalewania centrali POST-ami podczas jednego gestu, ta sama zasada co
// "debounce" w standardowych slider-UX wzorcach. Oba suwaki widoczne
// wylacznie w trybie 2 (patrz serwo_panel_tryb_odswiez), wiec bezpiecznie
// wola serwo_pozycja_wyslij() bez dodatkowego sprawdzania trybu tutaj.
static void serwo_slider_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t *slider = lv_event_get_target(e);
  lv_obj_t *label = (id == 0) ? serwoPozycjaLabelKlapa : serwoPozycjaLabelSyberka;
  lv_obj_t *arc = (id == 0) ? serwoArcKlapa : serwoArcSyberka;
  int pozycja = lv_slider_get_value(slider);
  char bufor[8];
  snprintf(bufor, sizeof(bufor), "%d%%", pozycja);
  lv_label_set_text(label, bufor); // etykieta zywa PODCZAS przeciagania
  // [ROZSZERZENIE, ta sesja - Panel SERWO Czesc 2] Luk zywy PODCZAS
  // przeciagania, ten sam wzorzec co etykieta powyzej - user widzi
  // natychmiastowa wizualna odpowiedz na gest, zanim POST na LV_EVENT_RELEASED
  // w ogole wyjdzie (a co dopiero zanim centrala potwierdzi realna pozycje).
  if (arc) lv_arc_set_value(arc, pozycja);
  serwo_idle_timer_resetuj();
  if (code == LV_EVENT_RELEASED) {
    serwo_pozycja_wyslij(id, pozycja);
  }
}

// [USUNIETE, ta sesja - panel SERWO v3] Przyciski szybkich pozycji
// (Zamknij/50%/Otworz, serwo_preset_ustaw/serwo_przycisk_tekst) zniknely -
// zakladaly JEDNA "aktywna" os (usunieta, patrz wyzej), a przy dwoch
// suwakach naraz miejsca na ekranie (480x320) brakuje na 2 komplety
// presetow. Jesli okaza sie potrzebne, wrocic do tego jako osobny,
// przemyslany krok (np. male przyciski -/+ przy kazdym suwaku) - nie
// zgadywac teraz "na wszelki wypadek".

static void serwo_btn_zamknij_panel_cb(lv_event_t *e) { (void)e; serwo_panel_zamknij(); }

static void serwo_ustawienia_otworz(); // forward - definicja ponizej, potrzebna juz w callbacku przycisku zebatki w serwo_panel_zbuduj()
static void serwo_btn_ustawienia_cb(lv_event_t *e) { (void)e; serwo_ustawienia_otworz(); }

// ----------------------------------------------------------------------------
// Ekran WYBOR SERWA - miedzyekran otwierany z jednego kafelka "SERWO" na
// dashboardzie, prowadzi do klapaCard/syberkaCard (dokladnie te same karty
// co poprzednio zylys wprost na dashboardzie - patrz komentarz przy
// deklaracji serwoWyborScr wyzej).
// ----------------------------------------------------------------------------
static void serwo_wybor_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_serwoWyborIdleTimer = nullptr;
  if (dashboardScr != nullptr) lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}
static void serwo_wybor_idle_resetuj() {
  if (g_serwoWyborIdleTimer != nullptr) { lv_timer_del(g_serwoWyborIdleTimer); g_serwoWyborIdleTimer = nullptr; }
  g_serwoWyborIdleTimer = lv_timer_create(serwo_wybor_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_serwoWyborIdleTimer, 1);
}
static void serwo_wybor_zamknij_cb(lv_event_t *e) {
  (void)e;
  if (g_serwoWyborIdleTimer != nullptr) { lv_timer_del(g_serwoWyborIdleTimer); g_serwoWyborIdleTimer = nullptr; }
  if (dashboardScr != nullptr) lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}
static void serwo_wybor_otworz_cb(lv_event_t *e) {
  (void)e;
  lv_scr_load_anim(serwoWyborScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  serwo_wybor_idle_resetuj();
}

// Kafelek WEJSCIOWY na dashboardzie - zajmuje DWA sloty siatki w poziomie
// (miejsce bylo wolne - rzad 2 mial tylko SERWO+DYM na 4 slotach, patrz
// wywolanie w lvgl_dashboard_poc_init()). [FIX, ta sesja] Poprzednio JEDEN
// slot bez zadnej z dwoch wartosci na zywo - user: "ikona serwo ma miec
// wielkosc dwoch ikonek, bo teraz nie ma wartosci a powinny byc na
// pierwszym ekranie". Szerszy kafelek ma teraz miejsce na obie wartosci
// naraz (podpis "Klapa NN%\nSyberka NN%", aktualizowany na zywo w
// lvgl_dashboard_poc_aktualizuj() - patrz serwoWejsciePodpis), wiec
// pierwotna obawa "pokazanie jednej sugerowaloby ze to jest 'ta' wartosc
// SERWA" (patrz stary komentarz nizej) juz nie dotyczy - obie wartosci sa
// widoczne razem, jasno podpisane, bez dwuznacznosci. Karta klikalna z tym
// samym 3D press-efektem co reszta, w=2*kw+odstepX (przekazywane przez
// wywolujacego, patrz lvgl_dashboard_poc_init()).
static void kafelek_serwo_wejscie(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_color(card, rgb565(KOL_RAMKA), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
  press_flash_zainstaluj(card, KOL_KARTA, 3);
  // [FIX, ta sesja] Otwiera serwoPanel BEZPOSREDNIO (ten sam
  // klapa_kafelek_klikniety_cb co dawne karty KLAPA/SYBERKA na usunietym
  // ekranie serwoWyborScr - wywoluje po prostu serwo_panel_otworz(), bez
  // parametru, bo panel juz pokazuje obie osie naraz). Poprzednio:
  // serwo_wybor_otworz_cb, ktory prowadzil na posredni ekran wyboru osi -
  // usuniety z nawigacji, user: "usun posredni ekran".
  lv_obj_add_event_cb(card, klapa_kafelek_klikniety_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, "SERWO");
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 6);

  // [POPRAWKA, ta sesja] Oryginalna, stala ikona "suwak" WRACA bez zmian
  // (user: "miala byc orginalna") - obok niej, PO LEWEJ, dochodzi DRUGA,
  // niezalezna ikonka - "znaczek trybu" (Auto/Reczny/Bezpieczna), zmieniana
  // w serwo_tile_odswiez(). offset -48 = -4 (margines suwak od krawedzi) -
  // 40 (szerokosc suwak, IKONA_ROZMIAR) - 4 (odstep) - liczone od prawej
  // krawedzi karty, wiec dziala niezaleznie od tego, ktora bitmapa trybu
  // (30 albo 40px) jest akurat wstawiona - lv_obj_align liczy pozycje
  // wzgledem WLASNEGO rozmiaru obiektu, nie suwaka.
  lv_obj_t *img = lv_img_create(card);
  lv_img_set_src(img, &lv_ikona_suwak);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(img, rgb565(KOL_SUWAK), 0);
  lv_obj_align(img, LV_ALIGN_TOP_RIGHT, -4, 6);

  lv_obj_t *trybImg = lv_img_create(card);
  lv_img_set_src(trybImg, &lv_ikona_auto_0_mala); // startowy stan = Auto (domyslny tryb)
  lv_obj_set_style_img_recolor_opa(trybImg, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(trybImg, rgb565(KOL_RAMKA), 0);
  // [FIX, ta sesja] lv_img_set_zoom() nie dziala na formacie ALPHA_1BIT
  // (potwierdzone w dokumentacji LVGL - transformacje runtime tylko na
  // TRUE_COLOR) - obrazek po prostu znikal. Zamiast tego uzywamy GOTOWEJ,
  // osobno wygenerowanej mniejszej bitmapy (lv_ikona_*_mala, 20px, patrz
  // ikony.h/ikony_lvgl.h) - realne pomniejszenie, nie transformacja.
  lv_obj_align(trybImg, LV_ALIGN_TOP_RIGHT, -48, 6);
  serwoWejscieTrybIcon = trybImg;

  // "Klapa NN% / Syberka NN%" - zywe wartosci zamiast samego podpisu bez
  // liczb (patrz komentarz przy funkcji wyzej). Font 20 (wiekszy niz
  // poprzednie 14, bo teraz jest na to miejsce w x2-szerokiej karcie) -
  // czytelne z odleglosci, ale wciaz miesci dwie linie w wysokosci kh=90
  // razem z etykieta SERWO i strzalka ponizej.
  lv_obj_t *podpis = lv_label_create(card);
  lv_label_set_text(podpis, "Klapa --%\nSyberka --%");
  lv_obj_set_style_text_color(podpis, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_text_font(podpis, &lv_font_montserrat_20, 0);
  lv_obj_align(podpis, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  serwoWejsciePodpis = podpis;

  // Strzalka w prawo, dolny-prawy rog - sygnal "to prowadzi dalej"
  // (LV_SYMBOL_RIGHT to gotowy glif z wbudowanej czcionki symboli LVGL,
  // zero nowej bitmapy potrzebne).
  lv_obj_t *strzalka = lv_label_create(card);
  lv_label_set_text(strzalka, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(strzalka, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(strzalka, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
}

// Ekran WYBOR SERWA - tytul + dwie duze karty KLAPA/SYBERKA (kafelek_serwo(),
// dokladnie ta sama funkcja/logika co poprzednio na dashboardzie - patrz
// definicja wyzej w pliku), tylko wieksze (wiecej miejsca, bo tylko 2 karty
// zamiast calej siatki 4x3) i wysrodkowane. Przycisk X wraca na dashboard.
// Forward declaration - kafelek_serwo() jest zdefiniowana dalej w pliku
// (przy pozostalych funkcjach kafelek_*), ale serwo_wybor_zbuduj() (ponizej)
// potrzebuje jej juz teraz.
static lv_obj_t *kafelek_serwo(lv_obj_t *parent, int x, int y, int w, int h,
                                const char *etykieta, lv_event_cb_t klikCb,
                                lv_obj_t **cardWyjscie, lv_obj_t **recznieIkonaWyjscie);

static void serwo_wybor_zbuduj(lv_obj_t *scr) {
  (void)scr;
  serwoWyborScr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(serwoWyborScr, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(serwoWyborScr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(serwoWyborScr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *tytul = lv_label_create(serwoWyborScr);
  lv_label_set_text(tytul, "SERWO - WYBIERZ OS");
  lv_obj_set_style_text_font(tytul, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(tytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(tytul, LV_ALIGN_TOP_MID, 0, 14);

  // Dwie karty 200x180, wysrodkowane w poziomie z 20px odstepem (razem
  // 420px < 480px ekranu -> 30px marginesu po kazdej stronie), zaczynajace
  // sie ponizej tytulu (y=64).
  const int kw = 200, kh = 180, gap = 20;
  const int totalW = kw * 2 + gap;
  const int x0 = (480 - totalW) / 2;
  const int y0 = 64;

  klapaValLabel = kafelek_serwo(serwoWyborScr, x0, y0, kw, kh, "SERWO\nKLAPA",
                                 klapa_kafelek_klikniety_cb, &klapaCard, &klapaRecznieIcon);
  syberkaValLabel = kafelek_serwo(serwoWyborScr, x0 + kw + gap, y0, kw, kh, "SERWO\nSYBERKA",
                                   syberka_kafelek_klikniety_cb, &syberkaCard, &syberkaRecznieIcon);

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(serwoWyborScr, serwo_wybor_zamknij_cb, -10, 14);
}

// [POPRAWKA, ta sesja - panel SERWO v3] Jeden wiersz = nazwa osi (lewo) +
// aktualna wartosc (prawo) + wlasny suwak pod spodem. Wolane 2x w
// serwo_panel_zbuduj() (KLAPA, SYBERKA) - zero duplikacji ukladu miedzy
// osiami, tylko `id` (0/1, przekazywany dalej jako user_data suwaka -
// patrz serwo_slider_event_cb) odroznia, ktorej osi dotyczy dany wiersz.
// [ROZSZERZENIE, ta sesja - Panel SERWO Czesc 2, arcOut] Piaty parametr
// wyjsciowy - luk pozycji, budowany i zwracany na tych samych zasadach co
// slider/label ponizej (wskaznik do globalnego uchwytu serwoArcKlapa/Syberka,
// przypisywanego przez wolajacego w serwo_panel_zbuduj()).
static void serwo_wiersz_osi_zbuduj(lv_obj_t *parent, int y, const char *nazwa, int id,
                                     lv_obj_t **sliderOut, lv_obj_t **labelOut, lv_obj_t **arcOut) {
  lv_obj_t *wiersz = lv_obj_create(parent);
  lv_obj_remove_style_all(wiersz);
  lv_obj_set_size(wiersz, 380, 58);
  lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *nazwaLabel = lv_label_create(wiersz);
  lv_label_set_text(nazwaLabel, nazwa);
  lv_obj_set_style_text_font(nazwaLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(nazwaLabel, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(nazwaLabel, LV_ALIGN_TOP_LEFT, 0, 4);

  // [ROZSZERZENIE, ta sesja - Panel SERWO Czesc 2] Maly luk (28x28) TUZ przed
  // etykieta wartosci, ten sam pionowy pasek co ona (TOP_RIGHT) - etykieta
  // przesunieta w lewo o szerokosc luku + odstep (x=-34), zeby zrobic mu
  // miejsce, bez ingerencji w resztę ukladu wiersza (nazwa/suwak ponizej bez
  // zmian). Rozmiar/styl na wzor pompaArc (lv_arc_create(card) wyzej w
  // pliku), ale mniejszy - tu to dodatkowy wskaznik OBOK juz istniejacej
  // etykiety procentowej, nie jedyny nosnik wartosci jak przy pompaArc+ikonie
  // pompy, wiec nie potrzebuje 48px. [NIEPOTWIERDZONE NA SPRZECIE - jak
  // wszystkie nowe wspolrzedne w tym pliku, patrz PLAN_WYKRESY_LVGL.md
  // "Tryb pracy nad kodem" - wymaga zdjecia z ekranu do potwierdzenia ze
  // 28px luk nie nachodzi na etykiete/nazwe przy najszerszym podpisie "100%".]
  lv_obj_t *arc = lv_arc_create(wiersz);
  lv_obj_set_size(arc, 28, 28);
  lv_obj_align(arc, LV_ALIGN_TOP_RIGHT, -2, -2);
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_arc_set_rotation(arc, 270);
  lv_arc_set_range(arc, 0, 100);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, rgb565(KOL_RAMKA), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, rgb565(KOL_SUWAK), LV_PART_INDICATOR);
  lv_arc_set_value(arc, 0);

  lv_obj_t *wartoscLabel = lv_label_create(wiersz);
  lv_label_set_text(wartoscLabel, "0%");
  lv_obj_set_style_text_font(wartoscLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wartoscLabel, rgb565(KOL_TEKST), 0);
  lv_obj_align(wartoscLabel, LV_ALIGN_TOP_RIGHT, -34, 4);

  // [FIX v2, ta sesja - potwierdzone na sprzecie] Poprzedni margines (24px),
  // zalozony na "promien galki ~8px", byl zgadywaniem - i jak podejrzewano w
  // NIEPOTWIERDZONE NA SPRZECIE przy wyswietlacz_jasnosc_wiersz_zbuduj(),
  // motyw jednak DOKLADA WLASNY padding do LV_PART_KNOB, wiec realny promien
  // byl wiekszy niz 8px - stad wciaz "odrobine za duze" (lekkie obciecie)
  // mimo poprzedniej poprawki.
  //
  // Naprawa raz a dobrze: pad_all na LV_PART_KNOB ustawiony JAWNIE (4px)
  // zamiast polegac na (nieznanym) domyslnym paddingu motywu - promien galki
  // jest teraz POLICZONY, nie zgadywany: 8 (polowa wysokosci suwaka) + 4
  // (jawny pad) = 12px. Margines wzgledem `wiersz` podniesiony do 32px
  // (16px/strone) = 12px na sam promien + 4px zapasu na antyaliasing.
  // Identyczny jawny pad + margines w wyswietlacz_jasnosc_wiersz_zbuduj()
  // (patrz tamten komentarz) - obie galki (SERWO, jasnosc LCD) maja teraz
  // ten sam, przewidywalny rozmiar niezaleznie od motywu.
  lv_obj_t *slider = lv_slider_create(wiersz);
  lv_obj_set_size(slider, 348, 16);
  lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 30);
  lv_slider_set_range(slider, 0, 100);
  lv_obj_set_style_bg_color(slider, rgb565(KOL_RAMKA), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, rgb565(KOL_SUWAK), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, rgb565(KOL_TEKST), LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
  // [POPRAWKA, ta sesja] W trybach AUTO/BEZPIECZNA suwak jest tylko do
  // podgladu (LV_STATE_DISABLED - patrz serwo_panel_tryb_odswiez()), wiec
  // biala "galka" do przeciagania nie powinna byc tam w ogole widoczna -
  // sugerowalaby, ze da sie ja chwycic palcem. Styl zarejestrowany na
  // selektorze LV_PART_KNOB|LV_STATE_DISABLED wlacza sie automatycznie
  // przez LVGL, gdy tylko obiekt dostanie LV_STATE_DISABLED - zero
  // dodatkowego kodu w serwo_panel_tryb_odswiez() do recznego chowania.
  lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DISABLED);
  // `id` przekazany jako user_data (nie domykanie/lambda - LVGL C API tego
  // nie ma) - serwo_slider_event_cb() odczytuje go z powrotem, zeby
  // wiedziec, ktora etykieta/os/luk aktualizowac.
  lv_obj_add_event_cb(slider, serwo_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
  lv_obj_add_event_cb(slider, serwo_slider_event_cb, LV_EVENT_RELEASED, (void *)(intptr_t)id);

  *sliderOut = slider;
  *labelOut = wartoscLabel;
  *arcOut = arc;
}

static void serwo_panel_zbuduj(lv_obj_t *scr) {
  dashboardScr = scr; // moze juz byc ustawione przez pompa_panel_zbuduj() - to samo pole, bezpieczne do powtorzenia
  serwoPanel = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(serwoPanel, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(serwoPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(serwoPanel, LV_OBJ_FLAG_SCROLLABLE);

  // [POPRAWKA, ta sesja - panel SERWO v3] Staly tekst zamiast SERWO_NAZWA[id]
  // ustawianego w serwo_panel_otworz() - panel juz nie pokazuje jednej osi na
  // raz, wiec nie ma "id" do przelozenia na tytul. Ustawiany raz, tutaj.
  serwoTytulLabel = lv_label_create(serwoPanel);
  lv_label_set_text(serwoTytulLabel, "STEROWANIE SERWAMI");
  lv_obj_set_style_text_font(serwoTytulLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(serwoTytulLabel, rgb565(KOL_TEKST), 0);
  lv_obj_align(serwoTytulLabel, LV_ALIGN_TOP_MID, 0, 14);

  // Jednozdaniowy opis - to samo miejsce/rola co "readonly-note" w koncepcie
  // HTML (piec_co_ui_koncept.html), tylko ze ten panel jest edytowalny.
  // [FIX, ta sesja - "niezbyt to wyglada", zdjecie z ekranu] Szerokosc
  // 320->440 (prawie cala szerokosc ekranu 480, 20px marginesu z kazdej
  // strony) - przy 320px najdluzsza kombinacja zawijala sie na 3 linie i
  // nachodzila na rzad przyciskow trybu ponizej. Przy 440px (i tekscie
  // teraz wylacznie per-tryb, patrz SERWO_TRYB_OPIS[] wyzej, krotszym niz
  // dawna sklejka baza+sufiks) miesci sie w 2 liniach z zapasem.
  serwoOpisLabel = lv_label_create(serwoPanel);
  lv_obj_set_style_text_font(serwoOpisLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(serwoOpisLabel, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_align(serwoOpisLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(serwoOpisLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(serwoOpisLabel, 440);
  lv_obj_align(serwoOpisLabel, LV_ALIGN_TOP_MID, 0, 48);

  // Rzad trybu: AUTO / RECZNY / BEZPIECZNA - trzy "przelacznikowe" przyciski.
  // [ROZSZERZENIE, ta sesja - Czesc 1 planu] Zamienione z czysto tekstowych
  // (serwo_przycisk_tekst) na panel_przycisk_ikona() (ikona + podpis, ten
  // sam wzorzec co panel POMPA) - AUTO dostaje wirujaca ikone
  // lv_ikona_auto_0..7, RECZNY pulsujaca dlon lv_ikona_recznie, BEZPIECZNA
  // nowa statyczna ikone tarczy lv_ikona_bezpieczna (patrz komentarz przy
  // deklaracji serwoAutoIcon/serwoRecznyIcon wyzej co do animacji).
  // [FIX, ta sesja - "niezbyt to wyglada", zdjecie z ekranu] Domyslny
  // rozmiar przycisku z panel_przycisk_ikona() (74x78, dobry dla krotkich
  // podpisow AUTO/WLACZ/WYLACZ w POMPIE) byl za WASKI dla "RECZNY"/
  // "BEZPIECZNA" - font montserrat_14 lamal je w polowie slowa
  // ("RECZ"/"NY", "BEZP"/"IECZ"/"NA"), a 3-liniowy podpis "BEZPIECZNA"
  // nachodzil na ikone. Kazdy przycisk poszerzony do 112px (nizej,
  // lv_obj_set_size PO stworzeniu - ten sam wzorzec co poprzednia wersja
  // tekstowa nadpisywala rozmiar) - przy tej szerokosci najdluzszy podpis
  // "BEZPIECZNA" miesci sie w JEDNEJ linii. Wysokosc 74 (z 78) - o tyle
  // mniej potrzeba miejsca pionowo, gdy podpis jest juz tylko 1 linia.
  // [FIX tej sesji - "bezpieczny napis sie nie miesci"] Ten sam problem i
  // ta sama poprawka co w panelu POMPA wyzej (rzad AUTO/WLACZ/WYLACZ,
  // rzadWybor TROCINIAK/KOPCIUCH/AUTOMATYCZNY): 112px bylo ciasne dla
  // "BEZPIECZNA" (10 znakow) - font montserrat_14 wciaz lamal/przycinal
  // podpis. Rzad poszerzony 380->440 (tyle co serwoOpisLabel, prawie cala
  // szerokosc ekranu). Przyciski poszerzone 112->138, zeby "BEZPIECZNA"
  // zmiescila sie w jednej linii - WYSOKOSC celowo BEZ zmian (74, jak
  // poprzednio), bo serwoRecznyWiersz ponizej jest juz ciasno dopasowany
  // do dolnej krawedzi ekranu (320px) - wyzszy rzadTryb zepchnalby go
  // poza ekran, patrz komentarz przy serwoRecznyWiersz nizej.
  lv_obj_t *rzadTryb = lv_obj_create(serwoPanel);
  lv_obj_remove_style_all(rzadTryb);
  lv_obj_set_size(rzadTryb, 440, 74);
  lv_obj_align(rzadTryb, LV_ALIGN_TOP_MID, 0, 94);
  lv_obj_set_flex_flow(rzadTryb, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rzadTryb, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(rzadTryb, LV_OBJ_FLAG_SCROLLABLE);

  // [Kolorystyka - ta sesja] KOL_RAMKA zamiast KOL_OGIEN - AUTO to stan
  // NEUTRALNY, dokladnie taki jak neutralna (szara) ramka kafelkow
  // KLAPA/SYBERKA w trybie automatycznym na glownej siatce (patrz
  // kafelek_serwo(), domyslne kolorRamki=KOL_RAMKA). KOL_OGIEN byl kolorem
  // "pozyczonym" z kafelka POMPA - nic wspolnego z domena SERWO.
  serwoAutoBtn = panel_przycisk_ikona(rzadTryb, &lv_ikona_auto_0, KOL_RAMKA, serwo_btn_auto_cb, &serwoAutoIcon, "AUTO");
  lv_obj_set_size(serwoAutoBtn, 138, 74);
  serwoRecznyBtn = panel_przycisk_ikona(rzadTryb, &lv_ikona_recznie, KOL_SUWAK, serwo_btn_reczny_cb, &serwoRecznyIcon, "RECZNY");
  lv_obj_set_size(serwoRecznyBtn, 138, 74);
  // [Kolorystyka - ta sesja] Wlasny zielony KOL_BEZPIECZNA zamiast
  // pozyczonego KOL_OGIEN - patrz komentarz przy definicji KOL_BEZPIECZNA.
  serwoBezpiecznaBtn = panel_przycisk_ikona(rzadTryb, &lv_ikona_bezpieczna, KOL_BEZPIECZNA, serwo_btn_bezpieczna_cb, nullptr, "BEZPIECZNA");
  lv_obj_set_size(serwoBezpiecznaBtn, 138, 74);

  // [POPRAWKA, ta sesja - panel SERWO v3] Wiersz reczny mial 1 suwak
  // (pozycja "aktywnej" osi) + 3 presety - zastapiony DWOMA wierszami
  // (KLAPA, SYBERKA), kazdy z WLASNYM suwakiem, widocznymi NARAZ. Presety
  // usuniete (patrz komentarz przy serwo_slider_event_cb) - miejsca starczylo
  // na oba suwaki wygodnie, ale nie na 2 komplety presetow tez.
  // HIDDEN w trybie innym niz Reczny (patrz serwo_panel_tryb_odswiez()).
  // y=174 (bez zmian wzgledem poprzedniej wersji - dopasowane do rzadTryb
  // konczacego sie na 94+74=168, +6px odstepu). Wysokosc 140 miesci 2 wiersze
  // po 58px + 12px odstepu (58+12+58=128), 12px marginesu od dolu ekranu.
  serwoRecznyWiersz = lv_obj_create(serwoPanel);
  lv_obj_remove_style_all(serwoRecznyWiersz);
  lv_obj_set_size(serwoRecznyWiersz, 380, 140);
  lv_obj_align(serwoRecznyWiersz, LV_ALIGN_TOP_MID, 0, 174);
  lv_obj_clear_flag(serwoRecznyWiersz, LV_OBJ_FLAG_SCROLLABLE);

  serwo_wiersz_osi_zbuduj(serwoRecznyWiersz, 0,  SERWO_NAZWA[0], 0, &serwoSliderKlapa,   &serwoPozycjaLabelKlapa,   &serwoArcKlapa);
  serwo_wiersz_osi_zbuduj(serwoRecznyWiersz, 70, SERWO_NAZWA[1], 1, &serwoSliderSyberka, &serwoPozycjaLabelSyberka, &serwoArcSyberka);

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(serwoPanel, serwo_btn_zamknij_panel_cb);

  // [ROZSZERZENIE, ta sesja] Zebatka w przeciwnym rogu (gora-lewo, symetrycznie
  // do X gora-prawo) - wejscie na ekran USTAWIENIA SERWA (6 parametrow
  // automatyki, wspolne dla OBU osi, patrz serwo_ustawienia_zbuduj() nizej).
  // Uzywa lv_img (nie lv_label z ikona fontu) bo mamy juz gotowa bitmape
  // lv_ikona_zebatka - ten sam wzorzec co ikony w panel_przycisk_ikona().
  lv_obj_t *zebatka = lv_img_create(serwoPanel);
  lv_img_set_src(zebatka, &lv_ikona_zebatka);
  lv_obj_set_style_img_recolor(zebatka, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_img_recolor_opa(zebatka, LV_OPA_COVER, 0);
  lv_obj_align(zebatka, LV_ALIGN_TOP_LEFT, 6, 4);
  lv_obj_add_flag(zebatka, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(zebatka, serwo_btn_ustawienia_cb, LV_EVENT_CLICKED, nullptr);
}

// ----------------------------------------------------------------------------
// Ekran USTAWIENIA SERWA - 6 wierszy "spinbox" (etykieta + duza wartosc +
// -/+ po bokach). LV_EVENT_LONG_PRESSED_REPEAT na przyciskach -/+ daje
// przytrzymanie = szybsze przewijanie, bez potrzeby wlasnego timera - to
// wbudowany mechanizm LVGL (repeat co ~300ms po ~400ms przytrzymania,
// wartosci domyslne, wystarczajace dla zakresow 10-100 z krokiem 1).
// ----------------------------------------------------------------------------

// idx zakodowany w user_data przycisku (0..5, patrz SERWO_USTAWIENIA_POLA[]).
// Wysyla ZAWSZE po kazdym +/- (nie dopiero po "zatwierdz") - dokladnie ten
// sam wzorzec co stary EKRAN_TRYBY (POLA[]+wyslijUstawienie() w main.cpp,
// patrz "dziala jak w Ustawieniach" tam) - user widzi natychmiastowy efekt,
// centrala i tak waliduje kazda wartosc osobno.
static void serwo_ust_idle_resetuj(); // forward - potrzebny juz w +/- callbackach ponizej.
                                       // [FIX, ta sesja - "dluzsza zmiana parametrow cofa do
                                       // menu"] Idle-timeout na tym ekranie liczyl sie WYLACZNIE
                                       // od momentu otwarcia ekranu (serwo_ust_idle_resetuj()
                                       // byl wolany tylko raz, przy budowie ekranu) - klikanie w
                                       // -/+ NIE resetowalo odliczania. Efekt: jesli user spedzil
                                       // na dobieraniu wartosci dluzej niz POMPA_STRONA_IDLE_MS
                                       // (13.5s), ekran zamykal sie sam w trakcie, niezaleznie od
                                       // tego, jak niedawno faktycznie kliknieto w parametr.

static void serwo_ust_wyslij(int idx) {
  char bufor[8];
  snprintf(bufor, sizeof(bufor), "%d", g_serwoUstWartosc[idx]);
  lv_label_set_text(serwoUstWartoscLabel[idx], bufor);
  if (g_serwoUstawieniaCb) g_serwoUstawieniaCb(SERWO_USTAWIENIA_POLA[idx].klucz, g_serwoUstWartosc[idx]);
}

static void serwo_ust_minus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const PoleServo &p = SERWO_USTAWIENIA_POLA[idx];
  g_serwoUstWartosc[idx] -= p.krok;
  if (g_serwoUstWartosc[idx] < p.minZakres) g_serwoUstWartosc[idx] = p.minZakres;
  serwo_ust_wyslij(idx);
  serwo_ust_idle_resetuj(); // [FIX] klikniecie w -/+ = aktywnosc, odlicza od nowa
}
static void serwo_ust_plus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const PoleServo &p = SERWO_USTAWIENIA_POLA[idx];
  g_serwoUstWartosc[idx] += p.krok;
  if (g_serwoUstWartosc[idx] > p.maxZakres) g_serwoUstWartosc[idx] = p.maxZakres;
  serwo_ust_wyslij(idx);
  serwo_ust_idle_resetuj(); // [FIX] klikniecie w -/+ = aktywnosc, odlicza od nowa
}

static void serwo_ustawienia_zamknij_cb(lv_event_t *e) {
  (void)e;
  if (serwoPanel != nullptr) lv_scr_load_anim(serwoPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (g_serwoUstIdleTimer != nullptr) { lv_timer_del(g_serwoUstIdleTimer); g_serwoUstIdleTimer = nullptr; }
  serwo_idle_timer_resetuj(); // [FIX] wznow idle-timeout panelu SERWO, do ktorego wracamy
}

// Idle-timeout - ten sam wzorzec co serwo_idle_timer_resetuj()/
// g_serwoPanelIdleTimer wyzej (wraca do panelu SERWO po bezczynnosci, zeby
// ekran nie zostal "zawieszony" na ustawieniach, jesli ktos odejdzie).
static void serwo_ust_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_serwoUstIdleTimer = nullptr;
  serwo_ustawienia_zamknij_cb(nullptr);
}
static void serwo_ust_idle_resetuj() {
  if (g_serwoUstIdleTimer != nullptr) { lv_timer_del(g_serwoUstIdleTimer); g_serwoUstIdleTimer = nullptr; }
  g_serwoUstIdleTimer = lv_timer_create(serwo_ust_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_serwoUstIdleTimer, 1);
}

// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja] Ekran USTAWIENIA POMPY - ten sam wzorzec spinbox
// +/- co USTAWIENIA SERWA powyzej (5 pol zamiast 6), plus jeden przycisk
// WL./WYL. dla antystopWlaczony. forward - definicje uzyte juz w pompa_panel_zbuduj()
// (zebatka wchodzaca na ten ekran), zanim sam ekran jest zbudowany nizej.
// ----------------------------------------------------------------------------
static void pompa_ustawienia_otworz(); // forward - potrzebny juz w pompa_btn_ustawienia_cb ponizej

static void pompa_btn_ustawienia_cb(lv_event_t *e) { (void)e; pompa_ustawienia_otworz(); }

static void pompa_ust_idle_resetuj(); // forward - potrzebny juz w +/- callbackach ponizej.
                                       // [FIX, ta sesja] Ten sam problem i to samo rozwiazanie
                                       // co serwo_ust_idle_resetuj() wyzej - patrz komentarz tam.

static void pompa_ust_wyslij(int idx) {
  char bufor[8];
  snprintf(bufor, sizeof(bufor), "%d", g_pompaUstWartosc[idx]);
  lv_label_set_text(pompaUstWartoscLabel[idx], bufor);
  if (g_pompaUstawieniaCb) g_pompaUstawieniaCb(POMPA_USTAWIENIA_POLA[idx].klucz, g_pompaUstWartosc[idx]);
}
static void pompa_ust_minus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const PolePompa &p = POMPA_USTAWIENIA_POLA[idx];
  g_pompaUstWartosc[idx] -= p.krok;
  if (g_pompaUstWartosc[idx] < p.minZakres) g_pompaUstWartosc[idx] = p.minZakres;
  pompa_ust_wyslij(idx);
  pompa_ust_idle_resetuj(); // [FIX] klikniecie w -/+ = aktywnosc, odlicza od nowa
}
static void pompa_ust_plus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const PolePompa &p = POMPA_USTAWIENIA_POLA[idx];
  g_pompaUstWartosc[idx] += p.krok;
  if (g_pompaUstWartosc[idx] > p.maxZakres) g_pompaUstWartosc[idx] = p.maxZakres;
  pompa_ust_wyslij(idx);
  pompa_ust_idle_resetuj(); // [FIX] klikniecie w -/+ = aktywnosc, odlicza od nowa
}

// Odswieza wyglad przycisku antystopu (kolor tla + tekst) wg. aktualnego
// g_pompaAntystopWlaczony - wspolna funkcja wolana i z callbacku klikniecia,
// i z pompa_ustawienia_otworz()/lvgl_dashboard_poc_aktualizuj_ustawienia_pompy_antystop(),
// zeby wyglad nigdy nie rozjechal sie ze stanem (ten sam powod co "nie klam"
// wzorzec spinboxow).
static void pompa_antystop_wyglad_odswiez() {
  if (pompaAntystopBtn == nullptr) return;
  lv_obj_set_style_bg_color(pompaAntystopBtn,
                             rgb565(g_pompaAntystopWlaczony ? KOL_BEZPIECZNA : KOL_KARTA),
                             LV_PART_MAIN);
  lv_label_set_text(pompaAntystopBtnLabel, g_pompaAntystopWlaczony ? "WL." : "WYL.");
}

// Przelacznik antystop - wysyla natychmiast po kazdym klikniecu, ten sam
// "brak przycisku Zatwierdz" wzorzec co spinboxy wyzej. [FIX] Zamiast
// LV_EVENT_VALUE_CHANGED na lv_switch (usuniete, patrz komentarz przy
// pompaAntystopBtn), zwykly LV_EVENT_CLICKED na lv_btn ktory sam sobie
// przelacza stan bool.
static void pompa_antystop_btn_cb(lv_event_t *e) {
  (void)e;
  g_pompaAntystopWlaczony = !g_pompaAntystopWlaczony;
  pompa_antystop_wyglad_odswiez();
  if (g_pompaAntystopCb) g_pompaAntystopCb(g_pompaAntystopWlaczony ? 1 : 0);
  pompa_ust_idle_resetuj(); // [FIX] klikniecie w antystop tez liczy sie jako aktywnosc
}

static void pompa_ustawienia_zamknij_cb(lv_event_t *e) {
  (void)e;
  if (pompaPanel != nullptr) lv_scr_load_anim(pompaPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (g_pompaUstIdleTimer != nullptr) { lv_timer_del(g_pompaUstIdleTimer); g_pompaUstIdleTimer = nullptr; }
  pompa_idle_timer_resetuj(); // [FIX] wznow idle-timeout panelu POMPA, do ktorego wracamy
}

// Idle-timeout - ten sam wzorzec co serwo_ust_idle_timeout_cb() wyzej (wraca
// do panelu POMPA po bezczynnosci, zeby ekran nie zostal "zawieszony" na
// ustawieniach, jesli ktos odejdzie).
static void pompa_ust_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_pompaUstIdleTimer = nullptr;
  pompa_ustawienia_zamknij_cb(nullptr);
}
static void pompa_ust_idle_resetuj() {
  if (g_pompaUstIdleTimer != nullptr) { lv_timer_del(g_pompaUstIdleTimer); g_pompaUstIdleTimer = nullptr; }
  g_pompaUstIdleTimer = lv_timer_create(pompa_ust_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_pompaUstIdleTimer, 1);
}

static void serwo_ustawienia_zbuduj(lv_obj_t *scr) {
  (void)scr;
  serwoUstawieniaScr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(serwoUstawieniaScr, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(serwoUstawieniaScr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(serwoUstawieniaScr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *tytul = lv_label_create(serwoUstawieniaScr);
  lv_label_set_text(tytul, "USTAWIENIA SERWA");
  lv_obj_set_style_text_font(tytul, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(tytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(tytul, LV_ALIGN_TOP_MID, 0, 10);

  // 6 wierszy, 40px kazdy (240px), start y=44 -> koniec 284px, miesci sie
  // w 320px wysokosci ekranu z zapasem na dole (LV_SYMBOL_CLOSE).
  const int wierszWys = 40;
  const int startY = 44;
  for (int i = 0; i < 6; i++) {
    const PoleServo &p = SERWO_USTAWIENIA_POLA[i];
    lv_obj_t *wiersz = lv_obj_create(serwoUstawieniaScr);
    lv_obj_remove_style_all(wiersz);
    lv_obj_set_size(wiersz, 460, wierszWys);
    lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, startY + i * wierszWys);
    lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *etykieta = lv_label_create(wiersz);
    lv_label_set_text(etykieta, p.etykieta);
    lv_obj_set_style_text_font(etykieta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(etykieta, rgb565(KOL_TEKST_DIM), 0);
    lv_obj_align(etykieta, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t *minus = lv_btn_create(wiersz);
    lv_obj_set_size(minus, 50, 34);
    lv_obj_set_style_radius(minus, 6, 0);
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -140, 0);
    press_flash_zainstaluj(minus, KOL_KARTA, 4);
    lv_obj_add_event_cb(minus, serwo_ust_minus_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(minus, serwo_ust_minus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)i);
    lv_obj_t *minusLbl = lv_label_create(minus);
    lv_label_set_text(minusLbl, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_color(minusLbl, rgb565(KOL_TEKST), 0);
    lv_obj_center(minusLbl);

    serwoUstWartoscLabel[i] = lv_label_create(wiersz);
    lv_label_set_text(serwoUstWartoscLabel[i], "0");
    lv_obj_set_style_text_font(serwoUstWartoscLabel[i], &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(serwoUstWartoscLabel[i], rgb565(KOL_TEKST), 0);
    lv_obj_align(serwoUstWartoscLabel[i], LV_ALIGN_RIGHT_MID, -80, 0);

    lv_obj_t *plus = lv_btn_create(wiersz);
    lv_obj_set_size(plus, 50, 34);
    lv_obj_set_style_radius(plus, 6, 0);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -10, 0);
    press_flash_zainstaluj(plus, KOL_KARTA, 4);
    lv_obj_add_event_cb(plus, serwo_ust_plus_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(plus, serwo_ust_plus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)i);
    lv_obj_t *plusLbl = lv_label_create(plus);
    lv_label_set_text(plusLbl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(plusLbl, rgb565(KOL_TEKST), 0);
    lv_obj_center(plusLbl);
  }

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(serwoUstawieniaScr, serwo_ustawienia_zamknij_cb);
}

static void serwo_ustawienia_otworz() {
  // Odswiez wszystkie 6 labelek z OSTATNIEJ znanej wartosci z centrali
  // (g_serwoUstWartosc[], wypelniane przez
  // lvgl_dashboard_poc_aktualizuj_ustawienia_serwa()) - zeby ekran nigdy nie
  // pokazal poprzedniej sesji/nieaktualnej wartosci przy otwarciu.
  for (int i = 0; i < 6; i++) {
    char bufor[8];
    snprintf(bufor, sizeof(bufor), "%d", g_serwoUstWartosc[i]);
    lv_label_set_text(serwoUstWartoscLabel[i], bufor);
  }
  lv_scr_load_anim(serwoUstawieniaScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  // [FIX, ta sesja - "cofa mnie do glownego menu podczas ustawiania"]
  // g_serwoPanelIdleTimer (idle-timeout ekranu SERWO, z ktorego wchodzimy
  // tutaj przez zebatke) NIE byl zatrzymywany przy wejsciu na ten ekran -
  // dzialal wiec dalej w tle, niezaleznie od wlasnego g_serwoUstIdleTimer
  // ponizej. Gdy wygasl (POMPA_STRONA_IDLE_MS od ostatniej aktywnosci NA
  // EKRANIE SERWO, sprzed wejscia tutaj), jego callback
  // (serwo_panel_idle_timeout_cb -> serwo_panel_zamknij()) skakal PROSTO do
  // dashboardScr (glowne menu) - bez wzgledu na to, ze user byl cala
  // odpowiedz aktywny na ekranie USTAWIENIA i mial wlasny, poprawnie
  // resetowany timer. Stad "cofniecie do glownego menu w trakcie klikania".
  if (g_serwoPanelIdleTimer != nullptr) {
    lv_timer_del(g_serwoPanelIdleTimer);
    g_serwoPanelIdleTimer = nullptr;
  }
  serwo_ust_idle_resetuj();
}

// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja] Ekran USTAWIENIA POMPY - ten sam "spinbox" wzorzec
// co USTAWIENIA SERWA powyzej (etykieta + duza wartosc + -/+ po bokach,
// LV_EVENT_LONG_PRESSED_REPEAT dla przytrzymania), rozszerzony o kolorowy
// pionowy pasek po lewej kazdego wiersza - wizualnie grupuje 6 wierszy w 3
// pary wg. tego, do KTOREJ strategii pompy nalezy dane pole (patrz komentarz
// przy POMPA_USTAWIENIA_POLA wyzej): KOL_ZAR = Trociniak (czasOn/czasOff),
// KOL_KROPLA = Kopciuch (tempOn/tempOff), KOL_BEZPIECZNA = Antystop
// (niezalezny od strategii - ten sam zielony akcent co przycisk BEZPIECZNA w
// panelu SERWO, bo to tez mechanizm ochronny). User: "ladnie i efektownie" -
// pasek kosztuje 4px szerokosci, a od razu tlumaczy ekran bez czytania
// opisow, dokladnie tak jak isManualLike() w koncepcie HTML koloruje ramke
// kafelka wg. stanu.
static void pompa_ust_wiersz_zbuduj(lv_obj_t *rodzic, int y, uint16_t akcent,
                                     const char *etykieta, lv_obj_t **wartoscLabelOut,
                                     lv_event_cb_t minusCb, lv_event_cb_t plusCb, int idx) {
  const int wierszWys = 40;
  lv_obj_t *wiersz = lv_obj_create(rodzic);
  lv_obj_remove_style_all(wiersz);
  lv_obj_set_size(wiersz, 460, wierszWys);
  lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *pasek = lv_obj_create(wiersz);
  lv_obj_remove_style_all(pasek);
  lv_obj_set_size(pasek, 4, 26);
  lv_obj_align(pasek, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_color(pasek, rgb565(akcent), 0);
  lv_obj_set_style_bg_opa(pasek, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(pasek, 2, 0);

  lv_obj_t *etykietaLbl = lv_label_create(wiersz);
  lv_label_set_text(etykietaLbl, etykieta);
  lv_obj_set_style_text_font(etykietaLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(etykietaLbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(etykietaLbl, LV_ALIGN_LEFT_MID, 14, 0);

  lv_obj_t *minus = lv_btn_create(wiersz);
  lv_obj_set_size(minus, 50, 34);
  lv_obj_set_style_radius(minus, 6, 0);
  lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -140, 0);
  press_flash_zainstaluj(minus, KOL_KARTA, 4);
  lv_obj_add_event_cb(minus, minusCb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(minus, minusCb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)idx);
  lv_obj_t *minusLbl = lv_label_create(minus);
  lv_label_set_text(minusLbl, LV_SYMBOL_MINUS);
  lv_obj_set_style_text_color(minusLbl, rgb565(KOL_TEKST), 0);
  lv_obj_center(minusLbl);

  *wartoscLabelOut = lv_label_create(wiersz);
  lv_label_set_text(*wartoscLabelOut, "0");
  lv_obj_set_style_text_font(*wartoscLabelOut, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(*wartoscLabelOut, rgb565(KOL_TEKST), 0);
  lv_obj_align(*wartoscLabelOut, LV_ALIGN_RIGHT_MID, -80, 0);

  lv_obj_t *plus = lv_btn_create(wiersz);
  lv_obj_set_size(plus, 50, 34);
  lv_obj_set_style_radius(plus, 6, 0);
  lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -10, 0);
  press_flash_zainstaluj(plus, KOL_KARTA, 4);
  lv_obj_add_event_cb(plus, plusCb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(plus, plusCb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)idx);
  lv_obj_t *plusLbl = lv_label_create(plus);
  lv_label_set_text(plusLbl, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_color(plusLbl, rgb565(KOL_TEKST), 0);
  lv_obj_center(plusLbl);

  // Zapamietujemy uchwyty (pasek/etykieta) + oryginalny kolor akcentu tego
  // wiersza - potrzebne pompa_ust_aktywnosc_odswiez() do przygaszania/
  // przywracania koloru wg. aktualnej strategii (g_pompaWybor). idx tu to
  // TEN SAM index co w POMPA_USTAWIENIA_POLA/g_pompaUstWartosc (0-4) -
  // przekazywany przez wywolujacego, wiec zero dodatkowego mapowania.
  if (idx >= 0 && idx < 5) {
    pompaUstPasek[idx] = pasek;
    pompaUstEtykieta[idx] = etykietaLbl;
    pompaUstAkcent[idx] = akcent;
  }
}

// Nazwa/kolor aktualnej strategii pompy - DOKLADNIE te same kolory co
// przyciski na podstronie WYBOR (patrz trociniakBtn/kopciuchBtn/
// automatycznyBtn w pompa_podstrone_wybor_zbuduj() - KOL_ZAR/KOL_KROPLA/
// KOL_OGIEN), zeby plomka w naglowku i paski wierszy uzywaly TEGO SAMEGO
// jezyka kolorow co reszta panelu POMPA - user nie musi uczyc sie nowego
// kodu kolorow tylko dla tego ekranu.
static const char *pompa_tryb_nazwa(int wybor) {
  if (wybor == 1) return "TROCINIAK";
  if (wybor == 2) return "KOPCIUCH";
  if (wybor == 3) return "AUTOMATYCZNY";
  return "?";
}
static uint16_t pompa_tryb_kolor(int wybor) {
  if (wybor == 1) return KOL_ZAR;
  if (wybor == 2) return KOL_KROPLA;
  return KOL_OGIEN; // 3 = Automatyczny
}

// [ROZSZERZENIE, ta sesja] Odswieza WYGLAD calego ekranu USTAWIENIA POMPY
// wg. aktualnej strategii (g_pompaWybor) - odpowiedz na "nie widac po
// kolorach co jest aktywne":
//   1) plomka w naglowku dostaje kolor + nazwe aktywnej strategii (TA SAMA
//      paleta co przyciski na podstronie WYBOR - patrz pompa_tryb_kolor()),
//   2) para pol NALEZACA do aktywnej strategii zostaje w pelnym kolorze +
//      jasnym tekscie, para NIEnalezaca zostaje przygaszona (szary pasek,
//      przygaszony tekst) - jednym rzutem oka widac co sie teraz liczy, bez
//      czytania etykiet.
// [ZALOZENIE] Przy Automatyczny (g_pompaWybor==3) nie wiadomo z tego ekranu,
// ktorej pary centrala akurat uzywa wewnetrznie w danej chwili - zamiast
// zgadywac, ZADNA para nie jest przygaszona (obie pelne), tylko plomka
// pokazuje kolor/nazwe AUTOMATYCZNY. Jesli main_centrala.cpp ma tu
// jednoznaczna regule, da sie to doprecyzowac.
// Wolana: raz po zbudowaniu ekranu (stan startowy), przy kazdym otwarciu
// (pompa_ustawienia_otworz() - na wypadek zmiany strategii z innego miejsca,
// np. strony WYBOR, podczas gdy ten ekran byl zamkniety) i przy KAZDEJ
// zmianie g_pompaWybor w locie (patrz lvgl_dashboard_poc_aktualizuj()).
static void pompa_ust_aktywnosc_odswiez() {
  if (pompaUstTrybPlomka != nullptr) {
    uint16_t kolorTrybu = pompa_tryb_kolor(g_pompaWybor);
    lv_obj_set_style_bg_color(pompaUstTrybPlomka, rgb565(kolorTrybu), 0);
    lv_label_set_text(pompaUstTrybPlomkaLbl, pompa_tryb_nazwa(g_pompaWybor));
  }
  for (int i = 0; i < 5; i++) {
    if (pompaUstPasek[i] == nullptr) continue; // ekran jeszcze nie zbudowany
    int grupa = POMPA_UST_GRUPA[i];
    bool aktywne = (grupa == 0) || (g_pompaWybor == 3) || (grupa == g_pompaWybor);
    lv_obj_set_style_bg_color(pompaUstPasek[i], rgb565(aktywne ? pompaUstAkcent[i] : KOL_TEKST_DIM), 0);
    lv_obj_set_style_bg_opa(pompaUstPasek[i], aktywne ? LV_OPA_COVER : LV_OPA_40, 0);
    lv_obj_set_style_text_opa(pompaUstEtykieta[i], aktywne ? LV_OPA_COVER : LV_OPA_50, 0);
    if (pompaUstWartoscLabel[i] != nullptr) {
      lv_obj_set_style_text_color(pompaUstWartoscLabel[i], rgb565(aktywne ? KOL_TEKST : KOL_TEKST_DIM), 0);
    }
  }
}

static void pompa_ustawienia_zbuduj(lv_obj_t *scr) {
  (void)scr;
  pompaUstawieniaScr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(pompaUstawieniaScr, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(pompaUstawieniaScr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(pompaUstawieniaScr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *tytul = lv_label_create(pompaUstawieniaScr);
  lv_label_set_text(tytul, "USTAWIENIA POMPY");
  lv_obj_set_style_text_font(tytul, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(tytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(tytul, LV_ALIGN_TOP_LEFT, 10, 10);

  // [ROZSZERZENIE, ta sesja] Plomka "aktywny tryb" - patrz obszerny
  // komentarz przy pompa_ust_aktywnosc_odswiez() wyzej. Tytul przesuniety z
  // TOP_MID na TOP_LEFT (bylo wysrodkowane), zeby zrobic miejsce na plomke
  // po prawej, przed przyciskiem zamkniecia X (ktory zostaje na TOP_RIGHT
  // -10,10 bez zmian).
  pompaUstTrybPlomka = lv_obj_create(pompaUstawieniaScr);
  lv_obj_remove_style_all(pompaUstTrybPlomka);
  lv_obj_set_size(pompaUstTrybPlomka, 130, 24);
  lv_obj_align(pompaUstTrybPlomka, LV_ALIGN_TOP_RIGHT, -36, 9);
  lv_obj_set_style_radius(pompaUstTrybPlomka, 12, 0);
  lv_obj_set_style_bg_opa(pompaUstTrybPlomka, LV_OPA_COVER, 0);
  pompaUstTrybPlomkaLbl = lv_label_create(pompaUstTrybPlomka);
  lv_obj_set_style_text_font(pompaUstTrybPlomkaLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(pompaUstTrybPlomkaLbl, rgb565(KOL_TLO), 0);
  lv_label_set_text(pompaUstTrybPlomkaLbl, "");
  lv_obj_center(pompaUstTrybPlomkaLbl);

  // 6 wierszy, 40px kazdy, start y=44 -> ta sama geometria co ekran USTAWIENIA
  // SERWA (koniec 284px, miesci sie w 320px z zapasem na dole).
  const int wierszWys = 40;
  const int startY = 44;

  // Wiersze 0-1: Trociniak (czasOn/czasOff) - POMPA_USTAWIENIA_POLA[0..1].
  pompa_ust_wiersz_zbuduj(pompaUstawieniaScr, startY + 0 * wierszWys, KOL_ZAR,
                          POMPA_USTAWIENIA_POLA[0].etykieta, &pompaUstWartoscLabel[0],
                          pompa_ust_minus_cb, pompa_ust_plus_cb, 0);
  pompa_ust_wiersz_zbuduj(pompaUstawieniaScr, startY + 1 * wierszWys, KOL_ZAR,
                          POMPA_USTAWIENIA_POLA[1].etykieta, &pompaUstWartoscLabel[1],
                          pompa_ust_minus_cb, pompa_ust_plus_cb, 1);

  // Wiersze 2-3: Kopciuch (tempOn/tempOff) - POMPA_USTAWIENIA_POLA[2..3].
  pompa_ust_wiersz_zbuduj(pompaUstawieniaScr, startY + 2 * wierszWys, KOL_KROPLA,
                          POMPA_USTAWIENIA_POLA[2].etykieta, &pompaUstWartoscLabel[2],
                          pompa_ust_minus_cb, pompa_ust_plus_cb, 2);
  pompa_ust_wiersz_zbuduj(pompaUstawieniaScr, startY + 3 * wierszWys, KOL_KROPLA,
                          POMPA_USTAWIENIA_POLA[3].etykieta, &pompaUstWartoscLabel[3],
                          pompa_ust_minus_cb, pompa_ust_plus_cb, 3);

  // Wiersz 4: Antystop wlacznik (przycisk WL./WYL., nie spinbox) - wlasny
  // ukladzik, ale ten sam pasek-akcent + etykieta po lewej co reszta
  // wierszy, zeby wizualnie pasowal do siatki.
  {
    lv_obj_t *wiersz = lv_obj_create(pompaUstawieniaScr);
    lv_obj_remove_style_all(wiersz);
    lv_obj_set_size(wiersz, 460, wierszWys);
    lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, startY + 4 * wierszWys);
    lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pasek = lv_obj_create(wiersz);
    lv_obj_remove_style_all(pasek);
    lv_obj_set_size(pasek, 4, 26);
    lv_obj_align(pasek, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(pasek, rgb565(KOL_BEZPIECZNA), 0);
    lv_obj_set_style_bg_opa(pasek, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pasek, 2, 0);

    lv_obj_t *etykietaLbl = lv_label_create(wiersz);
    lv_label_set_text(etykietaLbl, "Antystop wirnika");
    lv_obj_set_style_text_font(etykietaLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(etykietaLbl, rgb565(KOL_TEKST_DIM), 0);
    lv_obj_align(etykietaLbl, LV_ALIGN_LEFT_MID, 14, 0);

    pompaAntystopBtn = lv_btn_create(wiersz);
    lv_obj_set_size(pompaAntystopBtn, 74, 34);
    lv_obj_align(pompaAntystopBtn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_radius(pompaAntystopBtn, 8, 0);
    lv_obj_clear_flag(pompaAntystopBtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(pompaAntystopBtn, rgb565(KOL_KARTA), LV_PART_MAIN);
    press_flash_zainstaluj(pompaAntystopBtn, KOL_BEZPIECZNA, 5);
    lv_obj_add_event_cb(pompaAntystopBtn, pompa_antystop_btn_cb, LV_EVENT_CLICKED, nullptr);

    pompaAntystopBtnLabel = lv_label_create(pompaAntystopBtn);
    lv_label_set_text(pompaAntystopBtnLabel, "WL.");
    lv_obj_set_style_text_font(pompaAntystopBtnLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pompaAntystopBtnLabel, rgb565(KOL_TEKST), 0);
    lv_obj_center(pompaAntystopBtnLabel);
  }

  // Wiersz 5: Antystop - Dni bez ruchu (POMPA_USTAWIENIA_POLA[4]), spinbox
  // jak reszta, ten sam zielony akcent co wiersz przelacznika powyzej - oba
  // wizualnie naleza do tej samej grupy "Antystop".
  pompa_ust_wiersz_zbuduj(pompaUstawieniaScr, startY + 5 * wierszWys, KOL_BEZPIECZNA,
                          POMPA_USTAWIENIA_POLA[4].etykieta, &pompaUstWartoscLabel[4],
                          pompa_ust_minus_cb, pompa_ust_plus_cb, 4);

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(pompaUstawieniaScr, pompa_ustawienia_zamknij_cb);

  // Stan startowy plomki/przygaszenia - odzwierciedla g_pompaWybor juz
  // znany w momencie budowy ekranu (domyslnie 1, albo cokolwiek przyszlo z
  // centrali przed pierwszym otwarciem tego ekranu).
  pompa_ust_aktywnosc_odswiez();
}

static void pompa_ustawienia_otworz() {
  // Odswiez wszystkie labelki + pozycje przelacznika z OSTATNIEJ znanej
  // wartosci z centrali (g_pompaUstWartosc[]/g_pompaAntystopWlaczony,
  // wypelniane przez lvgl_dashboard_poc_aktualizuj_ustawienia_pompy*()) -
  // zeby ekran nigdy nie pokazal poprzedniej sesji/nieaktualnej wartosci przy
  // otwarciu, ten sam wzorzec co serwo_ustawienia_otworz() powyzej.
  for (int i = 0; i < 5; i++) {
    char bufor[8];
    snprintf(bufor, sizeof(bufor), "%d", g_pompaUstWartosc[i]);
    lv_label_set_text(pompaUstWartoscLabel[i], bufor);
  }
  pompa_antystop_wyglad_odswiez();
  // [ROZSZERZENIE, ta sesja] Odswiez plombe trybu + przygaszenie nieaktywnej
  // pary PRZY KAZDYM OTWARCIU - g_pompaWybor mogl sie zmienic (np. user
  // przelaczyl strategie na stronie WYBOR) podczas gdy ten ekran byl
  // zamkniety, wiec sam callback zmiany w lvgl_dashboard_poc_aktualizuj() to
  // za malo - trzeba tez odswiezyc tutaj, na wejsciu.
  pompa_ust_aktywnosc_odswiez();
  lv_scr_load_anim(pompaUstawieniaScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  // [FIX, ta sesja] Ten sam problem i to samo rozwiazanie co w
  // serwo_ustawienia_otworz() - patrz obszerny komentarz tam. Bez tego
  // g_pompaPanelIdleTimer (idle-timeout ekranu POMPA) dzialal dalej w tle
  // i po swoim czasie skakal prosto do dashboardScr, niezaleznie od
  // aktywnosci na tym ekranie ustawien.
  if (g_pompaPanelIdleTimer != nullptr) {
    lv_timer_del(g_pompaPanelIdleTimer);
    g_pompaPanelIdleTimer = nullptr;
  }
  pompa_ust_idle_resetuj();
}

// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja - PANEL DYM] Jeden pelnoekranowy panel: suwak progu
// alarmu (tlo z 3 statycznych pasm zielony/zolty/czerwony, widoczna tylko
// galka) + 3 wiersze spinbox dla czasow czujnika. Ten sam "nie klam" wzorzec
// co panele SERWO/POMPA - zero stanu zmienianego lokalnie "na wiare", ekran
// zawsze pokazuje ostatnia PRAWDZIWA wartosc z centrali
// (lvgl_dashboard_poc_aktualizuj_ustawienia_dymu()).
// ----------------------------------------------------------------------------
static void dym_ust_wyslij(int idx) {
  char bufor[8];
  snprintf(bufor, sizeof(bufor), "%d", g_dymUstWartosc[idx]);
  lv_label_set_text(dymUstWartoscLabel[idx], bufor);
  if (g_dymUstawieniaCb) g_dymUstawieniaCb(DYM_USTAWIENIA_POLA[idx].klucz, g_dymUstWartosc[idx]);
}
static void dym_ust_minus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const PoleDym &p = DYM_USTAWIENIA_POLA[idx];
  g_dymUstWartosc[idx] -= p.krok;
  if (g_dymUstWartosc[idx] < p.minZakres) g_dymUstWartosc[idx] = p.minZakres;
  dym_ust_wyslij(idx);
}
static void dym_ust_plus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const PoleDym &p = DYM_USTAWIENIA_POLA[idx];
  g_dymUstWartosc[idx] += p.krok;
  if (g_dymUstWartosc[idx] > p.maxZakres) g_dymUstWartosc[idx] = p.maxZakres;
  dym_ust_wyslij(idx);
}

// Ten sam wiersz "etykieta + duza wartosc + -/+ po bokach" co
// pompa_ust_wiersz_zbuduj(), ale BEZ kolorowego paska grupujacego (DYM nie
// ma dwoch rywalizujacych strategii jak POMPA, wiec nie ma co podswietlac/
// przygaszac) - lokalna, prostsza wersja, zeby nie naciagac tamtej funkcji
// (ktora pisze do pompaUstPasek[]/pompaUstAkcent[], niezwiazanych z DYM).
static void dym_ust_wiersz_zbuduj(lv_obj_t *rodzic, int y, const char *etykieta,
                                   lv_obj_t **wartoscLabelOut, int idx) {
  const int wierszWys = 40;
  lv_obj_t *wiersz = lv_obj_create(rodzic);
  lv_obj_remove_style_all(wiersz);
  lv_obj_set_size(wiersz, 440, wierszWys);
  lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *etykietaLbl = lv_label_create(wiersz);
  lv_label_set_text(etykietaLbl, etykieta);
  lv_obj_set_style_text_font(etykietaLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(etykietaLbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(etykietaLbl, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t *minus = lv_btn_create(wiersz);
  lv_obj_set_size(minus, 50, 34);
  lv_obj_set_style_radius(minus, 6, 0);
  lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -140, 0);
  press_flash_zainstaluj(minus, KOL_KARTA, 4);
  lv_obj_add_event_cb(minus, dym_ust_minus_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(minus, dym_ust_minus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)idx);
  lv_obj_t *minusLbl = lv_label_create(minus);
  lv_label_set_text(minusLbl, LV_SYMBOL_MINUS);
  lv_obj_set_style_text_color(minusLbl, rgb565(KOL_TEKST), 0);
  lv_obj_center(minusLbl);

  *wartoscLabelOut = lv_label_create(wiersz);
  lv_label_set_text(*wartoscLabelOut, "0");
  lv_obj_set_style_text_font(*wartoscLabelOut, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(*wartoscLabelOut, rgb565(KOL_TEKST), 0);
  lv_obj_align(*wartoscLabelOut, LV_ALIGN_RIGHT_MID, -80, 0);

  lv_obj_t *plus = lv_btn_create(wiersz);
  lv_obj_set_size(plus, 50, 34);
  lv_obj_set_style_radius(plus, 6, 0);
  lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -10, 0);
  press_flash_zainstaluj(plus, KOL_KARTA, 4);
  lv_obj_add_event_cb(plus, dym_ust_plus_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(plus, dym_ust_plus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)idx);
  lv_obj_t *plusLbl = lv_label_create(plus);
  lv_label_set_text(plusLbl, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_color(plusLbl, rgb565(KOL_TEKST), 0);
  lv_obj_center(plusLbl);
}

// Wysylka NA ZWOLNIENIE galki - ten sam debounce/wzorzec co serwo_slider_event_cb.
static void dym_slider_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  int wartosc = lv_slider_get_value(dymSlider);
  char bufor[16];
  snprintf(bufor, sizeof(bufor), "PROG: %d", wartosc);
  lv_label_set_text(dymProgLabel, bufor); // zywa etykieta PODCZAS przeciagania
  if (code == LV_EVENT_RELEASED) {
    g_dymProg = wartosc;
    if (g_dymProgCb) g_dymProgCb(wartosc);
  }
}

static void dym_panel_zamknij_cb(lv_event_t *e); // forward - potrzebny juz w idle-timeout ponizej
static void dym_panel_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_dymPanelIdleTimer = nullptr;
  dym_panel_zamknij_cb(nullptr);
}
static void dym_panel_idle_resetuj() {
  if (g_dymPanelIdleTimer != nullptr) { lv_timer_del(g_dymPanelIdleTimer); g_dymPanelIdleTimer = nullptr; }
  g_dymPanelIdleTimer = lv_timer_create(dym_panel_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_dymPanelIdleTimer, 1);
}
static void dym_panel_zamknij_cb(lv_event_t *e) {
  (void)e;
  if (dashboardScr != nullptr) lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (g_dymPanelIdleTimer != nullptr) { lv_timer_del(g_dymPanelIdleTimer); g_dymPanelIdleTimer = nullptr; }
}

static void dym_panel_otworz() {
  char bufor[16];
  snprintf(bufor, sizeof(bufor), "PROG: %d", g_dymProg);
  lv_label_set_text(dymProgLabel, bufor);
  lv_slider_set_value(dymSlider, g_dymProg, LV_ANIM_OFF); // start od PRAWDZIWEJ ostatniej wartosci
  for (int i = 0; i < 3; i++) {
    char b2[8];
    snprintf(b2, sizeof(b2), "%d", g_dymUstWartosc[i]);
    lv_label_set_text(dymUstWartoscLabel[i], b2);
  }
  lv_scr_load_anim(dymPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  dym_panel_idle_resetuj();
}

static void dym_kafelek_klikniety_cb(lv_event_t *e) { (void)e; dym_panel_otworz(); }

static void dym_panel_zbuduj(lv_obj_t *scr) {
  dashboardScr = scr; // moze juz byc ustawione przez pompa/serwo_panel_zbuduj() - bezpieczne do powtorzenia
  dymPanel = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(dymPanel, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(dymPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(dymPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *tytul = lv_label_create(dymPanel);
  lv_label_set_text(tytul, "DYM");
  lv_obj_set_style_text_font(tytul, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(tytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(tytul, LV_ALIGN_TOP_MID, 0, 10);

  dymProgLabel = lv_label_create(dymPanel);
  lv_label_set_text(dymProgLabel, "PROG: 500");
  lv_obj_set_style_text_font(dymProgLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(dymProgLabel, rgb565(KOL_TEKST), 0);
  lv_obj_align(dymProgLabel, LV_ALIGN_TOP_MID, 0, 48);

  // 3 statyczne pasma tla (rownej szerokosci, ~113px kazde przy 340px
  // calkowitej szerokosci) - orientacyjne "spokojnie/uwaga/alarm" wzdluz
  // STALEGO zakresu 0-1500 (patrz DYM_PROG_ZAKRES_MAX), NIE przeliczane w
  // locie wzgledem samego progu, zeby skala nie "uciekala" pod palcem
  // podczas przeciagania.
  const int suwSzer = 340, suwWys = 16;
  const int suwX0 = (480 - suwSzer) / 2;
  const int suwY = 84;
  const int pasmoSzer = suwSzer / 3;
  uint16_t pasmaKolory[3] = { KOL_BEZPIECZNA, KOL_WARN, KOL_ERR };
  for (int i = 0; i < 3; i++) {
    lv_obj_t *pasmo = lv_obj_create(dymPanel);
    lv_obj_remove_style_all(pasmo);
    lv_obj_set_size(pasmo, (i == 2) ? (suwSzer - 2 * pasmoSzer) : pasmoSzer, suwWys);
    lv_obj_set_pos(pasmo, suwX0 + i * pasmoSzer, suwY);
    lv_obj_set_style_bg_color(pasmo, rgb565(pasmaKolory[i]), 0);
    lv_obj_set_style_bg_opa(pasmo, LV_OPA_60, 0);
    lv_obj_set_style_radius(pasmo, (i == 0 || i == 2) ? 4 : 0, 0);
  }

  // Suwak NA WIERZCHU pasm - MAIN/INDICATOR przezroczyste (LV_OPA_TRANSP),
  // widoczna tylko galka (KNOB) jako wskaznik progu. Zakres STALY 0-1500,
  // NIE 0-4095 (patrz DYM_PROG_ZAKRES_MAX) - wartosci realne (typowo <700)
  // zajmowalyby zbyt maly wycinek pelnego zakresu ADC, trudny do trafienia
  // palcem.
  dymSlider = lv_slider_create(dymPanel);
  lv_obj_set_size(dymSlider, suwSzer, suwWys);
  lv_obj_set_pos(dymSlider, suwX0, suwY);
  lv_slider_set_range(dymSlider, 0, DYM_PROG_ZAKRES_MAX);
  lv_obj_set_style_bg_opa(dymSlider, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(dymSlider, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(dymSlider, rgb565(KOL_TEKST), LV_PART_KNOB);
  lv_obj_set_style_border_color(dymSlider, rgb565(KOL_TLO), LV_PART_KNOB);
  lv_obj_set_style_border_width(dymSlider, 2, LV_PART_KNOB);
  lv_obj_add_event_cb(dymSlider, dym_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(dymSlider, dym_slider_event_cb, LV_EVENT_RELEASED, nullptr);

  // 3 wiersze spinbox ponizej suwaka (suwY+suwWys+odstep = 84+16+30 = 130).
  const int startY = 130;
  dym_ust_wiersz_zbuduj(dymPanel, startY + 0 * 40, DYM_USTAWIENIA_POLA[0].etykieta, &dymUstWartoscLabel[0], 0);
  dym_ust_wiersz_zbuduj(dymPanel, startY + 1 * 40, DYM_USTAWIENIA_POLA[1].etykieta, &dymUstWartoscLabel[1], 1);
  dym_ust_wiersz_zbuduj(dymPanel, startY + 2 * 40, DYM_USTAWIENIA_POLA[2].etykieta, &dymUstWartoscLabel[2], 2);

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(dymPanel, dym_panel_zamknij_cb);
}

// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Wspolny panel dla
// kafelkow PIEC CO i PANEL - ten sam wzorzec co panel DYM powyzej (suwak +
// idle-timeout + zamkniecie), ale UPROSZCZONY: jeden prog, bez 3 stref w
// tle (nie ma tu osobnego progu "uwaga" - tylko OK/ALARM, patrz
// piec_alarm_odswiez()/panel_alarm_odswiez()) i bez dodatkowych wierszy
// spinbox (progAlarmTemp to jedyna wartosc tego ekranu).
static void alarm_temp_slider_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  int wartosc = lv_slider_get_value(alarmTempSlider);
  char bufor[16];
  snprintf(bufor, sizeof(bufor), "PROG: %d C", wartosc);
  lv_label_set_text(alarmTempProgLabel, bufor); // zywa etykieta PODCZAS przeciagania
  if (code == LV_EVENT_RELEASED) {
    g_alarmTempProg = wartosc;
    if (g_alarmTempProgCb) g_alarmTempProgCb(wartosc);
  }
}

static void alarm_temp_panel_zamknij_cb(lv_event_t *e); // forward - potrzebny juz w idle-timeout ponizej
static void alarm_temp_panel_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_alarmTempPanelIdleTimer = nullptr;
  alarm_temp_panel_zamknij_cb(nullptr);
}
static void alarm_temp_panel_idle_resetuj() {
  if (g_alarmTempPanelIdleTimer != nullptr) { lv_timer_del(g_alarmTempPanelIdleTimer); g_alarmTempPanelIdleTimer = nullptr; }
  g_alarmTempPanelIdleTimer = lv_timer_create(alarm_temp_panel_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_alarmTempPanelIdleTimer, 1);
}
static void alarm_temp_panel_zamknij_cb(lv_event_t *e) {
  (void)e;
  if (dashboardScr != nullptr) lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (g_alarmTempPanelIdleTimer != nullptr) { lv_timer_del(g_alarmTempPanelIdleTimer); g_alarmTempPanelIdleTimer = nullptr; }
}

static void alarm_temp_panel_otworz() {
  char bufor[16];
  snprintf(bufor, sizeof(bufor), "PROG: %d C", g_alarmTempProg);
  lv_label_set_text(alarmTempProgLabel, bufor);
  lv_slider_set_value(alarmTempSlider, g_alarmTempProg, LV_ANIM_OFF); // start od PRAWDZIWEJ ostatniej wartosci
  lv_scr_load_anim(alarmTempPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  alarm_temp_panel_idle_resetuj();
}

// Oba kafelki (PIEC CO i PANEL) wola TEN SAM panel - jeden wspolny prog.
static void piec_kafelek_klikniety_cb(lv_event_t *e) { (void)e; alarm_temp_panel_otworz(); }
static void panel_kafelek_klikniety_cb(lv_event_t *e) { (void)e; alarm_temp_panel_otworz(); }

static void alarm_temp_panel_zbuduj(lv_obj_t *scr) {
  dashboardScr = scr; // moze juz byc ustawione przez pompa/serwo/dym_panel_zbuduj() - bezpieczne do powtorzenia
  alarmTempPanel = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(alarmTempPanel, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(alarmTempPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(alarmTempPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *tytul = lv_label_create(alarmTempPanel);
  lv_label_set_text(tytul, "PROG PRZEGRZANIA");
  lv_obj_set_style_text_font(tytul, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(tytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(tytul, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *podtytul = lv_label_create(alarmTempPanel);
  lv_label_set_text(podtytul, "Wspolny dla PIEC CO i PANEL");
  lv_obj_set_style_text_font(podtytul, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(podtytul, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(podtytul, LV_ALIGN_TOP_MID, 0, 44);

  alarmTempProgLabel = lv_label_create(alarmTempPanel);
  lv_label_set_text(alarmTempProgLabel, "PROG: 80 C");
  lv_obj_set_style_text_font(alarmTempProgLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(alarmTempProgLabel, rgb565(KOL_TEKST), 0);
  lv_obj_align(alarmTempProgLabel, LV_ALIGN_TOP_MID, 0, 78);

  // Suwak PELNY (nie transparentny jak w DYM) - tu nie ma potrzeby ukrywania
  // MAIN/INDICATOR pod statycznymi pasmami, bo nie ma osobnej strefy
  // "uwaga" do zaznaczenia - jeden prog, wypelnienie po prostu rosnie z
  // wartoscia, tak jak w standardowym lv_slider.
  const int suwSzer = 340, suwWys = 16;
  const int suwX0 = (480 - suwSzer) / 2;
  const int suwY = 120;
  alarmTempSlider = lv_slider_create(alarmTempPanel);
  lv_obj_set_size(alarmTempSlider, suwSzer, suwWys);
  lv_obj_set_pos(alarmTempSlider, suwX0, suwY);
  lv_slider_set_range(alarmTempSlider, 0, 100); // ten sam zakres co POLA[] "progAlarmTemp" (min=0, max=100) w main.cpp
  lv_obj_set_style_bg_color(alarmTempSlider, rgb565(KOL_RAMKA), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(alarmTempSlider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(alarmTempSlider, rgb565(KOL_ERR), LV_PART_INDICATOR); // czerwony - "im dalej, tym blizej alarmu"
  lv_obj_set_style_bg_opa(alarmTempSlider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(alarmTempSlider, rgb565(KOL_TEKST), LV_PART_KNOB);
  lv_obj_set_style_border_color(alarmTempSlider, rgb565(KOL_TLO), LV_PART_KNOB);
  lv_obj_set_style_border_width(alarmTempSlider, 2, LV_PART_KNOB);
  lv_obj_add_event_cb(alarmTempSlider, alarm_temp_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(alarmTempSlider, alarm_temp_slider_event_cb, LV_EVENT_RELEASED, nullptr);

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(alarmTempPanel, alarm_temp_panel_zamknij_cb);
}

// ============================================================================
// EKRAN ALARMOWY, ta sesja - patrz PLAN_EKRAN_ALARMOWY.md. W odroznieniu od
// KAZDEGO innego ekranu w tym pliku: brak wlasnego kafelka otwierajacego
// (jedyne wejscie to alarm_ekran_odswiez(), wolane z
// lvgl_dashboard_poc_aktualizuj() na kazdym STATUS_INTERWAL_MS) i brak
// idle-timeoutu.
// ============================================================================

// Alarm MOZE wystartowac, gdy user jest akurat na innym pelnoekranowym
// panelu (POMPA/SERWO/USTAWIENIA/...) - w odroznieniu od normalnej
// nawigacji (zawsze przez WLASNY przycisk ZAMKNIJ panelu, ktory przy okazji
// kasuje SWOJ idle-timer), tu wchodzimy na inny ekran "z zewnatrz", z
// pominieciem tamtego zamkniecia. Bez tej funkcji osierocony idle-timer
// tamtego panelu odpaliby sie pozniej w tle i wykonal
// lv_scr_load_anim(dashboardScr,...) PRZEZ NASZ alarm - realny alarm
// znikalby uzytkownikowi z ekranu bez jego udzialu. Kasuje KAZDY idle-timer
// w pliku (nie tylko "prawdopodobne") - tania operacja (10 sprawdzen
// wskaznika), wolana tylko przy faktycznym pokazaniu ekranu (nie na kazdym
// cyklu aktualizuj()).
static void alarm_ekran_wszystkie_idle_timery_anuluj() {
  lv_timer_t **wszystkie[] = {
    &g_pompaPanelIdleTimer, &g_serwoWyborIdleTimer, &g_serwoPanelIdleTimer,
    &g_serwoUstIdleTimer, &g_pompaUstIdleTimer, &g_alarmTempPanelIdleTimer,
    &g_wykresyPanelIdleTimer, &g_drugiPulpitIdleTimer,
    &g_wyswietlaczPanelIdleTimer, &g_dymPanelIdleTimer,
  };
  for (lv_timer_t **t : wszystkie) {
    if (*t != nullptr) { lv_timer_del(*t); *t = nullptr; }
  }
}

// Ten sam dokladnie wzorzec co RECZNY w panelu SERWO / WYLACZ RECZNIE w
// panelu POMPA (patrz serwo_panel_animacje_start()) - panel_anim_recolor_opa_cb
// jest generyczna (dziala na dowolnym lv_obj_t* z ustawionym img_recolor),
// zero nowego kodu animacji.
static void alarm_ekran_animacja_start() {
  lv_anim_t aPulse;
  lv_anim_init(&aPulse);
  lv_anim_set_var(&aPulse, alarmEkranIkona);
  lv_anim_set_exec_cb(&aPulse, panel_anim_recolor_opa_cb);
  lv_anim_set_values(&aPulse, LV_OPA_50, LV_OPA_COVER);
  lv_anim_set_time(&aPulse, 700);
  lv_anim_set_playback_time(&aPulse, 700);
  lv_anim_set_repeat_count(&aPulse, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&aPulse, lv_anim_path_ease_in_out);
  lv_anim_start(&aPulse);
}

static void alarm_ekran_animacja_stop() {
  lv_anim_del(alarmEkranIkona, panel_anim_recolor_opa_cb);
  if (alarmEkranIkona) lv_obj_set_style_img_recolor_opa(alarmEkranIkona, LV_OPA_COVER, 0);
}

// Tresc ekranu (tytul/podtytul/kolor ikony+tytulu) dla danego poziomu -
// wspolna dla pierwszego pokazania I dla eskalacji/deeskalacji w trakcie,
// gdy ekran jest juz widoczny (patrz alarm_ekran_pokaz()). POZAR - czerwony
// (KOL_ERR, ten sam jezyk co reszta alarmow w pliku). OGRZEWANIE -
// bursztynowy (KOL_WARN) - jedno lub oba zrodla naraz, brzeczyk tez ich nie
// rozroznia (main.cpp, WZORZEC_OGRZEWANIE wspolny dla obu), wiec tekst
// podtytulu jest jedynym miejscem, gdzie user widzi KTORE dokladnie.
static void alarm_ekran_tresc_ustaw(AlarmPoziom poziom, bool alarmOgrzewanie, bool alarmPanel) {
  uint16_t kolor = (poziom == AlarmPoziom::POZAR) ? KOL_ERR : KOL_WARN;
  lv_obj_set_style_img_recolor(alarmEkranIkona, rgb565(kolor), 0);
  lv_obj_set_style_text_color(alarmEkranTytul, rgb565(kolor), 0);
  if (poziom == AlarmPoziom::POZAR) {
    lv_label_set_text(alarmEkranTytul, "POZAR / DYM");
    lv_label_set_text(alarmEkranPodtytul, "Sprawdz palenisko natychmiast");
  } else {
    lv_label_set_text(alarmEkranTytul, "PRZEGRZANIE");
    if (alarmOgrzewanie && alarmPanel) {
      lv_label_set_text(alarmEkranPodtytul, "Piec CO + Panel powyzej progu");
    } else if (alarmOgrzewanie) {
      lv_label_set_text(alarmEkranPodtytul, "Piec CO powyzej progu");
    } else {
      lv_label_set_text(alarmEkranPodtytul, "Panel powyzej progu");
    }
  }
}

static void alarm_ekran_pokaz(AlarmPoziom poziom, bool alarmOgrzewanie, bool alarmPanel) {
  bool juzWidoczny = (lv_scr_act() == alarmEkran);
  alarm_ekran_tresc_ustaw(poziom, alarmOgrzewanie, alarmPanel);
  if (!juzWidoczny) {
    lv_label_set_text(alarmEkranWyciszLabel, "WYCISZ"); // reset po ew. poprzednim "WYCISZONO"
    alarm_ekran_wszystkie_idle_timery_anuluj();
    lv_scr_load_anim(alarmEkran, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    alarm_ekran_animacja_start();
  }
  // gdy juzWidoczny==true: tylko aktualizacja tresci (eskalacja/deeskalacja
  // w trakcie) - ekran i animacja zostaja, bez restartu.
}

static void alarm_ekran_ukryj() {
  if (lv_scr_act() == alarmEkran && dashboardScr != nullptr) {
    lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  }
  alarm_ekran_animacja_stop();
}

static void alarm_ekran_zamknij_cb(lv_event_t *e) {
  (void)e;
  g_alarmOdrzucony = g_alarmPokazywany;
  alarm_ekran_ukryj();
}

static void alarm_ekran_wycisz_cb(lv_event_t *e) {
  (void)e;
  if (g_alarmWyciszCb) g_alarmWyciszCb();
  // Natychmiastowa informacja zwrotna - WYJATEK od zasady "nie zmieniaj nic
  // lokalnie na wiare" (patrz komentarz przy AlarmWyciszCb w
  // dashboard_lvgl_poc.h): to jedyny callback w calym pliku bez zadnego
  // HTTP w tle, wiec efekt JEST juz pewny w chwili powrotu z
  // g_alarmWyciszCb() - w odroznieniu od PompaAkcjaCb i reszty.
  if (alarmEkranWyciszLabel) lv_label_set_text(alarmEkranWyciszLabel, "WYCISZONO");
}

// Patrz obszerny komentarz przy alarmWskaznikDash (deklaracja) i przy jego
// budowie w alarm_ekran_zbuduj(). Resetuje g_alarmOdrzucony na BRAK -
// inaczej NASTEPNY cykl alarm_ekran_odswiez() (do STATUS_INTERWAL_MS pozniej)
// natychmiast schowalby ekran z powrotem, bo poziom wciaz bylby <= (juz
// swiadomie "przestarzalemu") g_alarmOdrzucony ustawionemu przy ZAMKNIJ.
static void alarm_wskaznik_tap_cb(lv_event_t *e) {
  (void)e;
  g_alarmOdrzucony = AlarmPoziom::BRAK;
  alarm_ekran_pokaz(g_alarmPokazywany, g_alarmOstatniOgrzewanie, g_alarmOstatniPanel);
}

// Wolane z lvgl_dashboard_poc_aktualizuj() na KAZDYM cyklu STATUS_INTERWAL_MS,
// zaraz po piec_alarm_odswiez()/panel_alarm_odswiez(). `poziom` liczony
// DOKLADNIE tak jak w main.cpp/brzeczykOdswiezZAlarmow (dym -> POZAR,
// inaczej ogrzewanie||panel -> OGRZEWANIE, inaczej BRAK) - priorytet MUSI
// byc identyczny w obu miejscach, zeby ekran i dzwiek zawsze zgadzaly sie
// ze soba co do tego, "co sie teraz dzieje".
//
// Tabela decyzji (pelne uzasadnienie w PLAN_EKRAN_ALARMOWY.md, sekcja 2):
//   poziom == BRAK                              -> ukryj (jesli pokazany),
//                                                   zresetuj g_alarmOdrzucony
//                                                   (kolejny alarm ma prawo
//                                                   pokazac sie od nowa)
//   poziom >  BRAK i poziom >  g_alarmOdrzucony -> pokaz/zaktualizuj (nowy
//                                                   alarm albo eskalacja
//                                                   ponad to, co user juz
//                                                   zamknal przyciskiem
//                                                   ZAMKNIJ)
//   poziom >  BRAK i poziom <= g_alarmOdrzucony -> ukryj, jesli pokazany
//                                                   (alarm trwa, ale na
//                                                   poziomie juz swiadomie
//                                                   zamknietym - nie nagaj
//                                                   ponownie)
static void alarm_ekran_odswiez(bool dymAlarm, bool alarmOgrzewanie, bool alarmPanel) {
  if (alarmEkran == nullptr) return; // ekran jeszcze nie zbudowany - nie powinno sie zdarzyc po init()

  g_alarmOstatniOgrzewanie = alarmOgrzewanie; // [WSKAZNIK POWROTU] patrz alarm_wskaznik_tap_cb()
  g_alarmOstatniPanel = alarmPanel;

  AlarmPoziom poziom = dymAlarm ? AlarmPoziom::POZAR
                     : (alarmOgrzewanie || alarmPanel) ? AlarmPoziom::OGRZEWANIE
                     : AlarmPoziom::BRAK;

  // [WSKAZNIK POWROTU, ta sesja] Dzwonek w pasku naglowka - widoczny
  // DOKLADNIE wtedy, gdy jest jakikolwiek aktywny alarm, niezaleznie od
  // tego, czy pelny ekran jest akurat pokazany czy zamkniety (ZAMKNIJ nie
  // zdejmuje alarmu, tylko go chowa). Zyje na dashboardScr, wiec i tak
  // widac go tylko gdy dashboard jest aktywnym ekranem - LVGL nie rysuje
  // nieaktywnych ekranow, wiec nie trzeba tego dodatkowo sprawdzac tutaj.
  if (alarmWskaznikDash != nullptr) {
    if (poziom == AlarmPoziom::BRAK) {
      lv_obj_add_flag(alarmWskaznikDash, LV_OBJ_FLAG_HIDDEN);
    } else {
      uint16_t kolor = (poziom == AlarmPoziom::POZAR) ? KOL_ERR : KOL_WARN;
      lv_obj_set_style_text_color(alarmWskaznikDash, rgb565(kolor), 0);
      lv_obj_clear_flag(alarmWskaznikDash, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (poziom == AlarmPoziom::BRAK) {
    if (g_alarmPokazywany != AlarmPoziom::BRAK) alarm_ekran_ukryj();
    g_alarmPokazywany = AlarmPoziom::BRAK;
    g_alarmOdrzucony = AlarmPoziom::BRAK;
    return;
  }

  if (poziom > g_alarmOdrzucony) {
    alarm_ekran_pokaz(poziom, alarmOgrzewanie, alarmPanel);
    g_alarmPokazywany = poziom;
  } else {
    if (g_alarmPokazywany != AlarmPoziom::BRAK) alarm_ekran_ukryj();
    g_alarmPokazywany = poziom;
  }
}

static void alarm_ekran_zbuduj(lv_obj_t *scr) {
  dashboardScr = scr; // moze juz byc ustawione przez inny *_zbuduj() - bezpieczne do powtorzenia
  alarmEkran = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(alarmEkran, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(alarmEkran, LV_OPA_COVER, 0);
  lv_obj_clear_flag(alarmEkran, LV_OBJ_FLAG_SCROLLABLE);

  alarmEkranTytul = lv_label_create(alarmEkran);
  lv_label_set_text(alarmEkranTytul, "ALARM");
  lv_obj_set_style_text_font(alarmEkranTytul, &lv_font_montserrat_28, 0);
  lv_obj_align(alarmEkranTytul, LV_ALIGN_TOP_MID, 0, 14);

  alarmEkranPodtytul = lv_label_create(alarmEkran);
  lv_label_set_text(alarmEkranPodtytul, "");
  lv_obj_set_style_text_font(alarmEkranPodtytul, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(alarmEkranPodtytul, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(alarmEkranPodtytul, LV_ALIGN_TOP_MID, 0, 50);

  // 128x128 (IKONA_ALARM_DUZA_ROZMIAR) - miesci sie wygodnie miedzy
  // podtytulem (konczy sie ok. y=68) a przyciskami (zaczynaja sie y=232).
  alarmEkranIkona = lv_img_create(alarmEkran);
  lv_img_set_src(alarmEkranIkona, &lv_ikona_alarm_duza);
  lv_obj_align(alarmEkranIkona, LV_ALIGN_TOP_MID, 0, 78);

  // [v1.2.1] Jedna linia miedzy ikona (konczy sie y=206) a przyciskami
  // (y=232): to samo, co stoi w naglowku dashboardu - "BRAK WIFI 14 s",
  // "RESTART ZA 7 s: ...", "SYMULACJA: piec". Ekran alarmu zaslania
  // naglowek, a wlasnie przy alarmie trzeba wiedziec, czy panel ma jeszcze
  // dane z centrali i czy wartosc, ktora wywolala alarm, jest pomiarem.
  // Jasny kolor tekstu, nie barwa alarmu - to informacja obok alarmu.
  alarmEkranZnacznik = lv_label_create(alarmEkran);
  lv_label_set_text(alarmEkranZnacznik, "");
  lv_obj_set_style_text_color(alarmEkranZnacznik, rgb565(KOL_TEKST), 0);
  lv_obj_align(alarmEkranZnacznik, LV_ALIGN_TOP_MID, 0, 210);
  lv_obj_add_flag(alarmEkranZnacznik, LV_OBJ_FLAG_HIDDEN);

  // X w rogu - ten sam fallback co kazdy inny panel w tym pliku (patrz
  // alarm_temp_panel_zbuduj() wyzej), rownowazny duzemu przyciskowi ZAMKNIJ
  // nizej (ta sama funkcja callback).
  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(alarmEkran, alarm_ekran_zamknij_cb);

  // Dwa duze przyciski obok siebie - tekstowe, bez ikon (mniejszy zakres
  // roboty, patrz PLAN_EKRAN_ALARMOWY.md sekcja 3/4). WYCISZ w barwie
  // ostrzegawczej (KOL_WARN), ZAMKNIJ neutralnie (KOL_KARTA) - ten sam
  // jezyk "kolor = waznosc akcji" co reszta przyciskow w pliku (patrz np.
  // pompaAntystopBtn/KOL_BEZPIECZNA).
  const int btnW = 200, btnH = 70, gap = 20;
  const int startX = (480 - (btnW * 2 + gap)) / 2;
  const int btnY = 232;

  lv_obj_t *wyciszBtn = lv_btn_create(alarmEkran);
  lv_obj_set_size(wyciszBtn, btnW, btnH);
  lv_obj_set_pos(wyciszBtn, startX, btnY);
  lv_obj_set_style_radius(wyciszBtn, 8, 0);
  lv_obj_clear_flag(wyciszBtn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(wyciszBtn, rgb565(KOL_WARN), 0);
  press_flash_zainstaluj(wyciszBtn, KOL_WARN, 5);
  lv_obj_add_event_cb(wyciszBtn, alarm_ekran_wycisz_cb, LV_EVENT_CLICKED, nullptr);
  alarmEkranWyciszLabel = lv_label_create(wyciszBtn);
  lv_label_set_text(alarmEkranWyciszLabel, "WYCISZ");
  lv_obj_set_style_text_font(alarmEkranWyciszLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(alarmEkranWyciszLabel, rgb565(KOL_TLO), 0);
  lv_obj_center(alarmEkranWyciszLabel);

  lv_obj_t *zamknijBtn = lv_btn_create(alarmEkran);
  lv_obj_set_size(zamknijBtn, btnW, btnH);
  lv_obj_set_pos(zamknijBtn, startX + btnW + gap, btnY);
  lv_obj_set_style_radius(zamknijBtn, 8, 0);
  lv_obj_clear_flag(zamknijBtn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(zamknijBtn, rgb565(KOL_KARTA), 0);
  press_flash_zainstaluj(zamknijBtn, KOL_RAMKA, 5);
  lv_obj_add_event_cb(zamknijBtn, alarm_ekran_zamknij_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *zamknijLabel = lv_label_create(zamknijBtn);
  lv_label_set_text(zamknijLabel, "ZAMKNIJ");
  lv_obj_set_style_text_font(zamknijLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(zamknijLabel, rgb565(KOL_TEKST), 0);
  lv_obj_center(zamknijLabel);

  // [WSKAZNIK POWROTU, ta sesja] Maly dzwonek w pasku naglowka dashboardu
  // (ten sam pasek co WiFi/globus internetu - patrz netGlobusSiatka w
  // lvgl_dashboard_poc_init(), juz zbudowany w tym momencie, wywolanie
  // alarm_ekran_zbuduj(scr) jest PO nim). LV_SYMBOL_BELL to wbudowany
  // glif LVGL (dziala z domyslna czcionka montserrat, zero nowej ikony-
  // bitmapy potrzebne). lv_obj_set_ext_click_area powieksza obszar dotyku
  // ponad wizualny rozmiar 14px glifu - sam glif zostaje maly/dyskretny w
  // pasku, ale nadal latwo trafic palcem na ekranie rezystancyjnym.
  // Domyslnie ukryty - pokazywany/ukrywany i przebarwiany w
  // alarm_ekran_odswiez() w zaleznosci od tego, czy jest aktywny alarm.
  alarmWskaznikDash = lv_label_create(scr);
  lv_label_set_text(alarmWskaznikDash, LV_SYMBOL_BELL);
  lv_obj_align_to(alarmWskaznikDash, netGlobusSiatka, LV_ALIGN_OUT_LEFT_MID, -10, 0);
  lv_obj_add_flag(alarmWskaznikDash, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(alarmWskaznikDash, 12);
  lv_obj_add_event_cb(alarmWskaznikDash, alarm_wskaznik_tap_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(alarmWskaznikDash, LV_OBJ_FLAG_HIDDEN);
}

// [DRUGI PULPIT, ta sesja] Uzytkownik: gest na dashboardzie ma NIE wchodzic
// wprost do Wykresow, tylko przenosic na DRUGI EKRAN, ktory jest wlasna
// siatka (jak dashboard) z kafelkami-launcherami - na start WYKRESY i
// USTAWIENIA WYSWIETLACZA, w przyszlosci mozna dopisywac kolejne bez
// przerabiania nawigacji gestowej. Wzorzec ekranu (pelnoekranowy
// lv_obj_create(nullptr), X jako fallback, idle-timeout, gest jako alternatywa
// dla X) identyczny jak reszta paneli w tym pliku.
static void drugi_pulpit_zamknij_cb(lv_event_t *e); // forward - potrzebny w idle-timeout ponizej
static void drugi_pulpit_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_drugiPulpitIdleTimer = nullptr;
  drugi_pulpit_zamknij_cb(nullptr);
}
static void drugi_pulpit_idle_resetuj() {
  if (g_drugiPulpitIdleTimer != nullptr) { lv_timer_del(g_drugiPulpitIdleTimer); g_drugiPulpitIdleTimer = nullptr; }
  g_drugiPulpitIdleTimer = lv_timer_create(drugi_pulpit_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_drugiPulpitIdleTimer, 1);
}
static void drugi_pulpit_zamknij_cb(lv_event_t *e) {
  (void)e;
  if (dashboardScr != nullptr) lv_scr_load_anim(dashboardScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (g_drugiPulpitIdleTimer != nullptr) { lv_timer_del(g_drugiPulpitIdleTimer); g_drugiPulpitIdleTimer = nullptr; }
}
static void drugi_pulpit_otworz() {
  lv_scr_load_anim(drugiPulpitScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  drugi_pulpit_idle_resetuj();
}
// Gest na drugim pulpicie: w KTORAKOLWIEK strone wraca na dashboard - ta sama
// symetria co ustalona wczesniej dla dashboard_gesture_cb()/wykresy_panel_gesture_cb().
static void drugi_pulpit_gesture_cb(lv_event_t *e) {
  lv_indev_t *indev = lv_indev_get_act();
  if (indev == nullptr) return;
  lv_dir_t kierunek = lv_indev_get_gesture_dir(indev);
  if (kierunek == LV_DIR_LEFT || kierunek == LV_DIR_RIGHT) {
    drugi_pulpit_zamknij_cb(nullptr);
    lv_indev_wait_release(indev); // ten sam fix co w dashboard_gesture_cb() - patrz komentarz tam
  }
}
// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Kafelek WYSWIETLACZ dostaje
// teraz wlasny, pelnoekranowy panel - ten sam wzorzec co panel DYM (suwak(i)
// + wiersze spinbox + idle-timeout), tylko wejscie/wyjscie jest hierarchiczne
// przez DRUGI PULPIT (jak WYKRESY), nie wprost z dashboardu - patrz
// wyswietlacz_panel_zamknij_cb() nizej.
//
// [WAZNE] W ODROZNIENIU od DYM/SERWO/POMPA/ALARM TEMP powyzej, te wartosci
// main.cpp NIE wysyla do centrali - main.cpp trzyma je lokalnie w NVS i
// steruje PWM na TFT_BL bezposrednio (patrz obszerny komentarz w
// dashboard_lvgl_poc.h). Dla TEGO pliku nie zmienia to NIC - callbacki
// wygladaja i dzialaja identycznie (LVGL nadal nie wie NIC o tym, co main.cpp
// robi po drugiej stronie, HTTP czy NVS).
// ----------------------------------------------------------------------------
static void wyswietlacz_panel_idle_resetuj(); // forward - potrzebny juz w callbackach ponizej

// Suwaki jasnosci - id 0=dzien,1=noc (user_data), ten sam wzorzec co
// serwo_slider_event_cb (id 0=klapa,1=syberek). Etykieta zywa PODCZAS
// przeciagania; od poprawki ponizej PWM tez reaguje na biezaco w trakcie
// przeciagania, a NA ZWOLNIENIE galki idzie dodatkowo zapis do NVS
// (rozroznione przez `koncowa` przekazywane do g_wyswietlaczJasnoscCb).
static void wyswietlacz_slider_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t *slider = lv_event_get_target(e);
  lv_obj_t *label = (id == 0) ? wyswietlaczLabelDzien : wyswietlaczLabelNoc;
  int procent = lv_slider_get_value(slider);
  char bufor[8];
  snprintf(bufor, sizeof(bufor), "%d%%", procent);
  lv_label_set_text(label, bufor);
  wyswietlacz_panel_idle_resetuj();
  if (id == 0) g_wyswietlaczJasnoscDzien = procent; else g_wyswietlaczJasnoscNoc = procent;
  // [FIX, ta sesja - "jasnosc widoczna dopiero po puszczeniu"] Wolane TERAZ
  // tez na VALUE_CHANGED (podglad PWM), nie tylko RELEASED (zapis NVS) -
  // main.cpp odroznia je przez `koncowa`.
  if (g_wyswietlaczJasnoscCb) g_wyswietlaczJasnoscCb(id, procent, code == LV_EVENT_RELEASED);
}

// Jeden wiersz "nazwa + wartosc% + suwak" - ten sam ukladzik co
// serwo_wiersz_osi_zbuduj(), tylko bez luku (nie potrzebny tutaj - patrz
// uzasadnienie luku tamtej funkcji, specyficzne dla panelu SERWO).
static void wyswietlacz_jasnosc_wiersz_zbuduj(lv_obj_t *parent, int y, const char *nazwa, int id,
                                               lv_obj_t **sliderOut, lv_obj_t **labelOut) {
  lv_obj_t *wiersz = lv_obj_create(parent);
  lv_obj_remove_style_all(wiersz);
  lv_obj_set_size(wiersz, 340, 50);
  lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *nazwaLabel = lv_label_create(wiersz);
  lv_label_set_text(nazwaLabel, nazwa);
  lv_obj_set_style_text_font(nazwaLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(nazwaLabel, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(nazwaLabel, LV_ALIGN_TOP_LEFT, 0, 2);

  lv_obj_t *wartoscLabel = lv_label_create(wiersz);
  lv_label_set_text(wartoscLabel, "0%");
  lv_obj_set_style_text_font(wartoscLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wartoscLabel, rgb565(KOL_TEKST), 0);
  lv_obj_align(wartoscLabel, LV_ALIGN_TOP_RIGHT, 0, -2);

  // [FIX v2, ta sesja - potwierdzone na sprzecie] Zgadywanie z poprzedniego
  // komentarza (NIEPOTWIERDZONE NA SPRZECIE) okazalo sie trafne - motyw
  // DOKLADA WLASNY padding do LV_PART_KNOB, wiec 24px marginesu bylo wciaz
  // "odrobine za duze" (lekkie obciecie). Naprawa: pad_all na KNOB ustawiony
  // JAWNIE (4px, promien = 8+4 = 12px, POLICZONY nie zgadywany) + margines
  // podniesiony do 32px (16px/strone). Pelne uzasadnienie i te same wartosci
  // co w serwo_wiersz_osi_zbuduj() (patrz tamten komentarz) - identyczny
  // 16px suwak, ten sam motyw.
  lv_obj_t *slider = lv_slider_create(wiersz);
  lv_obj_set_size(slider, 308, 16);
  lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_slider_set_range(slider, 0, 100);
  lv_obj_set_style_bg_color(slider, rgb565(KOL_RAMKA), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, rgb565(id == 0 ? KOL_WYSWIETLACZ_DZIEN : KOL_WYSWIETLACZ_NOC), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, rgb565(KOL_TEKST), LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, rgb565(KOL_TLO), LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
  lv_obj_add_event_cb(slider, wyswietlacz_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
  lv_obj_add_event_cb(slider, wyswietlacz_slider_event_cb, LV_EVENT_RELEASED, (void *)(intptr_t)id);

  *sliderOut = slider;
  *labelOut = wartoscLabel;
}

// Wiersze spinbox godzStart/godzKoniec - ten sam "etykieta + duza wartosc +
// -/+ po bokach" ukladzik co dym_ust_wiersz_zbuduj(), tylko wyswietla "H:00"
// zamiast golej liczby i ZAWIJA SIE zamiast klipsowac (patrz komentarz przy
// WYSWIETLACZ_USTAWIENIA_POLA wyzej).
static void wyswietlacz_ust_wyslij(int idx) {
  char bufor[8];
  snprintf(bufor, sizeof(bufor), "%d:00", g_wyswietlaczGodz[idx]);
  lv_label_set_text(wyswietlaczGodzLabel[idx], bufor);
  if (g_wyswietlaczUstawieniaCb) g_wyswietlaczUstawieniaCb(WYSWIETLACZ_USTAWIENIA_POLA[idx].klucz, g_wyswietlaczGodz[idx]);
}
static void wyswietlacz_ust_minus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  g_wyswietlaczGodz[idx] = (g_wyswietlaczGodz[idx] + 23) % 24; // -1 mod 24, bez ujemnych posrednich wartosci
  wyswietlacz_ust_wyslij(idx);
  wyswietlacz_panel_idle_resetuj();
}
static void wyswietlacz_ust_plus_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  g_wyswietlaczGodz[idx] = (g_wyswietlaczGodz[idx] + 1) % 24;
  wyswietlacz_ust_wyslij(idx);
  wyswietlacz_panel_idle_resetuj();
}
static void wyswietlacz_ust_wiersz_zbuduj(lv_obj_t *rodzic, int y, const char *etykieta,
                                           lv_obj_t **wartoscLabelOut, int idx) {
  const int wierszWys = 40;
  lv_obj_t *wiersz = lv_obj_create(rodzic);
  lv_obj_remove_style_all(wiersz);
  lv_obj_set_size(wiersz, 440, wierszWys);
  lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *etykietaLbl = lv_label_create(wiersz);
  lv_label_set_text(etykietaLbl, etykieta);
  lv_obj_set_style_text_font(etykietaLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(etykietaLbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align(etykietaLbl, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t *minus = lv_btn_create(wiersz);
  lv_obj_set_size(minus, 50, 34);
  lv_obj_set_style_radius(minus, 6, 0);
  lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -140, 0);
  press_flash_zainstaluj(minus, KOL_KARTA, 4);
  lv_obj_add_event_cb(minus, wyswietlacz_ust_minus_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(minus, wyswietlacz_ust_minus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)idx);
  lv_obj_t *minusLbl = lv_label_create(minus);
  lv_label_set_text(minusLbl, LV_SYMBOL_MINUS);
  lv_obj_set_style_text_color(minusLbl, rgb565(KOL_TEKST), 0);
  lv_obj_center(minusLbl);

  *wartoscLabelOut = lv_label_create(wiersz);
  lv_label_set_text(*wartoscLabelOut, "0:00");
  lv_obj_set_style_text_font(*wartoscLabelOut, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(*wartoscLabelOut, rgb565(KOL_TEKST), 0);
  lv_obj_align(*wartoscLabelOut, LV_ALIGN_RIGHT_MID, -80, 0);

  lv_obj_t *plus = lv_btn_create(wiersz);
  lv_obj_set_size(plus, 50, 34);
  lv_obj_set_style_radius(plus, 6, 0);
  lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -10, 0);
  press_flash_zainstaluj(plus, KOL_KARTA, 4);
  lv_obj_add_event_cb(plus, wyswietlacz_ust_plus_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(plus, wyswietlacz_ust_plus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)idx);
  lv_obj_t *plusLbl = lv_label_create(plus);
  lv_label_set_text(plusLbl, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_color(plusLbl, rgb565(KOL_TEKST), 0);
  lv_obj_center(plusLbl);
}

// Wlacznik/wylacznik calego harmonogramu - ten sam wzorzec co
// pompaAntystopBtn/pompa_antystop_wyglad_odswiez()/pompa_antystop_btn_cb()
// (lv_btn, NIE lv_switch - LV_USE_SWITCH jest wylaczony w lv_conf.h tego
// projektu).
static void wyswietlacz_harmonogram_wyglad_odswiez() {
  if (wyswietlaczHarmonogramBtn == nullptr) return;
  lv_obj_set_style_bg_color(wyswietlaczHarmonogramBtn,
                             rgb565(g_wyswietlaczHarmonogramWlaczony ? KOL_BEZPIECZNA : KOL_KARTA),
                             LV_PART_MAIN);
  lv_label_set_text(wyswietlaczHarmonogramBtnLabel, g_wyswietlaczHarmonogramWlaczony ? "WL." : "WYL.");
}
static void wyswietlacz_harmonogram_btn_cb(lv_event_t *e) {
  (void)e;
  g_wyswietlaczHarmonogramWlaczony = !g_wyswietlaczHarmonogramWlaczony;
  wyswietlacz_harmonogram_wyglad_odswiez();
  if (g_wyswietlaczHarmonogramCb) g_wyswietlaczHarmonogramCb(g_wyswietlaczHarmonogramWlaczony ? 1 : 0);
  wyswietlacz_panel_idle_resetuj();
}

// Idle-timeout + otwarcie/zamkniecie - ten sam hierarchiczny wzorzec
// "wraca na DRUGI PULPIT (nie od razu dashboard) i wznawia JEGO idle-timer"
// co Wykresy, patrz obszerny komentarz przy wykresy_panel_zamknij_cb()/
// wykresy_panel_otworz() nizej w tym pliku.
static void wyswietlacz_panel_zamknij_cb(lv_event_t *e); // forward - potrzebny juz w idle-timeout ponizej
static void wyswietlacz_panel_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_wyswietlaczPanelIdleTimer = nullptr;
  wyswietlacz_panel_zamknij_cb(nullptr);
}
static void wyswietlacz_panel_idle_resetuj() {
  if (g_wyswietlaczPanelIdleTimer != nullptr) { lv_timer_del(g_wyswietlaczPanelIdleTimer); g_wyswietlaczPanelIdleTimer = nullptr; }
  g_wyswietlaczPanelIdleTimer = lv_timer_create(wyswietlacz_panel_idle_timeout_cb, POMPA_STRONA_IDLE_MS, nullptr);
  lv_timer_set_repeat_count(g_wyswietlaczPanelIdleTimer, 1);
}
static void wyswietlacz_panel_zamknij_cb(lv_event_t *e) {
  (void)e;
  if (drugiPulpitScr != nullptr) lv_scr_load_anim(drugiPulpitScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (g_wyswietlaczPanelIdleTimer != nullptr) { lv_timer_del(g_wyswietlaczPanelIdleTimer); g_wyswietlaczPanelIdleTimer = nullptr; }
  // [FIX, ten sam powod co w wykresy_panel_zamknij_cb()] Idle-timer drugiego
  // pulpitu zostal anulowany przy WEJSCIU na ten panel (patrz
  // wyswietlacz_panel_otworz() nizej) i nic go od tamtej pory nie wznowilo -
  // bez tego drugi pulpit zostalby bez wlasnego idle-timera i juz nigdy sam
  // nie wrocilby do dashboardu.
  drugi_pulpit_idle_resetuj();
}
static void wyswietlacz_panel_otworz() {
  if (g_drugiPulpitIdleTimer != nullptr) { lv_timer_del(g_drugiPulpitIdleTimer); g_drugiPulpitIdleTimer = nullptr; }

  char bufor[8];
  snprintf(bufor, sizeof(bufor), "%d%%", g_wyswietlaczJasnoscDzien);
  lv_label_set_text(wyswietlaczLabelDzien, bufor);
  lv_slider_set_value(wyswietlaczSliderDzien, g_wyswietlaczJasnoscDzien, LV_ANIM_OFF);
  snprintf(bufor, sizeof(bufor), "%d%%", g_wyswietlaczJasnoscNoc);
  lv_label_set_text(wyswietlaczLabelNoc, bufor);
  lv_slider_set_value(wyswietlaczSliderNoc, g_wyswietlaczJasnoscNoc, LV_ANIM_OFF);
  for (int i = 0; i < 2; i++) {
    char b2[8];
    snprintf(b2, sizeof(b2), "%d:00", g_wyswietlaczGodz[i]);
    lv_label_set_text(wyswietlaczGodzLabel[i], b2);
  }
  wyswietlacz_harmonogram_wyglad_odswiez();

  lv_scr_load_anim(wyswietlaczPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  wyswietlacz_panel_idle_resetuj();
}
static void wyswietlacz_kafelek_klikniety_cb(lv_event_t *e) { (void)e; wyswietlacz_panel_otworz(); }

static void wyswietlacz_panel_zbuduj(lv_obj_t *scr) {
  dashboardScr = scr; // moze juz byc ustawione przez inny *_zbuduj() - bezpieczne do powtorzenia
  wyswietlaczPanel = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(wyswietlaczPanel, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(wyswietlaczPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(wyswietlaczPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *tytul = lv_label_create(wyswietlaczPanel);
  lv_label_set_text(tytul, "WYSWIETLACZ");
  lv_obj_set_style_text_font(tytul, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(tytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(tytul, LV_ALIGN_TOP_MID, 0, 10);

  // [NIEPOTWIERDZONE NA SPRZECIE - jak wszystkie nowe wspolrzedne w tym
  // pliku, patrz PLAN_WYKRESY_LVGL.md "Tryb pracy nad kodem"] Uklad policzony
  // tak: tytul (y10, font 28, ~38px) -> 2x wiersz jasnosci (50px kazdy, y=56
  // i y=118, 12px odstepu) -> 2x wiersz godzin (40px kazdy, y=186 i y=226) ->
  // wiersz harmonogramu (40px, y=266) -> koniec 306, 14px marginesu od dolu
  // (320 wys. ekranu) - wymaga zdjecia z ekranu do potwierdzenia.
  wyswietlacz_jasnosc_wiersz_zbuduj(wyswietlaczPanel, 56, "Jasnosc w dzien", 0,
                                     &wyswietlaczSliderDzien, &wyswietlaczLabelDzien);
  wyswietlacz_jasnosc_wiersz_zbuduj(wyswietlaczPanel, 118, "Jasnosc w nocy", 1,
                                     &wyswietlaczSliderNoc, &wyswietlaczLabelNoc);

  const int startY = 186;
  wyswietlacz_ust_wiersz_zbuduj(wyswietlaczPanel, startY + 0 * 40,
                                 WYSWIETLACZ_USTAWIENIA_POLA[0].etykieta, &wyswietlaczGodzLabel[0], 0);
  wyswietlacz_ust_wiersz_zbuduj(wyswietlaczPanel, startY + 1 * 40,
                                 WYSWIETLACZ_USTAWIENIA_POLA[1].etykieta, &wyswietlaczGodzLabel[1], 1);

  // Wiersz WL./WYL. harmonogramu - ten sam wzorzec co wiersz antystopu w
  // pompa_ustawienia_zbuduj() (pasek akcentu + etykieta + przycisk).
  {
    lv_obj_t *wiersz = lv_obj_create(wyswietlaczPanel);
    lv_obj_remove_style_all(wiersz);
    lv_obj_set_size(wiersz, 440, 40);
    lv_obj_align(wiersz, LV_ALIGN_TOP_MID, 0, startY + 2 * 40);
    lv_obj_clear_flag(wiersz, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *etykietaLbl = lv_label_create(wiersz);
    lv_label_set_text(etykietaLbl, "Automatyczny harmonogram");
    lv_obj_set_style_text_font(etykietaLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(etykietaLbl, rgb565(KOL_TEKST_DIM), 0);
    lv_obj_align(etykietaLbl, LV_ALIGN_LEFT_MID, 4, 0);

    wyswietlaczHarmonogramBtn = lv_btn_create(wiersz);
    lv_obj_set_size(wyswietlaczHarmonogramBtn, 74, 34);
    lv_obj_align(wyswietlaczHarmonogramBtn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_radius(wyswietlaczHarmonogramBtn, 8, 0);
    lv_obj_clear_flag(wyswietlaczHarmonogramBtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(wyswietlaczHarmonogramBtn, rgb565(KOL_KARTA), LV_PART_MAIN);
    press_flash_zainstaluj(wyswietlaczHarmonogramBtn, KOL_BEZPIECZNA, 5);
    lv_obj_add_event_cb(wyswietlaczHarmonogramBtn, wyswietlacz_harmonogram_btn_cb, LV_EVENT_CLICKED, nullptr);

    wyswietlaczHarmonogramBtnLabel = lv_label_create(wyswietlaczHarmonogramBtn);
    lv_label_set_text(wyswietlaczHarmonogramBtnLabel, "WL.");
    lv_obj_set_style_text_font(wyswietlaczHarmonogramBtnLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wyswietlaczHarmonogramBtnLabel, rgb565(KOL_TEKST), 0);
    lv_obj_center(wyswietlaczHarmonogramBtnLabel);
  }

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(wyswietlaczPanel, wyswietlacz_panel_zamknij_cb);
}
// Generyczny klikalny kafelek-launcher (etykieta + ikona, BEZ wartosci
// liczbowej jak w kafelek()) - do siatki drugiego pulpitu. Ten sam wzorzec
// 3D press-efektu co kafelek_pompa() (press_flash_zainstaluj), zeby siatka
// na tym ekranie wygladala/zachowywala sie identycznie jak dashboard.
static void kafelek_launcher(lv_obj_t *parent, int x, int y, int w, int h,
                              const char *etykieta, const lv_img_dsc_t *ikona, uint16_t kolorIkony,
                              lv_event_cb_t klikCb) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(card, rgb565(KOL_KARTA), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_color(card, rgb565(KOL_RAMKA), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
  press_flash_zainstaluj(card, KOL_KARTA, 3);
  lv_obj_add_event_cb(card, klikCb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, etykieta);
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 6);

  if (ikona != nullptr) {
    lv_obj_t *img = lv_img_create(card);
    lv_img_set_src(img, ikona);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img, rgb565(kolorIkony), 0);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 6);
  }
}
// Klikniecie w kafelek WYKRESY na drugim pulpicie - otwiera pelnowymiarowy
// ekran Wykresow (ta sama funkcja co dotychczas, tylko wolana z innego
// miejsca - z siatki menu zamiast wprost z gestu dashboardu).
static void wykresy_kafelek_klikniety_cb(lv_event_t *e); // forward, definicja wykresy_panel_otworz() jest nizej w pliku
static void drugi_pulpit_zbuduj(lv_obj_t *scr) {
  dashboardScr = scr; // moze juz byc ustawione przez inny *_zbuduj() - bezpieczne do powtorzenia

  drugiPulpitScr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(drugiPulpitScr, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(drugiPulpitScr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(drugiPulpitScr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(drugiPulpitScr, LV_OBJ_FLAG_CLICKABLE); // tlo lapie gest, jak wykresyPanel
  lv_obj_add_event_cb(drugiPulpitScr, drugi_pulpit_gesture_cb, LV_EVENT_GESTURE, nullptr);

  // [FIX tej sesji] Bez duzego tytulu "WIECEJ" - zabieral gorna czesc
  // ekranu i ograniczal siatke do mniejszej liczby rzedow. X w rogu
  // wystarcza do orientacji "jestem na innym ekranie niz dashboard", tak
  // samo jak na Wykresach/innych panelach.
  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(drugiPulpitScr, drugi_pulpit_zamknij_cb);

  // Ta sama geometria co siatka dashboardu (patrz const w
  // lvgl_dashboard_poc_init()) - drugi pulpit ma wygladac/zachowywac sie
  // identycznie, tylko z innym zestawem kafelkow, i miec docelowo miejsce
  // na az 12 slotow (4 kolumny x 3 rzedy), nie tylko na jeden rzad.
  const int x0 = 6, y0 = 30, kw = 114, kh = 90, odstepX = 4, odstepY = 6;
  (void)odstepY; // uzyte dopiero gdy dojdzie 2. rzad kafelkow
  kafelek_launcher(drugiPulpitScr, x0 + 0 * (kw + odstepX), y0, kw, kh,
                    "WYKRESY", &lv_ikona_wykresy, KOL_TEKST, wykresy_kafelek_klikniety_cb);
  kafelek_launcher(drugiPulpitScr, x0 + 1 * (kw + odstepX), y0, kw, kh,
                    "WYSWIETLACZ", &lv_ikona_zebatka, KOL_TEKST, wyswietlacz_kafelek_klikniety_cb);
  // Kolejne kafelki w przyszlosci: x0 + 2*(kw+odstepX), y0 (dalej w tym samym
  // rzedzie), a po zapelnieniu 4 kolumn - kolejny rzad na y0+kh+odstepY, itd.
  // - identyczny wzorzec jak rzady 0/1/2 w lvgl_dashboard_poc_init().
}

// ============================================================================
// [PARTIA 5b, ta sesja - DANE WYKRESOW] Mapowanie serii + parser CSV z
// redukcja punktow, patrz PLAN_WYKRESY_LVGL.md sekcja "Redukcja danych pod
// RAM". Zero zaleznosci od reszty ekranu Wykresy (5a) - ta sekcja tylko
// DOSTARCZA gotowe punkty do wykresu, UI (chipy 5c, lv_chart 5d) bedzie z
// niej korzystac w kolejnych partiach.
//
// Format CSV (potwierdzony w main_centrala.cpp):
//   KROTKI (/api/history, 13 kolumn):
//     etykieta,t_zewn,t_bojler,t_ogrz,t_ogrz_sr,t_powrot,t_panel,t_pokoj,
//     pompa,cisnienie,wilgotnosc,klapa_proc,syberko_proc
//   DLUGI (/api/history/long, 34 kolumny - etykieta + 11 pol x [sr,min,max],
//     w tej samej kolejnosci pol co powyzej, pompa pominieta bo nie ma sensu
//     usredniac boola):
//     etykieta,t_zewn_sr,t_zewn_min,t_zewn_max,t_bojler_sr,...,syberko_proc_max
// ============================================================================

// Docelowa liczba punktow na wykresie - "~80-100" z planu. 96 = wielokrotnosc
// typowych podzialow siatki lv_chart, wartosc arbitralna w tym przedziale.
static const int WYKRESY_MAX_PUNKTOW = 96;

// [FIX-WYKRESY-GODZINY] Do tej sesji GODZINY i DZIEN byly nierozroznialne -
// oba pobieraly caly bufor krotki (do 1440 wierszy, 24h) i kubelkowaly go
// TAK SAMO adaptacyjnym podwajaniem, wiec dawaly identyczny wykres (widoczne
// na zdjeciach: [Dzienny] i [Godzinny] piksel w piksel te same dane).
// Panel WWW (main_centrala.cpp, funkcja wykrRenderuj()) od dawna poprawnie
// rozroznia oba tryby z tego samego bufora krotkiego:
//   n = (_wykrTryb === 'godz') ? 60 : 1440;   // 60 probek x 60s = ~1h
// czyli "Godziny" to surowe (nieusrednione) ostatnie 60 probek, a "Dzien" to
// caly bufor. Ponizsza stala odtwarza TEN SAM podzial po stronie LVGL -
// patrz oknoPrzesuwne w WykresyStrumienStan i uzycie w
// wykresy_strumien_zamknij_kubelek().
static const int WYKRESY_GODZINY_OKNO = 60;

// Jedna "seria" wybieralna przez usera (przyszle chipy, Partia 5c) - wiaze
// nazwe/jednostke z INDEKSEM KOLUMNY w obu formatach CSV. kolDlugiSr ->
// min/max tej samej serii sa zawsze w kolejnych dwoch kolumnach (sr,min,max
// to staly, powtarzalny wzorzec w historiaDlugaZbudujCsv()), wiec nie trzeba
// osobnych pol kolDlugiMin/kolDlugiMax - patrz WYKRESY_KOL_DLUGI_MIN/MAX().
struct WykresySeriaDef {
  const char *nazwa;
  const char *jednostka;
  int kolKrotki;    // indeks kolumny (0=etykieta) w CSV bufora krotkiego
  int kolDlugiSr;   // indeks kolumny SREDNIEJ w CSV bufora dlugiego
  uint16_t kolor;   // RGB565 - patrz komentarz przy tablicy WYKRESY_SERIE nizej (Partia 5c)
};
static inline int WYKRESY_KOL_DLUGI_MIN(int kolDlugiSr) { return kolDlugiSr + 1; }
static inline int WYKRESY_KOL_DLUGI_MAX(int kolDlugiSr) { return kolDlugiSr + 2; }

// [DZIURY-CZAS, ta sesja - PORT Z PANELU WWW, wersja RAM-oszczedna] Patrz
// obszerny komentarz przy wykresy_strumien_wstaw_znacznik_dziury() nizej co
// do tego, DLACZEGO ta wersja NIE dodaje pol do WykresyPunkt (pierwsza
// wersja tej poprawki dodala tam 2x uint32_t = +8B/punkt = +6144B na 8
// slotow x 96 punktow - na tym celu, bez PSRAM, to okazalo sie namacalnie
// za duzo, patrz HISTORIA ZMIAN [FIX-WYKRESY-DZIURY-RAM]).
//
// main_centrala.cpp dopisuje znacznik czasu (Unix epoch) jako OSTATNIA
// kolumna obu formatow CSV - patrz historiaLiniaCsv()/historiaDlugaLiniaCsv()
// oraz HISTORIA ZMIAN main_centrala.cpp, wpis "[DZIURY-CZAS, v3.0.0.2]".
// Bufor krotki: 14 kolumn (0-13) -> czas na 13. Bufor dlugi: 1 etykieta +
// 11 pol x 3 (sr,min,max) = 34 kolumny danych (0-33) -> czas na 34. Te same
// liczby co c[13]/c[1+pola.length*3] w wykrParsujCsvKrotkie()/
// wykrParsujCsvDlugie() (main_centrala.cpp) - jesli tam kiedys przybedzie
// kolumna, trzeba przeliczyc rownolegle tutaj.
static const int WYKRESY_KOL_KROTKI_CZAS = 13;
static const int WYKRESY_KOL_DLUGI_CZAS  = 34;

// Progi wykrywania przerwy - przeniesione 1:1 z JS (main_centrala.cpp,
// PROG_DZIURY_KROTKI_S/PROG_DZIURY_DLUGI_S), zeby panel LCD i panel WWW
// zglaszaly przerwe w tym samym miejscu dla tych samych danych.
static const uint32_t WYKRESY_PROG_DZIURY_KROTKI_S = 6UL * 60UL;   // 360 s
static const uint32_t WYKRESY_PROG_DZIURY_DLUGI_S  = 90UL * 60UL;  // 5400 s

// Kolejnosc = kolejnosc pol w obu strukturach CSV (main_centrala.cpp).
// "KLAPA"/"SYBERKO" to int-procenty w zrodle, ale tu i tak trafiaja do
// float - parser nizej nie rozroznia int/float kolumn, wszystko to strtof().
// [PARTIA 5c, ta sesja] kolor - RGB565, przeliczony recznie z tej samej
// palety hex co panel WWW centrali (main_centrala.cpp, komentarz "Kolory
// identyczne z paleta Ryby" przy wykrRenderuj(): #00d4f5/#4ade80/#ff9f43/
// #ffd32a/#ff4d6d/#a855f7/#ffb347 dla 7 serii temperaturowych) - spojnosc
// wizualna miedzy web-panelem a LVGL, jak zapisano w PLAN_WYKRESY_LVGL.md
// (sekcja "Rzad chipow wyboru serii"). Konwersja RGB888->RGB565 tym samym
// wzorem co reszta KOL_* w tym pliku (r>>3<<11 | g>>2<<5 | b>>3). 4 nowe
// serie (CISNIENIE/WILGOTNOSC/KLAPA/SYBERKO), ktorych panel WWW jeszcze nie
// rysuje (STATUS_PROJEKTU_LVGL.md, Partia 4: "sparsowane, ale jeszcze
// nierysowane"), dostaly wlasne kolory dobrane recznie tak, zeby nie
// kolidowaly odcieniem/jasnoscia z 7 powyzszych.
static const WykresySeriaDef WYKRESY_SERIE[] = {
  { "ZEWN",       "C",   1,  1, 0x06BE },
  { "BOJLER",     "C",   2,  4, 0x4EF0 },
  { "Temp. C.O",     "C",   3,  7, 0xFCE8 },
  { "Temp. C.O Sr.", "C",   4, 10, 0xFE85 },
  { "POWROT",        "C",   5, 13, 0xFA6D },
  { "PANEL",         "C",   6, 16, 0xFD88 },
  { "Wewnetrzna",    "C",   7, 19, 0xAABE },
  { "CISNIENIE",     "hPa", 9, 22, 0x653F },
  { "WILGOTNOSC",    "%",  10, 25, 0x2EB7 },
  { "KLAPA",         "%",  11, 28, 0xC43F },
  { "Syberek",       "%",  12, 31, 0x8662 },
};
static const int WYKRESY_LICZBA_SERII = sizeof(WYKRESY_SERIE) / sizeof(WYKRESY_SERIE[0]);

// [PARTIA 8a, ta sesja - GRUPOWANIE SERII WG OSI] Nastepca limitu "max 2
// serie / 1 na os" z Partii 5c. Docelowo: do 8 wybranych serii naraz,
// wg jednostki (patrz PLAN_PARTIA8_OSIE_WYKRESOW.md, partie 8b/8c/8d).
// [ZMIANA, ta sesja - patrz PLAN_ZMIAN_OS_WYKRESOW.md] Pierwotny symetryczny
// podzial 4+4 zostal zastapiony asymetrycznym 7+1, bo realny rozklad serii
// wg jednostki w WYKRESY_SERIE to 7x "C", 1x "hPa", 3x "%" - skrajny
// przypadek (uzytkownik zaznacza wszystkie 7 serii C + 1 dowolna inna)
// wymaga 7 miejsc na JEDNEJ osi, czego podzial 4+4 (a nawet 6+2) by nie
// pokryl. WYKRESY_MAX_SLOTOW (8 = 7+1) zostaje bez zmian - to wciaz twardy
// limit liczby jednoczesnie wybranych serii, uzywany wprost przy alokacji
// bufora punktow i puli serii LVGL (8c) - nie tylko jako suma.
static const int WYKRESY_MAX_PRIMARY   = 7;
static const int WYKRESY_MAX_SECONDARY = 1;
static const int WYKRESY_MAX_NA_OSI[2] = { WYKRESY_MAX_PRIMARY, WYKRESY_MAX_SECONDARY };
static const int WYKRESY_MAX_SLOTOW = 8;

// Jeden zredukowany punkt wykresu - albo pojedyncza wartosc (bufor krotki),
// albo srednia+min+max (bufor dlugi, min/max wypelnione tylko gdy
// wykresy_redukuj_kolumne() wywolana z dodatkowymi kolumnami - patrz nizej).
struct WykresyPunkt {
  char  etykieta[12];  // starczy na dluzszy format dlugiego bufora "DD.MM HH:00"
  float wartosc;
  float minimum;   // uzyte tylko przy redukcji bufora dlugiego, inaczej ==wartosc
  float maksimum;  // jw.
};

// Wyciaga wartosc float z pojedynczej, JUZ WYCIETEJ linii CSV (bez '\n' na
// koncu) spod danego indeksu kolumny - bez alokacji, samym przesuwaniem
// wskaznika po przecinkach. Zwraca false gdy linia ma mniej kolumn niz
// zadany indeks (np. pusta/urwana linia na koncu odpowiedzi HTTP).
static bool wykresy_csv_kolumna_float(const char *linia, int indeksKolumny, float &wynikWyjscie) {
  const char *p = linia;
  for (int kol = 0; kol < indeksKolumny; kol++) {
    p = strchr(p, ',');
    if (!p) return false;
    p++;
  }
  wynikWyjscie = strtof(p, nullptr);
  return true;
}

// [DZIURY-CZAS, ta sesja] Jak wyzej, ale zwraca uint32_t przez strtoul()
// zamiast strtof(). Unix epoch rzedu 1.7 mld NIE miesci sie dokladnie w
// 24-bitowej mantysie float (przy tej wielkosci liczby precyzja to
// +/-~128), a przy odejmowaniu dwoch tak zaokraglonych znacznikow (patrz
// wykrywanie przerwy w lvgl_dashboard_poc_wykresy_strumien_linia()) blad
// rzedu +/-250s bylby zbyt duzy wzgledem progu 360s dla bufora krotkiego
// (WYKRESY_PROG_DZIURY_KROTKI_S). strtoul() na tym samym surowym tekscie
// zachowuje pelna precyzje 32-bitowa.
static bool wykresy_csv_kolumna_u32(const char *linia, int indeksKolumny, uint32_t &wynikWyjscie) {
  const char *p = linia;
  for (int kol = 0; kol < indeksKolumny; kol++) {
    p = strchr(p, ',');
    if (!p) return false;
    p++;
  }
  wynikWyjscie = (uint32_t)strtoul(p, nullptr, 10);
  return true;
}

// Kopiuje kolumne 0 (etykieta, zawsze tekstowa) z linii CSV do bufora
// wyjsciowego o zadanym rozmiarze.
static bool wykresy_csv_etykieta(const char *linia, char *wyjscie, size_t wyjscieMax) {
  const char *koniec = strchr(linia, ',');
  if (!koniec) return false;
  size_t dl = (size_t)(koniec - linia);
  if (dl >= wyjscieMax) dl = wyjscieMax - 1;
  memcpy(wyjscie, linia, dl);
  wyjscie[dl] = '\0';
  return true;
}

// [WYCOFANE, FIX-WYKRESY-RAM ta sesja] wykresy_redukuj_kolumne()/
// wykresy_dane_krotkie()/wykresy_dane_dlugie() wymagaly calego CSV jako
// JEDNEGO String w RAM (dwa przebiegi po tym samym stringu: 1) policz linie
// 2) parsuj) - dokladnie przyczyna cichej porazki "kod=200 ale
// odp.length()=0" w httpGet() (main.cpp), patrz log z sesji diagnostycznej:
// najwiekszy wolny blok=38900B, a odpowiedz ~60-84KB nie miala szans sie
// zmiescic w jednym kawalku, NIEZALEZNIE od tego ile razy user by probowal.
// Autor tego kodu przewidzial to juz w komentarzu przy Partii 5b ("chunki
// HTTP beda mogly byc przetwarzane w locie... poza zakresem tej partii") -
// to jest ta partia. Zastapione ponizej przez WykresyStrumienStan +
// lvgl_dashboard_poc_wykresy_strumien_{start,linia,koniec}(), wolane z
// main.cpp W MIARE odbierania danych z gniazda, bez buforowania calosci.
//
// Kluczowa roznica algorytmiczna: stary kod znal liczbeWierszy PRZED
// kubelkowaniem (1. przebieg), wiec krok=liczbaWierszy/maxPunktow byl od
// razu znany. Strumieniowo nie znamy liczby wierszy z gory (nie chcemy
// czytac calosci dwa razy - to podwoiloby transfer). Zamiast tego uzywamy
// ADAPTACYJNEGO PODWAJANIA: kubelki startuja jako 1 surowy wiersz = 1
// kubelek; gdy tablica wyjsciowa (WYKRESY_MAX_PUNKTOW=96 slotow) sie zapelni,
// PAROWO SCALAMY sasiednie kubelki (96 -> 48) i PODWAJAMY rozmiar kolejnych
// kubelkow (1 -> 2 surowe wiersze), po czym kontynuujemy zapelnianie od 48
// w gore. To gwarantuje STALY rozmiar tablicy (nigdy > 96 slotow x kilka
// float na serie = kilka KB, NIE 60-84KB) niezaleznie od dlugosci CSV -
// dokladnie ten sam wzorzec co strumieniowe histogramy w narzedziach typu
// Prometheus. Wszystkie wybrane serie dziela TE SAME granice kubelkow
// (wspolny wierszWKubelku/rozmiarKubelka/liczbaKubelkow), bo przetwarzane sa
// z tego samego jednego przebiegu po wierszach CSV - dodatkowa korzysc: stary
// kod parsowal CALY CSV OSOBNO DLA KAZDEJ wybranej serii (do 8x powtorzona
// robota), nowy robi to raz dla wszystkich naraz.
struct WykresyStrumienStan {
  bool  aktywny;
  bool  dlugi;
  // [FIX-WYKRESY-GODZINY] true wylacznie dla trybu GODZINY (zawsze !dlugi).
  // Zamiast adaptacyjnego podwajania kubelkow (ktore z czasem USREDNIA coraz
  // wiecej surowych wierszy w jeden punkt - dobre dla Dzien/Tydzien, gdzie
  // celem jest zmiescic caly bufor w WYKRESY_MAX_PUNKTOW punktach), tryb ten
  // trzyma OKNO PRZESUWNE ostatnich WYKRESY_GODZINY_OKNO surowych wierszy
  // (rozmiarKubelka zostaje na 1 na zawsze - zero usredniania) - patrz
  // wykresy_strumien_zamknij_kubelek().
  bool  oknoPrzesuwne;
  int   liczbaSerii;       // zamrozone na starcie (g_wykresyLiczbaWybranych w tym momencie)
  int   rozmiarKubelka;    // ile surowych wierszy = jeden kubelek; podwaja sie przy scaleniu
  int   wierszWKubelku;    // ile surowych wierszy juz w BIEZACYM otwartym kubelku
  int   liczbaKubelkow;    // ile kubelkow ZAMKNIETYCH (wspolne dla wszystkich serii)
  float suma[WYKRESY_MAX_SLOTOW];
  float minW[WYKRESY_MAX_SLOTOW];
  float maxW[WYKRESY_MAX_SLOTOW];
  int   licznik[WYKRESY_MAX_SLOTOW]; // ile PRAWIDLOWYCH wartosci danej serii w biezacym otwartym kubelku
  char  etykieta[12];      // etykieta ostatniego wiersza w biezacym otwartym kubelku
  // [DZIURY-CZAS, ta sesja] Czas (Unix epoch) OSTATNIEGO widzianego
  // surowego wiersza CSV, NIEZALEZNIE od granic kubelka - jedno pole na
  // caly strumien (nie per-punkt!), wiec koszt to 4B raz, nie 4B x liczba
  // punktow x liczba serii. Uzywane WYLACZNIE do wykrycia przerwy MIEDZY
  // KOLEJNYMI surowymi wierszami w lvgl_dashboard_poc_wykresy_strumien_linia()
  // - patrz tam. 0 = jeszcze nie widziano zadnego wiersza w tym strumieniu
  // (swiezo po _strumien_start(), ktory zeruje cala te strukture) - w tym
  // przypadku gap nigdy nie jest zglaszany dla PIERWSZEGO wiersza.
  uint32_t czasOstatniegoWiersza;
};
static WykresyStrumienStan g_wykresyStrumien;
// [UWAGA] wykresy_strumien_scal_kubelki()/wykresy_strumien_zamknij_kubelek()
// (uzywajace g_wykresyPunkty) zdefiniowane sa NIZEJ w pliku, zaraz PO
// deklaracji g_wykresyPunkty (Partia 8a) - g_wykresyPunkty nie istnieje
// jeszcze w tym miejscu pliku.

// [PARTIA 5a, ta sesja - EKRAN WYKRESY] Szkielet: tytul + podtytul-placeholder
// (miejsce na przyszly lv_chart w kolejnej partii) + X w rogu + idle-timeout,
// dokladnie ten sam wzorzec co alarm_temp_panel powyzej. Wejscie: kafelek
// WYKRESY na drugim pulpicie (patrz drugi_pulpit_zbuduj() wyzej). Wyjscie: X
// LUB gest w dowolna strone - wraca na drugi pulpit (nie od razu na
// dashboard), patrz wykresy_panel_gesture_cb() i wykresy_panel_zamknij_cb()
// nizej.
static void wykresy_panel_zamknij_cb(lv_event_t *e); // forward - potrzebny juz w idle-timeout ponizej
static void wykresy_pokaz_selekcje(); // forward - zdefiniowana nizej (sekcja "DWIE PODSTRONY"), potrzebna juz w wykresy_panel_otworz() ponizej
static void wykresy_panel_idle_timeout_cb(lv_timer_t *t) {
  (void)t;
  g_wykresyPanelIdleTimer = nullptr;
  wykresy_panel_zamknij_cb(nullptr);
}
// [ZMIANA, ta sesja] Przyjmuje teraz czas timeoutu jako parametr (domyslnie
// POMPA_STRONA_IDLE_MS, jak dotad) - podstrona Wykres wywoluje ta sama
// funkcje, ale z WYKRESY_WYKRES_IDLE_MS (patrz definicja stalej wyzej w
// pliku), zeby "cicha" obserwacja wykresu nie byla traktowana tak samo jak
// bezczynnosc na Selekcji.
static void wykresy_panel_idle_resetuj(uint32_t ms = POMPA_STRONA_IDLE_MS) {
  if (g_wykresyPanelIdleTimer != nullptr) { lv_timer_del(g_wykresyPanelIdleTimer); g_wykresyPanelIdleTimer = nullptr; }
  g_wykresyPanelIdleTimer = lv_timer_create(wykresy_panel_idle_timeout_cb, ms, nullptr);
  lv_timer_set_repeat_count(g_wykresyPanelIdleTimer, 1);
}
static void wykresy_panel_zamknij_cb(lv_event_t *e) {
  (void)e;
  // [ZMIANA tej sesji] Wraca na DRUGI PULPIT (skad Wykresy sa teraz
  // otwierane kafelkiem), nie od razu na dashboard - typowa hierarchiczna
  // nawigacja "wstecz o jeden poziom". drugiPulpitScr nie moze byc nullptr
  // w praktyce (budowany razem z reszta UI w lvgl_dashboard_poc_init()),
  // ale sprawdzenie zostaje dla bezpieczenstwa, tak jak przy dashboardScr
  // gdzie indziej w tym pliku.
  if (drugiPulpitScr != nullptr) lv_scr_load_anim(drugiPulpitScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (g_wykresyPanelIdleTimer != nullptr) { lv_timer_del(g_wykresyPanelIdleTimer); g_wykresyPanelIdleTimer = nullptr; }
  // [FIX tej sesji] Wracamy NA drugi pulpit - jego wlasny idle-timer zostal
  // anulowany przy wejsciu na Wykresy (patrz wykresy_panel_otworz()) i nic
  // go od tamtej pory nie wznowilo, wiec bez tego drugi pulpit zostawalby
  // tu bez zadnego idle-timera i JUZ NIGDY sam nie wrocilby na dashboard -
  // dokladnie zgloszony objaw "cofnie do drugiego pulpitu i tam zostaje".
  drugi_pulpit_idle_resetuj();
}
static void wykresy_panel_otworz() {
  // [FIX tej sesji] Opuszczamy drugi pulpit dla ekranu podrzednego - jego
  // WLASNY idle-timer (g_drugiPulpitIdleTimer, uruchomiony raz w
  // drugi_pulpit_otworz() i nieresetowany przez zadna aktywnosc na
  // Wykresach) musi przestac tykac, inaczej strzeli w tle dokladnie
  // POMPA_STRONA_IDLE_MS od momentu otwarcia drugiego pulpitu i wymusi
  // dashboard NIEZALEZNIE od tego, ze user w tym czasie aktywnie klika
  // chipy serii na Selekcji - dokladnie zgloszony objaw "cofa mimo
  // klikania". Ten sam wzorzec odwrotny (wznowienie) jest w
  // wykresy_panel_zamknij_cb() przy powrocie NA drugi pulpit.
  if (g_drugiPulpitIdleTimer != nullptr) { lv_timer_del(g_drugiPulpitIdleTimer); g_drugiPulpitIdleTimer = nullptr; }
  lv_scr_load_anim(wykresyPanel, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  // [DWIE PODSTRONY, ta sesja] Zawsze wchodzimy od Selekcji, nawet jesli
  // poprzednim razem user zdazyl dojsc do podstrony Wykres i wyszedl
  // stamtad przez X/gest (bez wracania strzalka) - "wykres" bez swiezego
  // wyboru nie ma sensu, a user i tak zazwyczaj bedzie chcial
  // sprawdzic/zmienic serie przy kazdym ponownym wejsciu.
  wykresy_pokaz_selekcje(); // woła tez wykresy_panel_idle_resetuj()
}
static void wykresy_kafelek_klikniety_cb(lv_event_t *e) {
  (void)e;
  wykresy_panel_otworz();
}

// Gest na dashboardzie: przeciagniecie w LEWO otwiera Wykresy. Wolane z
// LV_EVENT_GESTURE dodanego do `scr` w lvgl_dashboard_poc_init() - patrz
// komentarz przy lv_obj_add_event_cb(scr, dashboard_gesture_cb, ...) tam.
// Dziala z KAZDEGO miejsca na dashboardzie, w tym NAD kafelkami POMPA/
// PANEL/PIEC/DYM/SERWO - LVGL ma flage LV_OBJ_FLAG_GESTURE_BUBBLE, ktora
// jest DOMYSLNIE WLACZONA na kazdym obiekcie (trzeba ja swiadomie wylaczyc
// lv_obj_remove_flag(...), zeby gest przestal "bablowac" do rodzica) - nigdzie
// w tym pliku jej nie wylaczylismy, wiec gest zaczety na dowolnym kafelku i
// tak dociera do `scr`. To INNY mechanizm niz zwykle LV_EVENT_CLICKED, ktore
// NIE bablkuja domyslnie (potrzebowalyby LV_OBJ_FLAG_EVENT_BUBBLE) - dlatego
// klikniecie w kafelek POMPA nadal otwiera panel POMPA jak dotad, a gest na
// tym samym kafelku RUWNOLEGLE otworzy Wykresy, bez konfliktu.
static void dashboard_gesture_cb(lv_event_t *e) {
  lv_indev_t *indev = lv_indev_get_act();
  if (indev == nullptr) return;
  lv_dir_t kierunek = lv_indev_get_gesture_dir(indev);
  // [ZMIANA tej sesji] Gest (w KTORAKOLWIEK strone - lewo LUB prawo, bez
  // znaczenia kierunku) przenosi teraz na DRUGI PULPIT - wlasna siatke z
  // kafelkami-launcherami (WYKRESY, WYSWIETLACZ, w przyszlosci wiecej) -
  // patrz drugi_pulpit_zbuduj(). Wczesniej gest otwieral Wykresy WPROST;
  // uzytkownik chcial posredniego ekranu-menu zamiast tego.
  if (kierunek == LV_DIR_LEFT || kierunek == LV_DIR_RIGHT) {
    drugi_pulpit_otworz();
    // [FIX tej sesji] Bez tego: przy zamianie ekranu W TRAKCIE trwajacego
    // przeciagniecia (palec wciaz na szybie) LVGL resetuje stan dotyku dla
    // starego ekranu i zaczyna wykrywac DRUGI gest od zera na nowym ekranie
    // (wykresyPanel) - najmniejsze drgniecie/szarpniecie tuz przed
    // oderwaniem palca (typowe dla taniego rezystancyjnego XPT2046) starcza,
    // zeby ten drugi gest zostal odczytany jako "w prawo" i NATYCHMIAST
    // zamknal dopiero co otwarty panel z powrotem na dashboard - dokladnie
    // objaw zgloszony przez uzytkownika. lv_indev_wait_release() mowi LVGL
    // "ignoruj reszte tego dotyku (wlacznie z ew. drugim gestem), dopoki
    // palec nie zostanie faktycznie oderwany" - oficjalnie udokumentowany
    // sposob LVGL na dzialanie po geście (docs.lvgl.io/8/overview/indev.html).
    lv_indev_wait_release(indev);
  }
}
// Ten sam gest na EKRANIE Wykresy, w przeciwna strone (PRAWO) - powrot na
// dashboard. X w rogu (dodawany w wykresy_panel_zbuduj() nizej) zostaje
// jako fallback, tak jak zapisano w decyzji w STATUS_PROJEKTU_LVGL.md.
static void wykresy_panel_gesture_cb(lv_event_t *e) {
  lv_indev_t *indev = lv_indev_get_act();
  if (indev == nullptr) return;
  lv_dir_t kierunek = lv_indev_get_gesture_dir(indev);
  // [FIX tej sesji] Symetria jak w dashboard_gesture_cb() - gest w KTORAKOLWIEK
  // strone na ekranie Wykresy zamyka go, nie tylko PRAWO.
  if (kierunek == LV_DIR_LEFT || kierunek == LV_DIR_RIGHT) {
    wykresy_panel_zamknij_cb(nullptr);
    lv_indev_wait_release(indev); // ten sam fix co w dashboard_gesture_cb() - patrz komentarz tam
  }
}

// [PARTIA 5c, potem PRZEPISANE w Partii 8b, ta sesja - CHIPY WYBORU SERII]
// UI wyboru serii do wykresu. **[AKTUALNE od Partii 8b]** limit nie jest
// juz sztywno "2, pierwsza=os A, druga=os B" - do WYKRESY_MAX_SLOTOW (8)
// serii naraz, max WYKRESY_MAX_NA_OSI[os] (7 PRIMARY / 1 SECONDARY) na kazda z 2 fizycznych osi Y
// lv_chart, grupowane wg jednostki. Pelny algorytm przydzialu:
// wykresy_chip_klikniety_cb() nizej oraz PLAN_PARTIA8_OSIE_WYKRESOW.md,
// sekcja "Logika przydzialu osi". Chip po prostu nie reaguje na dotyk,
// gdy oba twarde limity (4/os, 8 lacznie) sa juz wyczerpane.
//
// Sam lv_chart z 8 seriami to wciaz kolejne partie (8c/8d) - tutaj tylko
// stan wyboru + wizualne chipy + tymczasowa etykieta statusu, zeby dalo
// sie to przetestowac na sprzecie osobno, zanim dojda kolejne partie.

// g_wykresyKolejnoscWyboru trzyma indeksy do WYKRESY_SERIE W KOLEJNOSCI
// WYBORU (nie wg pozycji w tablicy). g_wykresyOsWyboru (Partia 8a/8b,
// zadeklarowana nizej razem z reszta stanu grupowania) jest do niej
// ROWNOLEGLA - ta sama pozycja i = ta sama wybrana seria, jej os.
static bool g_wykresySerieWybrane[WYKRESY_LICZBA_SERII] = { false };
static int  g_wykresyKolejnoscWyboru[WYKRESY_MAX_SLOTOW] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int  g_wykresyLiczbaWybranych = 0;
static lv_obj_t *g_wykresyChipy[WYKRESY_LICZBA_SERII] = { nullptr };
// Tymczasowa etykieta ("Wybrano: X + Y") - zastapi ja realny lv_chart w
// Partii 5d, na razie to jedyny widoczny efekt wyboru na sprzecie.
static lv_obj_t *wykresyStatusLabel = nullptr;

// [PARTIA 8a, ta sesja - PRZYGOTOWANIE POD GRUPOWANIE WG OSI] Ponizsze
// struktury sa na razie NIEUZYWANE - nie podpiete jeszcze do
// wykresy_chip_klikniety_cb() (to Partia 8b) ani do puli serii LVGL (8c).
// Docelowy model, rownolegly do dzisiejszego g_wykresyKolejnoscWyboru[2]:
//   - g_wykresyOsWyboru[i]    - do ktorej osi (0=A/PRIMARY, 1=B/SECONDARY)
//                               trafila i-ta wybrana seria (rownolegle do
//                               g_wykresyKolejnoscWyboru, indeks po
//                               KOLEJNOSCI WYBORU, nie po pozycji w
//                               WYKRESY_SERIE).
//   - g_wykresyLiczbaNaOsi[o] - ile serii jest aktualnie na osi o (0/1),
//                               twardy limit WYKRESY_MAX_NA_OSI[os] kazda (7 PRIMARY / 1 SECONDARY).
//   - g_wykresyJednostkaOsi[o]- jednostka (wskaznik na WykresySeriaDef::
//                               jednostka, PORONWNYWANY przez strcmp, nie
//                               przez adres - literaly "C" w WYKRESY_SERIE
//                               MOGA, ale nie MUSZA byc tym samym adresem)
//                               obecnie "zajmujaca" os, albo nullptr gdy
//                               os jest pusta (jednostka "zapomniana").
//   - g_wykresyOstatniaOsDodania - ktora os (0/1) dostala OSTATNIO dodana
//                               serie - uzywane w 8b jako cel "wymuszenia"
//                               dla 3./kolejnej odrebnej jednostki, zgodnie
//                               z ustalona zasada (patrz plan, sekcja
//                               "Logika przydzialu osi").
static int  g_wykresyOsWyboru[WYKRESY_MAX_SLOTOW] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int  g_wykresyLiczbaNaOsi[2] = { 0, 0 };
static const char *g_wykresyJednostkaOsi[2] = { nullptr, nullptr };
static int  g_wykresyOstatniaOsDodania = 0;

// [ROZBUDOWA, ta sesja - DWIE PODSTRONY] User: "powinno byc okno z wyborem
// a jak wybierzemy to dopiero pojawia sie pelnowymiarowe okno wykresu, bo
// tak to ten wybor zajmuje polowe wyswietlacza". Dokladnie ten sam,
// juz sprawdzony wzorzec co pompaStronaGlowna/serwoPanel itd. (patrz
// obszerny komentarz przy dashboardScr/pompaStronaGlowna wyzej w pliku):
// DWA pelnowymiarowe kontenery wewnatrz wykresyPanel, przelaczane recznie
// flaga HIDDEN - zero lv_tabview, zero nowej powierzchni API.
//  - wykresySelekcjaCont: tytul + pasek zakresu (5d) + chipy serii (5c) +
//    status + przycisk "POKAZ WYKRES" - dokladnie to co juz bylo, tylko
//    teraz w OSOBNYM kontenerze zamiast bezposrednio w wykresyPanel.
//  - wykresyWykresCont: docelowo pelnowymiarowy lv_chart (6. partia,
//    jeszcze nie napisana) - na razie placeholder + strzalka powrotu.
// Panel ZAWSZE otwiera sie na Selekcji (patrz wykresy_panel_otworz()
// nizej) - "wykres" ma sens tylko gdy user cos juz wybral.
//
// Forward - obie zdefiniowane nizej w pliku, ale potrzebne juz w
// wykresy_pokaz_wykres() ponizej (naglowek podstrony Wykres).
static const WykresySeriaDef *wykresy_wybrana_seria(int pozycja);
static const char *wykresy_tryb_nazwa();
static bool wykresy_tryb_jest_dlugi(); // forward - zdefiniowana nizej razem z wykresy_tryb_nazwa(); potrzebna juz w wykresy_pokaz_wykres() ponizej (PARTIA 5e - fetch)
static void wykresy_chart_ukryj(); // forward - zdefiniowana nizej przy reszcie logiki lv_chart; potrzebna juz w wykresy_odswiez_wykres_fetch() ponizej (PARTIA 7)

static lv_obj_t *wykresySelekcjaCont = nullptr;
static lv_obj_t *wykresyWykresCont = nullptr;
static lv_obj_t *wykresyPokazBtn = nullptr;   // "POKAZ WYKRES" na Selekcji
static lv_obj_t *wykresyWykresTytul = nullptr; // naglowek na podstronie Wykres (seria(e) + tryb)
static lv_obj_t *wykresyWykresPlaceholder = nullptr; // status fetchu ("Ladowanie...", bledy) - widoczny ZAMIAST wykresyChart, nigdy razem
// [PARTIA 7, ta sesja] Prawdziwy lv_chart + podpis zakresu czasu pod nim.
// wykresyChart jest ukryty dopoki nie ma poprawnie sparsowanych danych -
// patrz wykresy_chart_pokaz()/wykresy_chart_ukryj() nizej.
static lv_obj_t *wykresyChart = nullptr;
// [PARTIA 8c, ta sesja] Pula 8 serii LVGL - jedna na kazdy mozliwy wybrany
// slot (WYKRESY_MAX_SLOTOW). Tworzona raz w wykresy_panel_zbuduj() i nigdy
// wiecej nie usuwana - patrz obszerny komentarz przy tworzeniu nizej.
static lv_chart_series_t *wykresyChartSer[WYKRESY_MAX_SLOTOW] = { nullptr };
static lv_obj_t *wykresyChartZakresLabel = nullptr; // "etykieta_pierwszy_punkt - etykieta_ostatni_punkt" pod wykresem
// [FIX - etykiety osi ucinaly sie przy roznych skalach danych, ta sesja]
// Czy dana os w tej chwili pokazuje 1 miejsce po przecinku, czy tylko liczby
// calkowite - patrz wykresy_chart_dopasuj_os() nizej. Ustawiane dynamicznie
// za kazdym razem gdy zakres osi sie zmienia (inna kolumna CSV = inna skala:
// temperatura ~0-40 potrzebuje dziesietnych, cisnienie ~950-1050 hPa juz nie).
static bool g_wykresyOsADziesietne = true;
static bool g_wykresyOsBDziesietne = true;
// [FIX - ostateczne rozwiazanie problemu ucinania etykiet, ta sesja]
// Po dwoch nieudanych probach naprawy przez parametry samego lv_chart
// (draw_size, potem padding - zadne nie dawalo spojnego efektu na tym
// sprzecie/tej wersji LVGL), etykiety osi Y sa teraz WLASNYMI widzetami
// lv_label, a nie wbudowanym mechanizmem wykresu. To daje pelna, prosta
// kontrole nad pozycja i szerokoscia (zwykle lv_obj_set_pos/set_width),
// bez zaleznosci od nieudokumentowanych szczegolow lv_chart. 5 = major_cnt
// (liczba glownych podzialek na osi, przekazywana do lv_chart_set_axis_tick
// z label_en=false ponizej - same kreski podzialki zostaja rysowane przez
// LVGL, tekst rysujemy sami).
#define WYKRESY_OS_LICZBA_ETYKIET 5
static lv_obj_t *wykresyEtykietyA[WYKRESY_OS_LICZBA_ETYKIET] = {nullptr};
static lv_obj_t *wykresyEtykietyB[WYKRESY_OS_LICZBA_ETYKIET] = {nullptr};

// [PARTIA 8e, ta sesja - LEGENDA] Rzad kropka+nazwa nad wykresem, do
// WYKRESY_MAX_SLOTOW (8) pozycji, zawijany do (docelowo) 2 wierszy - ten
// sam mechanizm zawijania (LV_FLEX_FLOW_ROW_WRAP) co chipy wyboru na
// Selekcji (wykresy_chipy_zbuduj()). Pula wielokrotnego uzytku, tworzona
// RAZ w wykresy_panel_zbuduj() - ten sam wzorzec co wykresyChartSer[]/
// wykresyEtykietyA/B wyzej (zero create/delete po starcie), odswiezana
// (tekst/kolor/widocznosc per pozycja) przez wykresy_legenda_odswiez()
// nizej, wolana z wykresy_chart_odswiez(). wykresyLegendaKropka[p] i
// wykresyLegendaTekst[p] sa DZIECMI wspolnego "itemu" (kropka+tekst w
// jednym flex-row) - sam item chowany/odslaniany jako calosc (patrz
// wykresy_legenda_odswiez()), stad brak osobnej tablicy na itemy: item
// pobierany w razie potrzeby przez lv_obj_get_parent(kropka).
static lv_obj_t *wykresyLegendaCont = nullptr;
static lv_obj_t *wykresyLegendaKropka[WYKRESY_MAX_SLOTOW] = { nullptr };
static lv_obj_t *wykresyLegendaTekst[WYKRESY_MAX_SLOTOW] = { nullptr };

// [PARTIA 5e, ta sesja] Most fetchu HTTP - main.cpp rejestruje sie tu (patrz
// komentarz przy WykresyFetchCb w dashboard_lvgl_poc.h), zeby ten plik nie
// musial znac HTTPClient/WiFi, dokladnie ten sam wzorzec co reszta
// callbackow "wyslij"/main.cpp w tym pliku.
static WykresyFetchCb g_wykresyFetchCb = nullptr;
void lvgl_dashboard_poc_ustaw_callback_wykresy_fetch(WykresyFetchCb cb) { g_wykresyFetchCb = cb; }

// Czy odpowiedz fetchu W TOKU wciaz odpowiada aktualnie wybranemu trybowi -
// jesli user przelaczy Godziny/Dzien/Tydzien zanim poprzedni GET wroci,
// stara odpowiedz (z lvgl_dashboard_poc_wykresy_dane_gotowe()) zostanie
// odrzucona jako "spozniona" zamiast nadpisac nowszy wybor - patrz uzycie
// w lvgl_dashboard_poc_wykresy_dane_gotowe() nizej.
static bool g_wykresyFetchOczekiwanyDlugi = false;
static bool g_wykresyFetchWToku = false;

// Startuje (lub ponawia) pobranie danych pod AKTUALNIE wybrany tryb -
// wolane przy wejsciu na podstrone Wykres i przy zmianie trybu, gdy user
// juz na niej jest (patrz wykresy_tryb_klikniety_cb() nizej). Nie robi nic,
// jesli main.cpp jeszcze nie zdazyl zarejestrowac callbacku (np. w trybie
// symulacji przed pelnym setup()) - ten sam ostrozny wzorzec co reszta
// "wyslij" callbackow w tym pliku (np. pompaManualWyslij sprawdzany przez
// main.cpp, nie tutaj, ale zasada ta sama: brak callbacku = cicho nic).
static void wykresy_odswiez_wykres_fetch() {
  if (!wykresyWykresPlaceholder) return;
  wykresy_chart_ukryj(); // [PARTIA 7] poprzedni wykres (jesli byl) znika, dopoki nowe dane nie doplyna
  lv_label_set_text(wykresyWykresPlaceholder, "Ladowanie danych...");
  lv_obj_set_style_text_color(wykresyWykresPlaceholder, rgb565(KOL_TEKST_DIM), 0);
  if (!g_wykresyFetchCb) {
    lv_label_set_text(wykresyWykresPlaceholder, "Brak polaczenia z fetchem (main.cpp)");
    return;
  }
  // [WAZNE] httpGet() w main.cpp jest BLOKUJACY (synchroniczny HTTPClient,
  // dokladnie jak pobierzStatus()) - bez wymuszenia repaintu TERAZ tekst
  // "Ladowanie danych..." ustawiony wyzej wisialby niewidoczny w buforze
  // LVGL do konca GET-a (LVGL normalnie odswieza ekran dopiero w kolejnym
  // obrocie lv_timer_handler() w loop(), a ten obrot jeszcze sie nie
  // zaczal - jestesmy w trakcie obslugi zdarzenia dotyku). lv_refr_now()
  // wymusza natychmiastowe wypchniecie biezacej klatki na TFT, wiec user
  // faktycznie widzi "Ladowanie..." zanim ekran na chwile "zamrozi" sie na
  // czas GET-a (~60KB CSV, moze to byc caly sekunda przy /api/history/long).
  lv_refr_now(nullptr);
  bool dlugi = wykresy_tryb_jest_dlugi();
  g_wykresyFetchOczekiwanyDlugi = dlugi;
  g_wykresyFetchWToku = true;
  g_wykresyFetchCb(dlugi);
}

// Przelacza widoczna podstrone. idle-timer resetowany przy KAZDYM przejsciu
// (dokladnie jak przy kazdej nawigacji miedzy podstronami POMPA/SERWO) -
// user wciaz jest aktywny na ekranie Wykresy, wiec auto-powrot na drugi
// pulpit nie powinien go zaskoczyc w trakcie ogladania wykresu.
static void wykresy_pokaz_selekcje() {
  if (wykresyWykresCont) lv_obj_add_flag(wykresyWykresCont, LV_OBJ_FLAG_HIDDEN);
  if (wykresySelekcjaCont) lv_obj_clear_flag(wykresySelekcjaCont, LV_OBJ_FLAG_HIDDEN);
  wykresy_panel_idle_resetuj();
}
static void wykresy_pokaz_wykres() {
  if (wykresySelekcjaCont) lv_obj_add_flag(wykresySelekcjaCont, LV_OBJ_FLAG_HIDDEN);
  if (wykresyWykresCont) lv_obj_clear_flag(wykresyWykresCont, LV_OBJ_FLAG_HIDDEN);
  if (wykresyWykresTytul) {
    // [PARTIA 8f, ta sesja - PORZADKI] Poprzednia wersja obslugiwala tylko
    // 1 albo "pierwsza + druga" (zalozenie sprzed Partii 8, gdy limit byl
    // sztywno 2) - przy >2 wybranych realnie pokazywalaby TYLKO pierwsze
    // dwie z pominieciem reszty, a etykieta nie ma ustawionej szerokosci/
    // long_mode (LV_LABEL_LONG_EXPAND domyslnie), wiec przy 8 dlugich
    // nazwach fontem 20px prawdopodobnie wyjechalaby poza ekran. Legenda
    // (Partia 8e) juz pokazuje pelna liste kolor+nazwa pod tytulem, wiec
    // od >2 wybranych tytul moze byc po prostu zwiezlym licznikiem -
    // krotki, zawsze miesci sie w jednej linii, zero ryzyka przepelnienia.
    if (g_wykresyLiczbaWybranych == 1) {
      lv_label_set_text_fmt(wykresyWykresTytul, "[%s] %s (%s)", wykresy_tryb_nazwa(),
                             wykresy_wybrana_seria(0)->nazwa, wykresy_wybrana_seria(0)->jednostka);
    } else if (g_wykresyLiczbaWybranych == 2) {
      lv_label_set_text_fmt(wykresyWykresTytul, "[%s] %s + %s", wykresy_tryb_nazwa(),
                             wykresy_wybrana_seria(0)->nazwa, wykresy_wybrana_seria(1)->nazwa);
    } else {
      lv_label_set_text_fmt(wykresyWykresTytul, "[%s] %d serii", wykresy_tryb_nazwa(),
                             g_wykresyLiczbaWybranych);
    }
  }
  // [PARTIA 5e, ta sesja] Kazde WEJSCIE na podstrone Wykres startuje swiezy
  // fetch pod aktualny tryb - prostsze niz cache'owanie/porownywanie z
  // poprzednim requestem, a ~60KB CSV raz na wejscie na ekran to nie jest
  // czestotliwosc, ktora obciazy centrale (patrz PLAN_WYKRESY_LVGL.md,
  // "Inny rytm niz /api/status").
  wykresy_odswiez_wykres_fetch();
  // [FIX, ta sesja] Dluzszy limit niz gdziekolwiek indziej - patrz komentarz
  // przy WYKRESY_WYKRES_IDLE_MS wyzej w pliku.
  wykresy_panel_idle_resetuj(WYKRESY_WYKRES_IDLE_MS);
}
static void wykresy_pokaz_wykres_cb(lv_event_t *e) {
  (void)e;
  // Przycisk jest "martwy" przy 0 wybranych serii - patrz
  // wykresy_pokaz_btn_odswiez() nizej.
  if (g_wykresyLiczbaWybranych == 0) return;
  wykresy_pokaz_wykres();
}
static void wykresy_wstecz_cb(lv_event_t *e) {
  (void)e;
  wykresy_pokaz_selekcje();
}

// [ZMIANA, ta sesja] Wczesniej: DOWOLNY dotyk na podstronie Wykres tylko
// resetowal idle-timer (user mogl trzymac wykres otwarty w nieskonczonosc,
// dotykajac go co jakis czas). Teraz: dotyk od razu WRACA do Selekcji -
// dokladnie to samo co strzalka wstecz (wykresy_wstecz_cb / patrz tam),
// tylko wywolane z dowolnego miejsca na podstronie, nie tylko z samej
// ikony strzalki. Podpieta pod wykresyWykresCont (tlo calej podstrony, wiec
// lapie tapniecie gdziekolwiek w jej obrebie) ORAZ pod sam wykresyChart
// (patrz wykresy_panel_zbuduj() nizej) - LV_EVENT_PRESSED nie bablkuje
// domyslnie w LVGL v8 (w przeciwienstwie do GESTURE, patrz komentarz przy
// dashboard_gesture_cb() wyzej), wiec potrzebne jest osobne podpiecie na
// obiekcie, ktory faktycznie zlapie dotyk - w tym wypadku najczesciej albo
// puste tlo, albo sam wykres.
//
// Drugi mechanizm (auto-powrot po WYKRESY_WYKRES_IDLE_MS calkowitej
// bezczynnosci, gdy user w ogole NIE dotknie ekranu) dziala rownolegle i
// niezaleznie - ustawiany raz przy wejsciu na podstrone w
// wykresy_pokaz_wykres(), nieresetowany juz stad. Efekt: pierwszy dotyk
// gdziekolwiek na wykresie = natychmiastowy powrot; brak dotyku w ogole =
// powrot po 30s.
static void wykresy_wykres_dotyk_cb(lv_event_t *e) {
  (void)e;
  wykresy_pokaz_selekcje();
}

// Szarzeje/podswietla przycisk "POKAZ WYKRES" wg tego, czy jest cokolwiek
// do pokazania - ten sam wzorzec wizualny co nieaktywny chip
// (wykresy_chip_odswiez()): przygaszone tlo + przygaszony tekst gdy 0
// wybranych, pelny KOL_OGIEN (akcent) gdy >=1.
static void wykresy_pokaz_btn_odswiez() {
  if (!wykresyPokazBtn) return;
  bool aktywny = (g_wykresyLiczbaWybranych > 0);
  lv_obj_set_style_bg_color(wykresyPokazBtn, rgb565(aktywny ? KOL_OGIEN : KOL_KARTA), 0);
  lv_obj_t *lbl = lv_obj_get_child(wykresyPokazBtn, 0);
  if (lbl) lv_obj_set_style_text_color(lbl, rgb565(aktywny ? KOL_TLO : KOL_TEKST_DIM), 0);
}

// [Do wykorzystania w Partii 5d] Zwraca wskaznik na definicje serii
// wybranej jako pierwsza (pozycja=0) lub druga (pozycja=1), albo nullptr
// gdy na tej pozycji nic nie jest jeszcze wybrane.
static const WykresySeriaDef *wykresy_wybrana_seria(int pozycja) {
  if (pozycja < 0 || pozycja >= g_wykresyLiczbaWybranych) return nullptr;
  return &WYKRESY_SERIE[g_wykresyKolejnoscWyboru[pozycja]];
}

// (wykresy_tryb_nazwa() juz forward-zadeklarowana wyzej, przy sekcji "DWIE
// PODSTRONY" - zdefiniowana nizej w sekcji "PARTIA 5d - PRZELACZNIK ZAKRESU".)

// [PARTIA 8b, ta sesja] Uogolnione z 0/1/2 na 0..WYKRESY_MAX_SLOTOW. Ta
// etykieta jest wciaz TYMCZASOWA - docelowa, czytelna prezentacja wyboru
// to legenda nad wykresem (Partia 8e), tutaj chodzi tylko o to, zeby na
// sprzecie dalo sie zweryfikowac ktore serie/na ktorej osi sa zaznaczone,
// zanim legenda powstanie. Przy >2 seriach tekst moze byc dlugi - stad
// LV_LABEL_LONG_DOT + stala szerokosc (ustawione przy tworzeniu
// widgetu nizej w wykresy_panel_zbuduj()), zeby nie wyjechal poza ekran.
static void wykresy_status_label_odswiez() {
  if (!wykresyStatusLabel) return;
  if (g_wykresyLiczbaWybranych == 0) {
    lv_label_set_text_fmt(wykresyStatusLabel, "[%s] Wybierz serie ponizej (max %d, %d+%d na os)",
                           wykresy_tryb_nazwa(), WYKRESY_MAX_SLOTOW, WYKRESY_MAX_PRIMARY, WYKRESY_MAX_SECONDARY);
  } else if (g_wykresyLiczbaWybranych == 1) {
    lv_label_set_text_fmt(wykresyStatusLabel, "[%s] Wybrano: %s (%s)", wykresy_tryb_nazwa(),
                           wykresy_wybrana_seria(0)->nazwa, wykresy_wybrana_seria(0)->jednostka);
  } else {
    char bufor[160];
    int dl = snprintf(bufor, sizeof(bufor), "[%s] Wybrano: ", wykresy_tryb_nazwa());
    for (int i = 0; i < g_wykresyLiczbaWybranych && dl < (int)sizeof(bufor); i++) {
      const char *lacznik = (i == 0) ? "" : " + ";
      dl += snprintf(bufor + dl, sizeof(bufor) - dl, "%s%s", lacznik, wykresy_wybrana_seria(i)->nazwa);
    }
    lv_label_set_text(wykresyStatusLabel, bufor);
  }
}

// Odswieza wyglad JEDNEGO chipa wg aktualnego stanu wybrania - wolane po
// kazdej zmianie (klikniecie) oraz raz na starcie dla wszystkich. Zaznaczony:
// tlo = przygaszony kolor serii (lv_color_darken, ten sam mechanizm co stan
// "wcisniety" w kafelek()), ramka = pelny kolor serii (grubsza), tekst =
// pelny kolor serii - od razu widac KTORA seria to na tle koloru linii,
// ktora pojawi sie w 5d. Niezaznaczony: tlo = KOL_KARTA jak reszta UI,
// cienka ramka w przygaszonym kolorze serii (chip "zdradza" swoj kolor,
// nawet gdy nieaktywny), tekst = KOL_TEKST_DIM.
static void wykresy_chip_odswiez(int idx) {
  lv_obj_t *chip = g_wykresyChipy[idx];
  if (!chip) return;
  bool wybrany = g_wykresySerieWybrane[idx];
  lv_color_t kolor = rgb565(WYKRESY_SERIE[idx].kolor);
  lv_obj_t *lbl = lv_obj_get_child(chip, 0);

  if (wybrany) {
    lv_obj_set_style_bg_color(chip, lv_color_darken(kolor, 110), 0);
    lv_obj_set_style_border_color(chip, kolor, 0);
    lv_obj_set_style_border_width(chip, 2, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, kolor, 0);
  } else {
    lv_obj_set_style_bg_color(chip, rgb565(KOL_KARTA), 0);
    lv_obj_set_style_border_color(chip, lv_color_darken(kolor, 60), 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  }
}

// [PARTIA 8b, ta sesja - PRZEPISANE POD GRUPOWANIE WG OSI] Nastepca
// poprzedniej wersji "limit 2, pierwsza=os A, druga=os B". Pelny opis
// algorytmu: PLAN_PARTIA8_OSIE_WYKRESOW.md, sekcja "Logika przydzialu
// osi". Skrot:
//   1) jednostka pasuje do jednostki JUZ obecnej na osi A lub B (i ta os
//      ma wolny slot, < WYKRESY_MAX_NA_OSI[os]) -> dolacz tam,
//   2) inaczej, ktoras os jest calkiem pusta -> zajmij ja, ustanawiajac
//      jej jednostke,
//   3) inaczej (odrebna jednostka, obie osie juz maja SWOJA jednostke) ->
//      wymuszona na g_wykresyOstatniaOsDodania, jesli ta os ma wolny slot
//      - w przeciwnym razie chip nie reaguje (twardy limit 4/os wygrywa).
// Uwaga przy 3): jednostka "wlasciciela" osi (g_wykresyJednostkaOsi) NIE
// jest nadpisywana przy wymuszeniu - etykiety liczbowe tej osi i tak
// przechodza na neutralny kolor przy mieszanych jednostkach (Partia 8d),
// wiec nie ma potrzeby udawac, ze os "nalezy" do nowej jednostki.
static void wykresy_chip_klikniety_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);

  // [FIX, ta sesja - idle-timeout resetowany klikami] Kazde klikniecie
  // chipa serii to AKTYWNOSC uzytkownika na podstronie Selekcji - musi
  // resetowac odliczanie idle-timeoutu (g_wykresyPanelIdleTimer,
  // POMPA_STRONA_IDLE_MS = 13.5s), tak samo jak dotyk resetuje idle-timeout
  // na kazdej innej stronie w tym pliku (patrz np. pompa_idle_timer_resetuj
  // wolane z eventow strony POMPA). Wczesniej timer byl ustawiany TYLKO raz
  // przy wejsciu na Selekcje (wykresy_pokaz_selekcje()) i klikanie chipow go
  // NIE resetowalo - przy wolniejszym wybieraniu (przerwy miedzy klikami >
  // 13.5s razem wziete od wejscia) user zostawal wyrzucany z powrotem do
  // dashboardu w trakcie aktywnego wybierania serii. Wywolanie na samym
  // poczatku, PRZED jakakolwiek logika zaznaczania/odznaczania, zeby dzialalo
  // identycznie dla obu galezi (zaznaczenie i odznaczenie) i nawet gdyby
  // ktoras z nich pozniej zrobila wczesny "return".
  wykresy_panel_idle_resetuj();

  if (g_wykresySerieWybrane[idx]) {
    // Odznaczenie - znajdz pozycje w kolejnosci wyboru i wytnij ja,
    // przesuwajac reszte o jeden w lewo (zero "dziur" w srodku - tak samo
    // jak w poprzedniej wersji dla 2 slotow, teraz uogolnione na 8).
    g_wykresySerieWybrane[idx] = false;
    int pozycja = -1;
    for (int i = 0; i < g_wykresyLiczbaWybranych; i++) {
      if (g_wykresyKolejnoscWyboru[i] == idx) { pozycja = i; break; }
    }
    if (pozycja >= 0) {
      int os = g_wykresyOsWyboru[pozycja];
      for (int i = pozycja; i < g_wykresyLiczbaWybranych - 1; i++) {
        g_wykresyKolejnoscWyboru[i] = g_wykresyKolejnoscWyboru[i + 1];
        g_wykresyOsWyboru[i] = g_wykresyOsWyboru[i + 1];
      }
      g_wykresyKolejnoscWyboru[g_wykresyLiczbaWybranych - 1] = -1;
      g_wykresyOsWyboru[g_wykresyLiczbaWybranych - 1] = -1;
      g_wykresyLiczbaWybranych--;
      if (os >= 0) {
        g_wykresyLiczbaNaOsi[os]--;
        if (g_wykresyLiczbaNaOsi[os] <= 0) {
          g_wykresyLiczbaNaOsi[os] = 0;
          g_wykresyJednostkaOsi[os] = nullptr; // os "zapomniana" - nastepny wybor moze ja zajac dowolna jednostka
        }
      }
    }
  } else {
    // Zaznaczenie - twardy limit laczny (naturalna konsekwencja 7+1,
    // sprawdzany explicit zeby nie polegac wylacznie na limitach per-os
    // ponizej).
    if (g_wykresyLiczbaWybranych >= WYKRESY_MAX_SLOTOW) return;

    const char *jednostka = WYKRESY_SERIE[idx].jednostka;
    int os = -1;

    // 1) pasujaca jednostka na osi z wolnym slotem.
    for (int o = 0; o < 2; o++) {
      if (g_wykresyJednostkaOsi[o] != nullptr &&
          strcmp(g_wykresyJednostkaOsi[o], jednostka) == 0 &&
          g_wykresyLiczbaNaOsi[o] < WYKRESY_MAX_NA_OSI[o]) {
        os = o;
        break;
      }
    }
    // 2) pusta os.
    if (os < 0) {
      for (int o = 0; o < 2; o++) {
        if (g_wykresyJednostkaOsi[o] == nullptr) { os = o; break; }
      }
      if (os >= 0) g_wykresyJednostkaOsi[os] = jednostka; // ustanawia jednostke tej osi
    }
    // 3) wymuszona na ostatnio dotknieta os, jesli ma wolny slot.
    if (os < 0) {
      int wymuszona = g_wykresyOstatniaOsDodania;
      if (g_wykresyLiczbaNaOsi[wymuszona] < WYKRESY_MAX_NA_OSI[wymuszona]) os = wymuszona;
    }
    // Obie osie pelne (7+1) - chip nie reaguje, tak jak trzeci chip przy
    // dawnym limicie 2.
    if (os < 0) return;

    g_wykresySerieWybrane[idx] = true;
    g_wykresyKolejnoscWyboru[g_wykresyLiczbaWybranych] = idx;
    g_wykresyOsWyboru[g_wykresyLiczbaWybranych] = os;
    g_wykresyLiczbaWybranych++;
    g_wykresyLiczbaNaOsi[os]++;
    g_wykresyOstatniaOsDodania = os;
  }

  wykresy_chip_odswiez(idx);
  wykresy_status_label_odswiez();
  wykresy_pokaz_btn_odswiez(); // [DWIE PODSTRONY] 0 vs >0 wybranych zmienia dostepnosc "POKAZ WYKRES"
}

// [SIATKA 4x3, ta sesja] Poprzednio: flex ROW_WRAP + chipy o szerokosci
// LV_SIZE_CONTENT (dopasowanej do dlugosci napisu) - efekt: chipy byly
// rozne szerokosci ("ZEWN" wyraznie wezszy niz "Temp. C.O Sr."), a
// zawijanie wypadalo tam gdzie akurat zabraklo miejsca w rzedzie, nie w
// rownych kolumnach. Zgloszone jako "zrob rowne, wieksze kafelki, jest 12
// do wyboru to zrob 3 rzedy po 4" - zamienione na prawdziwa siatke LVGL
// (LV_LAYOUT_GRID): 4 rowne kolumny x 3 rowne wiersze = 12 miejsc (dla
// obecnych WYKRESY_LICZBA_SERII=11 pozycji ostatnie miejsce siatki po
// prostu zostaje puste). Kazdy chip ma DOKLADNIE ten sam rozmiar
// (LV_GRID_ALIGN_STRETCH wypelnia cala komorke), bez wzgledu na dlugosc
// etykiety - dlugie napisy (np. "Temp. C.O Sr.") sa obcinane wielokropkiem
// (LV_LABEL_LONG_DOT) zamiast wystawac poza chip.
#define WYKRESY_CHIP_KOLUMNY 4
#define WYKRESY_CHIP_WIERSZE 3
static void wykresy_chipy_zbuduj(lv_obj_t *parent) {
  lv_obj_t *rzad = lv_obj_create(parent);
  lv_obj_remove_style_all(rzad);
  // 460px szerokosci / 4 kolumny po 109px + 3 odstepy po 8px = 460 dokladnie
  // (4*109 + 3*8 = 460). Wysokosc: 3 wiersze po 40px + 2 odstepy po 8px =
  // 136 - wieksze niz poprzednie 112 (patrz przesuniecie wykresyPokazBtn
  // nizej w wykresy_panel_zbuduj()), zgodnie z "zrob wieksze kafelki".
  static lv_coord_t KOL_DSC[WYKRESY_CHIP_KOLUMNY + 1] = { 109, 109, 109, 109, LV_GRID_TEMPLATE_LAST };
  static lv_coord_t WIERSZ_DSC[WYKRESY_CHIP_WIERSZE + 1] = { 40, 40, 40, LV_GRID_TEMPLATE_LAST };
  lv_obj_set_size(rzad, 460, 136);
  lv_obj_align(rzad, LV_ALIGN_TOP_MID, 0, 82);
  lv_obj_set_layout(rzad, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(rzad, KOL_DSC, WIERSZ_DSC);
  lv_obj_set_style_pad_column(rzad, 8, 0);
  lv_obj_set_style_pad_row(rzad, 8, 0);
  lv_obj_clear_flag(rzad, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < WYKRESY_LICZBA_SERII; i++) {
    lv_obj_t *chip = lv_btn_create(rzad);
    lv_obj_set_style_radius(chip, 12, 0);
    lv_obj_set_style_shadow_width(chip, 0, 0); // domyslny cien lv_btn tu zbedny, ma byc plaski
    // STRETCH = chip wypelnia cala komorke siatki (109x40) - rowny rozmiar
    // dla kazdego chipa, niezaleznie od dlugosci etykiety. Kolumna =
    // i % 4, wiersz = i / 4 (0..3 / 0..2 przy 11 pozycjach).
    lv_obj_set_grid_cell(chip, LV_GRID_ALIGN_STRETCH, i % WYKRESY_CHIP_KOLUMNY, 1,
                          LV_GRID_ALIGN_STRETCH, i / WYKRESY_CHIP_KOLUMNY, 1);

    lv_obj_t *lbl = lv_label_create(chip);
    lv_label_set_text(lbl, WYKRESY_SERIE[i].nazwa);
    // Font zostaje 14 (montserrat_12 nie jest u nas potwierdzony jako
    // wlaczony w lv_conf.h). Szerokosc etykiety ograniczona do wnetrza
    // chipa (109px - zapas na padding) + LONG_DOT, zeby najdluzsze nazwy
    // ("Temp. C.O Sr.") dostaly wielokropek zamiast wystawac poza chip.
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl, 96);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(chip, wykresy_chip_klikniety_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    g_wykresyChipy[i] = chip;
    wykresy_chip_odswiez(i); // stan startowy - wszystkie niezaznaczone
  }
}

// [PARTIA 5d, ta sesja - PRZELACZNIK ZAKRESU] Godziny/Dzien/Tydzien - ten
// sam wybor co panel WWW (main_centrala.cpp, `_wykrTryb`: 'godz'/'dzien'/
// 'tydz'), ale jako OSOBNY stan po stronie LVGL - ekran dotykowy nie
// dzieli stanu z przegladarka, wiec musi miec swoj wlasny. "Godziny" i
// "Dzien" beda czytac /api/history (bufor krotki - 1440 probek co 1 min =
// 24h, "Godziny" pokaze z niego tylko ostatnia godzine, "Dzien" caly
// bufor), "Tydzien" czyta /api/history/long (bufor dlugi, agregaty
// godzinowe) - dokladnie ta sama logika co loadCharts()/wykrRenderuj() w
// panelu WWW (PLAN_WYKRESY_LVGL.md, "Spojnosc z panelem WWW"). [PARTIA 5e]
// Fetch jest juz spiety - patrz wykresy_tryb_jest_dlugi()/
// wykresy_odswiez_wykres_fetch() nizej.
enum WykresyTryb { WYKR_TRYB_GODZINY, WYKR_TRYB_DZIEN, WYKR_TRYB_TYDZIEN };
static WykresyTryb g_wykresyTryb = WYKR_TRYB_DZIEN; // domyslnie "Dzien", jak panel WWW
static lv_obj_t *g_wykresyTrybBtn[3] = { nullptr, nullptr, nullptr };

static const char *wykresy_tryb_nazwa() {
  // [FIX NAZW TRYBOW, ta sesja] "Godziny/Dzien/Tydzien" (rzeczowniki) ->
  // "Godzinny/Dzienny/Tygodniowy" (przymiotniki) - zgloszone jako bledne
  // napisy na przyciskach zakresu (wykresy_tryb_zbuduj() nizej uzywa tych
  // samych trzech form). Zmiana czysto tekstowa, bez wplywu na
  // wykresy_tryb_jest_dlugi()/logike fetchu.
  switch (g_wykresyTryb) {
    case WYKR_TRYB_GODZINY: return "Godzinny";
    case WYKR_TRYB_TYDZIEN: return "Tygodniowy";
    default:                return "Dzienny";
  }
}

// [PARTIA 5e] "Godziny"/"Dzien" czytaja bufor krotki (/api/history),
// "Tydzien" czyta bufor dlugi (/api/history/long) - patrz komentarz przy
// deklaracji WykresyFetchCb w dashboard_lvgl_poc.h.
static bool wykresy_tryb_jest_dlugi() {
  return g_wykresyTryb == WYKR_TRYB_TYDZIEN;
}

// [PARTIA 8d, ta sesja - USUNIETO g_wykresyPunktyA/B] Dawne bufory na 2
// nazwane serie zastapione ponizsza pula 8 slotow - wykresy_chart_odswiez()
// i lvgl_dashboard_poc_wykresy_dane_gotowe() od tej partii pracuja
// wylacznie na g_wykresyPunkty[]/g_wykresyLiczbaPunktow[] (indeks = pozycja
// w kolejnosci wyboru, tak jak g_wykresyKolejnoscWyboru/g_wykresyOsWyboru).
//
// [PARTIA 8a] CELOWO NIE jako "static WykresyPunkt g_wykresyPunkty[8][WYKRESY_MAX_PUNKTOW]"
// (co byloby 8x24x96 = 18432 B w .bss/.dram0.bss) - to dokladnie ten sam
// mechanizm przepelnienia co opisany w lvgl_port.cpp przy buf1 (bufor
// rysowania LVGL): .dram0.bss ma staly, ciasny rozmiar dzielony ze WSZYSTKIMI
// zmiennymi globalnymi projektu (WiFi/HTTPClient/ArduinoJson/reszta
// main.cpp), a juz dzisiejsze 2x24x96=4608 B to i tak niemalo. Zamiast tego
// - tablica 8 WSKAZNIKOW (32 B, pomijalne) + malloc() per slot w runtime,
// tak samo jak `buf1` w lvgl_port.cpp. Uzycie samych wskaznikow - w Partii 8d.
//
// [v1.2.2 PAMIEC] Bufory (8 x 96 punktow x 24 B = 18,4 kB) istnieja TYLKO
// wtedy, gdy ekran Wykresy jest otwarty. Do v1.2.1 byly przydzielane raz, przy
// starcie, i trzymane do konca - takze wtedy, gdy nikt na wykresy nie
// patrzyl. Byl to drugi co do wielkosci przydzial w programie (po buforze
// rysowania), a panel pracowal z ok. 8 kB wolnego RAM-u (log z 2026-11-12,
// patrz HISTORIA ZMIAN w main.cpp). Dane wykresu i tak sa pobierane z
// centrali od nowa przy kazdym wejsciu na podstrone Wykres, wiec nic nie
// ginie. Przydzial i zwolnienie sa przypiete do zdarzen ekranu
// (wykresy_panel_ekran_event_cb() nizej), wiec dzialaja przy KAZDYM wejsciu
// i wyjsciu: przycisk X, gest, limit bezczynnosci, ekran alarmu.
// Zasada dla calego kodu wykresow: wskaznik pusty => liczba punktow 0;
// kazde uzycie sprawdza jedno albo drugie.
static WykresyPunkt *g_wykresyPunkty[WYKRESY_MAX_SLOTOW] = { nullptr };
static int g_wykresyLiczbaPunktow[WYKRESY_MAX_SLOTOW] = { 0 };

static void wykresy_bufory_8_zaalokuj() {
  for (int i = 0; i < WYKRESY_MAX_SLOTOW; i++) {
    if (g_wykresyPunkty[i] != nullptr) continue;   // juz jest (np. dwa zdarzenia wejscia z rzedu)
    g_wykresyLiczbaPunktow[i] = 0;
    g_wykresyPunkty[i] = (WykresyPunkt *)malloc(sizeof(WykresyPunkt) * WYKRESY_MAX_PUNKTOW);
    if (g_wykresyPunkty[i] == nullptr) {
      // Brak pamieci na ten slot: seria po prostu nie pojawi sie na wykresie
      // (kazde uzycie g_wykresyPunkty[] sprawdza wskaznik).
      Serial.printf("[WYKRESY] BLAD: brak RAM na bufor punktow slotu %d\n", i);
    }
  }
}

static void wykresy_bufory_8_zwolnij() {
  // Gdyby jakies pobieranie danych bylo jeszcze w toku, jego reszta ma
  // przepasc (strumien_linia() i strumien_koniec() sprawdzaja .aktywny).
  g_wykresyStrumien.aktywny = false;
  for (int i = 0; i < WYKRESY_MAX_SLOTOW; i++) {
    g_wykresyLiczbaPunktow[i] = 0;
    free(g_wykresyPunkty[i]);   // free(nullptr) jest dozwolone
    g_wykresyPunkty[i] = nullptr;
  }
}

// Zdarzenia ekranu Wykresy: LVGL wysyla LOAD_START, zanim ekran sie pokaze,
// i UNLOADED, gdy zastapil go inny - niezaleznie od tego, co to spowodowalo.
static void wykresy_panel_ekran_event_cb(lv_event_t *e) {
  const lv_event_code_t kod = lv_event_get_code(e);
  if (kod == LV_EVENT_SCREEN_LOAD_START) wykresy_bufory_8_zaalokuj();
  else if (kod == LV_EVENT_SCREEN_UNLOADED) wykresy_bufory_8_zwolnij();
}

// [FIX-WYKRESY-RAM, ta sesja] Parowe scalenie 96 zamknietych kubelkow w 48
// (i podwojenie rozmiarKubelka dla kolejnych) - wolane TUZ PRZED zapisem
// 97-go kubelka, nigdy nie pozwalajac tablicy urosnac ponad
// WYKRESY_MAX_PUNKTOW. Nadpisywanie w miejscu jest bezpieczne: kazdy
// docelowy indeks i<polowa jest zawsze <= zrodlowe 2*i, wiec zapis do i
// nastepuje dopiero PO odczytaniu 2*i/2*i+1 do lokalnej zmiennej
// (klasyczna kompakcja w miejscu). Patrz obszerny komentarz przy
// WykresyStrumienStan (wyzej w pliku) co do calego algorytmu.
static void wykresy_strumien_scal_kubelki() {
  int polowa = g_wykresyStrumien.liczbaKubelkow / 2;
  for (int s = 0; s < g_wykresyStrumien.liczbaSerii; s++) {
    if (!g_wykresyPunkty[s]) continue; // malloc sie nie udal dla tego slotu - bezpiecznik jak reszta *_8_zaalokuj()
    for (int i = 0; i < polowa; i++) {
      const WykresyPunkt &a = g_wykresyPunkty[s][2 * i];
      const WykresyPunkt &b = g_wykresyPunkty[s][2 * i + 1];
      WykresyPunkt scalony;
      // Rownowazna waga a i b - oba kubelki maja ten sam nominalny
      // rozmiarKubelka (podwajany zawsze RAZEM dla wszystkich kubelkow),
      // wiec zwykla srednia dwoch srednich odpowiada wazonej.
      scalony.wartosc = (a.wartosc + b.wartosc) * 0.5f;
      scalony.minimum = (a.minimum < b.minimum) ? a.minimum : b.minimum;
      scalony.maksimum = (a.maksimum > b.maksimum) ? a.maksimum : b.maksimum;
      memcpy(scalony.etykieta, b.etykieta, sizeof(scalony.etykieta)); // nowszy (b) wygrywa, jak w starym kodzie
      g_wykresyPunkty[s][i] = scalony;
    }
  }
  g_wykresyStrumien.liczbaKubelkow = polowa;
  g_wykresyStrumien.rozmiarKubelka *= 2;
}

// Domyka biezacy otwarty kubelek: usrednia nagromadzone sumy per seria do
// slotu g_wykresyPunkty[s][idx], scala tablice PRZED zapisem jesli akurat
// pelna, i resetuje akumulatory pod kolejny kubelek.
//
// [DZIURY-CZAS, ta sesja] Parametr znacznikDziury: gdy true, NIE usredniamy
// nic (akumulatory sa i tak puste - patrz wywolanie w
// lvgl_dashboard_poc_wykresy_strumien_linia()) - zamiast tego zapisujemy
// wartosc=NaN na WSZYSTKICH seriach. NaN zamiast dodatkowych pol czasu w
// WykresyPunkt to SWIADOMY wybor po tym, jak pierwsza wersja tej poprawki
// (2x uint32_t per punkt) dodala 6144B trwale zajetego RAM (8 slotow x 96
// punktow x 8B) na celu bez PSRAM - i to realnie zdestabilizowalo
// dzialanie (patrz HISTORIA ZMIAN [FIX-WYKRESY-DZIURY-RAM]). Kosztuje to
// zamiast tego JEDEN slot z WYKRESY_MAX_PUNKTOW za KAZDA wykryta przerwe
// (typowo pojedyncze wystapienia na caly tydzien danych) - pomijalne wobec
// 96 dostepnych. NaN naturalnie PRZETRWA kazde kolejne scalenie par w
// wykresy_strumien_scal_kubelki() (arytmetyka z NaN zawsze daje NaN, wiec
// scalony.wartosc = (a.wartosc+b.wartosc)*0.5f automatycznie zostaje NaN,
// gdy ktorykolwiek z pary byl znacznikiem) - przerwa nie "gubi sie" nawet
// po wielokrotnym podwojeniu rozmiaru kubelka dla starszych danych. Render
// (wykresy_chart_odswiez()) sprawdza isnan(wartosc) i wstawia
// LV_CHART_POINT_NONE - patrz tam.
static void wykresy_strumien_zamknij_kubelek(bool znacznikDziury = false) {
  // [FIX-WYKRESY-GODZINY] Tryb okna przesuwnego (GODZINY): gdy okno
  // (WYKRESY_GODZINY_OKNO wierszy) jest juz pelne, NIE usredniamy par jak
  // przy scaleniu ponizej (to dawaloby coraz "grubsze", coraz mniej
  // aktualne probki - dokladne przeciwienstwo tego, co ma pokazywac widok
  // "ostatnia godzina"). Zamiast tego wypychamy najstarszy wpis
  // (przesuniecie tablicy o 1 w lewo) i robimy miejsce na nowy na koncu -
  // klasyczne okno przesuwne. rozmiarKubelka zostaje 1 na zawsze w tym
  // trybie, wiec kazdy "kubelek" to dokladnie jeden surowy wiersz CSV.
  if (g_wykresyStrumien.oknoPrzesuwne && g_wykresyStrumien.liczbaKubelkow >= WYKRESY_GODZINY_OKNO) {
    for (int s = 0; s < g_wykresyStrumien.liczbaSerii; s++) {
      if (!g_wykresyPunkty[s]) continue; // malloc sie nie udal dla tego slotu - bezpiecznik jak reszta *_8_zaalokuj()
      memmove(&g_wykresyPunkty[s][0], &g_wykresyPunkty[s][1],
              sizeof(WykresyPunkt) * (WYKRESY_GODZINY_OKNO - 1));
    }
    g_wykresyStrumien.liczbaKubelkow = WYKRESY_GODZINY_OKNO - 1; // nowy wpis dopisze sie na zwolnionym miejscu na koncu, ponizej
  } else if (g_wykresyStrumien.liczbaKubelkow >= WYKRESY_MAX_PUNKTOW) {
    wykresy_strumien_scal_kubelki();
  }
  int idx = g_wykresyStrumien.liczbaKubelkow;
  for (int s = 0; s < g_wykresyStrumien.liczbaSerii; s++) {
    if (!g_wykresyPunkty[s]) continue; // malloc sie nie udal dla tego slotu - bezpiecznik jak reszta *_8_zaalokuj()
    WykresyPunkt &pkt = g_wykresyPunkty[s][idx];
    if (znacznikDziury) {
      pkt.wartosc = NAN;
      pkt.minimum = NAN;
      pkt.maksimum = NAN;
    } else if (g_wykresyStrumien.licznik[s] > 0) {
      pkt.wartosc = g_wykresyStrumien.suma[s] / g_wykresyStrumien.licznik[s];
      pkt.minimum = g_wykresyStrumien.minW[s];
      pkt.maksimum = g_wykresyStrumien.maxW[s];
    } else {
      // Brak sparsowanych wartosci tej serii w tym kubelku (np. urwana
      // linia) - powiel poprzedni punkt zamiast wstawiac zero, ktore
      // wygladaloby jak prawdziwy dolek na wykresie.
      pkt.wartosc = (idx > 0) ? g_wykresyPunkty[s][idx - 1].wartosc : 0.0f;
      pkt.minimum = pkt.wartosc;
      pkt.maksimum = pkt.wartosc;
    }
    memcpy(pkt.etykieta, g_wykresyStrumien.etykieta, sizeof(pkt.etykieta));
    g_wykresyStrumien.suma[s] = 0.0f;
    g_wykresyStrumien.minW[s] = 0.0f;
    g_wykresyStrumien.maxW[s] = 0.0f;
    g_wykresyStrumien.licznik[s] = 0;
  }
  g_wykresyStrumien.liczbaKubelkow++;
  g_wykresyStrumien.wierszWKubelku = 0;
}

// [PARTIA 7, ta sesja] lv_chart w LVGL v8 trzyma wartosci jako lv_coord_t
// (int16_t) - zero natywnego wsparcia float, w odroznieniu od reszty tego
// pliku, ktory wszedzie indziej pracuje na float. Zamiast tracic dokladnosc
// do pelnych liczb calkowitych, skalujemy KAZDA wartosc x10 (jedno miejsce
// po przecinku wystarcza dla wszystkich wielkosci na tych wykresach -
// temperatury/cisnienie/wilgotnosc/%) i etykiety osi Y odtwarzamy z
// powrotem do "x.y" we wlasnych widzetach lv_label (wykresyEtykietyA/B,
// patrz wykresy_chart_dopasuj_os() nizej) - NIE we wbudowanym mechanizmie
// etykiet lv_chart (patrz komentarz przy WYKRESY_OS_LICZBA_ETYKIET co do
// przyczyny tej zmiany). Margines +/-32000 zamiast pelnego zakresu int16_t
// (+/-32767) to celowy zapas przed samym overflow.
#define WYKRESY_CHART_SKALA 10.0f
static lv_coord_t wykresy_chart_skaluj(float wartosc) {
  long v = lroundf(wartosc * WYKRESY_CHART_SKALA);
  if (v > 32000) v = 32000;
  if (v < -32000) v = -32000;
  return (lv_coord_t)v;
}

// [PARTIA 8e, ta sesja - GEOMETRIA WYKRES+LEGENDA W JEDNYM MIEJSCU] Dawniej
// (przed ta partia) polozenie/wysokosc wykresu bylo "na sztywno" wpisane w
// dwoch miejscach (tworzenie widgetu w wykresy_panel_zbuduj() +
// wykresy_chart_dopasuj_os() do liczenia Y etykiet osi) - dochodzi trzecie
// (legenda), wiec zebrane tu jako nazwane stale, zeby te trzy miejsca nie
// mogly sie rozjechac. Dolna krawedz wykresu zostaje na tej samej
// wspolrzednej co PRZED ta partia (310) - podpis zakresu czasu
// (wykresyChartZakresLabel, y=292 wzgledem gornego rogu ekranu) i tak juz
// na niej "wisi" (patrz komentarz przy jej tworzeniu), wiec nie trzeba go
// ruszac. Legenda dostaje STALE 2 wiersze miejsca (46px), niezaleznie od
// tego, czy w danej chwili faktycznie potrzebuje 1 czy 2 - unika
// przeliczania polozenia wykresu przy kazdym odswiezeniu, ten sam powod co
// "pula, nie remove/create" gdzie indziej w tym module.
static const lv_coord_t WYKRESY_LEGENDA_Y = 36;
static const lv_coord_t WYKRESY_LEGENDA_H = 46;
static const lv_coord_t WYKRESY_CHART_DOL = 310;
static const lv_coord_t WYKRESY_CHART_Y = WYKRESY_LEGENDA_Y + WYKRESY_LEGENDA_H + 2;
static const lv_coord_t WYKRESY_CHART_H = WYKRESY_CHART_DOL - WYKRESY_CHART_Y;

// Chowa wykres i pokazuje etykiete statusu (blad/ladowanie) - albo
// odwrotnie. Te dwa obiekty nigdy nie sa widoczne jednoczesnie.
static void wykresy_chart_ukryj() {
  if (wykresyChart) lv_obj_add_flag(wykresyChart, LV_OBJ_FLAG_HIDDEN);
  if (wykresyChartZakresLabel) lv_obj_add_flag(wykresyChartZakresLabel, LV_OBJ_FLAG_HIDDEN);
  // [PARTIA 8e, ta sesja] Legenda nalezy wizualnie do wykresu (nad nim) -
  // chowana/odslaniana razem z nim, tak jak zakresLabel wyzej.
  if (wykresyLegendaCont) lv_obj_add_flag(wykresyLegendaCont, LV_OBJ_FLAG_HIDDEN);
  // [FIX, ta sesja] Wlasne etykiety osi (wykresyEtykietyA/B) sa osobnymi
  // widzetami, dziecmi wykresyWykresCont, a NIE wykresyChart - ukrycie
  // samego wykresu ich nie chowa, wiec trzeba to zrobic jawnie, inaczej
  // stare etykiety zostawalyby widoczne "zawieszone w powietrzu" nad
  // placeholderem bledu/ladowania.
  for (int i = 0; i < WYKRESY_OS_LICZBA_ETYKIET; i++) {
    if (wykresyEtykietyA[i]) lv_obj_add_flag(wykresyEtykietyA[i], LV_OBJ_FLAG_HIDDEN);
    if (wykresyEtykietyB[i]) lv_obj_add_flag(wykresyEtykietyB[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (wykresyWykresPlaceholder) lv_obj_clear_flag(wykresyWykresPlaceholder, LV_OBJ_FLAG_HIDDEN);
}

// [PARTIA 8d, ta sesja - ZASTEPUJE wykresy_chart_seria_zakres()] Przelicza
// zakres [min-margines, max+margines] PO WSZYSTKICH aktywnych slotach
// przypisanych do danej osi naraz (os: 0=PRIMARY/A, 1=SECONDARY/B), zamiast
// jednej serii - konieczne odkad do WYKRESY_MAX_NA_OSI[os] (7 PRIMARY / 1 SECONDARY) serii moze
// dzielic ta sama os fizyczna (grupowanie wg jednostki, Partia 8b). Reszta
// zasady bez zmian wzgledem poprzedniej wersji (min/max z realnych danych,
// margines >= 0.5 nawet przy plaskiej linii). Zwraca false, gdy zadna
// wybrana seria nie siedzi akurat na tej osi (nic do policzenia) - wywolujacy
// wtedy nie zmienia zakresu/nie pokazuje etykiet tej osi.
static bool wykresy_chart_zakres_osi(int os, float &minWyjscie, float &maxWyjscie) {
  bool mamDane = false;
  for (int p = 0; p < g_wykresyLiczbaWybranych; p++) {
    if (g_wykresyOsWyboru[p] != os) continue;
    int n = g_wykresyLiczbaPunktow[p];
    if (n <= 0) continue;
    const WykresyPunkt *punkty = g_wykresyPunkty[p];
    for (int i = 0; i < n; i++) {
      float v = punkty[i].wartosc;
      if (!mamDane) {
        minWyjscie = maxWyjscie = v;
        mamDane = true;
      } else {
        if (v < minWyjscie) minWyjscie = v;
        if (v > maxWyjscie) maxWyjscie = v;
      }
    }
  }
  if (!mamDane) return false;
  float margines = (maxWyjscie - minWyjscie) * 0.1f;
  if (margines < 0.5f) margines = 0.5f;
  minWyjscie -= margines;
  maxWyjscie += margines;
  return true;
}

// [PARTIA 8d, ta sesja] Kolor etykiet liczbowych danej osi: gdy dokladnie
// JEDNA wybrana seria siedzi na tej osi, uzywamy jej koloru (czytelnosc -
// jak dotychczas). Gdy na osi jest wiecej niz jedna seria (ta sama
// jednostka, rozne kolory linii) - neutralny KOL_TEKST_DIM, bo pojedynczy
// kolor etykiet nie mogby juz jednoznacznie wskazywac "ktora linia to ta
// os" (patrz tez PLAN_PARTIA8_OSIE_WYKRESOW.md, uwaga o etykietach przy
// mieszanych jednostkach - ten sam neutralny traktowanie, tylko szerszy
// warunek: nie tylko mieszane jednostki, kazda >1 seria/os).
static lv_color_t wykresy_chart_kolor_etykiet_osi(int os) {
  int idxJedynej = -1;
  int liczba = 0;
  for (int p = 0; p < g_wykresyLiczbaWybranych; p++) {
    if (g_wykresyOsWyboru[p] != os) continue;
    liczba++;
    idxJedynej = g_wykresyKolejnoscWyboru[p];
  }
  if (liczba == 1 && idxJedynej >= 0) return rgb565(WYKRESY_SERIE[idxJedynej].kolor);
  return rgb565(KOL_TEKST_DIM);
}

// [FIX - ostateczne rozwiazanie, ta sesja] Zamiast probowac wplynac na
// wewnetrzny, nieprzewidywalny mechanizm rezerwacji miejsca w lv_chart
// (draw_size ani padding nie dawaly spojnych rezultatow na tej wersji
// LVGL/tym sprzecie - dwie kolejne proby bez skutku), funkcja bezposrednio
// zarzadza wlasnymi widzetami etykiet (wykresyEtykietyA/B):
//   1. Mierzy realna szerokosc najgorszego przypadku etykiety (jak
//      poprzednio - lv_txt_get_width, ta sama czcionka co na etykietach) -
//      to i tak potrzebne do ustawienia szerokosci/wyrownania samej etykiety.
//   2. Pozycjonuje i wypelnia tekstem/kolorem 5 wlasnych etykiet lv_label
//      NA WIERZCHU wykresu, przy samej jego krawedzi - MY decydujemy
//      dokladnie gdzie staje kazdy piksel tekstu, wiec nic nie moze sie
//      "samo" uciac.
//
// [FIX - marnowane miejsce po bokach, ta sesja] Poprzednia wersja za kazdym
// razem ZWEZALA sam wykres o pelna zmierzona szerokosc etykiety (+zapas),
// zeby zrobic dla niej "czyste" miejsce obok wykresu. To dzialalo, ale przy
// dluzszych etykietach ("-32.0") zjadalo spory kawalek szerokosci ekranu po
// obu stronach, nawet gdy user prosil o wiecej miejsca na sam wykres.
// Etykiety maja WLASNY, wyzszy z-order niz wykres (tworzone PO nim w kodzie -
// LVGL rysuje dzieci w kolejnosci tworzenia, pozniejsze na wierzchu), wiec
// nie musza miec "czystego" miejsca obok linii - moga po prostu na nich
// leziec, tekst i tak zostanie w pelni czytelny. Dlatego teraz: wykres ma
// STALA, maksymalna szerokosc (caly kontener minus tylko maly margines na
// zaokraglony rog karty), a etykiety naklada sie na jego lewa/prawa krawedz.
// [FIX - brak jednostki na osi, ta sesja] "C" w WYKRESY_SERIE to surowa
// jednostka do porownan strcmp (g_wykresyJednostkaOsi) - do WYSWIETLENIA
// chcemy ten sam zapis co reszta UI ("°C", patrz np. tresc_zewn_odswiez()
// kilka tysiecy linii nizej: snprintf(..., "%.1f°C", ...)). hPa/% pokazujemy
// bez zmian, maja juz czytelna, krotka forme.
static const char *wykresy_jednostka_wyswietlana(const char *jednostkaSurowa) {
  if (jednostkaSurowa == nullptr) return "";
  if (strcmp(jednostkaSurowa, "C") == 0) return "°C";
  return jednostkaSurowa;
}

static void wykresy_chart_dopasuj_os(lv_chart_axis_t os, float minWartosc, float maxWartosc, lv_color_t kolorEtykiet, const char *jednostkaSurowa) {
  float maxAbs = fabsf(minWartosc);
  if (fabsf(maxWartosc) > maxAbs) maxAbs = fabsf(maxWartosc);
  bool ujemne = (minWartosc < 0.0f);
  bool dziesietne = (maxAbs < 100.0f);

  bool *dziesietneFlaga = (os == LV_CHART_AXIS_PRIMARY_Y) ? &g_wykresyOsADziesietne : &g_wykresyOsBDziesietne;
  *dziesietneFlaga = dziesietne;

  // [FIX - brak jednostki na osi, ta sesja] Jednostka dopisywana WYLACZNIE
  // do najwyzszej etykiety (i==0, wartosc MAX) kazdej osi - jedna etykieta
  // na os wystarczy, zeby bylo widac czy to °C/hPa/%, a powtarzanie jej przy
  // wszystkich 5 wartosciach tylko zaslanialoby wykres. Mierzymy szerokosc
  // najgorszego przypadku Z JEDNOSTKA (nie samej liczby) - dzieki temu ta
  // jedna etykieta z dopiskiem tez miesci sie w polu tekstu bez ucinania
  // (LV_LABEL_LONG_CLIP), a pozostale 4 etykiety (bez jednostki) po prostu
  // maja troche zapasu w swoim, nieco szerszym polu - bez wplywu na wyglad,
  // bo tekst jest LEWO wyrownany (LV_TEXT_ALIGN_LEFT).
  const char *jednostka = wykresy_jednostka_wyswietlana(jednostkaSurowa);

  char najgorszy[24];
  int calkowitaMax = (int)maxAbs;
  if (dziesietne) {
    lv_snprintf(najgorszy, sizeof(najgorszy), "%s%d.0 %s", ujemne ? "-" : "", calkowitaMax, jednostka);
  } else {
    lv_snprintf(najgorszy, sizeof(najgorszy), "%s%d %s", ujemne ? "-" : "", calkowitaMax, jednostka);
  }
  const lv_font_t *font = &lv_font_montserrat_14;
  lv_coord_t szerTekstu = lv_txt_get_width(najgorszy, strlen(najgorszy), font, 0, LV_TEXT_FLAG_NONE);

  // Wykres ma teraz STALA szerokosc niezalezna od dlugosci etykiet - caly
  // kontener (480px, wewnatrz wykresyWykresCont) minus tylko 4px zapasu po
  // kazdej stronie (zeby zaokraglony rog karty - radius 10 - nie wygladal
  // ucieciety przy samej krawedzi). To samo ustawiamy przy KAZDYM wywolaniu
  // (raz dla osi A, raz dla osi B) - tanie, wiec brak potrzeby pamietania
  // stanu miedzy wywolaniami jak poprzednio (gutterA/gutterB).
  const lv_coord_t chartX1 = 4;
  const lv_coord_t chartX2 = 476;
  lv_obj_set_x(wykresyChart, chartX1);
  lv_obj_set_width(wykresyChart, chartX2 - chartX1);

  // Wysokosc contentu wykresu (do rozstawienia etykiet pionowo) - stala,
  // czytana z WYKRESY_CHART_H (patrz komentarz przy tej stalej wyzej w
  // pliku - Partia 8e zmiejszyla ja, zeby zrobic miejsce na legende nad
  // wykresem, ale to i tak wciaz JEDNA nazwana stala, uzywana tu oraz przy
  // lv_obj_set_size() w wykresy_panel_zbuduj(), wiec nie moze sie rozjechac).
  // [FIX - zmarnowane miejsce U DOLU, ta sesja] Analogicznie do poziomej
  // naprawy wyzej: dawniej wykres konczyl sie na y=250, a ponizej byl
  // OSOBNY, pusty pas zarezerwowany wylacznie na etykiete zakresu czasu
  // ("12:0 - 12:59", wykresyChartZakresLabel) - marnujac sporo pionowej
  // przestrzeni ekranu (320px wysokosci), ktorej wykres wcale nie
  // potrzebowal osobno. Ta etykieta, tak jak etykiety osi Y, ma wyzszy
  // z-order niz wykres (tworzona PO nim w kodzie) i jest wyrownana do
  // srodka w poziomie, wiec moze bezpiecznie NACHODZIC na sam dol wykresu
  // (linie danych rzadko siegaja az na sam srodek dolnej krawedzi jednoczes-
  // nie z lewej i prawej strony) bez kolizji z etykietami osi, ktore siedza
  // przy lewej/prawej krawedzi - patrz nowa pozycja wykresyChartZakresLabel
  // przy jej tworzeniu, kilka linii pod tworzeniem samego wykresu.
  const lv_coord_t chartY = WYKRESY_CHART_Y;
  const lv_coord_t chartH = WYKRESY_CHART_H;
  const lv_coord_t etykietaWys = 16; // wysokosc jednej linii tekstu (font 14px + zapas)

  // Etykiety nakladaja sie na skrajne 2px wykresu z kazdej strony (zamiast
  // stac obok niego) - stad ta sama para stalych co chartX1/chartX2, tylko
  // z odwrotnym znaczeniem wyrownania: lewa kolumna liczb "wisi" tekstem w
  // prawo OD lewej krawedzi wykresu, prawa kolumna "wisi" tekstem w lewo OD
  // prawej krawedzi - dokladnie tak jak bylo widac na zdjeciu z testu, tylko
  // teraz bez dodatkowego, pustego pasa miedzy liczbami a wykresem.
  lv_obj_t **etykiety = (os == LV_CHART_AXIS_PRIMARY_Y) ? wykresyEtykietyA : wykresyEtykietyB;
  lv_coord_t etykietaX = (os == LV_CHART_AXIS_PRIMARY_Y) ? (chartX1 + 2) : (chartX2 - 2 - szerTekstu - 3);
  lv_coord_t etykietaSzer = szerTekstu + 3; // maly zapas, LONG_CLIP wiec bezpiecznie nie zawinie
  lv_text_align_t wyrownanie = LV_TEXT_ALIGN_LEFT;

  for (int i = 0; i < WYKRESY_OS_LICZBA_ETYKIET; i++) {
    if (!etykiety[i]) continue;
    lv_coord_t y = chartY + (int32_t)((int32_t)chartH * i) / (WYKRESY_OS_LICZBA_ETYKIET - 1) - etykietaWys / 2;
    lv_obj_set_pos(etykiety[i], etykietaX, y);
    lv_obj_set_width(etykiety[i], etykietaSzer);
    lv_obj_set_style_text_align(etykiety[i], wyrownanie, 0);

    // Wartosc na tej podzialce: LVGL rysuje ticki od MAX (u gory, i=0) do
    // MIN (u dolu, i=LICZBA-1), liniowo - ten sam porzadek odtwarzamy tu.
    float wartoscTick = maxWartosc - (maxWartosc - minWartosc) * ((float)i / (WYKRESY_OS_LICZBA_ETYKIET - 1));
    long v = lroundf(wartoscTick * 10.0f); // ta sama skala x10 co wykresy_chart_skaluj()
    bool tickUjemna = v < 0;
    if (tickUjemna) v = -v;
    int tickCalkowita = (int)(v / 10);
    int tickUlamkowa = (int)(v % 10);
    char bufer[24];
    // [FIX - brak jednostki na osi, ta sesja] Jednostka dopisana tylko przy
    // i==0 (najwyzsza etykieta, wartosc MAX danej osi) - patrz obszerny
    // komentarz przy pomiarze szerokosci w wykresy_chart_dopasuj_os() wyzej.
    const char *sufiks = (i == 0) ? jednostka : "";
    const char *odstep = (i == 0 && jednostka[0] != '\0') ? " " : "";
    if (dziesietne) {
      lv_snprintf(bufer, sizeof(bufer), "%s%d.%d%s%s", tickUjemna ? "-" : "", tickCalkowita, tickUlamkowa, odstep, sufiks);
    } else {
      int zaokraglona = tickCalkowita + (tickUlamkowa >= 5 ? 1 : 0);
      lv_snprintf(bufer, sizeof(bufer), "%s%d%s%s", tickUjemna ? "-" : "", zaokraglona, odstep, sufiks);
    }
    lv_label_set_text(etykiety[i], bufer);

    // [PARTIA 8d] Kolor etykiety - teraz przekazywany przez wywolujacego
    // (wykresy_chart_kolor_etykiet_osi()), zamiast czytany wprost z
    // SerA/SerB->color - przy do 4 seriach/os pojedynczy kolor serii juz
    // nie zawsze ma sens (patrz komentarz przy tej funkcji).
    lv_obj_set_style_text_color(etykiety[i], kolorEtykiet, 0);
  }
}

// Odswieza serie na lv_chart z aktualnej zawartosci g_wykresyPunkty[].
// [PARTIA 8e, ta sesja] Odswieza legende (kropka+nazwa) wg aktualnego
// wyboru - ten sam wzorzec "pula wielokrotnego uzytku" co reszta tego
// modulu: pozycje < g_wykresyLiczbaWybranych dostaja realny kolor/tekst i
// sa odslaniane, reszta (do WYKRESY_MAX_SLOTOW) jest chowana. Wolana z
// wykresy_chart_odswiez() ponizej, przed nadpisaniem serii/etykiet osi.
static void wykresy_legenda_odswiez() {
  for (int p = 0; p < WYKRESY_MAX_SLOTOW; p++) {
    lv_obj_t *kropka = wykresyLegendaKropka[p];
    if (!kropka) continue;
    lv_obj_t *item = lv_obj_get_parent(kropka); // "item" = kropka+tekst, patrz komentarz przy deklaracji puli
    if (p < g_wykresyLiczbaWybranych) {
      int idxSerii = g_wykresyKolejnoscWyboru[p];
      lv_color_t kolor = rgb565(WYKRESY_SERIE[idxSerii].kolor);
      lv_obj_set_style_bg_color(kropka, kolor, 0);
      lv_label_set_text(wykresyLegendaTekst[p], WYKRESY_SERIE[idxSerii].nazwa);
      lv_obj_set_style_text_color(wykresyLegendaTekst[p], kolor, 0);
      if (item) lv_obj_clear_flag(item, LV_OBJ_FLAG_HIDDEN);
    } else if (item) {
      lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// [FIX crash wykresow, patrz RAPORT_NAPRAWA_CRASH_WYKRESY_LVGL.md] Cala
// pula wykresyChartSer[] jest tworzona JEDNORAZOWO przy tworzeniu wykresu i
// NIGDY wiecej nie jest usuwana/tworzona od nowa tutaj - to wywolanie wylacznie
// NADPISUJE wartosci (oraz kolor) w juz istniejacych, raz zaalokowanych
// buforach. Powod: naprzemienne remove_series + set_point_count + add_series
// przy kazdym odswiezeniu fragmentowalo/przepelnialo wewnetrzny pool pamieci
// LVGL i powodowalo "free() target pointer is outside heap areas" przy
// DRUGIEJ z rzedu zmianie wyboru serii, niezaleznie od danych.
// [PARTIA 8d, ta sesja - PRZEPISANE POD PULE 8 SLOTOW] Zero parametrow -
// caly stan (ktore serie, na ktorej osi, ich dane) czytany wprost z
// g_wykresyKolejnoscWyboru/g_wykresyOsWyboru/g_wykresyPunkty/
// g_wykresyLiczbaPunktow (rownolegle tablice, indeks = POZYCJA W KOLEJNOSCI
// WYBORU, jak wszedzie indziej w tym module - patrz komentarz przy
// g_wykresyKolejnoscWyboru). [FIX PO KOMPILACJI NA SPRZECIE, ta sesja -
// poprzednia wersja tego komentarza mowila "slot LVGL o tym samym indeksie
// odpowiada dokladnie tej samej pozycji, zero osobnego mapowania" - juz
// NIEAKTUALNE, patrz obszerny komentarz przy samej petli nizej. Fizyczny
// slot puli (wykresyChartSer[]) ma OD TERAZ stala os (ustawiona raz przy
// tworzeniu), wiec pozycja wyboru p jest mapowana na fizyczny slot DOPIERO
// w tej funkcji, wg jej osi + licznika per-os, nie 1:1 przez wspolny indeks.
static void wykresy_chart_odswiez() {
  if (!wykresyChart) return;

  wykresy_legenda_odswiez(); // [PARTIA 8e] niezalezne od n/zakresow ponizej - dziala nawet gdyby dane byly puste

  // n = najdluzszy z aktywnych buforow - wspolna dlugosc "osi X" wykresu,
  // krotsze serie sa dopelniane powtorzeniem ostatniego punktu (tak jak w
  // poprzedniej, 2-slotowej wersji) - patrz petla wypelniania nizej.
  int n = 0;
  for (int p = 0; p < g_wykresyLiczbaWybranych; p++) {
    if (g_wykresyLiczbaPunktow[p] > n) n = g_wykresyLiczbaPunktow[p];
  }
  if (n <= 0) return;
  if (n > WYKRESY_MAX_PUNKTOW) n = WYKRESY_MAX_PUNKTOW; // bezpiecznik, nie powinno sie zdarzyc

  // Zakres + etykiety kazdej fizycznej osi (0=PRIMARY/A, 1=SECONDARY/B) -
  // liczony PO WSZYSTKICH aktywnych slotach danej osi naraz.
  lv_chart_axis_t osyLvgl[2] = { LV_CHART_AXIS_PRIMARY_Y, LV_CHART_AXIS_SECONDARY_Y };
  lv_obj_t **etykietyOsi[2] = { wykresyEtykietyA, wykresyEtykietyB };
  for (int os = 0; os < 2; os++) {
    float minOs, maxOs;
    if (wykresy_chart_zakres_osi(os, minOs, maxOs)) {
      lv_chart_set_range(wykresyChart, osyLvgl[os],
                          wykresy_chart_skaluj(minOs), wykresy_chart_skaluj(maxOs));
      wykresy_chart_dopasuj_os(osyLvgl[os], minOs, maxOs, wykresy_chart_kolor_etykiet_osi(os), g_wykresyJednostkaOsi[os]);
      for (int i = 0; i < WYKRESY_OS_LICZBA_ETYKIET; i++) {
        if (etykietyOsi[os][i]) lv_obj_clear_flag(etykietyOsi[os][i], LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      // Os bez zadnej aktywnej serii - chowamy jej etykiety, inaczej
      // zostalyby widoczne ze STARYMI wartosciami z poprzedniego wyboru.
      for (int i = 0; i < WYKRESY_OS_LICZBA_ETYKIET; i++) {
        if (etykietyOsi[os][i]) lv_obj_add_flag(etykietyOsi[os][i], LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  // Kazdy z WYKRESY_MAX_SLOTOW (8) slotow puli LVGL - aktywne pozycje
  // (< g_wykresyLiczbaWybranych) dostaja realne dane + kolor, reszta
  // jest wygaszana do samych LV_CHART_POINT_NONE (LVGL po prostu ich nie
  // rysuje) - dokladnie tak jak w poprzedniej wersji dla nieuzywanej
  // serii B, tylko teraz per-slot zamiast per-nazwana-zmienna.
  //
  // [FIX PO KOMPILACJI NA SPRZECIE, ta sesja] Patrz obszerny komentarz przy
  // tworzeniu puli (wykresy_panel_zbuduj()) - os kazdego fizycznego slotu
  // jest juz STALA od utworzenia (0..6=PRIMARY, 7=SECONDARY), wiec ta
  // petla NIE MOZE juz po prostu isc po POZYCJI WYBORU (p) i pisac wprost do
  // wykresyChartSer[p] jak poprzednio (to zakladalo dynamiczny zapis
  // ->axis, ktory sie nie skompilowal). Zamiast tego, dla kazdej wybranej
  // pozycji szukamy NASTEPNEGO WOLNEGO fizycznego slotu W RAMACH WLASCIWEJ
  // PODPULI (wg jej osi) przez dwa niezalezne liczniki (licznikOsi[0/1],
  // oba od 0) - logika wyboru (Partia 8b) juz gwarantuje twardy limit
  // WYKRESY_MAX_NA_OSI[os] (7 dla PRIMARY, 1 dla SECONDARY - patrz
  // PLAN_ZMIAN_OS_WYKRESOW.md) na kazda os, wiec zaden z liczników nigdy
  // nie przekroczy rozmiaru swojej podpuli.
  int licznikOsi[2] = { 0, 0 };
  bool slotUzyty[WYKRESY_MAX_SLOTOW] = { false };
  for (int p = 0; p < g_wykresyLiczbaWybranych; p++) {
    if (g_wykresyLiczbaPunktow[p] <= 0) continue;
    int os = g_wykresyOsWyboru[p];
    int podslot = licznikOsi[os]++;
    if (podslot >= WYKRESY_MAX_NA_OSI[os]) continue; // bezpiecznik - nie powinno sie zdarzyc, limit juz wymuszony w Partii 8b
    // Podzial fizycznych slotow: 0..6 = PRIMARY (7), 7 = SECONDARY (1).
    int fizycznySlot = (os == 0) ? podslot : (WYKRESY_MAX_PRIMARY + podslot);
    slotUzyty[fizycznySlot] = true;

    lv_chart_series_t *ser = wykresyChartSer[fizycznySlot];
    if (!ser) continue;
    lv_coord_t *tab = lv_chart_get_y_array(wykresyChart, ser);
    int idxSerii = g_wykresyKolejnoscWyboru[p];
    int liczbaP = g_wykresyLiczbaPunktow[p];
    const WykresyPunkt *punkty = g_wykresyPunkty[p];

    ser->color = rgb565(WYKRESY_SERIE[idxSerii].kolor);

    for (int i = 0; i < n; i++) {
      // Krotsza seria (mniej punktow niz najdluzsza aktywna, "n") -
      // powtorz jej ostatni znany punkt zamiast czytac poza tablica.
      int idx = i < liczbaP ? i : liczbaP - 1;
      // [DZIURY-CZAS, ta sesja] Kubelek-znacznik przerwy ma wartosc=NaN
      // (wstawiony w wykresy_strumien_zamknij_kubelek(true), patrz tam) -
      // isnan() zamiast normalnego skalowania daje LV_CHART_POINT_NONE, co
      // LVGL rysuje jako PRZERWANA linie w tym miejscu, zamiast falszywie
      // laczyc dwa odlegle w czasie odcinki tak, jakby powstaly tuz po
      // sobie.
      float w = punkty[idx].wartosc;
      tab[i] = isnan(w) ? LV_CHART_POINT_NONE : wykresy_chart_skaluj(w);
    }
    for (int i = n; i < WYKRESY_MAX_PUNKTOW; i++) tab[i] = LV_CHART_POINT_NONE;
  }
  // Sloty nieuzyte w tym odswiezeniu (nieprzydzielone zadnej wybranej
  // pozycji w petli powyzej - albo bo user ma mniej niz 8 wybranych, albo bo
  // odznaczyl cos od poprzedniego odswiezenia) - caly bufor pusty.
  for (int s = 0; s < WYKRESY_MAX_SLOTOW; s++) {
    if (slotUzyty[s]) continue;
    lv_chart_series_t *ser = wykresyChartSer[s];
    if (!ser) continue;
    lv_coord_t *tab = lv_chart_get_y_array(wykresyChart, ser);
    for (int i = 0; i < WYKRESY_MAX_PUNKTOW; i++) tab[i] = LV_CHART_POINT_NONE;
  }

  lv_chart_refresh(wykresyChart);

  // Podpis zakresu czasu pod wykresem - etykieta pierwszego i ostatniego
  // punktu (ta sama etykieta co przychodzi w CSV). Bierzemy pierwsza
  // aktywna pozycje z jakimikolwiek danymi (kolejnosc wyboru, nie kolejnosc
  // na osiach) - jak dotad, to tylko orientacyjny podpis czasu, wspolny
  // dla wszystkich serii (te sama redukcja/okno czasu).
  if (wykresyChartZakresLabel) {
    int pierwszaZDanymi = -1;
    for (int p = 0; p < g_wykresyLiczbaWybranych; p++) {
      if (g_wykresyLiczbaPunktow[p] > 0) { pierwszaZDanymi = p; break; }
    }
    if (pierwszaZDanymi >= 0) {
      const WykresyPunkt *punkty = g_wykresyPunkty[pierwszaZDanymi];
      int liczbaPunktow = g_wykresyLiczbaPunktow[pierwszaZDanymi];
      char zakresBuf[40];
      snprintf(zakresBuf, sizeof(zakresBuf), "%s  -  %s",
               punkty[0].etykieta, punkty[liczbaPunktow - 1].etykieta);
      lv_label_set_text(wykresyChartZakresLabel, zakresBuf);
    }
  }

  if (wykresyWykresPlaceholder) lv_obj_add_flag(wykresyWykresPlaceholder, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wykresyChart, LV_OBJ_FLAG_HIDDEN);
  if (wykresyChartZakresLabel) lv_obj_clear_flag(wykresyChartZakresLabel, LV_OBJ_FLAG_HIDDEN);
  // [PARTIA 8e, ta sesja] Legenda odslaniana razem z wykresem - jej wlasna
  // widocznosc per-pozycja (item ukryty/odslonity) juz zalatwiona przez
  // wykresy_legenda_odswiez() na poczatku tej funkcji; ten kontener
  // (rzad-wrap) trzeba jeszcze wywolic z ewentualnego wykresy_chart_ukryj().
  if (wykresyLegendaCont) lv_obj_clear_flag(wykresyLegendaCont, LV_OBJ_FLAG_HIDDEN);
  // [FIX, poprzednia sesja, wciaz aktualne] Odslaniamy tu tylko widzety-
  // kontenery etykiet osi faktycznie uzywanych w tym odswiezeniu - patrz
  // odpowiednie add_flag(HIDDEN)/clear_flag(HIDDEN) przy petli osi wyzej w
  // tej funkcji. Tutaj tylko upewniamy sie, ze caly wykres (a nie
  // placeholder bledu) jest tym, co user widzi.
}

// [FIX-WYKRESY-RAM, ta sesja] main.cpp woła to RAZ, TUZ PRZED wyslaniem GET
// /api/history(/long) - przygotowuje swiezy stan strumienia. `liczbaSerii`
// (i sama kolejnosc wyboru g_wykresyKolejnoscWyboru) jest ZAMRAZANA tutaj:
// httpGet()/strumien odbywa sie synchronicznie w obrebie jednego wywolania
// dotykowego (dokladnie jak stare, blokujace httpGet()), wiec user fizycznie
// nie moze zmienic wyboru serii w trakcie - nie potrzeba dodatkowej ochrony
// przed wyscigiem.
void lvgl_dashboard_poc_wykresy_strumien_start(bool dlugi) {
  memset(&g_wykresyStrumien, 0, sizeof(g_wykresyStrumien));
  g_wykresyStrumien.aktywny = true;
  g_wykresyStrumien.dlugi = dlugi;
  // [FIX-WYKRESY-GODZINY] g_wykresyTryb jest juz zdefiniowany wyzej w tym
  // pliku (enum WykresyTryb) - patrz obszerny komentarz przy
  // WYKRESY_GODZINY_OKNO co do tego, dlaczego to rozroznienie w ogole tu
  // jest. `!dlugi` w warunku czysto dla czytelnosci - GODZINY zawsze ma
  // dlugi=false (patrz wykresy_tryb_jest_dlugi()), ale nie zaskoczy nikogo
  // gdyby to kiedys przestalo byc prawda.
  g_wykresyStrumien.oknoPrzesuwne = !dlugi && (g_wykresyTryb == WYKR_TRYB_GODZINY);
  g_wykresyStrumien.liczbaSerii = g_wykresyLiczbaWybranych;
  g_wykresyStrumien.rozmiarKubelka = 1;
}

// main.cpp woła to RAZ na KAZDY wiersz CSV, w miare jak przychodzi z sieci
// (main.cpp dzieli strumien po '\n' - patrz wykresyFetchWykonaj()). Parsuje
// kolumny WSZYSTKICH aktualnie wybranych serii z tej JEDNEJ linii naraz
// (zamiast osobnego przebiegu calego CSV na kazda serie, jak w starym
// kodzie) i akumuluje je w biezacym otwartym kubelku g_wykresyStrumien.
void lvgl_dashboard_poc_wykresy_strumien_linia(const char *linia) {
  if (!g_wykresyStrumien.aktywny || !linia) return;
  const bool dlugi = g_wykresyStrumien.dlugi;

  // [DZIURY-CZAS, ta sesja] Sprawdzamy PRZED przetworzeniem wartosci z tego
  // wiersza, czy od POPRZEDNIEGO surowego wiersza minelo wiecej niz prog -
  // jesli tak, to realna przerwa w dzialaniu urzadzenia (restart, brak
  // WiFi/zasilania), nie zwykly odstep miedzy probkami. Domykamy WCZESNIEJ
  // biezacy (czesciowo wypelniony) kubelek jako normalne dane (jeśli cokolwiek
  // juz w nim jest), wstawiamy DEDYKOWANY kubelek-znacznik (NaN - patrz
  // wykresy_strumien_zamknij_kubelek()) w jego miejsce, i DOPIERO wtedy ten
  // wiersz zaczyna zupelnie nowy kubelek. Dzieki temu granica przerwy ZAWSZE
  // wypada na granicy kubelka - nigdy nie zostaje "ukryta" w srodku
  // usrednionego (po adaptacyjnym podwojeniu) kubelka obejmujacego wiele
  // surowych wierszy. Zero kosztu pamieci per punkt (patrz uzasadnienie przy
  // WYKRESY_PROG_DZIURY_KROTKI_S/DLUGI_S i przy samej funkcji zamknij_kubelek()).
  uint32_t czasTegoWiersza = 0;
  wykresy_csv_kolumna_u32(linia, dlugi ? WYKRESY_KOL_DLUGI_CZAS : WYKRESY_KOL_KROTKI_CZAS, czasTegoWiersza);
  if (czasTegoWiersza > 0 && g_wykresyStrumien.czasOstatniegoWiersza > 0 &&
      czasTegoWiersza > g_wykresyStrumien.czasOstatniegoWiersza) {
    uint32_t prog = dlugi ? WYKRESY_PROG_DZIURY_DLUGI_S : WYKRESY_PROG_DZIURY_KROTKI_S;
    if (czasTegoWiersza - g_wykresyStrumien.czasOstatniegoWiersza > prog) {
      if (g_wykresyStrumien.wierszWKubelku > 0) wykresy_strumien_zamknij_kubelek(false);
      wykresy_strumien_zamknij_kubelek(true);
    }
  }
  if (czasTegoWiersza > 0) g_wykresyStrumien.czasOstatniegoWiersza = czasTegoWiersza;

  for (int s = 0; s < g_wykresyStrumien.liczbaSerii; s++) {
    const WykresySeriaDef &seria = WYKRESY_SERIE[g_wykresyKolejnoscWyboru[s]];
    const int kolSr = dlugi ? seria.kolDlugiSr : seria.kolKrotki;
    float wSr;
    if (!wykresy_csv_kolumna_float(linia, kolSr, wSr)) continue;
    float wMin = wSr, wMaks = wSr;
    if (dlugi) {
      wykresy_csv_kolumna_float(linia, WYKRESY_KOL_DLUGI_MIN(seria.kolDlugiSr), wMin);
      wykresy_csv_kolumna_float(linia, WYKRESY_KOL_DLUGI_MAX(seria.kolDlugiSr), wMaks);
    }
    if (g_wykresyStrumien.licznik[s] == 0) {
      g_wykresyStrumien.minW[s] = wMin;
      g_wykresyStrumien.maxW[s] = wMaks;
    } else {
      if (wMin < g_wykresyStrumien.minW[s]) g_wykresyStrumien.minW[s] = wMin;
      if (wMaks > g_wykresyStrumien.maxW[s]) g_wykresyStrumien.maxW[s] = wMaks;
    }
    g_wykresyStrumien.suma[s] += wSr;
    g_wykresyStrumien.licznik[s]++;
  }
  wykresy_csv_etykieta(linia, g_wykresyStrumien.etykieta, sizeof(g_wykresyStrumien.etykieta));

  g_wykresyStrumien.wierszWKubelku++;
  if (g_wykresyStrumien.wierszWKubelku >= g_wykresyStrumien.rozmiarKubelka) {
    wykresy_strumien_zamknij_kubelek();
  }
}

// main.cpp woła to RAZ po zakonczeniu transferu (sukces LUB blad/brak WiFi -
// wtedy liczbaWierszy=0, dokladnie jak pusty String w starym API). Domyka
// ewentualny niepelny ostatni kubelek i przelacza UI na wykres/komunikat
// bledu - identyczna logika odrzucania "spoznionej" odpowiedzi i pustego
// wyniku jak w wycofanej lvgl_dashboard_poc_wykresy_dane_gotowe().
void lvgl_dashboard_poc_wykresy_strumien_koniec(bool dlugi, uint32_t liczbaWierszy) {
  g_wykresyFetchWToku = false;
  const bool bylAktywny = g_wykresyStrumien.aktywny;
  g_wykresyStrumien.aktywny = false;
  if (!wykresyWykresPlaceholder) return;

  // Odpowiedz spozniona wzgledem aktualnego wyboru trybu (user zdazyl
  // przelaczyc Godziny/Dzien/Tydzien zanim ten GET wrocil) - odrzucamy ja,
  // nowszy fetch (wykresy_odswiez_wykres_fetch()) juz jest albo zaraz
  // bedzie w drodze i to JEGO odpowiedz ma sie wyswietlic.
  if (dlugi != g_wykresyFetchOczekiwanyDlugi) return;
  // Podobnie: user zdazyl wrocic na Selekcje (lub zamknac caly ekran) -
  // nie ma po co odswiezac ukrytej podstrony.
  if (!wykresyWykresCont || lv_obj_has_flag(wykresyWykresCont, LV_OBJ_FLAG_HIDDEN)) return;

  if (!bylAktywny || liczbaWierszy == 0) {
    wykresy_chart_ukryj();
    lv_label_set_text(wykresyWykresPlaceholder, "Brak danych (WiFi/centrala niedostepna)");
    lv_obj_set_style_text_color(wykresyWykresPlaceholder, rgb565(KOL_OGIEN), 0);
    return;
  }

  // Ostatni kubelek zwykle jest NIEPELNY (liczba wierszy rzadko dzieli sie
  // rowno przez rozmiarKubelka) - domykamy go tak samo jak kazdy poprzedni,
  // pod warunkiem ze cokolwiek do niego trafilo.
  if (g_wykresyStrumien.wierszWKubelku > 0) {
    wykresy_strumien_zamknij_kubelek();
  }

  bool majaDane = false;
  for (int p = 0; p < WYKRESY_MAX_SLOTOW; p++) {
    if (p >= g_wykresyStrumien.liczbaSerii || !g_wykresyPunkty[p]) {
      g_wykresyLiczbaPunktow[p] = 0;
      continue;
    }
    g_wykresyLiczbaPunktow[p] = g_wykresyStrumien.liczbaKubelkow;
    if (g_wykresyStrumien.liczbaKubelkow > 0) majaDane = true;
  }

  if (!majaDane) {
    wykresy_chart_ukryj();
    lv_label_set_text(wykresyWykresPlaceholder, "CSV bez rozpoznawalnych punktow dla wybranej serii");
    lv_obj_set_style_text_color(wykresyWykresPlaceholder, rgb565(KOL_OGIEN), 0);
    return;
  }

  // [PARTIA 7, ta sesja] Dane sparsowane poprawnie - buduje/odswieza
  // realny lv_chart i przelacza widocznosc z placeholdera na wykres.
  wykresy_chart_odswiez();
}

// Podswietla aktywny przycisk (KOL_OGIEN, jak reszta akcentow w tym pliku -
// np. pompaAutoBtn), reszta wraca do neutralnego KOL_KARTA. Dokladnie ten
// sam wzorzec "radio" co pompa_panel_glowna_odswiez() dla
// pompaTrybCzasowyBtn/pompaTrybTempBtn/pompaTrybAutoBtn.
static void wykresy_tryb_odswiez_wyglad() {
  for (int i = 0; i < 3; i++) {
    if (!g_wykresyTrybBtn[i]) continue;
    bool aktywny = ((int)g_wykresyTryb == i);
    lv_obj_set_style_bg_color(g_wykresyTrybBtn[i], rgb565(aktywny ? KOL_OGIEN : KOL_KARTA), 0);
    lv_obj_t *lbl = lv_obj_get_child(g_wykresyTrybBtn[i], 0);
    if (lbl) lv_obj_set_style_text_color(lbl, rgb565(aktywny ? KOL_TLO : KOL_TEKST_DIM), 0);
  }
}

static void wykresy_tryb_klikniety_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  // [FIX, ta sesja - idle-timeout resetowany klikami] Patrz identyczny
  // komentarz w wykresy_chip_klikniety_cb() wyzej - ten sam problem dotyczyl
  // tez przyciskow zakresu (GODZINNY/DZIENNY/TYGODNIOWY). Wywolane PRZED
  // wczesnym "return" ponizej, zeby nawet ponowne klikniecie juz aktywnego
  // przycisku liczylo sie jako aktywnosc i resetowalo odliczanie.
  wykresy_panel_idle_resetuj();
  if ((int)g_wykresyTryb == idx) return; // juz aktywny - nic do zrobienia
  g_wykresyTryb = (WykresyTryb)idx;
  wykresy_tryb_odswiez_wyglad();
  wykresy_status_label_odswiez(); // status dopisuje tez aktywny tryb, patrz nizej
  // [PARTIA 5e, ta sesja] Jesli user jest WLASNIE na podstronie Wykres (a
  // nie na Selekcji) i zmienia zakres, wykres powinien sie odswiezyc od
  // razu, bez konieczosci cofania sie do Selekcji i klikania "POKAZ WYKRES"
  // ponownie. Na Selekcji nic tu nie robimy - fetch i tak wystartuje przy
  // wejsciu na Wykres (wykresy_pokaz_wykres()).
  if (wykresyWykresCont && !lv_obj_has_flag(wykresyWykresCont, LV_OBJ_FLAG_HIDDEN)) {
    wykresy_odswiez_wykres_fetch();
  }
}

// Rzad 3 przyciskow, dokladnie ten sam wzorzec ukladu co rzadTrybPracy w
// pompa_panel_zbuduj() (kontener o stalej szerokosci, SPACE_EVENLY).
static void wykresy_tryb_zbuduj(lv_obj_t *parent) {
  lv_obj_t *rzad = lv_obj_create(parent);
  lv_obj_remove_style_all(rzad);
  // [FIX NAZW TRYBOW, ta sesja] Szerokosc rzedu/przyciskow urosla
  // (300->340 / 92->104), zeby zmieszczic najdluzszy z poprawionych napisow
  // ("TYGODNIOWY", 10 znakow) bez wizualnego przycinania/nachodzenia na
  // sasiedni przycisk - patrz tez zmiana samych NAZWY[] ponizej.
  lv_obj_set_size(rzad, 340, 34);
  lv_obj_align(rzad, LV_ALIGN_TOP_MID, 0, 44);
  lv_obj_set_flex_flow(rzad, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rzad, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(rzad, LV_OBJ_FLAG_SCROLLABLE);

  // [FIX NAZW TRYBOW, ta sesja] Byle "GODZINY/DZIEN/TYDZIEN" (rzeczowniki,
  // niespojne z reszta UI) -> "GODZINNY/DZIENNY/TYGODNIOWY" (przymiotniki,
  // zgloszone jako poprawka). wykresy_tryb_nazwa() wyzej dostala te same
  // trzy formy (uzywana w naglowku podstrony Wykres), zeby oba miejsca byly
  // spojne.
  static const char *NAZWY[3] = { "GODZINNY", "DZIENNY", "TYGODNIOWY" };
  for (int i = 0; i < 3; i++) {
    lv_obj_t *btn = lv_btn_create(rzad);
    lv_obj_set_size(btn, 104, 30);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, NAZWY[i]);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, wykresy_tryb_klikniety_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    g_wykresyTrybBtn[i] = btn;
  }
  wykresy_tryb_odswiez_wyglad(); // stan startowy - "DZIEN" podswietlony
}

// Buduje pelnowymiarowy kontener-"podstrone" (ten sam przepis co reszta
// tego pliku: remove_style_all + set_size na caly ekran + brak scrolla),
// zeby wykresy_panel_zbuduj() nizej nie powtarzal 4 identycznych linii
// dwa razy.
static lv_obj_t *wykresy_podstrone_utworz(lv_obj_t *parent) {
  lv_obj_t *cont = lv_obj_create(parent);
  lv_obj_remove_style_all(cont);
  lv_obj_set_size(cont, 480, 320);
  lv_obj_set_pos(cont, 0, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  return cont;
}

static void wykresy_panel_zbuduj(lv_obj_t *scr) {
  dashboardScr = scr; // moze juz byc ustawione przez inny *_zbuduj() - bezpieczne do powtorzenia

  wykresyPanel = lv_obj_create(nullptr);
  // [v1.2.2 PAMIEC] Bufory punktow (18,4 kB) tylko na czas, gdy ten ekran
  // jest otwarty - patrz komentarz przy g_wykresyPunkty[].
  lv_obj_add_event_cb(wykresyPanel, wykresy_panel_ekran_event_cb, LV_EVENT_SCREEN_LOAD_START, nullptr);
  lv_obj_add_event_cb(wykresyPanel, wykresy_panel_ekran_event_cb, LV_EVENT_SCREEN_UNLOADED, nullptr);
  lv_obj_set_style_bg_color(wykresyPanel, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(wykresyPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(wykresyPanel, LV_OBJ_FLAG_SCROLLABLE);
  // CLICKABLE, zeby dotyk na pustym tle tego ekranu (nie ma tu jeszcze
  // zadnego interaktywnego widgetu) mial gdzie "wyladowac" i gest zostal
  // wykryty - patrz analogiczny komentarz przy scr w init(). Gest jest
  // NA wykresyPanel (rodzicu obu podstron), wiec dziala identycznie na
  // KAZDEJ z nich (bablowanie LV_OBJ_FLAG_GESTURE_BUBBLE, patrz komentarz
  // przy dashboard_gesture_cb() wyzej) - zawsze zamyka caly ekran Wykresy
  // na drugi pulpit, niezaleznie od tego ktora podstrona jest widoczna
  // [ZALOZENIE tej sesji, do potwierdzenia] - prostsza, jednolita
  // symetria z reszta panelu, zamiast osobnej logiki "gest na Wykresie
  // wraca tylko do Selekcji". Powrot TYLKO o jeden poziom z podstrony
  // Wykres jest dostepny przez strzalke wstecz (dotyk), patrz nizej.
  lv_obj_add_flag(wykresyPanel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(wykresyPanel, wykresy_panel_gesture_cb, LV_EVENT_GESTURE, nullptr);

  // ================= PODSTRONA 1: SELEKCJA =================
  wykresySelekcjaCont = wykresy_podstrone_utworz(wykresyPanel);

  lv_obj_t *tytul = lv_label_create(wykresySelekcjaCont);
  lv_label_set_text(tytul, "WYKRESY");
  lv_obj_set_style_text_font(tytul, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(tytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(tytul, LV_ALIGN_TOP_MID, 0, 10);

  // [PARTIA 5d, ta sesja] Pasek zakresu Godziny/Dzien/Tydzien nad chipami.
  wykresy_tryb_zbuduj(wykresySelekcjaCont);

  // [PARTIA 5c, ta sesja] Rzad chipow wyboru serii (max 2) pod paskiem zakresu.
  wykresy_chipy_zbuduj(wykresySelekcjaCont);

  // [USUNIETO, ta sesja] Etykieta statusu wyboru ("[Dzien] Wybierz serie
  // ponizej..."/"Wybrano: ..."), ktora byla tu wczesniej - patrz komentarz
  // "TYMCZASOWA" przy wykresy_status_label_odswiez() (byla tylko
  // prowizorka sprzed legendy nad wykresem, Partia 8e, ktora juz pokazuje
  // wybrane serie w czytelnej formie na podstronie Wykres). Miejsce po niej
  // poszlo w wieksze chipy powyzej (wykresy_chipy_zbuduj()). wykresyStatusLabel
  // zostaje NIEUTWORZONY (nullptr) - wykresy_status_label_odswiez() ma
  // wczesny return na `if (!wykresyStatusLabel) return;`, wiec wywolania
  // tej funkcji z callbackow chipow/zakresu zostaja nieszkodliwym no-opem,
  // bez potrzeby usuwania samych wywolan.

  // [DWIE PODSTRONY, ta sesja] Przycisk "POKAZ WYKRES" - jedyne wejscie na
  // podstrone Wykres. Martwy (przygaszony, klik nie reaguje - patrz
  // wykresy_pokaz_wykres_cb()) dopoki nie wybrano >=1 serii, dokladnie
  // jak trzeci chip przy limicie 2 w wykresy_chip_klikniety_cb().
  wykresyPokazBtn = lv_btn_create(wykresySelekcjaCont);
  lv_obj_set_size(wykresyPokazBtn, 220, 42);
  lv_obj_set_style_radius(wykresyPokazBtn, 10, 0);
  lv_obj_set_style_shadow_width(wykresyPokazBtn, 0, 0);
  // [ZMIANA, ta sesja - SIATKA 4x3] 210->228 - chipy przeszly z rzedu
  // 76->112px na siatke 4x3 wysokosci 136px (patrz wykresy_chipy_zbuduj()),
  // konczaca sie teraz na 82+136=218, wiec przycisk zsunieto nizej (218+10
  // marginesu = 228), zeby nie nachodzic na powiekszona siatke. Miesci sie
  // z zapasem do dolu ekranu (228+42=270 < 320).
  lv_obj_align(wykresyPokazBtn, LV_ALIGN_TOP_MID, 0, 228);
  lv_obj_t *pokazLbl = lv_label_create(wykresyPokazBtn);
  lv_label_set_text(pokazLbl, "POKAZ WYKRES");
  lv_obj_set_style_text_font(pokazLbl, &lv_font_montserrat_14, 0); // [FIX kompilacji] _16 nie jest wlaczony w lv_conf.h (tylko 14/20/28 sa builtowane w tym projekcie) - patrz identyczny FIX nizej przy wykresyWykresTytul
  lv_obj_center(pokazLbl);
  lv_obj_add_event_cb(wykresyPokazBtn, wykresy_pokaz_wykres_cb, LV_EVENT_CLICKED, nullptr);
  wykresy_pokaz_btn_odswiez(); // stan startowy - przygaszony (0 wybranych)

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(wykresySelekcjaCont, wykresy_panel_zamknij_cb);

  // ================= PODSTRONA 2: WYKRES (pelnoekranowy) =================
  // [PARTIA 8f, ta sesja - AKTUALIZACJA KOMENTARZA] Ponizsza podstrona ma
  // od Partii 7 prawdziwy lv_chart (wczesniejszy komentarz mowil "jeszcze
  // nie istnieje" - nieaktualne od dawna, poprawione przy tych porzadkach).
  // Naglowek (tytul + tryb, wykresy_pokaz_wykres()) + legenda (Partia 8e,
  // wykresyLegendaCont nizej) + sam wykres (do 8 serii na 2 osiach,
  // Partia 8) + strzalka wstecz + X.
  wykresyWykresCont = wykresy_podstrone_utworz(wykresyPanel);
  lv_obj_add_flag(wykresyWykresCont, LV_OBJ_FLAG_HIDDEN); // domyslnie ukryta - startujemy na Selekcji
  // [FIX, ta sesja] Tlo tej podstrony musi byc CLICKABLE, zeby tapniecie w
  // puste miejsce (nie tylko w konkretny widget) mialo gdzie "wyladowac" i
  // odpalilo PRESSED - ten sam wzorzec co przy wykresyPanel/scr (patrz
  // komentarze przy dashboard_gesture_cb() wyzej). Bez tego reset idle-timera
  // dzialalby tylko po dotkniecie strzalki/X/wykresu, nie samego tla.
  lv_obj_add_flag(wykresyWykresCont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(wykresyWykresCont, wykresy_wykres_dotyk_cb, LV_EVENT_PRESSED, nullptr);

  lv_obj_t *wstecz = lv_img_create(wykresyWykresCont);
  lv_img_set_src(wstecz, &lv_ikona_wstecz);
  lv_obj_set_style_img_recolor(wstecz, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_img_recolor_opa(wstecz, LV_OPA_COVER, 0);
  lv_obj_align(wstecz, LV_ALIGN_TOP_LEFT, 6, 8);
  lv_obj_add_flag(wstecz, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(wstecz, wykresy_wstecz_cb, LV_EVENT_CLICKED, nullptr);

  wykresyWykresTytul = lv_label_create(wykresyWykresCont);
  lv_obj_set_style_text_font(wykresyWykresTytul, &lv_font_montserrat_20, 0); // [FIX kompilacji] _18 nie jest wlaczony w lv_conf.h - patrz identyczny FIX wyzej przy pokazLbl; _20 juz uzywany gdzie indziej w tym pliku (11x), wiec na pewno wbudowany
  lv_obj_set_style_text_color(wykresyWykresTytul, rgb565(KOL_TEKST), 0);
  lv_obj_align(wykresyWykresTytul, LV_ALIGN_TOP_MID, 0, 14);

  // [PARTIA 8e, ta sesja - LEGENDA] Rzad kropka+nazwa, do WYKRESY_MAX_SLOTOW
  // (8) pozycji, zawijany do 2 wierszy - ten sam mechanizm co chipy wyboru
  // na Selekcji (LV_FLEX_FLOW_ROW_WRAP + FLEX_ALIGN_CENTER). Geometria z
  // WYKRESY_LEGENDA_Y/H (patrz nazwana stala wyzej w pliku, przy
  // WYKRESY_CHART_Y/H - te dwie pary sa celowo obliczane razem, zeby nie
  // mogly sie rozjechac). Pula tworzona RAZ tutaj, elementy startowo puste
  // i ukryte - wykresy_legenda_odswiez() wypelnia je i odslania wg
  // aktualnego wyboru przy kazdym odswiezeniu wykresu.
  wykresyLegendaCont = lv_obj_create(wykresyWykresCont);
  lv_obj_remove_style_all(wykresyLegendaCont);
  lv_obj_set_size(wykresyLegendaCont, 460, WYKRESY_LEGENDA_H);
  lv_obj_align(wykresyLegendaCont, LV_ALIGN_TOP_MID, 0, WYKRESY_LEGENDA_Y);
  lv_obj_set_flex_flow(wykresyLegendaCont, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(wykresyLegendaCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(wykresyLegendaCont, LV_OBJ_FLAG_SCROLLABLE);

  for (int p = 0; p < WYKRESY_MAX_SLOTOW; p++) {
    lv_obj_t *item = lv_obj_create(wykresyLegendaCont);
    lv_obj_remove_style_all(item);
    lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(item, 6, 0);
    lv_obj_set_style_pad_ver(item, 2, 0);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN); // startowo puste - odslaniane w wykresy_legenda_odswiez()

    lv_obj_t *kropka = lv_obj_create(item);
    lv_obj_remove_style_all(kropka);
    lv_obj_set_size(kropka, 10, 10);
    lv_obj_set_style_radius(kropka, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(kropka, LV_OPA_COVER, 0);
    lv_obj_clear_flag(kropka, LV_OBJ_FLAG_SCROLLABLE);
    wykresyLegendaKropka[p] = kropka;

    lv_obj_t *tekst = lv_label_create(item);
    lv_obj_set_style_text_font(tekst, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_left(tekst, 4, 0);
    lv_label_set_text(tekst, "");
    wykresyLegendaTekst[p] = tekst;
  }

  // [PARTIA 5e, ta sesja] Zapamietujemy uchwyt (byl lokalny w 5a) - teraz
  // wykresy_odswiez_wykres_fetch()/lvgl_dashboard_poc_wykresy_dane_gotowe()
  // podmieniaja jej tresc na "Ladowanie..." / wynik CSV / komunikat bledu.
  // Docelowo (6./7. partia) ten label zniknie na rzecz prawdziwego lv_chart
  // w tym samym miejscu.
  wykresyWykresPlaceholder = lv_label_create(wykresyWykresCont);
  lv_label_set_text(wykresyWykresPlaceholder, "Ladowanie danych...");
  lv_obj_set_style_text_font(wykresyWykresPlaceholder, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wykresyWykresPlaceholder, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_width(wykresyWykresPlaceholder, 400);
  lv_obj_set_style_text_align(wykresyWykresPlaceholder, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(wykresyWykresPlaceholder, LV_LABEL_LONG_WRAP);
  lv_obj_center(wykresyWykresPlaceholder);

  // [PARTIA 7, ta sesja] Prawdziwy lv_chart - ukryty na starcie (widoczny
  // dopiero gdy dane doplyna, patrz wykresy_chart_odswiez()/wykresy_chart_ukryj()
  // wyzej w pliku). Rozmiar/pozycja z WYKRESY_CHART_Y/WYKRESY_CHART_H
  // (patrz nazwana stala wyzej w pliku - Partia 8e zrobila mu miejsce pod
  // legenda, stad te stale zamiast dawnych liczb "50"/"260" wprost tutaj).
  // Szerokosc/X i tak sa NADPISYWANE przy kazdym odswiezeniu przez
  // wykresy_chart_dopasuj_os() (patrz komentarz tam) - podana tu wartosc to
  // tylko bezpieczny stan startowy przed pierwszym odswiezeniem.
  wykresyChart = lv_chart_create(wykresyWykresCont);
  lv_obj_set_size(wykresyChart, 440, WYKRESY_CHART_H);
  lv_obj_align(wykresyChart, LV_ALIGN_TOP_MID, 0, WYKRESY_CHART_Y);
  lv_obj_set_style_bg_color(wykresyChart, rgb565(KOL_KARTA), 0);
  lv_obj_set_style_bg_opa(wykresyChart, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(wykresyChart, 0, 0);
  lv_obj_set_style_radius(wykresyChart, 10, 0);
  lv_chart_set_type(wykresyChart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(wykresyChart, 3, 5);
  lv_chart_set_update_mode(wykresyChart, LV_CHART_UPDATE_MODE_CIRCULAR);
  // Bez markerow punktow (LV_PART_INDICATOR size=0) - na ekranie 480px
  // szerokosci przy do 96 punktach markery tylko zaszumialyby linie.
  lv_obj_set_style_size(wykresyChart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(wykresyChart, 2, LV_PART_ITEMS);
  // 5 glownych podzialek na obu osiach Y - same kreski podzialki nadal
  // rysuje LVGL (major_len=6, minor_len=3, major_cnt=5, minor_cnt=2), ale
  // BEZ wbudowanych etykiet tekstowych (label_en=false). Tekst rysujemy
  // sami, patrz wykresyEtykietyA/B nizej i wykresy_chart_dopasuj_os() -
  // obszerny komentarz przy deklaracji WYKRESY_OS_LICZBA_ETYKIET wyzej w
  // pliku wyjasnia dlaczego. draw_size=0 bo z label_en=false LVGL nie
  // potrzebuje juz zadnego dodatkowego miejsca na invalidacje etykiet
  // (ext_draw_size) - wlasne widzety etykiet maja to zalatwione przez
  // zwykly layout, poza obszarem samego wykresu.
  lv_chart_set_axis_tick(wykresyChart, LV_CHART_AXIS_PRIMARY_Y, 6, 3, 5, 2, false, 0);
  lv_chart_set_axis_tick(wykresyChart, LV_CHART_AXIS_SECONDARY_Y, 6, 3, 5, 2, false, 0);
  // [FIX - ostateczne rozwiazanie, ta sesja] wlasne widzety etykiet osi -
  // 5 lewych (wykresyEtykietyA, os PRIMARY) + 5 prawych (wykresyEtykietyB,
  // os SECONDARY). Tworzone RAZ tutaj (analogicznie do serii wykresu,
  // patrz RAPORT_NAPRAWA_CRASH_WYKRESY_LVGL.md - unikamy cyklicznego
  // create/delete widgetow). Pozycja/tekst/kolor kazdej etykiety sa
  // ustawiane pozniej przez wykresy_chart_dopasuj_os() za kazdym razem,
  // gdy zmienia sie zakres danej osi - tutaj tylko tworzymy widzety i
  // nadajemy im startowy wyglad (pusty tekst, ukryte, dopoki nie doplyna
  // pierwsze dane - patrz wykresy_chart_ukryj()/pokaz odpowiednika).
  // [FIX - etykiety zawijaly sie do drugiej linii, ta sesja] Domyslny
  // long_mode etykiety w LVGL to LV_LABEL_LONG_WRAP - gdy szerokosc widzetu
  // byla ustawiona DOKLADNIE na zmierzona szerokosc tekstu (patrz
  // wykresy_chart_dopasuj_os()), najmniejsze zaokraglenie rasteryzacji
  // czcionki powodowalo zlamanie tekstu do drugiej linii (widac na
  // screenshocie: "26.0" -> "26." + "0"). Przy dluzszych liczbach (np.
  // cisnienie "1013.0", 4 cyfry) byloby to jeszcze czestsze. LONG_CLIP
  // zamiast tego po prostu utnie ewentualna nadwyzke zamiast zawijac -
  // w polaczeniu z malym zapasem szerokosci w dopasuj_os() (ponizej) w
  // praktyce nic nie bedzie ucinane, bo szerokosc jest i tak liczona z
  // realnego pomiaru tekstu tej samej czcionki.
  for (int i = 0; i < WYKRESY_OS_LICZBA_ETYKIET; i++) {
    wykresyEtykietyA[i] = lv_label_create(wykresyWykresCont);
    lv_label_set_text(wykresyEtykietyA[i], "");
    lv_obj_set_style_text_font(wykresyEtykietyA[i], &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(wykresyEtykietyA[i], LV_LABEL_LONG_CLIP);
    lv_obj_add_flag(wykresyEtykietyA[i], LV_OBJ_FLAG_HIDDEN);

    wykresyEtykietyB[i] = lv_label_create(wykresyWykresCont);
    lv_label_set_text(wykresyEtykietyB[i], "");
    lv_obj_set_style_text_font(wykresyEtykietyB[i], &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(wykresyEtykietyB[i], LV_LABEL_LONG_CLIP);
    lv_obj_add_flag(wykresyEtykietyB[i], LV_OBJ_FLAG_HIDDEN);
  }
  // [FIX, ta sesja] Osobne podpiecie na samym wykresie - to najbardziej
  // oczywiste miejsce, w ktore user dotknie podczas ogladania (np. probujac
  // "przesunac"/dotknac linie), a PRESSED na wykresyWykresCont go NIE zlapie
  // (patrz komentarz przy wykresy_wykres_dotyk_cb() wyzej - brak bablowania).
  lv_obj_add_event_cb(wykresyChart, wykresy_wykres_dotyk_cb, LV_EVENT_PRESSED, nullptr);

  // [FIX crash wykresow, patrz RAPORT_NAPRAWA_CRASH_WYKRESY_LVGL.md] Serie
  // tworzone JEDNORAZOWO, na caly czas zycia programu, i nigdy wiecej nie
  // usuwane. point_count ustawiony raz na WYKRESY_MAX_PUNKTOW - bufory
  // wszystkich serii sa od razu zaalokowane na maksymalny rozmiar i juz sie
  // nie zmieniaja. wykresy_chart_odswiez() odtad tylko NADPISUJE wartosci w
  // tych samych buforach (lv_chart_get_y_array) zamiast robic
  // remove_series+set_point_count+add_series, co przy powtarzanym cyklu
  // fragmentowalo/przepelnialo wewnetrzny pool pamieci LVGL i powodowalo
  // "free() target pointer is outside heap areas" przy DRUGIEJ zmianie
  // wyboru serii.
  //
  // [PARTIA 8c, potem 8d, ta sesja - PULA 8 SERII] Zamiast dwoch nazwanych
  // obiektow (SerA=glowna os, SerB=dodatkowa os), tworzymy RAZ pule
  // WYKRESY_MAX_SLOTOW (8) serii, po jednym slocie na kazda mozliwa
  // wybrana serie (patrz g_wykresyKolejnoscWyboru/g_wykresyOsWyboru z
  // Partii 8a/8b).
  //
  // [FIX PO KOMPILACJI NA SPRZECIE, ta sesja - PLAN_PARTIA8_OSIE_WYKRESOW.md
  // OKAZAL SIE BLEDNY W TYM PUNKCIE] Pierwotny plan (dynamiczny zapis
  // ser->axis tuz przed kazdym odswiezeniem, patrz historia w
  // PLAN_PARTIA8_OSIE_WYKRESOW.md) zakladal, ze ->axis to zwykle,
  // niepubliczne-ale-dostepne pole struktury - "NIEPOTWIERDZONE jeszcze na
  // sprzecie" ostrzegal wtedy komentarz. Kompilator na prawdziwym sprzecie:
  // "'struct lv_chart_series_t' has no member named 'axis'" - w tej
  // konkretnej wersji LVGL pole w ogole nie jest publicznie dostepne, wiec
  // zalozenie bylo bledne, nie tylko "nieoficjalne". Os serii da sie ustawic
  // WYLACZNIE raz, w lv_chart_add_series() - wiec pula jest teraz NA STALE
  // podzielona na dwie podpule, ASYMETRYCZNIE - fizyczne sloty 0..6
  // (WYKRESY_MAX_PRIMARY=7) tworzone od razu z PRIMARY_Y, slot 7
  // (WYKRESY_MAX_SECONDARY=1) od razu z SECONDARY_Y.
  // [ZMIANA, ta sesja - patrz PLAN_ZMIAN_OS_WYKRESOW.md] Pierwotny
  // symetryczny podzial 4+4 (opisany w tym komentarzu wczesniej) zostal
  // zastapiony 7+1, bo realny rozklad serii w WYKRESY_SERIE ma 7 pozycji
  // o jednostce "C" - skrajny wybor (wszystkie 7 + 1 inna) potrzebuje 7
  // slotow na JEDNEJ osi, czego 4+4 by nie pokrylo. wykresy_chart_odswiez()
  // (patrz przepisana ponizej) mapuje teraz POZYCJE WYBORU na te stale
  // podpule (licznik per-os, limit z WYKRESY_MAX_NA_OSI[os]) zamiast pisac
  // do ->axis.
  lv_chart_set_point_count(wykresyChart, WYKRESY_MAX_PUNKTOW);
  for (int i = 0; i < WYKRESY_MAX_PRIMARY; i++) {
    // [FIX PO KOMPILACJI NA SPRZECIE] rgb565() oczekuje wartosci JUZ w
    // formacie RGB565 (16-bit), nie 24-bit hex 0xRRGGBB (patrz komentarz przy
    // definicji rgb565() na poczatku pliku) - 0xFFFFFF bylo blednym zapisem
    // (kompilator ostrzegal: "unsigned conversion... changes value from
    // 16777215 to 65535"), dzialajacym przypadkiem (obcięcie do dolnych 16
    // bitow 0xFFFFFF daje akurat 0xFFFF = tez biel), ale nieporzadnym i
    // mylacym. Kolor i tak od razu nadpisywany prawdziwym kolorem serii w
    // wykresy_chart_odswiez() - to tylko placeholder startowy, jak dawniej.
    wykresyChartSer[i] = lv_chart_add_series(wykresyChart, rgb565(0xFFFF), LV_CHART_AXIS_PRIMARY_Y);
  }
  for (int i = WYKRESY_MAX_PRIMARY; i < WYKRESY_MAX_SLOTOW; i++) {
    wykresyChartSer[i] = lv_chart_add_series(wykresyChart, rgb565(0xFFFF), LV_CHART_AXIS_SECONDARY_Y);
  }

  lv_obj_add_flag(wykresyChart, LV_OBJ_FLAG_HIDDEN);

  wykresyChartZakresLabel = lv_label_create(wykresyWykresCont);
  lv_label_set_text(wykresyChartZakresLabel, "");
  lv_obj_set_style_text_font(wykresyChartZakresLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wykresyChartZakresLabel, rgb565(KOL_TEKST_DIM), 0);
  // [FIX - zmarnowane miejsce u dolu wykresu, ta sesja] Nakladamy etykiete
  // na sam dol wykresu (ktory teraz siega do y=310, patrz lv_obj_set_size
  // wyzej + chartY/chartH w wykresy_chart_dopasuj_os()) zamiast trzymac ja
  // w osobnym pasie pod nim - ten sam wzorzec co etykiety osi Y po bokach.
  // Wyrownana do srodka w poziomie, wiec nie koliduje z etykietami osi
  // (te siedza przy samych krawedziach lewej/prawej).
  lv_obj_align(wykresyChartZakresLabel, LV_ALIGN_TOP_MID, 0, 292);
  lv_obj_add_flag(wykresyChartZakresLabel, LV_OBJ_FLAG_HIDDEN);

  // [FIX WYGLAD, ta sesja - Partia 2] Patrz krzyzyk_zamkniecia_utworz().
  krzyzyk_zamkniecia_utworz(wykresyWykresCont, wykresy_panel_zamknij_cb);
}

// Kafelek KLAPA/SYBERKA - interaktywny wariant kafelek(), na wzor
// kafelek_pompa(): cala karta klikalna + 3D press-efekt + ramka podswietlana
// w trybie recznym (dokladnie jak isManualLike() w piec_co_ui_koncept.html).
// Zwraca uchwyt do labelki WARTOSCI (jak kafelek()); `cardWyjscie`/
// `recznieIkonaWyjscie` dodatkowo zwracaja uchwyty potrzebne do
// serwo_tile_odswiez() (ramka, znaczek dloni).
static lv_obj_t *kafelek_serwo(lv_obj_t *parent, int x, int y, int w, int h,
                                const char *etykieta, lv_event_cb_t klikCb,
                                lv_obj_t **cardWyjscie, lv_obj_t **recznieIkonaWyjscie) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, rgb565(KOL_RAMKA), 0); // startowo AUTO - kolor ustawia serwo_tile_odswiez() nizej

  press_flash_zainstaluj(card, KOL_KARTA, 3);
  lv_obj_add_event_cb(card, klikCb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, etykieta);
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 6);

  lv_obj_t *img = lv_img_create(card);
  lv_img_set_src(img, &lv_ikona_suwak);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(img, rgb565(KOL_SUWAK), 0);
  lv_obj_align(img, LV_ALIGN_TOP_RIGHT, -4, 6);

  lv_obj_t *val = lv_label_create(card);
  lv_label_set_text(val, "--");
  lv_obj_set_style_text_color(val, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
  lv_obj_align(val, LV_ALIGN_TOP_LEFT, 8, 52);

  // [POPRAWKA, ta sesja] Ikona trybu - byla wczesniej TYLKO znaczkiem dloni,
  // widocznym jedynie w trybach Reczny/Bezpieczna (oba ta sama ikona, tryb
  // rozrozniany wylacznie kolorem ramki karty) - user na glownym panelu nie
  // widzial od razu, w jakim trybie jest SERWO, bez otwierania panelu.
  // Teraz ikona jest ZAWSZE widoczna i ZMIENIA KSZTALT wg aktualnego
  // g_trybSerwa (Auto/Reczny/Bezpieczna maja rozne bitmapy, patrz
  // serwo_tile_odswiez() nizej) - dwa niezalezne sygnaly (ksztalt + kolor
  // ramki karty) zamiast jednego (sam kolor), czytelne nawet dla kogos kto
  // nie pamieta konwencji kolorow. Nazwa uchwytu zostaje "recznieIcon" (bez
  // zmiany sygnatury funkcji/wywolan), ale semantyka to teraz "ikona trybu".
  // lv_obj_align dziala poprawnie mimo roznych rozmiarow bitmap (30/40px) -
  // przelicza pozycje od aktualnego rozmiaru obiektu przy kazdym wywolaniu
  // w serwo_tile_odswiez().
  lv_obj_t *recznieIcon = lv_img_create(card);
  lv_img_set_src(recznieIcon, &lv_ikona_auto_0); // startowy stan = Auto (domyslny tryb)
  lv_obj_set_style_img_recolor_opa(recznieIcon, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(recznieIcon, rgb565(KOL_RAMKA), 0);
  lv_obj_align(recznieIcon, LV_ALIGN_BOTTOM_RIGHT, -6, -6);

  if (cardWyjscie != nullptr) *cardWyjscie = card;
  if (recznieIkonaWyjscie != nullptr) *recznieIkonaWyjscie = recznieIcon;
  return val;
}

// Odswieza WYGLAD obu kafelkow SERWO (ramka, znaczek dloni) pod aktualne
// g_trybSerwa/g_klapaPozycja/g_syberkaPozycja - wolane wylacznie z
// lvgl_dashboard_poc_aktualizuj(), po kazdym prawdziwym pobierzStatus() -
// ten sam wzorzec co pompa_tile_odswiez().
// [POPRAWKA, ta sesja - panel SERWO v2] Oba kafelki podswietlaja sie RAZEM
// (nie niezaleznie) - dziela WSPOLNY g_trybSerwa. Kolor rozroznia 3 stany:
// Auto = neutralna ramka, Reczny = akcent SUWAK (jak poprzednio), Bezpieczna
// = akcent OGIEN (wymuszony/awaryjny stan, wizualnie inny niz zwykly reczny).
static void serwo_tile_odswiez() {
  // [FIX, ta sesja] Wczesniejszy straznik "if (klapaValLabel == nullptr)
  // return;" zakladal, ze klapaCard/klapaValLabel ZAWSZE istnieja - to byla
  // prawda, dopoki serwo_wybor_zbuduj() (jedyne miejsce, ktore je tworzy)
  // bylo wolane z inicjalizacji. Zostalo STAMTAD USUNIETE (patrz komentarz
  // przy lvgl_dashboard_poc_init(), "serwo_wybor_zbuduj(scr) USUNIETE"),
  // wiec klapaValLabel jest teraz ZAWSZE nullptr, a caly ten straznik
  // blokowal wykonanie funkcji W OGOLE - w tym takze odswiezanie
  // serwoWejscieTrybIcon (jedynego znaczka trybu widocznego realnie na
  // dashboardzie). Kazdy dostep do klapaCard/syberkaCard/klapaRecznieIcon/
  // syberkaRecznieIcon ponizej jest juz osobno zabezpieczony sprawdzeniem
  // na nullptr (dokladnie tak jak mowil ten komentarz przy usunieciu), wiec
  // bezpiecznie usuwamy blokujacy return.

  // [POPRAWKA, ta sesja] Ikona (klapaRecznieIcon/syberkaRecznieIcon) jest
  // teraz ZAWSZE widoczna (nie ma juz show/hide) - zamiast tego podmieniamy
  // jej ZRODLO (ksztalt) i kolor wg trybu, wiec na pierwszy rzut oka z
  // glownego panelu widac, ktory z 3 trybow jest aktywny, bez otwierania
  // panelu SERWO. Ikony wspoldzielone z panelem SERWO (te same bitmapy co
  // serwoAutoIcon/serwoRecznyIcon/serwoBezpiecznaBtn tam) - zero nowych
  // danych bitmapowych. AUTO uzywa statycznej pierwszej klatki (lv_ikona_
  // auto_0) - na tak malej ikonie na kafelku animacja obrotu (jak w panelu)
  // bylaby zbednym obciazeniem/roztargnieniem, wystarczy sam ksztalt.
  uint16_t kolorRamki = KOL_RAMKA;
  const lv_img_dsc_t *ikonaTrybu = &lv_ikona_auto_0;
  const lv_img_dsc_t *ikonaTrybuMala = &lv_ikona_auto_0_mala;
  if (g_trybSerwa == 2) { kolorRamki = KOL_SUWAK; ikonaTrybu = &lv_ikona_recznie; ikonaTrybuMala = &lv_ikona_recznie_mala; }
  else if (g_trybSerwa == 3) { kolorRamki = KOL_OGIEN; ikonaTrybu = &lv_ikona_bezpieczna; ikonaTrybuMala = &lv_ikona_bezpieczna_mala; }

  if (klapaCard != nullptr) lv_obj_set_style_border_color(klapaCard, rgb565(kolorRamki), 0);
  if (syberkaCard != nullptr) lv_obj_set_style_border_color(syberkaCard, rgb565(kolorRamki), 0);

  if (klapaRecznieIcon != nullptr) {
    lv_img_set_src(klapaRecznieIcon, ikonaTrybu);
    lv_obj_set_style_img_recolor(klapaRecznieIcon, rgb565(kolorRamki), 0);
    lv_obj_align(klapaRecznieIcon, LV_ALIGN_BOTTOM_RIGHT, -6, -6); // realign - rozmiar bitmapy rozny per tryb (30/40px)
  }

  if (syberkaRecznieIcon != nullptr) {
    lv_img_set_src(syberkaRecznieIcon, ikonaTrybu);
    lv_obj_set_style_img_recolor(syberkaRecznieIcon, rgb565(kolorRamki), 0);
    lv_obj_align(syberkaRecznieIcon, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
  }

  // [POPRAWKA, ta sesja] Ten sam znaczek trybu takze na GLOWNYM kafelku
  // SERWO (kafelek_serwo_wejscie(), jedynym widocznym wprost na pierwszym
  // ekranie dashboardu) - bez tego zmiana trybu byla widoczna wylacznie
  // na posrednim ekranie WYBOR SERWA, do ktorego user w ogole nie
  // zagladal. serwoWejscieTrybIcon moze byc jeszcze nullptr, jesli ten
  // kafelek nie zostal zbudowany w danej konfiguracji ekranu - zabezpieczone.
  if (serwoWejscieTrybIcon != nullptr) {
    lv_img_set_src(serwoWejscieTrybIcon, ikonaTrybuMala);
    lv_obj_set_style_img_recolor(serwoWejscieTrybIcon, rgb565(kolorRamki), 0);
    lv_obj_align(serwoWejscieTrybIcon, LV_ALIGN_TOP_RIGHT, -48, 6); // -48: obok oryginalnej ikony suwak (patrz komentarz w kafelek_serwo_wejscie())
  }
}

// [UI-PIEC CO/SR - FINALNY KAFELEK, PRZEPROJEKTOWANY] Polaczony kafelek
// temperatury pieca C.O. + srednia (SR). User po 1. wersji: "kiepsko
// zrobione", "nazwa to Piec C.O. i SR", "ogarnij rozmiar i ikonki".
//
// Zmiany wzgledem 1. wersji:
//  - Etykieta "OGRZ" -> "PIEC CO" (dokladna nazwa tego odczytu, nie skrot).
//  - SR przeniesiony z ciasnego rogu na WLASNY, pelnoprawny wiersz na dole
//    karty (dostepny dzieki wiekszej karcie kh=90 z poprzedniej sesji) -
//    "SR 42.0 C" zamiast "SR" nad "42.0" w rogu. Czytelniejsze, bez ryzyka
//    kolizji (caly wiersz ma dla siebie ~78px szerokosci, wartosc "SR
//    -15.0 C" (najgorszy przypadek zimą) to i tak tylko ok. 55px na foncie
//    14).
//  - Dodana ikona "grzejnik" (nowa, patrz ikony.h) w prawym gornym rogu -
//    dokladnie ten sam wzorzec pozycjonowania co reszta kafelkow (kafelek()
//    z ikona), zamiast pustego miejsca jak w 1. wersji.
//  - Ukladu 3 wierszy (etykieta+ikona / glowna wartosc / SR) mozliwy tylko
//    dzieki wiekszej wysokosci karty (kh=90) - przy starym kh=46 nie byloby
//    miejsca na 3 osobne wiersze, stad prowizoryczny "SR w rogu" w 1.
//    wersji.
static lv_obj_t *kafelek_ogrz_polaczony(lv_obj_t *parent, int x, int y, int w, int h,
                                          lv_obj_t **sredniaWyjscie,
                                          lv_obj_t **kartaWyjscie = nullptr,
                                          lv_obj_t **ikonaWyjscie = nullptr) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, rgb565(KOL_KARTA), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_color(card, rgb565(KOL_ZAR), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);

  // Wiersz 1: etykieta (lewo) + ikona grzejnika (prawo) - identyczny wzorzec
  // jak w kafelek() z ikona (np. BOJLER), zeby caly rzad kafelkow wygladal
  // spojnie, nie jak "brakujacy element" tego jednego.
  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, "PIEC CO");
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 6);

  lv_obj_t *img = lv_img_create(card);
  lv_img_set_src(img, &lv_ikona_grzejnik);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(img, rgb565(KOL_ZAR), 0);
  lv_obj_align(img, LV_ALIGN_TOP_RIGHT, -6, 6);
  // [ROZSZERZENIE, ta sesja - alarm PIEC CO] Uchwyty do karty/ikony, ten sam
  // wzorzec co reszta kafelkow - potrzebne piec_alarm_odswiez().
  if (kartaWyjscie != nullptr) *kartaWyjscie = card;
  if (ikonaWyjscie != nullptr) *ikonaWyjscie = img;

  // Wiersz 2: glowna wartosc (font 28 - kh=90 ma juz na to miejsce, dawne
  // 20 bylo kompromisem tylko dla ciasnej karty 46px).
  lv_obj_t *glowna = lv_label_create(card);
  lv_label_set_text(glowna, "--");
  lv_obj_set_style_text_color(glowna, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_text_font(glowna, &lv_font_montserrat_28, 0);
  lv_obj_align(glowna, LV_ALIGN_TOP_LEFT, 8, 30);

  // Wiersz 3: SR jako pelny, czytelny wiersz na dole karty (nie ciasny rog).
  lv_obj_t *srWiersz = lv_label_create(card);
  lv_label_set_text(srWiersz, "SR --");
  lv_obj_set_style_text_color(srWiersz, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(srWiersz, &lv_font_montserrat_14, 0);
  lv_obj_align(srWiersz, LV_ALIGN_BOTTOM_LEFT, 8, -6);

  if (sredniaWyjscie != nullptr) *sredniaWyjscie = srWiersz;
  return glowna;
}

// [LVGL-DATA-CZAS, ta sesja] Kafelek DATA I CZAS - readonly (bez interakcji,
// tak jak ZEWN/BOJLER/DYM), ostatni wolny slot siatki (rzad 2, obok
// SERWO/DYM, patrz wywolanie w lvgl_dashboard_poc_init()). Ten sam
// trzy-wierszowy uklad co kafelek_ogrz_polaczony() (etykieta / glowna
// wartosc duzym fontem / maly wiersz na dole), ale BEZ ikony w wierszu 1 -
// brak gotowej bitmapy zegara w ikony.h/ikony_lvgl.h (IKONA_CISNIENIE
// wyglada jak tarcza zegara, ale jest juz uzyta na kafelku CISNIENIE -
// uzycie jej tu drugi raz na inny kafelek myliloby, wiec kafelek jest czysto
// tekstowy, do uzupelnienia wlasna ikona pozniej jesli zechcesz).
// Godzina:minuta (font 28, najbardziej rzucajaca sie w oczy wartosc - to,
// co user najczesciej chce wiedziec jednym rzutem oka) jako GLOWNA wartosc,
// data (font 14, dim) jako maly wiersz na dole - dokladnie ten sam wzorzec co
// "SR --" w kafelek_ogrz_polaczony() powyzej.
static void kafelek_data_czas(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, rgb565(KOL_KARTA), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_color(card, rgb565(KOL_RAMKA), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, "DATA I CZAS");
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 6);

  lv_obj_t *czas = lv_label_create(card);
  lv_label_set_text(czas, "--:--");
  lv_obj_set_style_text_color(czas, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_text_font(czas, &lv_font_montserrat_28, 0);
  lv_obj_align(czas, LV_ALIGN_TOP_LEFT, 8, 30);
  dataCzasCzasLabel = czas;

  lv_obj_t *data = lv_label_create(card);
  lv_label_set_text(data, "--.--.----");
  lv_obj_set_style_text_color(data, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(data, &lv_font_montserrat_14, 0);
  lv_obj_align(data, LV_ALIGN_BOTTOM_LEFT, 8, -6);
  dataCzasDataLabel = data;
}

// [ROZSZERZENIE, ta sesja] Kafelek DYM - dedykowana funkcja (nie generyczny
// kafelek()) bo user chcial DYNAMICZNY PASEK pod wartoscia liczbowa, ktorego
// zwykly kafelek() nie ma miejsca/wsparcia. Ustawia statyczne uchwyty pliku
// (dymCard/dymIcon/dymValLabel/dymBar) BEZPOSREDNIO - ten sam wzorzec co
// kafelek_pompa() (void, zero out-parametrow), bo to jednorazowy, w pelni
// wyspecjalizowany kafelek, nie generyczny szablon uzywany w wielu miejscach.
//
// Pionowy uklad w kh=90: etykieta+ikona (y=6, ikona 40x40 -> dol=46),
// wartosc (y=48, font28 -> dol=78), pasek (y=80, wys=6 -> dol=86, 4px
// marginesu do dolu karty) - 4px mniej niz oryginalne 8px, ale nadal
// bezpieczny margines, sprawdzony na 90px wysokosci karty.
//
// Pasek to lv_bar (nie lv_slider jak w panelu DYM) - tu nie potrzeba
// przeciagania palcem, tylko WIZUALNEJ dlugosci/koloru. Zakres 0-100
// (procent progu alarmowego), wartosc/kolor liczone w dym_ikona_odswiez()
// (patrz nizej) - PRZELICZANE W LOCIE wzgledem aktualnego g_dymProg (w
// odroznieniu od suwaka w PANELU DYM, ktory ma zakres STALY - user
// wczesniej doprecyzowal: "w glownym menu przeliczony w locie ale przy
// ustawieniu raczej staly").
static void kafelek_dym(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, rgb565(KOL_KARTA), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_set_style_border_color(card, rgb565(KOL_RAMKA), 0); // startowo neutralna, dym_ikona_odswiez() nadpisze po 1. aktualizuj()
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, "DYM");
  lv_obj_set_style_text_color(lbl, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 6);

  lv_obj_t *img = lv_img_create(card);
  lv_img_set_src(img, &lv_ikona_bezpieczna); // startowa bitmapa, dym_ikona_odswiez() ja nadpisze wg. stanu
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(img, rgb565(KOL_BEZPIECZNA), 0);
  lv_obj_align(img, LV_ALIGN_TOP_RIGHT, -4, 6);
  dymIcon = img;

  lv_obj_t *val = lv_label_create(card);
  lv_label_set_text(val, "--");
  lv_obj_set_style_text_color(val, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
  lv_obj_align(val, LV_ALIGN_TOP_LEFT, 8, 48);
  dymValLabel = val;

  // [FIX tej sesji] Poprzednia wersja to byl pojedynczy lv_bar - przy niskich
  // wartosciach (np. dym=12 na progu=500, czyli ~2%) wypelnienie jest
  // wizualnie nieodrozniale od pustego tla (oba w praktyce "prawie nic"),
  // wiec user nie widzial ANI zoltej/czerwonej strefy (bo do niej daleko),
  // ANI tego, jak daleko faktycznie jest do progu - caly pasek wygladal jak
  // plaska szara kreska.
  //
  // Nowe podejscie: STATYCZNY pasek stref w tle (zielona 0-70%, bursztynowa
  // 70-100% - te same granice co "uwaga" w dym_ikona_odswiez(), czyli
  // (g_dymProg*7)/10) + WSKAZNIK pozycji (cienka pionowa kreska) jezdzacy po
  // tym tle wg. aktualnego procentu. Dzieki temu skala (gdzie jest 70%,
  // gdzie 100%=alarm) jest widoczna ZAWSZE, a nie tylko wtedy, gdy pasek
  // akurat tam siega - user widzi na pierwszy rzut oka, jak duzy jest zapas.
  int barW = w - 16;
  lv_obj_t *barBg = lv_obj_create(card);
  lv_obj_remove_style_all(barBg);
  lv_obj_set_size(barBg, barW, 6);
  lv_obj_align(barBg, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_clear_flag(barBg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(barBg, 3, 0);
  lv_obj_set_style_clip_corner(barBg, true, 0);
  lv_obj_set_style_bg_opa(barBg, LV_OPA_TRANSP, 0); // sam kontener bez wlasnego tla - tlo daja 2 strefy ponizej

  // Strefa 0-70% (zielona/spokojna) - stala szerokosc, NIE przeliczana per-klatke.
  int zonaOkSzer = (barW * 70) / 100;
  lv_obj_t *zonaOk = lv_obj_create(barBg);
  lv_obj_remove_style_all(zonaOk);
  lv_obj_set_size(zonaOk, zonaOkSzer, 6);
  lv_obj_set_pos(zonaOk, 0, 0);
  lv_obj_set_style_bg_color(zonaOk, lv_color_darken(rgb565(KOL_BEZPIECZNA), 70), 0); // przygaszona zielen (ten sam kolor co ikona OK, tylko ciemniejszy)
  lv_obj_set_style_bg_opa(zonaOk, LV_OPA_COVER, 0);

  // Strefa 70-100% (bursztynowa/uwaga) - reszta szerokosci paska.
  lv_obj_t *zonaUwaga = lv_obj_create(barBg);
  lv_obj_remove_style_all(zonaUwaga);
  lv_obj_set_size(zonaUwaga, barW - zonaOkSzer, 6);
  lv_obj_set_pos(zonaUwaga, zonaOkSzer, 0);
  lv_obj_set_style_bg_color(zonaUwaga, lv_color_darken(rgb565(KOL_WARN), 70), 0); // przygaszony bursztyn (ten sam kolor co ikona UWAGA, tylko ciemniejszy)
  lv_obj_set_style_bg_opa(zonaUwaga, LV_OPA_COVER, 0);

  // Wskaznik pozycji - waska pionowa kreska, WYSTAJACA ponad/pod tlo stref
  // (10px vs 6px tla) zeby byla widoczna nawet na granicy 0%, gdzie inaczej
  // znikalaby w rogu. Kolor nadpisywany w dym_ikona_odswiez() (ten sam co
  // ikona/ramka), pozycja x=0 dla dym=0, wiec zawsze widoczna nawet przy
  // wartosci zerowej/bliskiej zeru.
  lv_obj_t *wskaznik = lv_obj_create(barBg);
  lv_obj_remove_style_all(wskaznik);
  lv_obj_set_size(wskaznik, 3, 10);
  lv_obj_set_pos(wskaznik, 0, -2);
  lv_obj_set_style_radius(wskaznik, 1, 0);
  lv_obj_set_style_bg_color(wskaznik, rgb565(KOL_BEZPIECZNA), 0); // dym_ikona_odswiez() nadpisze wg. stanu
  lv_obj_set_style_bg_opa(wskaznik, LV_OPA_COVER, 0);
  dymBar = wskaznik;
  dymBarSzer = barW;

  dymCard = card;
}

void lvgl_dashboard_poc_init() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, rgb565(KOL_TLO), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // [UI-WIFI-SYGNAL, ta sesja] Staly napis "PIEC CO - LVGL proof of concept"
  // zastapiony graficznym wskaznikiem sygnalu WiFi (decyzja: sama ikona, bez
  // tekstu, na srodku naglowka). 7 kresek rosnacej wysokosci, wyrownane do
  // dolu (klasyczny ksztalt "signal bars"). Kontener bez wlasnego tla/
  // przewijania - same kreski sa jego dziecmi, pozycjonowane recznie wg.
  // indeksu. Rozmiar kontenera: 7*4 (kreski) + 6*2 (odstepy) = 40px szer.,
  // 18px wys. (najwyzsza kreska 16px + margines).
  static const int WIFI_LICZBA_KRESEK = 7;
  static const int WIFI_SZER_KRESKI = 4;
  static const int WIFI_ODSTEP_KRESEK = 2;
  static const int WIFI_WYS_MIN = 4;
  static const int WIFI_WYS_KROK = 2;
  static const int WIFI_WYS_KONTENERA = 18;

  lv_obj_t *wifiKontener = lv_obj_create(scr);
  lv_obj_remove_style_all(wifiKontener);
  lv_obj_set_size(wifiKontener,
                   WIFI_LICZBA_KRESEK * WIFI_SZER_KRESKI + (WIFI_LICZBA_KRESEK - 1) * WIFI_ODSTEP_KRESEK,
                   WIFI_WYS_KONTENERA);
  lv_obj_set_style_bg_opa(wifiKontener, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(wifiKontener, LV_OBJ_FLAG_SCROLLABLE);
  // [FIX, ta sesja] User: "to powinno byc w prawym rogu" - przeniesione z
  // gory-srodka do gory-prawego rogu naglowka (jak wskaznik sygnalu w
  // telefonie). -8 od prawej krawedzi ekranu, ten sam y=6 co poprzednio.
  lv_obj_align(wifiKontener, LV_ALIGN_TOP_RIGHT, -8, 6);

  for (int i = 0; i < WIFI_LICZBA_KRESEK; i++) {
    int wys = WIFI_WYS_MIN + i * WIFI_WYS_KROK; // 4,6,8,10,12,14,16
    lv_obj_t *kreska = lv_obj_create(wifiKontener);
    lv_obj_remove_style_all(kreska);
    lv_obj_set_size(kreska, WIFI_SZER_KRESKI, wys);
    lv_obj_set_style_radius(kreska, 1, 0);
    lv_obj_set_style_bg_opa(kreska, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(kreska, rgb565(KOL_TEKST_DIM), 0); // startowo wyszarzona, wifi_ikona_odswiez() nadpisze po 1. aktualizuj()
    lv_obj_set_pos(kreska, i * (WIFI_SZER_KRESKI + WIFI_ODSTEP_KRESEK), WIFI_WYS_KONTENERA - wys); // dolne wyrownanie
    wifiPasek[i] = kreska;
  }

  // Symbol "brak WiFi" - nad kreskami (wyszarzonymi), ukryty dopoki
  // wifi_ikona_odswiez() nie ustali polaczono==false. Osobny obiekt na scr
  // (nie w wifiKontener), zeby jego wyrownanie nie zalezalo od jego
  // rodzica-kontenera bez wlasnego tla.
  wifiX = lv_label_create(scr);
  lv_label_set_text(wifiX, "X");
  lv_obj_set_style_text_color(wifiX, rgb565(KOL_ERR), 0);
  // [FIX, ta sesja] Wyrownanie WZGLEDEM wifiKontener (nie wzgledem scr) -
  // dzieki temu X zawsze zostaje wysrodkowany nad kreskami niezaleznie od
  // tego, gdzie stoi kontener (poprzednio oba na sztywno TOP_MID scr,
  // teraz kontener jest w prawym rogu - X musi za nim podazac).
  lv_obj_align_to(wifiX, wifiKontener, LV_ALIGN_TOP_MID, 0, -2);
  lv_obj_add_flag(wifiX, LV_OBJ_FLAG_HIDDEN);

  // [UI-INTERNET, ta sesja] Globus (dostepnosc internetu, fbAppReady) - tuz
  // na lewo od wskaznika sygnalu WiFi, ten sam pasek naglowka. Dwie
  // rozlaczne warstwy 16x16 (siatka+lad) w tym samym miejscu na ekranie -
  // dokladnie ten sam trik co BOJLER (kontur+woda)/PANEL (slonce+kolektor),
  // tylko obie warstwy przebarwiane RAZEM (nie osobno) w
  // internet_ikona_odswiez(), bo tu nie ma stanu "czesciowo aktywny".
  netGlobusSiatka = lv_img_create(scr);
  lv_img_set_src(netGlobusSiatka, &lv_ikona_globus_siatka);
  lv_obj_set_style_img_recolor_opa(netGlobusSiatka, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(netGlobusSiatka, rgb565(KOL_GLOBUS_SIATKA), 0);
  // 8px odstep od lewej krawedzi wifiKontener, wysrodkowane pionowo wzgledem
  // paska WiFi (WIFI_WYS_KONTENERA=18px, globus=16px -> 1px marginesu).
  lv_obj_align_to(netGlobusSiatka, wifiKontener, LV_ALIGN_OUT_LEFT_MID, -8, 0);

  netGlobusLady = lv_img_create(scr);
  lv_img_set_src(netGlobusLady, &lv_ikona_globus_lady);
  lv_obj_set_style_img_recolor_opa(netGlobusLady, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(netGlobusLady, rgb565(KOL_GLOBUS_LADY), 0);
  // Dokladnie na wierzchu siatki (ta sama pozycja, obie warstwy razem tworza
  // jeden obrazek globusu) - wyrownanie wzgledem netGlobusSiatka, nie scr.
  lv_obj_align_to(netGlobusLady, netGlobusSiatka, LV_ALIGN_TOP_LEFT, 0, 0);

  // Symbol "brak internetu" - ten sam mechanizm co wifiX, wyrownany
  // wzgledem globusu (nie scr), zeby podazal za nim niezaleznie od pozycji.
  netX = lv_label_create(scr);
  lv_label_set_text(netX, "X");
  lv_obj_set_style_text_color(netX, rgb565(KOL_ERR), 0);
  lv_obj_align_to(netX, netGlobusSiatka, LV_ALIGN_TOP_MID, 0, -2);
  lv_obj_add_flag(netX, LV_OBJ_FLAG_HIDDEN);

  // [UI-BATERIA, ta sesja] Ikonka baterii - lustrzane odbicie wskaznika WiFi
  // (ten sam naglowek, przeciwny rog). Obrys (border, bg_opa=TRANSP) +
  // wewnetrzne wypelnienie (dziecko, startowa szerokosc 0 - pierwsza
  // lvgl_dashboard_poc_aktualizuj_bateria() ja ustawi, patrz nizej w tym
  // pliku) + "nib" (biegun +, po prawej stronie obrysu) + etykieta
  // procentowa. BAT_RAMKA sluzy podwojnie - grubosc obrysu ORAZ margines
  // wypelnienia od krawedzi (patrz maxSzer w funkcji aktualizujacej).
  lv_obj_t *bateriaKontener = lv_obj_create(scr);
  lv_obj_remove_style_all(bateriaKontener);
  lv_obj_set_size(bateriaKontener, BAT_SZER, BAT_WYS);
  lv_obj_set_style_bg_opa(bateriaKontener, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(bateriaKontener, rgb565(KOL_TEKST), 0);
  lv_obj_set_style_border_width(bateriaKontener, BAT_RAMKA, 0);
  lv_obj_set_style_border_opa(bateriaKontener, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bateriaKontener, 2, 0);
  lv_obj_clear_flag(bateriaKontener, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(bateriaKontener, LV_ALIGN_TOP_LEFT, 8, 6);

  bateriaWypelnienie = lv_obj_create(bateriaKontener);
  lv_obj_remove_style_all(bateriaWypelnienie);
  lv_obj_set_size(bateriaWypelnienie, 0, BAT_WYS - 2 * BAT_RAMKA);
  lv_obj_set_pos(bateriaWypelnienie, BAT_RAMKA, BAT_RAMKA);
  lv_obj_set_style_bg_opa(bateriaWypelnienie, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bateriaWypelnienie, rgb565(KOL_TEKST_DIM), 0); // startowo wyszarzona, jak kreski WiFi

  lv_obj_t *bateriaNib = lv_obj_create(scr);
  lv_obj_remove_style_all(bateriaNib);
  lv_obj_set_size(bateriaNib, 2, BAT_WYS / 2);
  lv_obj_set_style_bg_opa(bateriaNib, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bateriaNib, rgb565(KOL_TEKST), 0);
  lv_obj_align_to(bateriaNib, bateriaKontener, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

  bateriaEtykieta = lv_label_create(scr);
  lv_label_set_text(bateriaEtykieta, "--");
  lv_obj_set_style_text_color(bateriaEtykieta, rgb565(KOL_TEKST_DIM), 0);
  lv_obj_align_to(bateriaEtykieta, bateriaNib, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

  // [v1.2.0 CENTRALA-ID] Srodek naglowka jest wolny od czasu usuniecia napisu
  // "PIEC CO - LVGL proof of concept" (patrz UI-WIFI-SYGNAL wyzej): bateria
  // konczy sie ok. x=75, globus zaczyna ok. x=400. Najdluzszy znacznik, jaki
  // sklada diag::znacznikCentrali() ("SYMULACJA: zewn +9  2 CENTRALE (BB:CC)"),
  // zajmuje fontem 14 x=87..393 (zmierzone w testy_host, scenariusz
  // gola_plytka) - miesci sie, ale bez zapasu: nie wydluzac tego tekstu.
  // Ukryty, dopoki main.cpp nie poda niepustego tekstu.
  centralaZnacznikLabel = lv_label_create(scr);
  lv_label_set_text(centralaZnacznikLabel, "");
  lv_obj_set_style_text_color(centralaZnacznikLabel, rgb565(KOL_WARN), 0);
  lv_obj_align(centralaZnacznikLabel, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_add_flag(centralaZnacznikLabel, LV_OBJ_FLAG_HIDDEN);

  // [UI-SIATKA, PELNY EKRAN] User: "wywnioskuj wielkosc kafelkow, zeby objac
  // caly ekran" - policzone tak:
  //   - SZEROKOSC: bez zmian wzgledem main.cpp (juz idealnie pasuje).
  //     4 kolumny * 114px + 3 odstepy * 4px + 2 marginesy * 6px =
  //     456 + 12 + 12 = 480 = dokladnie szerokosc ekranu (0px straty).
  //   - WYSOKOSC: max. jednoczesnie widocznych kafelkow to 12 (najgorszy
  //     przypadek: zawsze-widoczne 5 [ZEWN/POKOJ/WILGOTNOSC/CISNIENIE/
  //     GODZINA] + grupa "piec aktywny" 6 [BOJLER/OGRZ+SR/KLAPA/SYBERKA/
  //     DYM/POMPA] + PANEL w lecie gdy piec>30 = 5+6+1=12) -> dokladnie
  //     4 kolumny x 3 rzedy = 12 slotow, zero pustych/nadmiarowych.
  //     320 (wys. ekranu) = y0(30, nagłowek) + 3*kh + 2*odstepY(6) + zapas.
  //     3*kh = 320-30-12 = 278 -> kh=92 (uzywamy 90 dla okraglej wartosci,
  //     zostaje ~8px marginesu na dole zamiast wychodzic idealnie na styk).
  //   Poprzednio kh=46 (o polowe mniejsze) zostawialo ~140px czarnej,
  //   niewykorzystanej przestrzeni pod kafelkami - dokladnie to widac na
  //   zdjeciu z telefonu (kafelki stloczone u gory, dol ekranu pusty).
  const int x0 = 6, y0 = 30, kw = 114, kh = 90, odstepX = 4, odstepY = 6;

  // [UI-ZEWN, FIX tej sesji] Ikona startuje na "neutralnie" (placeholder do
  // 1. realnej aktualizacji) i jest przelaczana w runtime w aktualizuj() wg.
  // progow z ikony.h. Font 20 zostal USUNIETY - byl to odlog z czasow
  // ciasnej karty kh=46 (patrz historia, punkt 18: przy foncie 28 "-15.0 C"
  // mialo nachodzic na ikone 20px w rogu). Ten powod juz nie istnieje: przy
  // obecnym kh=90 wartosc siedzi w DOLNYM rzedzie karty (LV_ALIGN_BOTTOM_LEFT),
  // a ikona w GORNYM (LV_ALIGN_TOP_RIGHT) - te dwa elementy dziela sie na
  // dwa rozne piony i fizycznie nie moga na siebie nachodzic bez wzgledu na
  // szerokosc tekstu. Efekt uboczny nieusuniecia tego wtedy: ZEWN byl
  // JEDYNYM kafelkiem z mniejsza czcionka wartosci niz reszta (widoczne na
  // zdjeciu z LCD - "3.5 C" wyraznie drobniejsze od "52.0 C"/"45.0 C").
  // Teraz brak override -> kafelek() sam ustawia font 28 + wyrownanie -4,
  // dokladnie jak BOJLER/DYM/PIEC CO - caly rzad wyglada spojnie.
  // [Partia 5, ta sesja] Przeukladanie calej siatki na zyczenie usera:
  // "najpierw temp zew, pozniej wew, wilgotnosc, cisnienie, piec, serwo" +
  // "boiler przed piecem i panel przed piecem, potem pompa i dym". [FIX tej
  // sesji] User pozniej doprecyzowal: "po piecu ma byc pompa" - POMPA
  // przeniesiona z rzedu 2 na koniec rzedu 1 (zaraz po PIEC), KLAPA/SYBERKA/
  // DYM przesuniete o jeden slot w dol. Aktualna kolejnosc (row-major, 4
  // kolumny x 3 rzedy):
  //   rzad 0: ZEWN, POKOJ, WILGOTNOSC, CISNIENIE   (4 zawsze-widoczne odczyty)
  //   rzad 1: BOJLER, PANEL, PIEC CO/SR, POMPA
  //   rzad 2: SERWO KLAPA, SERWO SYBERKA, DYM
  // Wspolrzedne x0/y0/y1/y2 licza sie identycznie jak wczesniej (kw/kh/odstep
  // bez zmian) - zmienia sie tylko KTORY kafelek trafia w ktory slot, nie
  // sama geometria siatki.
  const int y1 = y0 + kh + odstepY;
  const int y2 = y1 + kh + odstepY;

  // --- rzad 0: ZEWN, POKOJ, WILGOTNOSC, CISNIENIE ---
  zewnValLabel = kafelek(scr, x0 + 0 * (kw + odstepX), y0, kw, kh, "ZEWN", "--", KOL_RAMKA, &lv_ikona_zewn_neutralny, KOL_ZEWN_NEUTRALNY, &zewnIcon);

  // [FIX, ta sesja - "chce miec logo pokoju kolorowe"] Dwie warstwy zamiast
  // jednej jednokolorowej ikony - dach w KOL_POKOJ_DACH (braz/terakota),
  // termometr w KOL_POKOJ (koralowy, jak dotad) - ten sam wzorzec co BOJLER
  // (kontur+woda) i PANEL (slonce+kolektor) wyzej w tym pliku.
  pokojValLabel = kafelek(scr, x0 + 1 * (kw + odstepX), y0, kw, kh, "POKOJ", "--", KOL_RAMKA,
                           &lv_ikona_pokoj_dach, KOL_POKOJ_DACH, nullptr,
                           &lv_ikona_pokoj_termometr, KOL_POKOJ);

  // WILGOTNOSC - kropla (droplet) to uniwersalny, miedzynarodowy symbol
  // wilgotnosci (research potwierdzony - ten sam symbol na pilotach klimy w
  // trybie "osuszanie" i w czujnikach pogodowych), a IKONA_KROPLA juz
  // istnieje (uzyta na warstwie wody w BOJLERZE) - zero nowej bitmapy.
  // Kolor KOL_KROPLA (niebieski), ten sam co warstwa wody w BOJLERZE - ten
  // sam kolor = ta sama domena "woda/wilgoc" w calej siatce.
  // [Partia 3, ta sesja] User: "kropla troche mniejsza musi byc". NIE
  // lv_img_set_zoom() - format LV_IMG_CF_ALPHA_1BIT po cichu ignoruje
  // zoom/angle w LVGL v8 (patrz "Wazne ograniczenie LVGL v8" w
  // STATUS_PROJEKTU_LVGL.md, znalezione juz raz przy animacji wirnika
  // POMPY). Zamiast tego osobna bitmapa 32px (lv_ikona_kropla_mala,
  // ikony.h/ikony_lvgl.h) - ten sam ksztalt, wygenerowany tym samym
  // procesem (LANCZOS + prog binarny) ze zrodla 40px. Wilgotnosc dostaje
  // uchwyt, bo mniejsza ikona przesuwa sie nieco w kafelku (realign po
  // stworzeniu - kafelek() wyrownuje wg. rozmiaru zrodlowego bitmapy, tu
  // juz 32px, wiec pojedynczy align wystarcza, bez dodatkowego kroku).
  wilgotnoscValLabel = kafelek(scr, x0 + 2 * (kw + odstepX), y0, kw, kh, "WILGOTNOSC", "--", KOL_RAMKA, &lv_ikona_kropla_mala, KOL_KROPLA, &wilgotnoscIkonaUchwyt);
  // [Partia 4, ta sesja] User na zywym sprzecie: "w wilgotnosci kropelka
  // nizej bo wchodzi na napis" - etykieta "WILGOTNOSC" (10 znakow, font 14)
  // jest dosc szeroka, zeby jej prawy koniec siegal pod domyslne
  // wyrownanie ikony (TOP_RIGHT, y=6 - ten sam wiersz co etykieta w
  // kafelek()). Realign w dol, POD wiersz etykiety, zamiast obok niego.
  if (wilgotnoscIkonaUchwyt != nullptr) {
    lv_obj_align(wilgotnoscIkonaUchwyt, LV_ALIGN_TOP_RIGHT, -4, 22);
  }

  // [Partia 3, ta sesja] Byly KOL_TEKST_DIM (placeholder-szary z Partii 2) -
  // user: "ikonka jest szara ... powinno byc w kolorach nie szare". Kazdy
  // dostaje teraz wlasny kolor domeny (KOL_CISNIENIE/KOL_POKOJ, patrz wyzej).
  // CISNIENIE dodatkowo przesuniete kilka px w dol (user: "odrobine w dul") -
  // manometr wizualnie "unosil sie" za blisko gornej krawedzi karty.
  cisnienieValLabel = kafelek(scr, x0 + 3 * (kw + odstepX), y0, kw, kh, "CISNIENIE", "--", KOL_RAMKA, &lv_ikona_cisnienie, KOL_CISNIENIE, &cisnienieIkonaUchwyt);
  if (cisnienieIkonaUchwyt != nullptr) {
    lv_obj_align(cisnienieIkonaUchwyt, LV_ALIGN_TOP_RIGHT, -4, 12);
  }
  // [Partia 4, ta sesja] Mniejszy font (20 zamiast domyslnych 28 z
  // kafelek()) TYLKO dla wartosci CISNIENIE - "1013 hPa" przy foncie 28 nie
  // miescilo sie w karcie (patrz komentarz przy formatowaniu w
  // lvgl_dashboard_poc_aktualizuj()). cisnienieValLabel to wskaznik
  // zwrocony przez kafelek() (val label), wiec zwykly override stylu po
  // stworzeniu - bez zmian w samej funkcji kafelek().
  lv_obj_set_style_text_font(cisnienieValLabel, &lv_font_montserrat_20, 0);

  // --- rzad 1: BOJLER, PANEL, PIEC CO/SR, POMPA ---
  // [24] BOJLER: v1 (jeden kolor, cienka fala) byla nieczytelna na LCD.
  // v2 - dwuwarstwowa ikona 32x32: kontur (pomaranczowy) + lite wypelnienie
  // wody (niebieskie, KOL_KROPLA) - patrz STATUS_PROJEKTU_LVGL.md pkt 24.
  bojlerValLabel = kafelek(scr, x0 + 0 * (kw + odstepX), y1, kw, kh, "BOJLER", "--", KOL_RAMKA,
                            &lv_ikona_bojler_kontur, KOL_ZAR, nullptr,
                            &lv_ikona_bojler_woda, KOL_KROPLA);

  // [Partia 5, ta sesja] PANEL (t_panel) - nowy kafelek, patrz deklaracja
  // panelValLabel wyzej. [FIX tej sesji] User: "panel to jest panel grzewczy
  // na slonce ktory podgrzewa wode" - to kolektor sloneczny, NIE zrodlo
  // ognia, wiec ikona plomienia (dzielona z BOJLER/PIEC) byla mylaca.
  // Wlasna ikona (slonce nad przechylonym kolektorem, patrz
  // ikony.h/ikony_lvgl.h) - kolor zostaje KOL_ZAR (ten sam "cieply" kolor co
  // reszta kafelkow grzewczych, tylko zrodlo cieplna sie zmienilo). Zawsze
  // widoczny (podzial sezonowy usuniety) - stad brak lv_obj_get_parent()/
  // *Card ponizej, bo nic juz nie musi go chowac.
  // [ROZSZERZENIE, ta sesja - alarm PANEL] Dopisane uchwyty ikony i karty
  // (poprzednio oba nullptr/pomijane) - potrzebne panel_alarm_odswiez(), zeby
  // przebarwic ikone/ramke na czerwono przy alarm_panel z centrali.
  // [UI-PANEL-KOLOR, ta sesja] User: "slonce zulto reszta jak jest" -
  // dwuwarstwowa ikona jak BOJLER (patrz kafelek() wyzej). Pierwsza warstwa
  // (ikona/panelIcon) to teraz TYLKO kolektor - dalej ten sam uchwyt, wiec
  // panel_alarm_odswiez() dalej przebarwia go na KOL_ERR przy alarmie bez
  // zadnych zmian w tamtej funkcji. Druga warstwa (ikona2) to TYLKO slonce,
  // na stale KOL_SLONCE (zolty) - nie ma wlasnego uchwytu, wiec alarm jej
  // nie dotyka, zgodnie z prosba "reszta jak jest" (reszta = kolektor nadal
  // reaguje na alarm jak dotychczas, samo slonce zawsze zolte).
  panelValLabel = kafelek(scr, x0 + 1 * (kw + odstepX), y1, kw, kh, "PANEL", "--", KOL_RAMKA, &lv_ikona_panel_kolektor, KOL_ZAR, &panelIcon, &lv_ikona_panel_slonce, KOL_SLONCE, &panelCard);

  // [UI-PIEC CO/SR] Byl OGRZ_TILE_PREVIEW - Wariant A wygral porownanie na
  // zywo (patrz obszerny komentarz przy kafelek_ogrz_polaczony() powyzej),
  // teraz staly element siatki, zasilany realnymi danymi tOgrz/tOgrzSr
  // zamiast demo-tekstu "45.0 C"/"SR 42.0 C". Zawsze widoczny.
  ogrzGlownaLabel = kafelek_ogrz_polaczony(scr, x0 + 2 * (kw + odstepX), y1, kw, kh, &ogrzSredniaLabel, &ogrzCard, &ogrzIcon);

  // [FIX tej sesji] POMPA przeniesiona tutaj (koniec rzedu 1, zaraz po PIEC
  // CO/SR) - user: "po piecu ma byc pompa". Wlasny wzorzec budowy
  // (kafelek_pompa(), nie generyczny kafelek()) bez zmian, zmienia sie tylko
  // pozycja x/y wywolania.
  kafelek_pompa(scr, x0 + 3 * (kw + odstepX), y1, kw, kh);

  // --- rzad 2: SERWO (wejscie do podmenu, x2 szerokosc), DYM ---
  // [ROZSZERZENIE, ta sesja] Byly to DWA osobne kafelki (SERWO KLAPA, SERWO
  // SYBERKA) zajmujace 2 sloty siatki - user: "na glownym menu jedna ikona
  // do serwa, dopiero w podmenu po kliknieciu na wybrane pojawia sie opcje
  // dla danego". Zastapione JEDNYM kafelkiem wejsciowym
  // (kafelek_serwo_wejscie(), patrz definicja wyzej w pliku) - klika sie w
  // niego, zeby przejsc na serwoWyborScr, gdzie dopiero widac dwie osobne
  // karty KLAPA/SYBERKA (dokladnie te same kafelek_serwo() co poprzednio
  // tutaj, tylko z innym rodzicem - patrz serwo_wybor_zbuduj()).
  // [FIX, ta sesja] User: "ikona serwo ma miec wielkosc dwoch ikonek, bo
  // teraz nie ma wartosci a powinny byc na pierwszym ekranie". Rzad 2 mial
  // od poczatku tylko 2 z 4 slotow zajete (SERWO+DYM, 2 sloty zawsze byly
  // wolne - patrz komentarz przy [UI-SIATKA] wyzej: "12 slotow, zero
  // pustych" liczyl STARY plan sprzed tego FIX-u, ktory sie nie zmaterializowal
  // 1:1), wiec kafelek SERWO rozciagniety na 2*kw+odstepX (sloty 0-1) BEZ
  // przesuwania czegokolwiek poza DYM - zero utraconej przestrzeni, tylko
  // wykorzystanie tej, ktora i tak stala pusta. DYM przesuniety o jeden
  // slot w prawo (byl slot1, teraz slot2), zeby zrobic miejsce.
  kafelek_serwo_wejscie(scr, x0 + 0 * (kw + odstepX), y2, 2 * kw + odstepX, kh);

  kafelek_dym(scr, x0 + 2 * (kw + odstepX), y2, kw, kh);

  // [LVGL-DATA-CZAS, ta sesja] Ostatni, dotychczas pusty slot siatki (rzad 2,
  // slot 3) - patrz komentarz przy kafelek_serwo_wejscie() wyzej co do tego,
  // ze rzad 2 mial od poczatku tylko 3 z 4 slotow zajete (SERWO x2 + DYM).
  kafelek_data_czas(scr, x0 + 3 * (kw + odstepX), y2, kw, kh);

  // [ROZSZERZENIE, ta sesja - PANEL DYM] Kafelek DYM staje sie klikalny -
  // dokladnie ten sam "dopisz clickable+press-flash+event do juz istniejacej
  // karty" wzorzec jak przy kafelek_serwo_wejscie(). Kolor ramki NADAL
  // sterowany przez dym_ikona_odswiez() (wolane z aktualizuj()) -
  // press_flash_zainstaluj zmienia tylko chwilowy kolor TLA przy dotknieciu,
  // nie ramki, wiec obie logiki nie koliduja.
  lv_obj_add_flag(dymCard, LV_OBJ_FLAG_CLICKABLE);
  press_flash_zainstaluj(dymCard, KOL_KARTA, 3);
  lv_obj_add_event_cb(dymCard, dym_kafelek_klikniety_cb, LV_EVENT_CLICKED, nullptr);

  // [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Kafelki PIEC CO i
  // PANEL staja sie klikalne, ten sam wzorzec co DYM powyzej - oba otwieraja
  // WSPOLNY panel (alarm_temp_panel_otworz()), bo maja jeden wspolny prog na
  // centrali. Kolor ramki nadal sterowany przez piec_alarm_odswiez()/
  // panel_alarm_odswiez() (patrz aktualizuj()) - press_flash zmienia tylko
  // chwilowy kolor TLA, nie ramki, bez kolizji.
  lv_obj_add_flag(ogrzCard, LV_OBJ_FLAG_CLICKABLE);
  press_flash_zainstaluj(ogrzCard, KOL_KARTA, 3);
  lv_obj_add_event_cb(ogrzCard, piec_kafelek_klikniety_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_add_flag(panelCard, LV_OBJ_FLAG_CLICKABLE);
  press_flash_zainstaluj(panelCard, KOL_KARTA, 3);
  lv_obj_add_event_cb(panelCard, panel_kafelek_klikniety_cb, LV_EVENT_CLICKED, nullptr);

  pompa_panel_zbuduj(scr);
  pompa_ustawienia_zbuduj(scr);
  // [FIX, ta sesja] serwo_wybor_zbuduj(scr) USUNIETE z inicjalizacji -
  // posredni ekran "SERWO - WYBIERZ OS" nie jest juz w ogole budowany (nie
  // tylko pominiety w nawigacji). serwoWyborScr zostaje nullptr, co jest
  // bezpieczne: klapaValLabel/klapaCard/klapaRecznieIcon (i syberka-
  // odpowiedniki), jedyne rzeczy tworzone PRZEZ serwo_wybor_zbuduj(),
  // zostaja tez nullptr - a kazde ich uzycie w pliku (serwo_tile_odswiez(),
  // lvgl_dashboard_poc_aktualizuj()) jest juz zabezpieczone sprawdzeniem
  // na nullptr, wiec nic sie nie wywali. Funkcja serwo_wybor_zbuduj() i
  // powiazane callbacki (serwo_wybor_otworz_cb/zamknij_cb/idle_*) zostaja
  // w pliku jako martwy kod - do usuniecia w oddzielnym kroku porzadkowym,
  // jesli chcesz; teraz nie sa wolane wiec nie wplywaja na dzialanie.
  serwo_panel_zbuduj(scr);
  serwo_ustawienia_zbuduj(scr);
  dym_panel_zbuduj(scr);
  alarm_temp_panel_zbuduj(scr);
  alarm_ekran_zbuduj(scr); // [EKRAN ALARMOWY, ta sesja] patrz PLAN_EKRAN_ALARMOWY.md

  // [PARTIA 5a, ta sesja - EKRAN WYKRESY] `scr` (dashboard) musi byc
  // CLICKABLE, zeby dostac zdarzenia gestu na PUSTYM tle (miedzy kartami,
  // ktore same przez lv_obj_create() NIE sa clickable). Nad kafelkami,
  // ktore SA clickable (POMPA/PANEL/PIEC/DYM/SERWO), gest i tak dociera
  // tutaj przez domyslnie wlaczona LV_OBJ_FLAG_GESTURE_BUBBLE - patrz
  // obszerny komentarz przy dashboard_gesture_cb() wyzej.
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, dashboard_gesture_cb, LV_EVENT_GESTURE, nullptr);
  wykresy_panel_zbuduj(scr);
  drugi_pulpit_zbuduj(scr);
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Musi byc PO
  // drugi_pulpit_zbuduj(scr) - kafelek WYSWIETLACZ zbudowany tam juz
  // odwoluje sie do wyswietlacz_kafelek_klikniety_cb(), ale samo zbudowanie
  // EKRANU panelu nie zalezy od kolejnosci wzgledem drugiego pulpitu (ten
  // sam brak zaleznosci co wykresy_panel_zbuduj() vs drugi_pulpit_zbuduj()
  // powyzej - kolejnosc tych dwoch linii tez mozna by odwrocic bez zmiany
  // zachowania).
  wyswietlacz_panel_zbuduj(scr);
}

void lvgl_dashboard_poc_ustaw_callback_pompy(PompaAkcjaCb cb) {
  g_pompaCb = cb;
}

// [ROZBUDOWA, ta sesja] Analogiczny setter dla wyboru strategii - patrz
// PompaWyborCb w dashboard_lvgl_poc.h.
void lvgl_dashboard_poc_ustaw_callback_wyboru(PompaWyborCb cb) {
  g_pompaWyborCb = cb;
}

// [POPRAWKA, ta sesja - panel SERWO v2] Dwa osobne settery - patrz
// ServoTrybCb/ServoPozycjaCb w dashboard_lvgl_poc.h.
void lvgl_dashboard_poc_ustaw_callback_serwo_tryb(ServoTrybCb cb) {
  g_serwoTrybCb = cb;
}
void lvgl_dashboard_poc_ustaw_callback_serwo_pozycja(ServoPozycjaCb cb) {
  g_serwoPozycjaCb = cb;
}

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA]
void lvgl_dashboard_poc_ustaw_callback_serwo_ustawienia(ServoUstawieniaCb cb) {
  g_serwoUstawieniaCb = cb;
}

// Wolane przez main.cpp razem z lvgl_dashboard_poc_aktualizuj() (patrz
// obszerny komentarz w dashboard_lvgl_poc.h) - tylko zapamietuje wartosci
// lokalnie (g_serwoUstWartosc[]); labelki na ekranie odswieza dopiero
// serwo_ustawienia_otworz() przy wejsciu, zeby nie przerysowywac tekstu 6x
// co STATUS_INTERWAL_MS, gdy ekran jest zamkniety (ten sam oszczedny wzorzec
// co reszta kafelkow - odswiezanie tylko widocznych elementow).
void lvgl_dashboard_poc_aktualizuj_ustawienia_serwa(int tempZadServo, int histerServo,
                                                     int skokKlapy, int skokSyberka,
                                                     int odchylTemp, int mnoznik) {
  g_serwoUstWartosc[0] = tempZadServo;
  g_serwoUstWartosc[1] = histerServo;
  g_serwoUstWartosc[2] = skokKlapy;
  g_serwoUstWartosc[3] = skokSyberka;
  g_serwoUstWartosc[4] = odchylTemp;
  g_serwoUstWartosc[5] = mnoznik;
  // Jesli ekran jest AKTUALNIE otwarty (user stoi na nim, gdy przychodzi
  // kolejny pobierzStatus()), odswiez tez widoczne labelki na zywo -
  // inaczej pokazywalyby wartosc sprzed ostatniego POST-a az do zamkniecia
  // i ponownego otwarcia ekranu.
  if (lv_scr_act() == serwoUstawieniaScr) {
    for (int i = 0; i < 6; i++) {
      char bufor[8];
      snprintf(bufor, sizeof(bufor), "%d", g_serwoUstWartosc[i]);
      lv_label_set_text(serwoUstWartoscLabel[i], bufor);
    }
  }
}

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY]
void lvgl_dashboard_poc_ustaw_callback_pompa_ustawienia(PompaUstawieniaCb cb) {
  g_pompaUstawieniaCb = cb;
}
void lvgl_dashboard_poc_ustaw_callback_pompa_antystop(PompaAntystopCb cb) {
  g_pompaAntystopCb = cb;
}

// Ten sam wzorzec co lvgl_dashboard_poc_aktualizuj_ustawienia_serwa() wyzej -
// tylko zapamietuje lokalnie, odswieza widoczne labelki na zywo TYLKO jesli
// user akurat stoi na tym ekranie.
void lvgl_dashboard_poc_aktualizuj_ustawienia_pompy(int czasOn, int czasOff,
                                                     int tempOn, int tempOff,
                                                     int antystopDni) {
  g_pompaUstWartosc[0] = czasOn;
  g_pompaUstWartosc[1] = czasOff;
  g_pompaUstWartosc[2] = tempOn;
  g_pompaUstWartosc[3] = tempOff;
  g_pompaUstWartosc[4] = antystopDni;
  if (lv_scr_act() == pompaUstawieniaScr) {
    for (int i = 0; i < 5; i++) {
      char bufor[8];
      snprintf(bufor, sizeof(bufor), "%d", g_pompaUstWartosc[i]);
      lv_label_set_text(pompaUstWartoscLabel[i], bufor);
    }
  }
}

// Osobna funkcja dla antystopWlaczony (bool) - patrz uzasadnienie w
// dashboard_lvgl_poc.h przy deklaracji.
void lvgl_dashboard_poc_aktualizuj_ustawienia_pompy_antystop(bool antystopWlaczony) {
  g_pompaAntystopWlaczony = antystopWlaczony;
  if (lv_scr_act() == pompaUstawieniaScr) {
    pompa_antystop_wyglad_odswiez();
  }
}

// [UI-BATERIA, ta sesja] Prawdziwa implementacja wg PLAN_BATERIA.md #6.3 -
// zastepuje wczesniejszy tymczasowy stub z tej samej sesji (main.cpp juz
// dostarcza poprawne procent/ladowanie, patrz procentBat/zasilanieUSB() w
// main.cpp loop()). Szerokosc wypelnienia proporcjonalna do %, kolor:
// ladowanie (KOL_BEZPIECZNA, zielony) NADPISUJE progi niski/krytyczny -
// tak jak w telefonie, priorytet "dobrze sie dzieje" nad alarmem, gdy USB
// podpiete.
void lvgl_dashboard_poc_aktualizuj_bateria(int procent, bool ladowanie) {
  if (bateriaWypelnienie == nullptr) return;
  // [v1.2.0 BATERIA-BRAK] procent < 0 = brak ogniwa (patrz komentarz w .h).
  // `ladowanie` jest tu celowo ignorowane: bez ogniwa nie ma czego ladowac,
  // a sam pin detekcji (GPIO35) bez zewnetrznego rezystora plywa.
  if (procent < 0) {
    lv_obj_set_width(bateriaWypelnienie, 0);
    lv_label_set_text(bateriaEtykieta, "USB");
    lv_obj_set_style_text_color(bateriaEtykieta, rgb565(KOL_TEKST_DIM), 0);
    return;
  }
  procent = constrain(procent, 0, 100);

  int maxSzer = BAT_SZER - 2 * BAT_RAMKA;
  lv_obj_set_width(bateriaWypelnienie, (procent * maxSzer) / 100);

  lv_color_t kolor;
  if (ladowanie)          kolor = rgb565(KOL_BEZPIECZNA);
  else if (procent <= 10) kolor = rgb565(KOL_ERR);
  else if (procent <= 20) kolor = rgb565(KOL_WARN);
  else                    kolor = rgb565(KOL_TEKST);

  lv_obj_set_style_bg_color(bateriaWypelnienie, kolor, 0);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", procent);
  lv_label_set_text(bateriaEtykieta, buf);
  lv_obj_set_style_text_color(bateriaEtykieta, kolor, 0);
}

// [v1.2.0 CENTRALA-ID] Patrz opis w dashboard_lvgl_poc.h.
void lvgl_dashboard_poc_aktualizuj_centrale(const char *znacznik) {
  const bool pusty = (znacznik == nullptr || znacznik[0] == '\0');
  // [v1.2.1] Ten sam tekst na ekranie alarmu (patrz alarm_ekran_zbuduj()).
  if (alarmEkranZnacznik != nullptr) {
    if (pusty) {
      lv_obj_add_flag(alarmEkranZnacznik, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(alarmEkranZnacznik, znacznik);
      lv_obj_align(alarmEkranZnacznik, LV_ALIGN_TOP_MID, 0, 210);
      lv_obj_clear_flag(alarmEkranZnacznik, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (centralaZnacznikLabel == nullptr) return;
  if (pusty) {
    lv_obj_add_flag(centralaZnacznikLabel, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_label_set_text(centralaZnacznikLabel, znacznik);
  // Ponowne wyrownanie: etykieta ma szerokosc zalezna od tekstu, a
  // LV_ALIGN_TOP_MID liczy pozycje raz, w chwili wywolania lv_obj_align().
  lv_obj_align(centralaZnacznikLabel, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_clear_flag(centralaZnacznikLabel, LV_OBJ_FLAG_HIDDEN);
}

// [ROZSZERZENIE, ta sesja - PANEL DYM] Settery + updater, dokladnie ten sam
// wzorzec co SERWO/POMPA powyzej.
void lvgl_dashboard_poc_ustaw_callback_dym_prog(DymProgCb cb) {
  g_dymProgCb = cb;
}
void lvgl_dashboard_poc_ustaw_callback_dym_ustawienia(DymUstawieniaCb cb) {
  g_dymUstawieniaCb = cb;
}

// Zapamietuje lokalnie; jesli user AKURAT stoi na panelu DYM gdy przychodzi
// kolejny pobierzUstawienia(), odswieza tez widoczne labelki na zywo - ten
// sam wzorzec co lvgl_dashboard_poc_aktualizuj_ustawienia_serwa/pompy().
void lvgl_dashboard_poc_aktualizuj_ustawienia_dymu(int progAlarmDym, int dymCzasOn,
                                                    int dymCzasOff, int dymCzasStabilizacji) {
  g_dymProg = progAlarmDym;
  g_dymUstWartosc[0] = dymCzasOn;
  g_dymUstWartosc[1] = dymCzasOff;
  g_dymUstWartosc[2] = dymCzasStabilizacji;
  if (lv_scr_act() == dymPanel) {
    char bufor[16];
    snprintf(bufor, sizeof(bufor), "PROG: %d", g_dymProg);
    lv_label_set_text(dymProgLabel, bufor);
    lv_slider_set_value(dymSlider, g_dymProg, LV_ANIM_OFF);
    for (int i = 0; i < 3; i++) {
      char b2[8];
      snprintf(b2, sizeof(b2), "%d", g_dymUstWartosc[i]);
      lv_label_set_text(dymUstWartoscLabel[i], b2);
    }
  }
}

// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Ten sam wzorzec co
// para funkcji DYM powyzej.
void lvgl_dashboard_poc_ustaw_callback_alarm_temp_prog(AlarmTempProgCb cb) {
  g_alarmTempProgCb = cb;
}

// [EKRAN ALARMOWY, ta sesja] Patrz AlarmWyciszCb w dashboard_lvgl_poc.h.
void lvgl_dashboard_poc_ustaw_callback_alarm_wycisz(AlarmWyciszCb cb) {
  g_alarmWyciszCb = cb;
}

void lvgl_dashboard_poc_aktualizuj_ustawienia_alarmu_temp(int progAlarmTemp) {
  g_alarmTempProg = progAlarmTemp;
  if (lv_scr_act() == alarmTempPanel) {
    char bufor[16];
    snprintf(bufor, sizeof(bufor), "PROG: %d C", g_alarmTempProg);
    lv_label_set_text(alarmTempProgLabel, bufor);
    lv_slider_set_value(alarmTempSlider, g_alarmTempProg, LV_ANIM_OFF);
  }
}

// [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Settery + updater, ten sam
// wzorzec co DYM/ALARM TEMP powyzej - jedyna roznica (niewidoczna z tego
// pliku) to co main.cpp robi PO otrzymaniu wywolania callbacku: NVS+PWM
// zamiast httpPost(), patrz obszerny komentarz w dashboard_lvgl_poc.h.
void lvgl_dashboard_poc_ustaw_callback_wyswietlacz_jasnosc(WyswietlaczJasnoscCb cb) {
  g_wyswietlaczJasnoscCb = cb;
}
void lvgl_dashboard_poc_ustaw_callback_wyswietlacz_ustawienia(WyswietlaczUstawieniaCb cb) {
  g_wyswietlaczUstawieniaCb = cb;
}
void lvgl_dashboard_poc_ustaw_callback_wyswietlacz_harmonogram(WyswietlaczHarmonogramCb cb) {
  g_wyswietlaczHarmonogramCb = cb;
}

// Zapamietuje lokalnie; jesli user AKURAT stoi na panelu WYSWIETLACZ (rzadkie,
// bo w odroznieniu od DYM/SERWO/POMPA nic zewnetrznego nie moze zmienic tych
// wartosci poza samym tym panelem - wolane w praktyce raz, zaraz po starcie
// main.cpp, gdy NVS zdazyl juz sie wczytac) - odswiez tez widoczne widgety na
// zywo, ten sam wzorzec co reszta funkcji _aktualizuj_ustawienia_*() powyzej.
void lvgl_dashboard_poc_aktualizuj_ustawienia_wyswietlacza(int jasnoscDzien, int jasnoscNoc,
                                                            int godzStart, int godzKoniec,
                                                            bool harmonogramWlaczony) {
  g_wyswietlaczJasnoscDzien = jasnoscDzien;
  g_wyswietlaczJasnoscNoc   = jasnoscNoc;
  g_wyswietlaczGodz[0] = godzStart;
  g_wyswietlaczGodz[1] = godzKoniec;
  g_wyswietlaczHarmonogramWlaczony = harmonogramWlaczony;
  if (lv_scr_act() == wyswietlaczPanel) {
    char bufor[8];
    snprintf(bufor, sizeof(bufor), "%d%%", g_wyswietlaczJasnoscDzien);
    lv_label_set_text(wyswietlaczLabelDzien, bufor);
    lv_slider_set_value(wyswietlaczSliderDzien, g_wyswietlaczJasnoscDzien, LV_ANIM_OFF);
    snprintf(bufor, sizeof(bufor), "%d%%", g_wyswietlaczJasnoscNoc);
    lv_label_set_text(wyswietlaczLabelNoc, bufor);
    lv_slider_set_value(wyswietlaczSliderNoc, g_wyswietlaczJasnoscNoc, LV_ANIM_OFF);
    for (int i = 0; i < 2; i++) {
      char b2[8];
      snprintf(b2, sizeof(b2), "%d:00", g_wyswietlaczGodz[i]);
      lv_label_set_text(wyswietlaczGodzLabel[i], b2);
    }
    wyswietlacz_harmonogram_wyglad_odswiez();
  }
}

// [UI-ZEWN] Przelacza bitmape+kolor ikony ZEWN wg. progow z ikony.h (mroz<0
// st.C, neutralnie 0-15 st.C, cieplo >=15 st.C - patrz komentarz tam, progi
// "do ew. korekty po obserwacji", nie twarda norma). Kolory zgodne z tymi,
// ktore byly uzyte w usunietym bloku podgladowym ZEWN_ICON_PREVIEW.
// [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Nowy parametr deszczStan (0/1/2, z
// centrali przez /api/status pola deszcz_stan) NADPISUJE progi temperatury,
// gdy != 0 - rzut oka na kafelek ZEWN ma priorytetowo pokazywac "zbliza
// sie/pada", nie biezaca temperature, dokladnie jak user prosil ("ikona ma
// sie zmienic jak sie zbliza deszcz"). deszczStan==0 -> zero zmiany
// zachowania wzgledem stanu SPRZED tej funkcjonalnosci (te same 3 progi).
// [FIX, ta sesja - User: "czemu snieżka przy +16 na dworze"] Czujnik
// DS18B20 zewn. rozlaczony -> main_centrala.cpp ustawia sentinel bledu
// temperaturaNaZewnatrz=-1 (NIE prawdziwy odczyt, patrz linia ~11560 tam) -
// ten sam -1.0 leci dalej przez t_zewn w /api/status, wiec tZewn tutaj tez
// nim jest. Stary kod nie odrozial "sentinel bledu" od "naprawde ponizej
// zera" - obie sytuacje dawaly tZewn<0.0f, wiec obie pokazywaly MROZ. Bug
// byl od zawsze w tej funkcji (nie od deszczu), ujawnil sie dopiero teraz
// bo user zauwazyl rozjazd z realna pogoda. Naprawa: rozpoznaj DOKLADNIE
// ten sentinel PRZED progami temperatury - realny odczyt DS18B20 (krok
// 0.0625 st.C) praktycznie nigdy nie trafi w rowne -1.0000, wiec exact
// (z mala tolerancja na zaokraglenia float) match jest bezpieczny.
static void zewn_ikona_odswiez(float tZewn, int deszczStan) {
  if (zewnIcon == nullptr) return;
  const lv_img_dsc_t *src;
  uint16_t kolor;
  bool czujnikBrak = (fabsf(tZewn - (-1.0f)) < 0.01f);
  if (deszczStan == 1) {
    // Nadciaga - chmura+kropelki (patrz obszerny komentarz przy
    // IKONA_ZEWN_DESZCZ w ikony.h), ten sam kolor "chmura" co stan neutralny
    // (to wciaz chmura, tylko z zapowiedzia opadu, nie inny typ pogody).
    src = &lv_ikona_zewn_deszcz;
    kolor = KOL_ZEWN_NEUTRALNY;
  } else if (deszczStan == 2) {
    // Pada TERAZ - reuzywa juz istniejaca duza kropla (kafelek WILGOTNOSC ma
    // wlasny, mniejszy wariant IKONA_KROPLA_MALA - tu bierzemy pelnowymiarowa
    // IKONA_KROPLA, bo ZEWN uzywa standardowego IKONA_ZEWN_ROZMIAR=40, tak
    // samo jak reszta stanow tego kafelka).
    src = &lv_ikona_kropla;
    kolor = KOL_KROPLA;
  } else if (czujnikBrak) {
    // Sentinel bledu czujnika - NIE zgadujemy "mroz" ani "cieplo" bez
    // realnych danych. Neutralna chmura jest najmniej falszywym wyborem
    // (nie twierdzi nic konkretnego o temperaturze), do czasu naprawy
    // czujnika DS18B20.
    src = &lv_ikona_zewn_neutralny;
    kolor = KOL_ZEWN_NEUTRALNY;
  } else if (tZewn < 0.0f) {
    src = &lv_ikona_zewn_mroz;
    kolor = KOL_KROPLA;
  } else if (tZewn < 15.0f) {
    src = &lv_ikona_zewn_neutralny;
    kolor = KOL_ZEWN_NEUTRALNY;
  } else {
    src = &lv_ikona_zewn_cieplo;
    kolor = KOL_ZAR;
  }
  lv_img_set_src(zewnIcon, src);
  lv_obj_set_style_img_recolor(zewnIcon, rgb565(kolor), 0);
}

// [ROZSZERZENIE, ta sesja] User: "ikonka DYM na glownym panelu powinna byc
// interaktywna (juz jest - klikalna), jak jest OK to zielony znaczek ok, a
// nie trojkat - zloty gdy blisko progu, czerwony gdy alarm". 3 stany zamiast
// dotychczasowych 2 (tylko amber/czerwony trojkat):
//   OK (dym wyraznie ponizej progu)  -> lv_ikona_bezpieczna (tarcza z
//                                        checkmarkiem - JUZ w projekcie,
//                                        uzywana w przycisku BEZPIECZNA
//                                        panelu SERWO), kolor KOL_BEZPIECZNA
//                                        (zielony) - dokladnie ten sam
//                                        jezyk "stan bezpieczny/OK", co tam.
//   UWAGA (dym >=70% progu, ale bez alarmu z centrali) -> nadal trojkat
//                                        (lv_ikona_alarm), kolor KOL_WARN
//                                        (bursztyn) - bez zmiany wzgledem
//                                        poprzedniego zachowania.
//   ALARM (dymAlarm==true z centrali) -> trojkat, kolor KOL_ERR (czerwony) -
//                                        bez zmiany wzgledem poprzedniego.
// Prog 70% to wlasna propozycja (centrala nie wysyla osobnego "prog
// ostrzegawczy", tylko finalny progAlarmDym) - daje uzytkownikowi
// wczesniejszy sygnal wizualny zanim faktyczny alarm sie uruchomi. g_dymProg
// to ostatnia znana wartosc progu z centrali (patrz
// lvgl_dashboard_poc_aktualizuj_ustawienia_dymu()) - jesli jeszcze nie
// przyszla (g_dymProg<=0, teoretycznie niemozliwe bo domyslne 500, ale
// zabezpieczone), pomijamy strefe UWAGA i pokazujemy tylko OK/ALARM.
// [ROZSZERZENIE, ta sesja - alarmy PIEC CO/PANEL] Prostsze niz DYM (tylko
// 2 stany: OK/ALARM, centrala nie daje tu osobnego progu "uwaga" jak
// progAlarmDym - progAlarmowyTemperatury to jedna, wspolna wartosc dla obu
// alarmow, wiec nie ma tu odpowiednika strefy bursztynowej). Przy alarmie:
// ramka i ikona na czerwono (KOL_ERR). Bez alarmu: powrot do wlasnych
// kolorow "spokojnych" kazdego kafelka (KOL_ZAR dla PIEC CO - jego stala
// ramka/ikona od poczatku, KOL_RAMKA/KOL_ZAR dla PANEL - jego domyslne
// wartosci z wywolania kafelek() w init()).
static void piec_alarm_odswiez(bool alarmOgrzewanie) {
  if (ogrzCard != nullptr) {
    lv_obj_set_style_border_color(ogrzCard, rgb565(alarmOgrzewanie ? KOL_ERR : KOL_ZAR), 0);
  }
  if (ogrzIcon != nullptr) {
    lv_obj_set_style_img_recolor(ogrzIcon, rgb565(alarmOgrzewanie ? KOL_ERR : KOL_ZAR), 0);
  }
}

static void panel_alarm_odswiez(bool alarmPanel) {
  if (panelCard != nullptr) {
    lv_obj_set_style_border_color(panelCard, rgb565(alarmPanel ? KOL_ERR : KOL_RAMKA), 0);
  }
  if (panelIcon != nullptr) {
    lv_obj_set_style_img_recolor(panelIcon, rgb565(alarmPanel ? KOL_ERR : KOL_ZAR), 0);
  }
}

// [UI-WIFI-SYGNAL, ta sesja] Progi dBm -> liczba aktywnych kresek (1-7).
// Zakres oparty o typowe wartosci RSSI zglaszane przez modul WiFi ESP32
// (praktycznie -30 dBm tuz przy routerze do ok. -90/-95 dBm na granicy
// zasiegu) - 7 progow rozlozonych co ~6 dBm miedzy -95 (1 kreska) a -55
// (wszystkie 7 kresek, "pelny" sygnal). Wartosc rssi ma sens tylko przy
// polaczono==true (patrz komentarz przy parametrze w dashboard_lvgl_poc.h) -
// ta funkcja jest wolana z fallbackiem 0 kresek gdy polaczono==false, wiec
// nie musi sama sprawdzac tego przypadku.
static int wifi_liczba_kresek(int rssi) {
  static const int PROGI[7] = { -95, -89, -83, -77, -71, -65, -55 };
  int n = 0;
  for (int i = 0; i < 7; i++) {
    if (rssi >= PROGI[i]) n = i + 1;
  }
  return n;
}

static void wifi_ikona_odswiez(bool polaczono, int rssi) {
  if (wifiX == nullptr) return; // widget jeszcze nie zbudowany (nie powinno sie zdarzyc po init())

  if (!polaczono) {
    // Decyzja: wyszarzone wszystkie kreski + symbol X widoczny.
    for (int i = 0; i < 7; i++) {
      if (wifiPasek[i] != nullptr) {
        lv_obj_set_style_bg_color(wifiPasek[i], rgb565(KOL_TEKST_DIM), 0);
      }
    }
    lv_obj_clear_flag(wifiX, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_add_flag(wifiX, LV_OBJ_FLAG_HIDDEN);
  int aktywne = wifi_liczba_kresek(rssi);
  for (int i = 0; i < 7; i++) {
    if (wifiPasek[i] == nullptr) continue;
    bool aktywna = (i < aktywne);
    lv_obj_set_style_bg_color(wifiPasek[i], rgb565(aktywna ? KOL_TEKST : KOL_TEKST_DIM), 0);
  }
}

// [UI-INTERNET, ta sesja] Analogiczne do wifi_ikona_odswiez(), ale bez
// stopniowania (fbAppReady jest bool, nie ma "sily sygnalu") - albo obie
// warstwy globusu w swoich stalych kolorach (KOL_GLOBUS_SIATKA/LADY) i X
// ukryte, albo obie wyszarzone na KOL_TEKST_DIM i X widoczne.
static void internet_ikona_odswiez(bool internet) {
  if (netX == nullptr) return; // widget jeszcze nie zbudowany

  uint16_t kolorSiatka = internet ? KOL_GLOBUS_SIATKA : KOL_TEKST_DIM;
  uint16_t kolorLady = internet ? KOL_GLOBUS_LADY : KOL_TEKST_DIM;
  if (netGlobusSiatka != nullptr) {
    lv_obj_set_style_img_recolor(netGlobusSiatka, rgb565(kolorSiatka), 0);
  }
  if (netGlobusLady != nullptr) {
    lv_obj_set_style_img_recolor(netGlobusLady, rgb565(kolorLady), 0);
  }
  if (internet) {
    lv_obj_add_flag(netX, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(netX, LV_OBJ_FLAG_HIDDEN);
  }
}

static void dym_ikona_odswiez(int dym, bool dymAlarm) {
  if (dymIcon == nullptr) return;
  const lv_img_dsc_t *src;
  uint16_t kolor;
  bool uwaga = (g_dymProg > 0) && (dym >= (g_dymProg * 7) / 10);
  if (dymAlarm) {
    src = &lv_ikona_alarm;
    kolor = KOL_ERR;
  } else if (uwaga) {
    src = &lv_ikona_alarm;
    kolor = KOL_WARN;
  } else {
    src = &lv_ikona_bezpieczna;
    kolor = KOL_BEZPIECZNA;
  }
  lv_img_set_src(dymIcon, src);
  lv_obj_set_style_img_recolor(dymIcon, rgb565(kolor), 0);
  // Ramka karty - ten sam 3-stanowy jezyk kolorow co ikona (poprzednio tylko
  // 2 stany: KOL_ERR/KOL_OGIEN). W stanie OK ramka wraca do neutralnej
  // KOL_RAMKA (ten sam "spokojny" kolor co karty SERWO w trybie Auto) zamiast
  // KOL_OGIEN (turkus zarezerwowany na kafelek POMPA - patrz komentarz przy
  // KOL_BEZPIECZNA na poczatku pliku co do "pozyczonych" kolorow).
  uint16_t kolorRamki = dymAlarm ? KOL_ERR : (uwaga ? KOL_WARN : KOL_RAMKA);
  lv_obj_set_style_border_color(dymCard, rgb565(kolorRamki), 0);

  // [FIX tej sesji] Wskaznik pozycji zamiast wypelnienia lv_bar - patrz
  // obszerny komentarz w kafelek_dym() co do tego, dlaczego (przy niskich
  // wartosciach wypelnienie ginelo na tle pustego paska, wiec nie bylo
  // widac ani stref, ani odleglosci do progu). Pozycja X = procent *
  // (dymBarSzer - szerokosc wskaznika), czyli dym/g_dymProg*100
  // PRZELICZANE W LOCIE wzgledem aktualnego progu (w odroznieniu od suwaka
  // w PANELU DYM, ktory ma zakres staly), zaciete na 0-100% zeby wskaznik
  // nie wyjechal poza tlo stref.
  if (dymBar != nullptr && dymBarSzer > 0) {
    int procent = 0;
    if (g_dymProg > 0) {
      procent = (int)(((long)dym * 100) / g_dymProg);
      if (procent > 100) procent = 100;
      if (procent < 0) procent = 0;
    }
    int wskaznikSzer = 3;
    int x = (int)(((long)procent * (dymBarSzer - wskaznikSzer)) / 100);
    lv_obj_set_x(dymBar, x);
    lv_obj_set_style_bg_color(dymBar, rgb565(kolor), 0);
  }
}

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
                                    bool internet,
                                    int deszczStan, int deszczMinuty) {
  g_polaczono = polaczono;
  wifi_ikona_odswiez(polaczono, rssi); // [UI-WIFI-SYGNAL, ta sesja]
  internet_ikona_odswiez(internet); // [UI-INTERNET, ta sesja]
  // [POPRAWKA, ta sesja - panel SERWO v2] Cache pozycji zawsze aktualizowany
  // (nawet gdy panel akurat zamkniety) - potrzebny, zeby serwo_panel_otworz()
  // mogl zainicjowac slider PRAWDZIWA wartoscia przy nastepnym otwarciu, nie
  // ostatnia recznie ustawiona/zerem. `g_trybSerwa` (1/2/3) ZASTEPUJE dawne
  // dwie flagi bool - patrz komentarz przy deklaracji.
  g_klapaPozycja = klapa;
  g_syberkaPozycja = syberka;
  // [ROZSZERZENIE, ta sesja - Panel SERWO Czesc 2 - UWAGA] Luki (serwoArcKlapa/
  // Syberka) CELOWO NIE sa tu odswiezane. Suwak i etykieta procentowa tez nie
  // sa - obie sa zywe WYLACZNIE przy otwarciu panelu (serwo_panel_otworz(), z
  // cache'a powyzej) i podczas przeciagania (serwo_slider_event_cb()), nigdy
  // z tego cyklu STATUS_INTERWAL_MS. Doczepienie tu luku wprowadziloby realna
  // niespojnosc (luk "skakalby" niezaleznie od suwaka/etykiety przy kazdym
  // odswiezeniu statusu, nawet w trakcie przeciagania palcem) - luk trzyma
  // sie zamiast tego dokladnie tego samego zrodla co reszta wiersza.
  g_trybSerwa = trybSerwa;
  g_pompaWlaczona = pompaWlaczona;
  g_pompaOverrideMin = pompaOverrideMin;
  // [ROZBUDOWA, ta sesja] Tylko przy realnej zmianie - unikamy zbednego
  // lv_obj_set_style_bg_color() na kazdym cyklu STATUS_INTERWAL_MS (2s), gdy
  // strategia i tak sie nie zmienila (zdecydowana wiekszosc wywolan).
  // Podswietlenie zyje niezaleznie od tego, czy strona POMPA jest akurat
  // otwarta - patrz pompa_tryb_odswiez() (bezpieczne wywolanie na ukrytych
  // obiektach, sprawdza tylko czy uchwyty != nullptr).
  if (pompaWybor != g_pompaWybor) {
    g_pompaWybor = pompaWybor;
    pompa_tryb_odswiez();
    pompa_ust_aktywnosc_odswiez();
  }

  // [FIX tej sesji] Podzial sezonowy zima/lato zostal calkowicie usuniety -
  // wszystkie 11 kafelkow (w tym PIEC CO/SR, KLAPA, SYBERKA, DYM i PANEL)
  // miesci sie razem na jednym ekranie, wiec chowanie ktoregokolwiek z nich
  // wg sezonu nie jest juz potrzebne i tylko zostawialo pusta dziure w
  // siatce (PANEL byl niewidoczny zimą, mimo ze stoi miedzy BOJLER i PIEC).
  // Wszystkie sa teraz ZAWSZE widoczne - stad brak parametru `zima` i calej
  // powyzszej logiki pokaz/ukryj.

  if (!polaczono) {
    lv_label_set_text(zewnValLabel, "--");
    lv_label_set_text(bojlerValLabel, "--");
    lv_label_set_text(dymValLabel, "--");
    if (ogrzGlownaLabel != nullptr) lv_label_set_text(ogrzGlownaLabel, "--");
    if (ogrzSredniaLabel != nullptr) lv_label_set_text(ogrzSredniaLabel, "SR --");
    if (panelValLabel != nullptr) lv_label_set_text(panelValLabel, "--");
    if (pokojValLabel != nullptr) lv_label_set_text(pokojValLabel, "--");
    if (wilgotnoscValLabel != nullptr) lv_label_set_text(wilgotnoscValLabel, "--");
    if (cisnienieValLabel != nullptr) lv_label_set_text(cisnienieValLabel, "--");
    if (klapaValLabel != nullptr) lv_label_set_text(klapaValLabel, "--");
    if (syberkaValLabel != nullptr) lv_label_set_text(syberkaValLabel, "--");
    if (serwoWejsciePodpis != nullptr) lv_label_set_text(serwoWejsciePodpis, "Klapa --%\nSyberka --%");
    // Kafelek POMPA tez nie moze pokazywac ostatniego znanego AUTO/WL/WYL
    // jakby to wciaz byla aktualna prawda - to samo "UI nie klamie", co przy
    // nieudanym POST z panelu (patrz komentarz przy przyciskach wyzej).
    if (pompaValLabel != nullptr) {
      lv_label_set_text(pompaValLabel, "--");
      lv_obj_add_flag(pompaArc, LV_OBJ_FLAG_HIDDEN);
      // [v1.2.0 DANE-NIEAKTUALNE] Od v1.2.0 ta galaz jest osiagana takze w
      // trakcie pracy (dane starsze niz prog w main.cpp), wiec ikona moze
      // tu trafic w stanie "pompa pracuje": wygas ja i schowaj przekreslenie
      // - oba znaki mowilyby o stanie, ktorego panel juz nie zna. Wirnik
      // zatrzymuje pompa_wirnik_timer_cb() (patrz !g_polaczono tam). Po
      // powrocie danych kolor i przekreslenie ustawia pompa_tile_odswiez().
      lv_obj_set_style_img_recolor(pompaIcon, rgb565(KOL_TEKST_DIM), 0);
      if (pompaPrzekreslenie != nullptr) lv_obj_add_flag(pompaPrzekreslenie, LV_OBJ_FLAG_HIDDEN);
    }
    // Ta sama zasada dla SERWO - brak polaczenia nie moze pokazywac ostatnio
    // znanego trybu recznego jakby to wciaz byla aktualna prawda.
    if (klapaCard != nullptr) {
      lv_obj_set_style_border_color(klapaCard, rgb565(KOL_RAMKA), 0);
      lv_obj_add_flag(klapaRecznieIcon, LV_OBJ_FLAG_HIDDEN);
    }
    if (syberkaCard != nullptr) {
      lv_obj_set_style_border_color(syberkaCard, rgb565(KOL_RAMKA), 0);
      lv_obj_add_flag(syberkaRecznieIcon, LV_OBJ_FLAG_HIDDEN);
    }
    // [LVGL-DATA-CZAS, ta sesja] Brak polaczenia -> ostatnia znana godzina
    // moglaby juz byc nieaktualna, tak samo jak reszta kafelkow wyzej.
    if (dataCzasCzasLabel != nullptr) lv_label_set_text(dataCzasCzasLabel, "--:--");
    if (dataCzasDataLabel != nullptr) lv_label_set_text(dataCzasDataLabel, "--.--.----");
    return;
  }

  char bufor[16];
  // [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Gdy zbliza sie/pada deszcz, etykieta
  // wartosci ZEWN pokazuje odliczanie minut ZAMIAST temperatury - kafelek ma
  // tylko JEDNA etykiete wartosci (font montserrat_28, kw=114 - juz "na
  // styk" dla "100.0°C", patrz komentarz w kafelek()), wiec podmieniamy
  // TRESC tej samej etykiety zamiast dokladac druga (ryzyko kolizji w tak
  // ciasnym layoucie). Format "%dm" identyczny jak juz uzywany dla
  // pompaOverrideMin (kafelek POMPA) - krotszy niz "100.0°C", wiec komfortowo
  // miesci sie bez zmiany czcionki. deszczStan==0 -> zero zmiany zachowania
  // (dokladnie ten sam kod co przed ta funkcjonalnoscia).
  if (deszczStan == 1) {
    snprintf(bufor, sizeof(bufor), "za %dm", deszczMinuty);
  } else if (deszczStan == 2) {
    if (deszczMinuty >= 0) snprintf(bufor, sizeof(bufor), "kon %dm", deszczMinuty);
    else snprintf(bufor, sizeof(bufor), "pada");
  } else {
    // [FIX tej sesji] User: "dodaj do wszystkich wartosci temp znaczek stopni,
    // bo jest np. 35 C". Dodany znak stopnia (°, U+00B0) przed "C" w KAZDEJ
    // etykiecie temperatury nizej. W przeciwienstwie do polskich znakow
    // diakrytycznych (celowo pominietych w calym UI - "WILGOTNOSC" bez "SC",
    // "CISNIENIE" bez "S" - bo domyslne wbudowane fonty LVGL montserrat_14/20/
    // 28 NIE zawieraja Latin Extended-A), znak stopnia jest w standardowym
    // zakresie glifow tych fontow (LVGL dolacza go domyslnie obok ASCII i
    // bullet) - powinien wyswietlic sie poprawnie bez zmiany fontu. Warto to
    // jednak potwierdzic na prawdziwym sprzecie przy pierwszej okazji.
    snprintf(bufor, sizeof(bufor), "%.1f°C", tZewn);
  }
  lv_label_set_text(zewnValLabel, bufor);
  zewn_ikona_odswiez(tZewn, deszczStan);

  // [Partia 4, ta sesja] User na zywym sprzecie: "temp boilera i pieca ma
  // byc dziesietna bez przecinka" - czyli calkowita, bez ".0" na koncu.
  // ZEWN/POKOJ zostaja przy %.1f (user tego nie zglosil, i tam liczba
  // miejsca ma - te dwa kafelki nie maja obok siebie drugiej wartosci jak
  // PIEC CO/SR).
  snprintf(bufor, sizeof(bufor), "%.0f°C", tBojler);
  lv_label_set_text(bojlerValLabel, bufor);

  snprintf(bufor, sizeof(bufor), "%d", dym);
  lv_label_set_text(dymValLabel, bufor);

  if (ogrzGlownaLabel != nullptr) {
    snprintf(bufor, sizeof(bufor), "%.0f°C", tOgrz);
    lv_label_set_text(ogrzGlownaLabel, bufor);
  }
  // [Partia 5, ta sesja] PANEL - sam format co BOJLER (%.0f, "dziesietna bez
  // przecinka", patrz komentarz nad BOJLEREM wyzej - ta sama prosba usera
  // dotyczy tez tego kafelka bo uzywa tego samego wzorca kafelek()).
  if (panelValLabel != nullptr) {
    snprintf(bufor, sizeof(bufor), "%.0f°C", tPanel);
    lv_label_set_text(panelValLabel, bufor);
  }

  if (ogrzSredniaLabel != nullptr) {
    snprintf(bufor, sizeof(bufor), "SR %.0f°C", tOgrzSr);
    lv_label_set_text(ogrzSredniaLabel, bufor);
  }

  // [Partia 2, ta sesja] 5 nowych kafelkow - te same formaty co w
  // tresciKafelka() starego dashboardu (String(x,1)/(x,0) + jednostka).
  if (pokojValLabel != nullptr) {
    snprintf(bufor, sizeof(bufor), "%.1f°C", tPokoj);
    lv_label_set_text(pokojValLabel, bufor);
  }
  if (wilgotnoscValLabel != nullptr) {
    snprintf(bufor, sizeof(bufor), "%.0f %%", wilgotnosc);
    lv_label_set_text(wilgotnoscValLabel, bufor);
  }
  if (cisnienieValLabel != nullptr) {
    // [Partia 4, ta sesja] User na zywym sprzecie: "cisnienie jest aby h a
    // powinno byc hPa" - "1013.0 hPa" przy foncie 28 (~10 znakow) jest
    // szersze niz karta (kw=114, ok. 106px dostepnego miejsca), wiec "Pa"
    // wychodzilo poza karte i bylo obcinane przez domyslne przycinanie
    // dzieci LVGL do granic rodzica. Naprawione DWIE rzeczy razem (patrz
    // font ustawiony po stworzeniu kafelka w lvgl_dashboard_poc_init()):
    // usunieta czesc dziesietna (1 hPa dokladnosci wystarczy) + mniejszy
    // font (20 zamiast 28) dla TEJ jednej etykiety - "1013 hPa" mieści się
    // w calosci.
    snprintf(bufor, sizeof(bufor), "%.0f hPa", cisnienie);
    lv_label_set_text(cisnienieValLabel, bufor);
  }
  if (klapaValLabel != nullptr) {
    snprintf(bufor, sizeof(bufor), "%d %%", klapa);
    lv_label_set_text(klapaValLabel, bufor);
  }
  if (syberkaValLabel != nullptr) {
    snprintf(bufor, sizeof(bufor), "%d %%", syberka);
    lv_label_set_text(syberkaValLabel, bufor);
  }
  if (serwoWejsciePodpis != nullptr) {
    // Osobny, wiekszy bufor niz `bufor[16]` uzywany wyzej - "Klapa 100%\n
    // Syberka 100%" to az 23 znaki + terminator, `bufor[16]` by sie ucielo.
    char boforSerwo[28];
    snprintf(boforSerwo, sizeof(boforSerwo), "Klapa %d%%\nSyberka %d%%", klapa, syberka);
    lv_label_set_text(serwoWejsciePodpis, boforSerwo);
  }

  // [ROZSZERZENIE, ta sesja] 3-stanowa ikona/ramka (OK zielony znaczek /
  // UWAGA bursztynowy trojkat / ALARM czerwony trojkat) - patrz
  // dym_ikona_odswiez() wyzej, zastepuje poprzednia 2-stanowa logike wprost
  // tutaj.
  dym_ikona_odswiez(dym, dymAlarm);
  piec_alarm_odswiez(alarmOgrzewanie);
  panel_alarm_odswiez(alarmPanel);
  // [EKRAN ALARMOWY, ta sesja] Patrz PLAN_EKRAN_ALARMOWY.md. Celowo TU (nie
  // wczesniej w tej funkcji) - po bloku "if (!polaczono) return" wyzej, czyli
  // dziedziczy dokladnie ta sama zasade co dym/piec/panel_alarm_odswiez:
  // brak polaczenia = zamrozenie ostatniego znanego stanu, nie zgadywanie.
  alarm_ekran_odswiez(dymAlarm, alarmOgrzewanie, alarmPanel);

  pompa_tile_odswiez();
  // [FIX UX, ta sesja] Wolane tu (nie tylko przy otwarciu strony) - jesli
  // uzytkownik zmieni AUTO/WLACZ/WYLACZ z INNEGO urzadzenia (np. starego
  // webowego panelu) podczas gdy strona POMPA jest akurat otwarta na tym
  // ekranie, podswietlenie ma sie odswiezyc od razu przy kolejnym
  // pobierzStatus(), tak samo jak reszta kafelkow - bez czekania na
  // ponowne wejscie na strone.
  pompa_panel_glowna_odswiez();
  serwo_tile_odswiez();

  // [LVGL-DATA-CZAS, ta sesja] rtcOk==false (brak/awaria RTC na centrali,
  // patrz g_rtcGotowy w main_centrala.cpp) -> "--:--"/"--.--.----" zamiast
  // mylacego "00:00"/"00.00.0000" (dokladnie ta sama zasada co "brak
  // polaczenia" w bloku !polaczono wyzej - UI nie ma pokazywac wartosci,
  // ktorej nie jest pewne).
  if (dataCzasCzasLabel != nullptr) {
    if (rtcOk) {
      snprintf(bufor, sizeof(bufor), "%02d:%02d", godzina, minuta);
      lv_label_set_text(dataCzasCzasLabel, bufor);
    } else {
      lv_label_set_text(dataCzasCzasLabel, "--:--");
    }
  }
  if (dataCzasDataLabel != nullptr) {
    if (rtcOk) {
      snprintf(bufor, sizeof(bufor), "%02d.%02d.%04d", dzien, miesiac, rok);
      lv_label_set_text(dataCzasDataLabel, bufor);
    } else {
      lv_label_set_text(dataCzasDataLabel, "--.--.----");
    }
  }
}

// [v1.2.1 RATUNEK] Patrz opis w dashboard_lvgl_poc.h.
int lvgl_dashboard_poc_alarm_odrzucony() {
  return (int)g_alarmOdrzucony;
}

void lvgl_dashboard_poc_przywroc_alarm(bool dymAlarm, bool alarmOgrzewanie, bool alarmPanel, int odrzuconyPoziom) {
  if (odrzuconyPoziom < (int)AlarmPoziom::BRAK || odrzuconyPoziom > (int)AlarmPoziom::POZAR) odrzuconyPoziom = (int)AlarmPoziom::BRAK;
  g_alarmOdrzucony = (AlarmPoziom)odrzuconyPoziom;
  // Te same trzy wywolania i w tej samej kolejnosci co na koncu
  // lvgl_dashboard_poc_aktualizuj(). Wartosci czujnika dymu po restarcie nie
  // ma (kafelek pokazuje "--"), stad 0 - ikona zalezy wtedy tylko od alarmu.
  dym_ikona_odswiez(0, dymAlarm);
  piec_alarm_odswiez(alarmOgrzewanie);
  panel_alarm_odswiez(alarmPanel);
  alarm_ekran_odswiez(dymAlarm, alarmOgrzewanie, alarmPanel);
}
