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

int main() {
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
