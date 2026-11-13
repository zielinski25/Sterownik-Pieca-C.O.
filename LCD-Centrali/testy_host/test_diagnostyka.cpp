// ============================================================================
// Testy czystych funkcji z src/diagnostyka_panelu.h - NA PC (g++), bez plytki.
// ============================================================================
// Uruchamianie:  ./uruchom_testy.sh   (albo recznie:
//   g++ -std=c++17 -Wall -Wextra -o /tmp/t test_diagnostyka.cpp && /tmp/t )
// Kazdy SPRAWDZ() to jeden warunek; program konczy sie kodem 1, gdy
// ktorykolwiek nie przejdzie - skrypt wtedy przerywa.
// ============================================================================
#include "../src/diagnostyka_panelu.h"

#include <cstdio>
#include <cstring>
#include <string>

static int g_ok = 0, g_zle = 0;

#define SPRAWDZ(warunek)                                                      \
  do {                                                                        \
    if (warunek) {                                                            \
      g_ok++;                                                                 \
    } else {                                                                  \
      g_zle++;                                                                \
      std::printf("  NIE PRZESZLO  %s:%d  %s\n", __FILE__, __LINE__, #warunek); \
    }                                                                         \
  } while (0)

// Wycinek od wskazanego miejsca, do porownan w testach znajdzZlaWartosc().
static std::string odMiejsca(const char* p, size_t ile) {
  if (p == nullptr) return "(null)";
  return std::string(p).substr(0, ile);
}

static void testHttp() {
  using namespace diag;
  // --- kod z pierwszej linii
  SPRAWDZ(kodHttp("HTTP/1.1 200 OK\r\n\r\n{}") == 200);
  SPRAWDZ(kodHttp("HTTP/1.0 404 Not Found\r\n\r\n") == 404);
  SPRAWDZ(kodHttp("HTTP/1.1 500\r\n\r\n") == 500);
  SPRAWDZ(kodHttp("HTTP/1.1  302 Found\r\n") == 302);   // dwie spacje tez sie zdarzaja
  SPRAWDZ(kodHttp("") == 0);
  SPRAWDZ(kodHttp(nullptr) == 0);
  SPRAWDZ(kodHttp("{\"t_zewn\":1}") == 0);              // samo cialo bez naglowkow
  SPRAWDZ(kodHttp("HTTP/1.1") == 0);                    // ucieta linia statusu
  SPRAWDZ(kodHttp("HTTP/1.1 20") == 0);                 // za malo cyfr
  SPRAWDZ(kodHttp("HTTP/1.1 OK") == 0);

  // --- cialo
  SPRAWDZ(std::strcmp(cialoHttp("HTTP/1.1 200 OK\r\nA: b\r\n\r\n{\"x\":1}"), "{\"x\":1}") == 0);
  SPRAWDZ(cialoHttp("HTTP/1.1 200 OK\r\nA: b\r\n") == nullptr);   // naglowki bez pustej linii
  SPRAWDZ(cialoHttp(nullptr) == nullptr);

  // --- sprawdzOdpowiedz: kazda przyczyna ma WLASNY wynik
  int kod = -1;
  const char* cialo = (const char*)1;
  SPRAWDZ(sprawdzOdpowiedz("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}", &kod, &cialo) == WYNIK_OK);
  SPRAWDZ(kod == 200 && cialo != nullptr && std::strcmp(cialo, "{}") == 0);

  // To jest przypadek, ktory v1.1.0 przepuszczal po cichu: serwer zamknal
  // gniazdo, nie wysylajac NIC (warstwa sieciowa: "sukces").
  SPRAWDZ(sprawdzOdpowiedz("", &kod, &cialo) == WYNIK_NIE_HTTP);
  SPRAWDZ(kod == 0 && cialo == nullptr);

  SPRAWDZ(sprawdzOdpowiedz("HTTP/1.1 500 Internal Server Error\r\n\r\nblad", &kod, &cialo) == WYNIK_KOD_HTTP);
  SPRAWDZ(kod == 500 && cialo != nullptr);
  // Przekierowanie na "/" (onNotFound centrali) - tez nie sa to dane.
  SPRAWDZ(sprawdzOdpowiedz("HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n", &kod, &cialo) == WYNIK_KOD_HTTP);
  SPRAWDZ(kod == 302);

  SPRAWDZ(sprawdzOdpowiedz("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", &kod, &cialo) == WYNIK_BRAK_CIALA);
  SPRAWDZ(sprawdzOdpowiedz("HTTP/1.1 200 OK\r\nContent-Le", &kod, &cialo) == WYNIK_BRAK_CIALA);   // ucieta w naglowkach
  SPRAWDZ(kod == 200 && cialo == nullptr);

  // wskazniki wyjsciowe sa opcjonalne
  SPRAWDZ(sprawdzOdpowiedz("HTTP/1.1 200 OK\r\n\r\n{}", nullptr, nullptr) == WYNIK_OK);

  // --- teksty do logu: kazdy wynik ma inna, niepusta nazwe
  const Wynik wszystkie[] = {WYNIK_OK, WYNIK_NIE_HTTP, WYNIK_KOD_HTTP, WYNIK_BRAK_CIALA, WYNIK_JSON, WYNIK_TRANSPORT};
  for (size_t i = 0; i < 6; i++) {
    SPRAWDZ(std::strlen(wynikTekst(wszystkie[i])) > 0);
    for (size_t j = i + 1; j < 6; j++) SPRAWDZ(std::strcmp(wynikTekst(wszystkie[i]), wynikTekst(wszystkie[j])) != 0);
  }
}

static void testSkrotIOgon() {
  using namespace diag;
  char b[16];
  skrotDoLogu("{\"a\":1}\r\n", b, sizeof(b));
  SPRAWDZ(std::strcmp(b, "{.a.:1}..") == 0);            // cudzyslowy i CR/LF -> kropki (log jednolinijkowy)
  skrotDoLogu("0123456789ABCDEFGHIJ", b, sizeof(b));
  SPRAWDZ(std::strcmp(b, "0123456789ABCDE") == 0);      // obciecie do n-1 znakow
  skrotDoLogu(nullptr, b, sizeof(b));
  SPRAWDZ(b[0] == '\0');
  skrotDoLogu("", b, sizeof(b));
  SPRAWDZ(b[0] == '\0');
  char jeden[1] = {'x'};
  skrotDoLogu("abc", jeden, sizeof(jeden));
  SPRAWDZ(jeden[0] == '\0');                            // bufor 1-znakowy: sam terminator, bez wyjscia poza bufor
  skrotDoLogu("abc", nullptr, 0);                       // nie moze sie wywrocic

  SPRAWDZ(std::strcmp(ogon("0123456789", 4), "6789") == 0);
  SPRAWDZ(std::strcmp(ogon("abc", 10), "abc") == 0);
  SPRAWDZ(std::strcmp(ogon("", 4), "") == 0);
  SPRAWDZ(ogon(nullptr, 4) == nullptr);
}

static void testZlaWartosc() {
  using namespace diag;
  // --- poprawny JSON: nic do wskazania
  SPRAWDZ(znajdzZlaWartosc("{\"a\":1,\"b\":-2.5,\"c\":\"x\",\"d\":true,\"e\":false,\"f\":null,\"g\":{\"h\":[1,2]}}") == nullptr);
  SPRAWDZ(znajdzZlaWartosc("{ \"a\" : 1 ,\r\n \"b\" :\t\"x\" }") == nullptr);
  SPRAWDZ(znajdzZlaWartosc("{}") == nullptr);
  SPRAWDZ(znajdzZlaWartosc("") == nullptr);
  SPRAWDZ(znajdzZlaWartosc(nullptr) == nullptr);
  // dwukropek i "nan" WEWNATRZ napisu to zwykly tekst, nie blad
  SPRAWDZ(znajdzZlaWartosc("{\"ip\":\"192.168.4.1\",\"opis\":\"a:nan, b:inf\",\"x\":1}") == nullptr);
  // cudzyslow poprzedzony ukosnikiem nie konczy napisu
  SPRAWDZ(znajdzZlaWartosc("{\"a\":\"cytat \\\" : nan\",\"b\":2}") == nullptr);

  // --- to, co Arduino wypisuje dla float NaN/nieskonczonosc
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"t_zewn\":10.0,\"cisnienie\":nan,\"dym\":240}"), 15) == "\"cisnienie\":nan");
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":1,\"wilgotnosc\":inf}"), 16) == "\"wilgotnosc\":inf");
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":1,\"x\":-inf}"), 8) == "\"x\":-inf");
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":1,\"x\":NaN}"), 7) == "\"x\":NaN");
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":1,\"x\":-nan}"), 8) == "\"x\":-nan");
  // wsp::formatFloat() centrali testowej dla nie-liczby
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":1,\"t_bojler\":--,\"b\":2}"), 13) == "\"t_bojler\":--");
  // brak wartosci
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":,\"b\":2}"), 5) == "\"a\":,");
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":}"), 5) == "\"a\":}");
  // tresc ucieta tuz po dwukropku
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":1,\"dym\":"), 6) == "\"dym\":");
  // "nul"/"tru" to nie sa literaly
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":nul}"), 4) == "\"a\":");
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":truex}"), 4) == "\"a\":");
  // wskazywane jest PIERWSZE zle pole
  SPRAWDZ(odMiejsca(znajdzZlaWartosc("{\"a\":nan,\"b\":nan}"), 4) == "\"a\":");

  // --- tresc ucieta w srodku napisu: to nie "zla wartosc" (ArduinoJson
  //     zglosi IncompleteInput, a w logu widac koncowke)
  SPRAWDZ(znajdzZlaWartosc("{\"a\":1,\"wifi_ss") == nullptr);
}

static void testDlawik() {
  using namespace diag;
  Dlawik d;
  SPRAWDZ(d.pozwol(1000, 10000));          // pierwszy wpis zawsze przechodzi
  SPRAWDZ(!d.pozwol(3000, 10000));         // blad powtarza sie co 2 s...
  SPRAWDZ(!d.pozwol(5000, 10000));
  SPRAWDZ(!d.pozwol(7000, 10000));
  SPRAWDZ(!d.pozwol(9000, 10000));
  SPRAWDZ(d.pozwol(11000, 10000));         // ...a do logu trafia raz na 10 s
  SPRAWDZ(d.zabierzPominiete() == 4);      // z informacja, ile wpisow pominieto
  SPRAWDZ(d.zabierzPominiete() == 0);      // licznik sie zeruje

  // pierwszy wpis przechodzi takze przy millis() == 0
  Dlawik odZera;
  SPRAWDZ(odZera.pozwol(0, 10000));
  SPRAWDZ(!odZera.pozwol(1, 10000));

  // przewiniecie millis() (po ok. 49,7 dnia) nie blokuje logu na zawsze
  Dlawik w;
  SPRAWDZ(w.pozwol(0xFFFFFF00u, 10000));
  SPRAWDZ(!w.pozwol(0xFFFFFFF0u, 10000));
  SPRAWDZ(!w.pozwol(100u, 10000));                 // 356 ms po poprzednim (przez przewiniecie)
  SPRAWDZ(w.pozwol(0xFFFFFF00u + 10000u, 10000));  // dokladnie 10 s po poprzednim
}

static void testLicznikiIStraznik() {
  using namespace diag;
  Liczniki l;
  l.statusOk = 5; l.statusBlad = 1; l.ustawieniaOk = 2; l.ustawieniaBlad = 3; l.ekranOdswiezen = 5;
  l.zeruj();
  SPRAWDZ(l.statusOk == 0 && l.statusBlad == 0 && l.ustawieniaOk == 0 && l.ustawieniaBlad == 0 && l.ekranOdswiezen == 0);

  StraznikSwiezosci s;
  SPRAWDZ(!s.wlasniePrzeterminowane(false));   // dane swieze - nic
  SPRAWDZ(s.wlasniePrzeterminowane(true));     // prog przekroczony - czysc kafelki...
  SPRAWDZ(!s.wlasniePrzeterminowane(true));    // ...ale tylko RAZ, nie co obrot petli
  SPRAWDZ(!s.wlasniePrzeterminowane(true));
  SPRAWDZ(s.odnotujSwiezeDane());              // dane wrocily - do logu
  SPRAWDZ(!s.odnotujSwiezeDane());             // kolejny dobry odczyt to juz nie "powrot"
  SPRAWDZ(!s.wlasniePrzeterminowane(false));
  SPRAWDZ(s.wlasniePrzeterminowane(true));     // drugi zanik jest znow zglaszany
}

static void testWifi() {
  using namespace diag;
  char b[18];
  const uint8_t mac[6] = {0x24, 0x6F, 0x28, 0xAB, 0x0C, 0xD1};
  formatBssid(mac, b, sizeof(b));
  SPRAWDZ(std::strcmp(b, "24:6F:28:AB:0C:D1") == 0);
  formatBssid(nullptr, b, sizeof(b));
  SPRAWDZ(std::strcmp(b, "--:--:--:--:--:--") == 0);
  char krotki[6];
  formatBssid(mac, krotki, sizeof(krotki));
  SPRAWDZ(std::strlen(krotki) == 5);           // obciete, ale zakonczone zerem

  // Dwie centrale o tej samej nazwie: prawdziwa (daleko, kanal 1) i testowa
  // (na biurku, kanal 6). Szybki skan v1.1.0 bral pierwsza znaleziona -
  // v1.2.0 ma wziac silniejsza, niezaleznie od kolejnosci na liscie.
  KandydatAp dwie[2] = {{{1, 1, 1, 1, 1, 1}, 1, -78}, {{2, 2, 2, 2, 2, 2}, 6, -41}};
  SPRAWDZ(wybierzNajsilniejszy(dwie, 2) == 1);
  KandydatAp odwrotnie[2] = {{{2, 2, 2, 2, 2, 2}, 6, -41}, {{1, 1, 1, 1, 1, 1}, 1, -78}};
  SPRAWDZ(wybierzNajsilniejszy(odwrotnie, 2) == 0);
  KandydatAp remis[3] = {{{1, 0, 0, 0, 0, 0}, 1, -60}, {{2, 0, 0, 0, 0, 0}, 6, -60}, {{3, 0, 0, 0, 0, 0}, 11, -75}};
  SPRAWDZ(wybierzNajsilniejszy(remis, 3) == 0);   // remis: pierwszy z listy (wynik powtarzalny)
  SPRAWDZ(wybierzNajsilniejszy(dwie, 1) == 0);
  SPRAWDZ(wybierzNajsilniejszy(dwie, 0) == -1);

  // --- znacznik w naglowku dashboardu
  char z[40];
  znacznikCentrali(z, sizeof(z), false, 1, mac);
  SPRAWDZ(std::strcmp(z, "") == 0);                                // zwykla, jedna centrala: nic
  znacznikCentrali(z, sizeof(z), false, 0, nullptr);
  SPRAWDZ(std::strcmp(z, "") == 0);                                // nie skanowano: nic
  znacznikCentrali(z, sizeof(z), true, 1, mac);
  SPRAWDZ(std::strcmp(z, "SYMULATOR") == 0);
  znacznikCentrali(z, sizeof(z), false, 2, mac);
  SPRAWDZ(std::strcmp(z, "2 CENTRALE (0C:D1)") == 0);
  znacznikCentrali(z, sizeof(z), false, 2, nullptr);
  SPRAWDZ(std::strcmp(z, "2 CENTRALE") == 0);
  znacznikCentrali(z, sizeof(z), true, 3, mac);
  SPRAWDZ(std::strcmp(z, "SYMULATOR  3 CENTRALE (0C:D1)") == 0);
  char maly[8];
  znacznikCentrali(maly, sizeof(maly), true, 2, mac);
  SPRAWDZ(std::strcmp(maly, "SYMULAT") == 0);                      // obciecie bez wyjscia poza bufor
  znacznikCentrali(nullptr, 0, true, 2, mac);                      // nie moze sie wywrocic

  // --- symulacja czujnikow w prawdziwej centrali (pola "symulacja_*" w /api/status)
  // Kolejnosc bitow = kolejnosc CZUJNIKI_SYMULOWANE: 0 zewn, 1 pokoj, 2 piec, 3 bojler, ...
  SPRAWDZ(LICZBA_CZUJNIKOW_SYMULOWANYCH == 10);
  SPRAWDZ(std::strcmp(CZUJNIKI_SYMULOWANE[0].pole, "symulacja_zewn") == 0);
  SPRAWDZ(std::strcmp(CZUJNIKI_SYMULOWANE[2].pole, "symulacja_ogrz") == 0 && std::strcmp(CZUJNIKI_SYMULOWANE[2].skrot, "piec") == 0);
  char lista[64];
  listaSymulowanych(0, lista, sizeof(lista));
  SPRAWDZ(std::strcmp(lista, "") == 0);
  listaSymulowanych(0x0001, lista, sizeof(lista));
  SPRAWDZ(std::strcmp(lista, "zewn") == 0);                         // tak bylo w logu centrali z 2026-11-11 (zewn=18.5C)
  listaSymulowanych(0x0007, lista, sizeof(lista));
  SPRAWDZ(std::strcmp(lista, "zewn,pokoj,piec") == 0);
  listaSymulowanych(0x03FF, lista, sizeof(lista));
  SPRAWDZ(std::strcmp(lista, "zewn,pokoj,piec,bojler,panel,dym,wilg,cisn,powrot,trociny") == 0);
  listaSymulowanych(0x03FF, lista, sizeof(lista), 2, ", ");
  SPRAWDZ(std::strcmp(lista, "zewn, pokoj +8") == 0);
  listaSymulowanych(0xFC00, lista, sizeof(lista));
  SPRAWDZ(std::strcmp(lista, "") == 0);                             // bity spoza tabeli sa ignorowane
  char ciasny[8];
  listaSymulowanych(0x0007, ciasny, sizeof(ciasny));
  SPRAWDZ(std::strlen(ciasny) <= 7);                                // obciecie bez wyjscia poza bufor
  listaSymulowanych(0x0007, nullptr, 0);                            // nie moze sie wywrocic

  znacznikCentrali(z, sizeof(z), false, 1, mac, 0x0001);
  SPRAWDZ(std::strcmp(z, "SYMULACJA: zewn") == 0);
  znacznikCentrali(z, sizeof(z), false, 1, mac, 0x0003);
  SPRAWDZ(std::strcmp(z, "SYMULACJA: zewn, pokoj") == 0);
  znacznikCentrali(z, sizeof(z), false, 1, mac, 0x03FF);
  SPRAWDZ(std::strcmp(z, "SYMULACJA: zewn, pokoj +8") == 0);
  znacznikCentrali(z, sizeof(z), false, 2, mac, 0x0003);
  SPRAWDZ(std::strcmp(z, "SYMULACJA: zewn +1  2 CENTRALE (0C:D1)") == 0);   // z druga czescia: jedna nazwa
  SPRAWDZ(std::strlen(z) <= 39);
  znacznikCentrali(z, sizeof(z), true, 1, mac, 0x0003);
  SPRAWDZ(std::strcmp(z, "SYMULATOR") == 0);                        // symulator C3 ma pierwszenstwo
  znacznikCentrali(z, sizeof(z), false, 1, mac, 0);
  SPRAWDZ(std::strcmp(z, "") == 0);
}

static void testBateria() {
  using namespace diag;
  SPRAWDZ(!bateriaObecna(0.28f));   // odczyt z logu 2026-11-10: sam przetwornik, bez ogniwa
  SPRAWDZ(!bateriaObecna(0.0f));
  SPRAWDZ(!bateriaObecna(2.49f));
  SPRAWDZ(bateriaObecna(2.5f));
  SPRAWDZ(bateriaObecna(3.27f));    // 0% wg TABELA_BAT w main.cpp - rozladowane ogniwo to wciaz ogniwo
  SPRAWDZ(bateriaObecna(4.20f));
}

// --- v1.2.1: brak danych - przyczyna, drabina ratunkowa, pamiec autorestartu ---
static void testPrzyczyna() {
  using namespace diag;
  SPRAWDZ(ustalPrzyczyne(false, false) == PRZYCZYNA_WIFI);
  SPRAWDZ(ustalPrzyczyne(false, true) == PRZYCZYNA_WIFI);        // bez WiFi rodzaj ostatniego bledu nie ma znaczenia
  SPRAWDZ(ustalPrzyczyne(true, false) == PRZYCZYNA_MILCZY);
  SPRAWDZ(ustalPrzyczyne(true, true) == PRZYCZYNA_ODPOWIEDZ);
  SPRAWDZ(std::strcmp(przyczynaNapis(PRZYCZYNA_WIFI), "BRAK WIFI") == 0);
  SPRAWDZ(std::strcmp(przyczynaNapis(PRZYCZYNA_MILCZY), "CENTRALA MILCZY") == 0);
  SPRAWDZ(std::strcmp(przyczynaNapis(PRZYCZYNA_ODPOWIEDZ), "ZLA ODPOWIEDZ") == 0);
  SPRAWDZ(std::strcmp(przyczynaNapis(PRZYCZYNA_BRAK), "") == 0);
  SPRAWDZ(std::strcmp(przyczynaSkrot(PRZYCZYNA_WIFI), "wifi") == 0);
  SPRAWDZ(std::strcmp(przyczynaSkrot(PRZYCZYNA_MILCZY), "milczy") == 0);
  SPRAWDZ(std::strcmp(przyczynaSkrot(PRZYCZYNA_ODPOWIEDZ), "odp") == 0);

  // [v1.2.2 PAMIEC] Panelowi zabraklo pamieci - osobna przyczyna (log z
  // 2026-11-12: "json=NoMemory" bylo zgloszone jako wina centrali, "odp").
  SPRAWDZ(ustalPrzyczyne(true, true, true) == PRZYCZYNA_PAMIEC);    // odpowiedz przyszla, zabraklo pamieci na jej odczytanie
  SPRAWDZ(ustalPrzyczyne(true, false, true) == PRZYCZYNA_PAMIEC);   // socket()/connect() odrzucone z braku pamieci
  SPRAWDZ(ustalPrzyczyne(false, true, true) == PRZYCZYNA_WIFI);     // bez WiFi nadal "brak WiFi"
  SPRAWDZ(ustalPrzyczyne(true, true, false) == PRZYCZYNA_ODPOWIEDZ);
  SPRAWDZ(std::strcmp(przyczynaNapis(PRZYCZYNA_PAMIEC), "BRAK PAMIECI") == 0);
  SPRAWDZ(std::strcmp(przyczynaSkrot(PRZYCZYNA_PAMIEC), "pam") == 0);
  SPRAWDZ(std::strcmp(przyczynaNapis(PRZYCZYNA_LVGL), "BLAD EKRANU") == 0);
  SPRAWDZ(std::strcmp(przyczynaSkrot(PRZYCZYNA_LVGL), "lvgl") == 0);
  // Wartosci liczbowe sa zapisywane w pamieci RTC - nie moga sie przesunac.
  SPRAWDZ(PRZYCZYNA_BRAK == 0 && PRZYCZYNA_WIFI == 1 && PRZYCZYNA_MILCZY == 2 && PRZYCZYNA_ODPOWIEDZ == 3 &&
          PRZYCZYNA_PAMIEC == 4 && PRZYCZYNA_LVGL == 5);
  SPRAWDZ(bladGniazdaToBrakPamieci(12) && bladGniazdaToBrakPamieci(105) && bladGniazdaToBrakPamieci(23) && bladGniazdaToBrakPamieci(24));
  SPRAWDZ(!bladGniazdaToBrakPamieci(0));     // limit czasu
  SPRAWDZ(!bladGniazdaToBrakPamieci(118));   // EHOSTUNREACH - brak drogi do centrali
  SPRAWDZ(!bladGniazdaToBrakPamieci(104) && !bladGniazdaToBrakPamieci(113) && !bladGniazdaToBrakPamieci(-1));
  SPRAWDZ(RAM_PROG_OSTRZEZENIA_B == 16000UL);

  SPRAWDZ(std::strcmp(nazwaKoduRozlaczenia(201), "NO_AP_FOUND") == 0);
  SPRAWDZ(std::strcmp(nazwaKoduRozlaczenia(200), "BEACON_TIMEOUT") == 0);
  SPRAWDZ(std::strcmp(nazwaKoduRozlaczenia(8), "ASSOC_LEAVE") == 0);
  SPRAWDZ(std::strcmp(nazwaKoduRozlaczenia(15), "4WAY_HANDSHAKE_TIMEOUT") == 0);
  SPRAWDZ(std::strcmp(nazwaKoduRozlaczenia(0), "brak") == 0);
  SPRAWDZ(std::strcmp(nazwaKoduRozlaczenia(123), "?") == 0);       // nieznany kod nie wywraca programu

  char z[48];
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_MILCZY, 14, 31);
  SPRAWDZ(std::strcmp(z, "CENTRALA MILCZY 14 s") == 0);
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_WIFI, 34, 11);
  SPRAWDZ(std::strcmp(z, "BRAK WIFI 34 s") == 0);                  // 11 s do restartu: jeszcze zwykly napis
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_WIFI, 35, 10);
  SPRAWDZ(std::strcmp(z, "RESTART ZA 10 s: BRAK WIFI") == 0);      // od 10 s: odliczanie
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_ODPOWIEDZ, 45, 0);
  SPRAWDZ(std::strcmp(z, "RESTART ZA 0 s: ZLA ODPOWIEDZ") == 0);
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_PAMIEC, 14, 31);
  SPRAWDZ(std::strcmp(z, "BRAK PAMIECI 14 s") == 0);
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_PAMIEC, 38, 7);
  SPRAWDZ(std::strcmp(z, "RESTART ZA 7 s: BRAK PAMIECI") == 0);
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_BRAK, 3, 42);
  SPRAWDZ(std::strcmp(z, "") == 0);                                // dane sa - nic do pokazania
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_MILCZY, 299, 300);
  SPRAWDZ(std::strlen(z) <= 31);                                   // najdluzszy zwykly napis
  znacznikBrakuDanych(z, sizeof(z), PRZYCZYNA_ODPOWIEDZ, 200, BEZ_RESTARTU);
  SPRAWDZ(std::strcmp(z, "ZLA ODPOWIEDZ 200 s") == 0);             // restartu nie bedzie - bez odliczania
  char ciasny[10];
  znacznikBrakuDanych(ciasny, sizeof(ciasny), PRZYCZYNA_MILCZY, 14, 3);
  SPRAWDZ(std::strlen(ciasny) == 9);                               // obciecie bez wyjscia poza bufor
  znacznikBrakuDanych(nullptr, 0, PRZYCZYNA_MILCZY, 1, 1);         // nie moze sie wywrocic
}

static void testRatunek() {
  using namespace diag;
  SPRAWDZ(progRestartuMs(0) == 45000UL);
  SPRAWDZ(progRestartuMs(1) == 90000UL);
  SPRAWDZ(progRestartuMs(2) == 180000UL);
  SPRAWDZ(progRestartuMs(3) == 300000UL);
  SPRAWDZ(progRestartuMs(250) == 300000UL);                        // dalej juz zawsze co 5 min

  // 1) Zwykla praca: status co 2 s -> nigdy zadnej akcji.
  {
    Ratunek r;
    bool cos = false;
    for (uint32_t t = 0; t <= 600000; t += 5) {
      if (r.krok(t, true, 45000) != Ratunek::NIC) cos = true;
      if (t % 2000 == 0) r.daneSwieze();
    }
    SPRAWDZ(!cos);
    SPRAWDZ(r.bezDanychMs < 2100 && r.wymuszen == 0);
  }
  // 2) WiFi polaczone, centrala milczy: wymuszenie po 12 s, drugie po kolejnych
  //    12 s milczenia (bez BSSID), restart w 45. sekundzie - kazda akcja raz.
  {
    Ratunek r;
    uint32_t t1 = 0, t2 = 0, tRestart = 0;
    int wymuszen = 0, restartow = 0;
    for (uint32_t t = 0; t <= 60000; t += 5) {
      const Ratunek::Akcja a = r.krok(t, true, 45000);
      if (a == Ratunek::POLACZ_PONOWNIE) { t1 = t; wymuszen++; }
      if (a == Ratunek::POLACZ_PONOWNIE_BEZ_BSSID) { t2 = t; wymuszen++; }
      if (a == Ratunek::RESTART) { tRestart = t; restartow++; }
    }
    SPRAWDZ(t1 == 12000);
    SPRAWDZ(t2 == 24000);
    SPRAWDZ(wymuszen == 2);                                         // trzeciego wymuszenia nie ma
    SPRAWDZ(tRestart == 45000 && restartow == 1);
    SPRAWDZ(r.doRestartuMs(45000) == 0);
  }
  // 3) Bez WiFi: zadnego wymuszania (dziala zwykle ponawianie), tylko restart.
  {
    Ratunek r;
    int wymuszen = 0;
    uint32_t tRestart = 0;
    for (uint32_t t = 0; t <= 50000; t += 5) {
      const Ratunek::Akcja a = r.krok(t, false, 45000);
      if (a == Ratunek::POLACZ_PONOWNIE || a == Ratunek::POLACZ_PONOWNIE_BEZ_BSSID) wymuszen++;
      if (a == Ratunek::RESTART && tRestart == 0) tRestart = t;
    }
    SPRAWDZ(wymuszen == 0);
    SPRAWDZ(tRestart == 45000);
  }
  // 4) WiFi na przemian jest i go nie ma: licznik milczenia liczy tylko
  //    NIEPRZERWANE milczenie przy polaczonym WiFi.
  {
    Ratunek r;
    int wymuszen = 0;
    for (uint32_t t = 0; t <= 40000; t += 5) {
      const bool wifi = ((t / 8000) % 2) == 0;                      // 8 s jest, 8 s nie ma
      const Ratunek::Akcja a = r.krok(t, wifi, 45000);
      if (a == Ratunek::POLACZ_PONOWNIE || a == Ratunek::POLACZ_PONOWNIE_BEZ_BSSID) wymuszen++;
    }
    SPRAWDZ(wymuszen == 0);
    SPRAWDZ(r.bezDanychMs == 40000);                                // czas bez danych liczy sie caly czas
  }
  // 5) Petla zatrzymana na dlugo (pobieranie wykresow, OTA): jedna przerwa
  //    liczy sie najwyzej jako 1 s, wiec zaraz po niej nie ma restartu.
  {
    Ratunek r;
    r.daneSwieze();
    SPRAWDZ(r.krok(1000, true, 45000) == Ratunek::NIC);
    SPRAWDZ(r.krok(1005, true, 45000) == Ratunek::NIC);
    SPRAWDZ(r.krok(121005, true, 45000) == Ratunek::NIC);           // 2 minuty przerwy w wywolaniach
    SPRAWDZ(r.bezDanychMs == 1005);
    SPRAWDZ(r.doRestartuMs(45000) == 43995);
  }
  // 6) Dane wracaja w trakcie: wszystko od poczatku.
  {
    Ratunek r;
    for (uint32_t t = 0; t <= 30000; t += 5) r.krok(t, true, 45000);
    SPRAWDZ(r.wymuszen == 2 && r.bezDanychMs == 30000);
    r.daneSwieze();
    SPRAWDZ(r.wymuszen == 0 && r.bezDanychMs == 0 && r.milczyMs == 0);
    Ratunek::Akcja a = Ratunek::NIC;
    for (uint32_t t = 30005; t <= 42005 && a == Ratunek::NIC; t += 5) a = r.krok(t, true, 45000);
    SPRAWDZ(a == Ratunek::POLACZ_PONOWNIE);                         // znow najpierw lagodniejszy krok
  }
  // 7) Dluzszy prog (po wczesniejszym samoczynnym restarcie).
  {
    Ratunek r;
    uint32_t tRestart = 0;
    for (uint32_t t = 0; t <= 100000 && tRestart == 0; t += 5)
      if (r.krok(t, false, progRestartuMs(1)) == Ratunek::RESTART) tRestart = t;
    SPRAWDZ(tRestart == 90000);
  }
  // 7a) Restart odrzucony przez wolajacego: krok() zglasza go znowu.
  {
    Ratunek r;
    int zgloszen = 0;
    for (uint32_t t = 0; t <= 46000; t += 5) {
      if (r.krok(t, false, 45000) == Ratunek::RESTART) { zgloszen++; r.restartOdrzucony(); }
    }
    SPRAWDZ(zgloszen == 201);                                       // 45000..46000 co 5 ms - przy kazdym obrocie
  }
  SPRAWDZ(restartMaSens(PRZYCZYNA_WIFI, 0) && restartMaSens(PRZYCZYNA_WIFI, 200));
  SPRAWDZ(restartMaSens(PRZYCZYNA_MILCZY, 0) && restartMaSens(PRZYCZYNA_MILCZY, 200));
  SPRAWDZ(restartMaSens(PRZYCZYNA_ODPOWIEDZ, 0) && restartMaSens(PRZYCZYNA_ODPOWIEDZ, 1));
  SPRAWDZ(!restartMaSens(PRZYCZYNA_ODPOWIEDZ, 2) && !restartMaSens(PRZYCZYNA_ODPOWIEDZ, 9));
  // Brak pamieci to wina panelu - restart pomaga zawsze, bez ograniczenia.
  SPRAWDZ(restartMaSens(PRZYCZYNA_PAMIEC, 0) && restartMaSens(PRZYCZYNA_PAMIEC, 200));
  SPRAWDZ(restartMaSens(PRZYCZYNA_LVGL, 0) && restartMaSens(PRZYCZYNA_LVGL, 200));
  SPRAWDZ(CZAS_STABILNYCH_DANYCH_MS == 120000UL);
  // 8) Przewiniecie zegara millis() (po ok. 49 dniach) nie robi skoku.
  {
    Ratunek r;
    r.krok(0xFFFFFFF0UL, false, 45000);
    r.krok(0x00000004UL, false, 45000);                             // 20 ms pozniej
    SPRAWDZ(r.bezDanychMs == 20);
  }
}

static void testPamiecAutorestartu() {
  using namespace diag;
  PamiecAutorestartu m;
  std::memset(&m, 0xA5, sizeof(m));                                 // przypadkowe wartosci po wlaczeniu zasilania
  SPRAWDZ(!m.wazna());
  std::memset(&m, 0x00, sizeof(m));
  SPRAWDZ(!m.wazna());                                              // same zera to tez nie jest wazny zapis
  m.wyczysc();
  SPRAWDZ(m.wazna());
  SPRAWDZ(m.oczekuje == 0 && m.kolejnych == 0 && m.lacznie == 0);
  m.kolejnych = 2;
  SPRAWDZ(!m.wazna());                                              // zmiana bez zapieczetuj() = zapis niewazny
  m.zapieczetuj();
  SPRAWDZ(m.wazna());
  m.bladNumer ^= 1;
  SPRAWDZ(!m.wazna());                                              // nowe pola tez sa objete suma
  m.bladNumer ^= 1;
  SPRAWDZ(m.wazna());
  m.seria ^= 1;
  SPRAWDZ(!m.wazna());
  m.seria ^= 1;
  m.zgloszonyNr ^= 1;
  SPRAWDZ(!m.wazna());
  m.zgloszonyNr ^= 1;
  SPRAWDZ(m.wazna());
  SPRAWDZ(sizeof(PamiecAutorestartu) == 28);                        // uklad pamieci RTC - zmiana wymaga nowej wartosci MAGIA

  m.wyczysc();
  m.przyczyna = PRZYCZYNA_MILCZY; m.kodRozlaczenia = 0; m.kolejnych = 1; m.seria = 1; m.bezDanychS = 45; m.heapKb = 61; m.blokKb = 31;
  m.bladEtap = 'l'; m.bladNumer = 0;
  char t[TOKEN_AUTORESTARTU_MAKS];
  tokenAutorestartu(t, sizeof(t), m);
  SPRAWDZ(std::strcmp(t, "auto_milczy_r0_n1_t45_h61_b31_el0") == 0);
  m.przyczyna = PRZYCZYNA_WIFI; m.kodRozlaczenia = 201; m.bladEtap = 'n'; m.bladNumer = 118;
  tokenAutorestartu(t, sizeof(t), m);
  SPRAWDZ(std::strcmp(t, "auto_wifi_r201_n1_t45_h61_b31_en118") == 0);
  m.bladEtap = '-';
  tokenAutorestartu(t, sizeof(t), m);
  SPRAWDZ(std::strcmp(t, "auto_wifi_r201_n1_t45_h61_b31_ex118") == 0);   // '-' nie moze trafic do adresu zapytania
  // [v1.2.2] brak pamieci na odczytanie odpowiedzi; blad wewnetrzny LVGL
  m.przyczyna = PRZYCZYNA_PAMIEC; m.kodRozlaczenia = 0; m.heapKb = 1; m.blokKb = 0; m.bladEtap = 'm'; m.bladNumer = 200;
  tokenAutorestartu(t, sizeof(t), m);
  SPRAWDZ(std::strcmp(t, "auto_pam_r0_n1_t45_h1_b0_em200") == 0);
  m.bladNumer = 12;   // ...i juz na jej odebranie (NonBlockingHttp: etap 'm', ENOMEM)
  tokenAutorestartu(t, sizeof(t), m);
  SPRAWDZ(std::strcmp(t, "auto_pam_r0_n1_t45_h1_b0_em12") == 0);
  m.przyczyna = PRZYCZYNA_LVGL; m.bezDanychS = 0; m.heapKb = 0; m.bladEtap = 'v'; m.bladNumer = 0;
  tokenAutorestartu(t, sizeof(t), m);
  SPRAWDZ(std::strcmp(t, "auto_lvgl_r0_n1_t0_h0_b0_ev0") == 0);
  // Najgorszy przypadek: wszystkie pola maksymalne - miesci sie i ma tylko dozwolone znaki.
  m.przyczyna = PRZYCZYNA_MILCZY; m.kodRozlaczenia = 255; m.kolejnych = 250; m.seria = 250; m.bezDanychS = 65535; m.heapKb = 65535; m.blokKb = 65535;
  m.bladEtap = 'c'; m.bladNumer = 65535;
  tokenAutorestartu(t, sizeof(t), m);
  SPRAWDZ(std::strlen(t) == 50);
  bool dozwolone = true;
  for (const char* p = t; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) dozwolone = false;
  SPRAWDZ(dozwolone);
  SPRAWDZ(etapDoTokenu('l') == 'l' && etapDoTokenu('-') == 'x' && etapDoTokenu(0) == 'x' && etapDoTokenu('&') == 'x');

  // Seria restartow: ktore z nich panel zglasza centrali.
  SPRAWDZ(zglosicRestart(1, 0) && zglosicRestart(2, 1) && zglosicRestart(3, 2));        // pierwsze trzy zawsze
  SPRAWDZ(!zglosicRestart(4, 3) && !zglosicRestart(5, 3) && zglosicRestart(6, 3));       // potem dopiero numer 2x wiekszy
  SPRAWDZ(!zglosicRestart(11, 6) && zglosicRestart(12, 6) && zglosicRestart(13, 6));     // niedostarczone zgloszenie nie przepada
  SPRAWDZ(zglosicRestart(37, 0) && zglosicRestart(250, 0));                              // centrala nie wie o zadnym - zglos dowolny
  SPRAWDZ(zglosicRestart(250, 125) && !zglosicRestart(250, 126) && !zglosicRestart(250, 250));
  {
    // Przebieg: 300 restartow jeden po drugim, centrala za kazdym razem odpowiada.
    PamiecAutorestartu r;
    r.wyczysc();
    r.przyczyna = PRZYCZYNA_WIFI; r.kodRozlaczenia = 201; r.bezDanychS = 45; r.heapKb = 61; r.blokKb = 31; r.bladEtap = 'n'; r.bladNumer = 118;
    int zgloszen = 0;
    bool zawszeWazna = true;
    std::string numery;
    for (int nr = 1; nr <= 300; nr++) {
      r.odnotujRestart();
      r.zapieczetuj();
      if (!(r.wazna() && r.oczekuje == 1)) zawszeWazna = false;
      r.oczekuje = 0;
      if (zglosicRestart(r.seria, r.zgloszonyNr)) {
        zgloszen++;
        numery += std::to_string(r.seria) + " ";
        r.zgloszenieDotarlo();
        if (!r.wazna()) zawszeWazna = false;
      }
    }
    SPRAWDZ(zawszeWazna);
    SPRAWDZ(numery == "1 2 3 6 12 24 48 96 192 ");                 // 9 zgloszen na 300 restartow
    SPRAWDZ(zgloszen == 9);
    SPRAWDZ(r.seria == 250 && r.kolejnych == 250 && r.lacznie == 300 && r.zgloszonyNr == 192);   // nasycenie licznikow

    // Dobre dane: 2 min zeruja "z rzedu", godzina konczy serie.
    SPRAWDZ(!r.poDobrychDanych(119999) && r.kolejnych == 250 && r.seria == 250);
    SPRAWDZ(r.poDobrychDanych(120000) && r.kolejnych == 0 && r.seria == 250 && r.zgloszonyNr == 192 && r.wazna());
    SPRAWDZ(!r.poDobrychDanych(3599999) && r.seria == 250);
    SPRAWDZ(r.poDobrychDanych(3600000) && r.seria == 0 && r.zgloszonyNr == 0 && r.wazna());
    SPRAWDZ(!r.poDobrychDanych(4000000));                           // nic do wyzerowania - bez zmian
    r.odnotujRestart();
    tokenAutorestartu(t, sizeof(t), r);
    SPRAWDZ(std::strcmp(t, "auto_wifi_r201_n1_t45_h61_b31_en118") == 0 && zglosicRestart(r.seria, r.zgloszonyNr));   // nowa seria: znow od 1
    SPRAWDZ(r.lacznie == 301);

    // Centrala wylaczona na dlugo: nic nie dociera, a gdy wroci, panel zglasza ostatni restart.
    r.wyczysc();
    r.przyczyna = PRZYCZYNA_WIFI; r.kodRozlaczenia = 201; r.bezDanychS = 300; r.heapKb = 61; r.blokKb = 31; r.bladEtap = 'n'; r.bladNumer = 118;
    for (int nr = 1; nr <= 37; nr++) r.odnotujRestart();
    SPRAWDZ(r.seria == 37 && r.zgloszonyNr == 0 && zglosicRestart(r.seria, r.zgloszonyNr));
    tokenAutorestartu(t, sizeof(t), r);
    SPRAWDZ(std::strcmp(t, "auto_wifi_r201_n37_t300_h61_b31_en118") == 0);
    r.zgloszenieDotarlo();
    SPRAWDZ(r.zgloszonyNr == 37 && r.wazna());
  }

  // Stan alarmow w jednym bajcie.
  SPRAWDZ(spakujAlarmy(false, false, false, false, 0, 0) == 0x00);
  SPRAWDZ(spakujAlarmy(true, false, false, false, 0, 0) == 0x01);
  SPRAWDZ(spakujAlarmy(false, true, false, false, 0, 0) == 0x02);
  SPRAWDZ(spakujAlarmy(false, false, true, false, 0, 0) == 0x04);
  SPRAWDZ(spakujAlarmy(false, true, false, true, 2, 1) == 0x6A);
  const uint8_t a = spakujAlarmy(true, false, true, true, 1, 2);
  SPRAWDZ(alarmyDym(a) && !alarmyOgrzewanie(a) && alarmyPanel(a) && alarmyWyciszony(a));
  SPRAWDZ(alarmyWyciszonyTyp(a) == 1 && alarmyOdrzucony(a) == 2 && alarmyJakikolwiek(a));
  SPRAWDZ(!alarmyJakikolwiek(spakujAlarmy(false, false, false, true, 2, 2)));   // samo wyciszenie to nie alarm
  SPRAWDZ(alarmyWyciszonyTyp(0xFF) == 0 && alarmyOdrzucony(0xFF) == 0);         // wartosc 3 spoza zakresu -> 0
  m.wyczysc();
  m.alarmy = 0x02;
  SPRAWDZ(!m.wazna());                                              // pole alarmy tez jest objete suma
  m.zapieczetuj();
  SPRAWDZ(m.wazna());
}

// --- [v1.4.0 OTA] ------------------------------------------------------------
static diag::OtaWarunki otaWarunkiDobre() {
  diag::OtaWarunki w;
  w.alarm = false;
  w.daneSwieze = true;
  w.wifi = true;
  w.siecZajeta = false;
  w.odStartuMs = 60000;
  w.bezDotykuMs = 60000;
  w.bateriaJest = false;
  w.zasilanieUsb = true;
  w.bateriaProcent = 0;
  w.ramWolny = 45000;
  w.ramBlok = 30000;
  w.bylaProba = false;
  w.odOstatniejProbyMs = 0;
  return w;
}

static void testOtaNarzedzia() {
  using namespace diag;
  // --- identyfikator obrazu z sumy SHA-256
  const uint8_t suma[32] = {0x00, 0x1f, 0xa0, 0xff, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
  char id[17];
  otaObrazZSumy(suma, id);
  SPRAWDZ(strcmp(id, "001fa0ff12345678") == 0);   // tylko 8 pierwszych bajtow, male litery
  SPRAWDZ(otaObrazPoprawny(id));
  SPRAWDZ(!otaObrazPoprawny(""));
  SPRAWDZ(!otaObrazPoprawny(nullptr));
  SPRAWDZ(!otaObrazPoprawny("001fa0ff1234567"));     // 15 znakow
  SPRAWDZ(!otaObrazPoprawny("001fa0ff123456789"));   // 17 znakow
  SPRAWDZ(!otaObrazPoprawny("001FA0FF12345678"));    // wielkie litery
  SPRAWDZ(!otaObrazPoprawny("001fa0ff1234567g"));
  SPRAWDZ(!otaObrazPoprawny("001fa0ff 2345678"));

  // --- kodowanie hasla do adresu
  char b[64];
  SPRAWDZ(urlKoduj("Haslo-OTA_1.2~x", b, sizeof(b)) && strcmp(b, "Haslo-OTA_1.2~x") == 0);   // zwykle haslo: bez zmian
  SPRAWDZ(urlKoduj("a b&c=d+e%f#g?h/i", b, sizeof(b)) && strcmp(b, "a%20b%26c%3Dd%2Be%25f%23g%3Fh%2Fi") == 0);
  SPRAWDZ(urlKoduj("z\xC5\x82oty", b, sizeof(b)) && strcmp(b, "z%C5%82oty") == 0);            // znak spoza ASCII (UTF-8)
  SPRAWDZ(urlKoduj("", b, sizeof(b)) && b[0] == '\0');
  SPRAWDZ(urlKoduj(nullptr, b, sizeof(b)) && b[0] == '\0');
  char maly[6];
  SPRAWDZ(urlKoduj("abcde", maly, sizeof(maly)) && strcmp(maly, "abcde") == 0);   // dokladnie na styk
  SPRAWDZ(!urlKoduj("abcdef", maly, sizeof(maly)) && maly[0] == '\0');            // o jeden za duzo
  SPRAWDZ(urlKoduj("ab&", maly, sizeof(maly)) && strcmp(maly, "ab%26") == 0);     // znak kodowany liczy sie za trzy
  SPRAWDZ(!urlKoduj("ab&c", maly, sizeof(maly)) && maly[0] == '\0');
  SPRAWDZ(!urlKoduj("abc&", maly, sizeof(maly)) && maly[0] == '\0');              // "%26" nie moze wejsc w polowie
  SPRAWDZ(!urlKoduj("a", nullptr, 10));
  SPRAWDZ(!urlKoduj("a", b, 0));

  // --- teksty powodow: te same, ktore zna centrala (panelOtaOpisOdmowy)
  SPRAWDZ(strcmp(otaPowodTekst(OTA_POWOD_POBIERANIE), "pobieranie") == 0);
  SPRAWDZ(strcmp(otaPowodTekst(OTA_POWOD_OBRAZ), "obraz") == 0);
  SPRAWDZ(strcmp(otaPowodTekst(OTA_POWOD_COFNIETA), "cofnieta") == 0);
  SPRAWDZ(strcmp(otaPowodTekst(OTA_POWOD_ZA_DUZY), "za_duzy") == 0);
  SPRAWDZ(strcmp(otaPowodTekst(OTA_POWOD_HASLO), "haslo") == 0);
  SPRAWDZ(strcmp(otaPowodTekst(OTA_POWOD_ZAPIS), "zapis") == 0);
  SPRAWDZ(strcmp(otaPowodTekst(200), "inny") == 0);

  // --- klucz oferty
  char k[OTA_KLUCZ_MAKS];
  otaKlucz(k, sizeof(k), true, 7, "001fa0ff12345678", "3.58.6");
  SPRAWDZ(strcmp(k, "n7:001fa0ff12345678") == 0);
  otaKlucz(k, sizeof(k), true, 4294967295UL, "001fa0ff12345678", "x");
  SPRAWDZ(strcmp(k, "n4294967295:001fa0ff12345678") == 0);   // najdluzszy numer tez sie miesci
  otaKlucz(k, sizeof(k), false, 7, "", "3.58.6");
  SPRAWDZ(strcmp(k, "t:3.58.6") == 0);
  otaKlucz(k, sizeof(k), false, 0, "", "bardzo-dlugi-tag-wydania-ktory-ma-ponad-31-znakow");
  SPRAWDZ(strcmp(k, "t:bardzo-dlugi-tag-wydania-ktory-") == 0 && strlen(k) == 33);    // tag obciety do 31 znakow
  otaKlucz(k, sizeof(k), false, 0, nullptr, nullptr);
  SPRAWDZ(strcmp(k, "t:") == 0);

  // --- adres zapytania o status
  char s[200];
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "poweron", "", true, false, 0, 0));
  SPRAWDZ(strcmp(s, "/api/status?panel_fw=v1.4.0&reset_powod=poweron") == 0);   // bez identyfikatora: jak do v1.3.0
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "sw", "001fa0ff12345678", true, false, 0, 0));
  SPRAWDZ(strcmp(s, "/api/status?panel_fw=v1.4.0&reset_powod=sw&panel_obraz=001fa0ff12345678") == 0);
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "sw", "001fa0ff12345678", false, false, 0, 0));
  SPRAWDZ(strcmp(s, "/api/status?panel_fw=v1.4.0&reset_powod=sw&panel_obraz_proba=001fa0ff12345678") == 0);
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "sw", "001fa0ff12345678", true, true, 12, OTA_POWOD_COFNIETA));
  SPRAWDZ(strcmp(s, "/api/status?panel_fw=v1.4.0&reset_powod=sw&panel_obraz=001fa0ff12345678&panel_ota_odmowa=12-cofnieta") == 0);
  // najdluzszy adres: token autorestartu (50 znakow) + obraz + odmowa z najdluzszym numerem i powodem
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "auto_milczy_r201_n255_t65535_h65535_b65535_eo65535", "001fa0ff12345678", false, true,
                            4294967295UL, OTA_POWOD_POBIERANIE));
  SPRAWDZ(strlen(s) == 164);   // bufor w main.cpp ma 200 znakow
  // powod czekania: osobny parametr; pusty albo nullptr = bez parametru
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "sw", "001fa0ff12345678", true, false, 0, 0, "bateria"));
  SPRAWDZ(strcmp(s, "/api/status?panel_fw=v1.4.0&reset_powod=sw&panel_obraz=001fa0ff12345678&panel_ota_czeka=bateria") == 0);
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "sw", "001fa0ff12345678", true, false, 0, 0, ""));
  SPRAWDZ(strcmp(s, "/api/status?panel_fw=v1.4.0&reset_powod=sw&panel_obraz=001fa0ff12345678") == 0);
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "sw", "001fa0ff12345678", true, false, 0, 0, nullptr));
  SPRAWDZ(strcmp(s, "/api/status?panel_fw=v1.4.0&reset_powod=sw&panel_obraz=001fa0ff12345678") == 0);
  // najdluzszy adres z powodem czekania (najdluzsze powody maja 11 znakow) - tez miesci sie w 200
  SPRAWDZ(otaSciezkaStatusu(s, sizeof(s), "v1.4.0", "auto_milczy_r201_n255_t65535_h65535_b65535_eo65535", "001fa0ff12345678", false, false, 0, 0,
                            "siec_zajeta"));
  SPRAWDZ(strlen(s) == 153);
  // kazdy powod, ktory zwraca otaDecyzja() przy CZEKAJ, sklada sie ze znakow a-z i _ i ma najwyzej 15 znakow
  // (tyle przyjmuje centrala: panelOtaCzytajSlowo() w main_centrala.cpp 3.58.6)
  for (const char* powod : {"alarm", "brak_wifi", "brak_danych", "po_starcie", "odstep", "siec_zajeta", "dotyk", "bateria", "pamiec"}) {
    bool dobry = strlen(powod) >= 1 && strlen(powod) <= 15;
    for (const char* z = powod; *z != '\0'; z++) dobry = dobry && ((*z >= 'a' && *z <= 'z') || *z == '_');
    SPRAWDZ(dobry);
  }
  char ciasny[60];
  SPRAWDZ(!otaSciezkaStatusu(ciasny, sizeof(ciasny), "v1.4.0", "sw", "001fa0ff12345678", true, false, 0, 0));   // nie miesci sie: false
  SPRAWDZ(!otaSciezkaStatusu(nullptr, 10, "v", "sw", "", true, false, 0, 0));

  // --- pierwsze 24 i ostatnie 32 bajty pliku, podawanego kawalkami dowolnej dlugosci
  uint8_t plik[300];
  for (int i = 0; i < 300; i++) plik[i] = (uint8_t)(i * 7 + 3);
  plik[0] = 0xE9; plik[12] = 0; plik[13] = 0; plik[23] = 1;
  for (int krok : {1, 2, 5, 23, 24, 31, 32, 33, 64, 299, 300}) {
    OtaSkan sk;
    otaSkanStart(sk);
    for (int od = 0; od < 300; od += krok) otaSkanDodaj(sk, plik + od, (size_t)(od + krok <= 300 ? krok : 300 - od));
    SPRAWDZ(sk.lacznie == 300);
    SPRAWDZ(memcmp(sk.naglowek, plik, 24) == 0);
    SPRAWDZ(memcmp(sk.ogon, plik + 300 - 32, 32) == 0);
    SPRAWDZ(otaSkanOcenNaglowek(sk) == OTA_POWOD_BRAK);
    char ido[17], idz[17];
    otaSkanObraz(sk, ido);
    otaObrazZSumy(plik + 300 - 32, idz);
    SPRAWDZ(strcmp(ido, idz) == 0 && otaObrazPoprawny(ido));
  }
  {
    OtaSkan sk;
    otaSkanStart(sk);
    otaSkanDodaj(sk, plik, 10);
    SPRAWDZ(otaSkanOcenNaglowek(sk) == OTA_POWOD_BRAK);   // naglowka jeszcze nie ma: bez oceny
    otaSkanDodaj(sk, nullptr, 5);
    otaSkanDodaj(sk, plik, 0);
    SPRAWDZ(sk.lacznie == 10);
    char ido[17] = "x";
    otaSkanObraz(sk, ido);
    SPRAWDZ(ido[0] == '\0');                              // za krotki plik: bez identyfikatora
  }
  {
    uint8_t zly[100];
    memcpy(zly, plik, 100);
    zly[0] = '<';                                         // strona HTML zamiast programu
    OtaSkan sk;
    otaSkanStart(sk);
    otaSkanDodaj(sk, zly, 100);
    SPRAWDZ(otaSkanOcenNaglowek(sk) == OTA_POWOD_OBRAZ);
    zly[0] = 0xE9; zly[12] = 0x09;                        // program dla ESP32-S3 (firmware centrali)
    otaSkanStart(sk);
    otaSkanDodaj(sk, zly, 100);
    SPRAWDZ(otaSkanOcenNaglowek(sk) == OTA_POWOD_OBRAZ);
    zly[12] = 0; zly[13] = 1;
    otaSkanStart(sk);
    otaSkanDodaj(sk, zly, 100);
    SPRAWDZ(otaSkanOcenNaglowek(sk) == OTA_POWOD_OBRAZ);
    zly[13] = 0; zly[23] = 0;                             // program ESP32 bez dopisanej sumy
    otaSkanStart(sk);
    otaSkanDodaj(sk, zly, 100);
    SPRAWDZ(otaSkanOcenNaglowek(sk) == OTA_POWOD_BRAK);
    char ido[17] = "x";
    otaSkanObraz(sk, ido);
    SPRAWDZ(ido[0] == '\0');                              // bez sumy nie ma identyfikatora
  }
}

static void testOtaZapisIDecyzja() {
  using namespace diag;
  const char* OBRAZ_A = "aaaaaaaaaaaaaaaa";   // obraz, z ktorego panel pracuje
  const char* OBRAZ_B = "bbbbbbbbbbbbbbbb";   // obraz oferowany przez centrale
  OtaZapis z;
  memset(&z, 0, sizeof(z));
  const OtaWarunki dobre = otaWarunkiDobre();
  const OtaOferta brak = {false, false, 0, "", ""};
  const OtaOferta nowa = {true, true, 5, OBRAZ_B, "3.58.6"};
  const OtaOferta stara = {true, false, 0, "", "3.58.6"};

  // --- bez oferty: nic, niezaleznie od reszty
  SPRAWDZ(otaDecyzja(brak, OBRAZ_A, z, dobre).akcja == OTA_AKCJA_NIC);

  // --- oferta i dobre warunki: start
  SPRAWDZ(otaDecyzja(nowa, OBRAZ_A, z, dobre).akcja == OTA_AKCJA_START);
  SPRAWDZ(otaDecyzja(stara, OBRAZ_A, z, dobre).akcja == OTA_AKCJA_START);
  SPRAWDZ(otaDecyzja(nowa, "", z, dobre).akcja == OTA_AKCJA_START);          // wlasny obraz nieznany: nie blokuje

  // --- panel juz pracuje z oferowanego pliku: nic do zrobienia (centrala sama to zobaczy po panel_obraz)
  const OtaDecyzja mam = otaDecyzja(nowa, OBRAZ_B, z, dobre);
  SPRAWDZ(mam.akcja == OTA_AKCJA_POMIN && strcmp(mam.powod, "juz_mam") == 0);

  // --- kazdy warunek z osobna wstrzymuje start; kolejnosc: alarm przed wszystkim
  struct { const char* powod; void (*zepsuj)(OtaWarunki&); } przypadki[] = {
      {"alarm", [](OtaWarunki& w) { w.alarm = true; }},
      {"brak_wifi", [](OtaWarunki& w) { w.wifi = false; }},
      {"brak_danych", [](OtaWarunki& w) { w.daneSwieze = false; }},
      {"po_starcie", [](OtaWarunki& w) { w.odStartuMs = OTA_PO_STARCIE_MS - 1; }},
      {"odstep", [](OtaWarunki& w) { w.bylaProba = true; w.odOstatniejProbyMs = OTA_ODSTEP_PROB_MS - 1; }},
      {"siec_zajeta", [](OtaWarunki& w) { w.siecZajeta = true; }},
      {"dotyk", [](OtaWarunki& w) { w.bezDotykuMs = OTA_BEZ_DOTYKU_MS - 1; }},
      {"bateria", [](OtaWarunki& w) { w.bateriaJest = true; w.zasilanieUsb = false; w.bateriaProcent = OTA_BATERIA_MIN_PROC - 1; }},
      {"pamiec", [](OtaWarunki& w) { w.ramWolny = OTA_RAM_MIN_B - 1; }},
      {"pamiec", [](OtaWarunki& w) { w.ramBlok = OTA_BLOK_MIN_B - 1; }},
  };
  for (const auto& pr : przypadki) {
    OtaWarunki w = dobre;
    pr.zepsuj(w);
    const OtaDecyzja d = otaDecyzja(nowa, OBRAZ_A, z, w);
    SPRAWDZ(d.akcja == OTA_AKCJA_CZEKAJ && strcmp(d.powod, pr.powod) == 0);
    w.alarm = true;
    SPRAWDZ(strcmp(otaDecyzja(nowa, OBRAZ_A, z, w).powod, "alarm") == 0);
  }
  {
    OtaWarunki w = dobre;   // wartosci dokladnie na progu wystarczaja
    w.odStartuMs = OTA_PO_STARCIE_MS; w.bezDotykuMs = OTA_BEZ_DOTYKU_MS; w.ramWolny = OTA_RAM_MIN_B; w.ramBlok = OTA_BLOK_MIN_B;
    w.bylaProba = true; w.odOstatniejProbyMs = OTA_ODSTEP_PROB_MS;
    w.bateriaJest = true; w.zasilanieUsb = false; w.bateriaProcent = OTA_BATERIA_MIN_PROC;
    SPRAWDZ(otaDecyzja(nowa, OBRAZ_A, z, w).akcja == OTA_AKCJA_START);
    w.bateriaProcent = 5; w.zasilanieUsb = true;    // slabe ogniwo, ale panel jest na USB
    SPRAWDZ(otaDecyzja(nowa, OBRAZ_A, z, w).akcja == OTA_AKCJA_START);
    w.zasilanieUsb = false; w.bateriaJest = false;  // ogniwa nie ma (pomiar 0%): nie blokuje
    SPRAWDZ(otaDecyzja(nowa, OBRAZ_A, z, w).akcja == OTA_AKCJA_START);
    w.odOstatniejProbyMs = 0; w.bylaProba = false;  // "0 ms od proby" bez proby w tej sesji: nie blokuje
    SPRAWDZ(otaDecyzja(nowa, OBRAZ_A, z, w).akcja == OTA_AKCJA_START);
  }

  // --- trzy proby i odmowa (nowa centrala)
  char klucz[OTA_KLUCZ_MAKS];
  otaKlucz(klucz, sizeof(klucz), true, 5, OBRAZ_B, "3.58.6");
  otaZapisRozpocznijProbe(z, klucz);
  SPRAWDZ(z.stan == OTA_ZAPIS_PROBY && z.prob == 1 && strcmp(z.klucz, klucz) == 0);
  SPRAWDZ(!otaZapisNieudana(z, OTA_POWOD_POBIERANIE, false) && z.stan == OTA_ZAPIS_PROBY);
  SPRAWDZ(otaDecyzja(nowa, OBRAZ_A, z, dobre).akcja == OTA_AKCJA_START);     // po pierwszej nieudanej: jeszcze wolno
  otaZapisRozpocznijProbe(z, klucz);
  SPRAWDZ(z.prob == 2);
  otaZapisOddajProbe(z);                                                     // przerwana przez alarm: nie liczy sie
  SPRAWDZ(z.prob == 1);
  otaZapisRozpocznijProbe(z, klucz);
  otaZapisRozpocznijProbe(z, klucz);
  SPRAWDZ(z.prob == 3);
  // panel zawiesil sie w trakcie trzeciej proby (nie zdazyl zapisac wyniku): po restarcie juz nie zaczyna
  const OtaDecyzja wycz = otaDecyzja(nowa, OBRAZ_A, z, dobre);
  SPRAWDZ(wycz.akcja == OTA_AKCJA_ODMOW && wycz.powodOdmowy == OTA_POWOD_POBIERANIE && strcmp(wycz.powod, "proby_wyczerpane") == 0);
  SPRAWDZ(otaZapisNieudana(z, OTA_POWOD_ZAPIS, false) && z.stan == OTA_ZAPIS_ODMOWA);   // trzecia nieudana = odmowa
  const OtaDecyzja odm = otaDecyzja(nowa, OBRAZ_A, z, dobre);
  SPRAWDZ(odm.akcja == OTA_AKCJA_ODMOW && odm.powodOdmowy == OTA_POWOD_ZAPIS);
  {
    OtaWarunki w = dobre;
    w.alarm = true;                                                          // odmowa jest zglaszana takze w czasie alarmu
    SPRAWDZ(otaDecyzja(nowa, OBRAZ_A, z, w).akcja == OTA_AKCJA_ODMOW);
  }
  // ta sama oferta wystawiona ponownie (/update_panel jeszcze raz = nowy numer): panel probuje od nowa
  const OtaOferta ponowiona = {true, true, 6, OBRAZ_B, "3.58.6"};
  SPRAWDZ(otaDecyzja(ponowiona, OBRAZ_A, z, dobre).akcja == OTA_AKCJA_START);
  otaKlucz(klucz, sizeof(klucz), true, 6, OBRAZ_B, "3.58.6");
  otaZapisRozpocznijProbe(z, klucz);
  SPRAWDZ(z.prob == 1 && z.stan == OTA_ZAPIS_PROBY && z.powod == OTA_POWOD_BRAK);   // nowy klucz zeruje licznik i powod

  // --- blad, przy ktorym powtarzanie nie ma sensu: odmowa od razu
  memset(&z, 0, sizeof(z));
  otaZapisRozpocznijProbe(z, klucz);
  SPRAWDZ(otaZapisNieudana(z, OTA_POWOD_OBRAZ, true) && z.stan == OTA_ZAPIS_ODMOWA && z.prob == 1);

  // --- udane wgranie i ocena po restarcie
  memset(&z, 0, sizeof(z));
  SPRAWDZ(otaOcenPoStarcie(z, OBRAZ_A) == 0);                                // nic nie bylo wgrywane
  otaZapisRozpocznijProbe(z, klucz);
  otaZapisWgrane(z, OBRAZ_B);
  SPRAWDZ(z.stan == OTA_ZAPIS_WGRANE && strcmp(z.oczekiwany, OBRAZ_B) == 0);
  SPRAWDZ(otaDecyzja(ponowiona, OBRAZ_A, z, dobre).akcja == OTA_AKCJA_POMIN);   // przed ocena: nie zaczynaj drugi raz
  OtaZapis kopia = z;
  SPRAWDZ(otaOcenPoStarcie(kopia, OBRAZ_B) == 1 && kopia.stan == OTA_ZAPIS_DZIALA);   // dziala nowy obraz
  SPRAWDZ(otaOcenPoStarcie(kopia, OBRAZ_B) == 0);                                     // ocena tylko raz
  kopia = z;
  SPRAWDZ(otaOcenPoStarcie(kopia, OBRAZ_A) == 2 && kopia.stan == OTA_ZAPIS_ODMOWA && kopia.powod == OTA_POWOD_COFNIETA);
  const OtaDecyzja cof = otaDecyzja(ponowiona, OBRAZ_A, kopia, dobre);
  SPRAWDZ(cof.akcja == OTA_AKCJA_ODMOW && cof.powodOdmowy == OTA_POWOD_COFNIETA);     // wycofany plik: nie wgrywaj znowu, zglos
  kopia = z;
  SPRAWDZ(otaOcenPoStarcie(kopia, "") == 1);                                          // wlasny obraz nieznany: bez zgloszenia wycofania
  kopia = z;
  kopia.oczekiwany[0] = '\0';                                                         // plik nie mial sumy: jw.
  SPRAWDZ(otaOcenPoStarcie(kopia, OBRAZ_A) == 1);

  // --- starsza centrala (flaga nie znika nigdy): po udanym wgraniu ten sam tag jest pomijany
  memset(&z, 0, sizeof(z));
  otaKlucz(klucz, sizeof(klucz), false, 0, "", "3.58.6");
  otaZapisRozpocznijProbe(z, klucz);
  otaZapisWgrane(z, OBRAZ_B);
  SPRAWDZ(otaOcenPoStarcie(z, OBRAZ_B) == 1);
  const OtaDecyzja juz = otaDecyzja(stara, OBRAZ_B, z, dobre);
  SPRAWDZ(juz.akcja == OTA_AKCJA_POMIN && strcmp(juz.powod, "juz_wgrane") == 0);
  const OtaOferta staraInnyTag = {true, false, 0, "", "3.58.7"};
  SPRAWDZ(otaDecyzja(staraInnyTag, OBRAZ_B, z, dobre).akcja == OTA_AKCJA_START);      // nowe wydanie: wgrywa
  // ta sama centrala po aktualizacji do 3.58.6 oferuje INNY plik pod tym samym tagiem: klucz jest inny, wgrywa
  const OtaOferta nowaTenSamTag = {true, true, 1, "cccccccccccccccc", "3.58.6"};
  SPRAWDZ(otaDecyzja(nowaTenSamTag, OBRAZ_B, z, dobre).akcja == OTA_AKCJA_START);
}

int main() {
  testOtaNarzedzia();
  testOtaZapisIDecyzja();
  testPrzyczyna();
  testRatunek();
  testPamiecAutorestartu();
  testHttp();
  testSkrotIOgon();
  testZlaWartosc();
  testDlawik();
  testLicznikiIStraznik();
  testWifi();
  testBateria();
  std::printf("diagnostyka_panelu.h: %d sprawdzen OK, %d nie przeszlo\n", g_ok, g_zle);
  return g_zle == 0 ? 0 : 1;
}
