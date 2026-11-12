// diagnostyka_panelu.h
// ============================================================================
// PANEL LCD - male, CZYSTE funkcje pomocnicze diagnostyki (v1.2.0 - v1.2.2)
// ============================================================================
// PO CO TEN PLIK:
//   Log z 2026-11-10 (serial-20261110_200814.log) pokazal 3 minuty samych
//   linii "[NET-DIAG] ... etap=sukces", a mimo to kafelki na ekranie nie
//   reagowaly na zmiany w centrali testowej. Z takiego logu NIE dalo sie
//   ustalic przyczyny, bo v1.1.0:
//     - "sukces" znaczyl tylko "serwer zamknal gniazdo" (bez sprawdzenia kodu
//       HTTP i bez informacji, czy JSON dal sie odczytac),
//     - nieudane parsowanie JSON-a konczylo sie cichym `return` (zero logu),
//     - panel nigdzie nie pisal, Z KTORA centrala (BSSID) jest polaczony -
//       a prawdziwa centrala i centrala testowa nadaja TA SAMA siec
//       "PiecCO_AP" pod tym samym adresem 192.168.4.1.
//   Ten plik zbiera logike, ktora te luki zamyka, w postaci malych funkcji
//   bez ZADNEJ zaleznosci od Arduino/ESP - dzieki temu kompiluja sie i sa
//   testowane na PC (patrz testy_host/test_diagnostyka.cpp, uruchamiane
//   skryptem testy_host/uruchom_testy.sh), tak samo jak rdzen centrali testowej.
//
// Konwencja jak w centrala-testowa-c3/src/wspolne.h: zero String, zero
// alokacji, wszystko na char[] + snprintf.
// ============================================================================
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>   // static_assert przy PamiecAutorestartu

namespace diag {

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------
// Kod z pierwszej linii odpowiedzi ("HTTP/1.1 200 OK" -> 200).
// 0 = to nie wyglada na odpowiedz HTTP (np. pusty bufor, smieci).
inline int kodHttp(const char* surowa) {
  if (surowa == nullptr || strncmp(surowa, "HTTP/", 5) != 0) return 0;
  const char* p = surowa + 5;
  while (*p != '\0' && *p != ' ' && *p != '\r' && *p != '\n') p++;   // wersja, np. "1.1"
  if (*p != ' ') return 0;
  while (*p == ' ') p++;
  int kod = 0, cyfr = 0;
  while (*p >= '0' && *p <= '9' && cyfr < 3) {
    kod = kod * 10 + (*p - '0');
    p++;
    cyfr++;
  }
  return (cyfr == 3) ? kod : 0;
}

// Poczatek ciala (pierwszy znak za pusta linia naglowkow) albo nullptr, gdy
// naglowki nie sa kompletne.
inline const char* cialoHttp(const char* surowa) {
  if (surowa == nullptr) return nullptr;
  const char* p = strstr(surowa, "\r\n\r\n");
  return (p != nullptr) ? p + 4 : nullptr;
}

// Co poszlo nie tak z odpowiedzia centrali - jedna wartosc na kazda przyczyne,
// zeby log i licznik mowily DOKLADNIE, na ktorym etapie dane przepadly.
enum Wynik : uint8_t {
  WYNIK_OK = 0,          // HTTP 200 + niepuste cialo (JSON sprawdza dopiero ArduinoJson)
  WYNIK_NIE_HTTP,        // brak linii "HTTP/..." - to nie jest odpowiedz HTTP
  WYNIK_KOD_HTTP,        // kod inny niz 200 (np. 404, 500, 302)
  WYNIK_BRAK_CIALA,      // naglowki bez pustej linii albo cialo o dlugosci 0
  WYNIK_JSON,            // ArduinoJson odrzucil tresc (ustawiane w main.cpp)
  WYNIK_TRANSPORT        // gniazdo: timeout / blad polaczenia (ustawiane w main.cpp)
};

inline const char* wynikTekst(Wynik w) {
  switch (w) {
    case WYNIK_OK:         return "ok";
    case WYNIK_NIE_HTTP:   return "nie_http";
    case WYNIK_KOD_HTTP:   return "kod_http";
    case WYNIK_BRAK_CIALA: return "brak_ciala";
    case WYNIK_JSON:       return "json";
    case WYNIK_TRANSPORT:  return "transport";
  }
  return "?";
}

// Sprawdza surowa odpowiedz (naglowki+cialo). `kod` i `cialo` sa zawsze
// ustawiane (0 / nullptr przy bledzie), zeby wolajacy mogl je wypisac w logu.
inline Wynik sprawdzOdpowiedz(const char* surowa, int* kod, const char** cialo) {
  const int k = kodHttp(surowa);
  const char* c = cialoHttp(surowa);
  if (kod != nullptr) *kod = k;
  if (cialo != nullptr) *cialo = c;
  if (k == 0) return WYNIK_NIE_HTTP;
  if (k != 200) return WYNIK_KOD_HTTP;
  if (c == nullptr || *c == '\0') return WYNIK_BRAK_CIALA;
  return WYNIK_OK;
}

// Poczatek tresci w postaci bezpiecznej dla jednolinijkowego logu: znaki
// sterujace i cudzyslow zamienione na '.', obciecie do n-1 znakow.
inline void skrotDoLogu(const char* wejscie, char* wyjscie, size_t n) {
  if (wyjscie == nullptr || n == 0) return;
  size_t i = 0;
  if (wejscie != nullptr) {
    for (; wejscie[i] != '\0' && i + 1 < n; i++) {
      const unsigned char c = (unsigned char)wejscie[i];
      wyjscie[i] = (c < 0x20 || c == '"' || c == 0x7F) ? '.' : (char)c;
    }
  }
  wyjscie[i] = '\0';
}

// Wskaznik na ostatnie `ile` znakow tekstu (albo na caly tekst, gdy jest
// krotszy). Do logu: po koncowce widac, czy odpowiedz zostala ucieta.
inline const char* ogon(const char* tekst, size_t ile) {
  if (tekst == nullptr) return nullptr;
  const size_t n = strlen(tekst);
  return (n > ile) ? tekst + (n - ile) : tekst;
}

// ---------------------------------------------------------------------------
// JSON: KTORE pole jest zle?
// ---------------------------------------------------------------------------
// ArduinoJson przy bledzie skladni mowi tylko "InvalidInput" - bez pozycji i
// bez nazwy pola, a odrzuca CALA odpowiedz. Najczestsza przyczyna w tym
// projekcie jest znana z historii centrali (wpis "znalezione przy tescie na
// golym ESP, 2026-08-23" przy funkcji cisnienie() w main_centrala.cpp):
// centrala sklada JSON recznie przez String(float, 1), a dla wartosci NaN /
// nieskonczonosc Arduino wypisuje wtedy doslownie `nan` / `inf` - to nie jest
// JSON. Jedno takie pole (np. z niepodlaczonego czujnika na plytce testowej)
// i panel nie odczyta ZADNEJ wartosci z calej odpowiedzi.
//
// znajdzZlaWartosc() wskazuje pierwsze pole obiektu, ktorego wartosc nie
// zaczyna sie jak poprawna wartosc JSON (`"cisnienie":nan`, `"x":inf`,
// `"y":--`, `"z":,`, `"q":` na samym koncu ucietej tresci). Zwraca wskaznik
// na cudzyslow otwierajacy NAZWE tego pola - tak, zeby wycinek w logu
// zaczynal sie od nazwy - albo nullptr, gdy niczego takiego nie ma (blad lezy
// wtedy gdzie indziej).
// Ograniczenie (swiadome): sprawdzane sa tylko wartosci po dwukropku, czyli
// pola obiektow; elementy tablic nie. /api/status i /api/ustawienia-get to
// plaskie obiekty, wiec to wystarcza.
inline bool koniecLiteraluJson(char c) {
  return c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0';
}

inline bool poczatekWartosciJson(const char* w) {
  const char c = *w;
  if (c == '"' || c == '\'' || c == '{' || c == '[') return true;
  if (c >= '0' && c <= '9') return true;
  if (c == '-') return (w[1] >= '0' && w[1] <= '9');
  if (strncmp(w, "true", 4) == 0) return koniecLiteraluJson(w[4]);
  if (strncmp(w, "false", 5) == 0) return koniecLiteraluJson(w[5]);
  if (strncmp(w, "null", 4) == 0) return koniecLiteraluJson(w[4]);
  return false;   // m.in. nan, NaN, inf, -inf, --, przecinek, klamra, koniec tresci
}

inline const char* znajdzZlaWartosc(const char* json) {
  if (json == nullptr) return nullptr;
  const char* ostatniNapis = nullptr;   // poczatek ostatnio minietego napisu = kandydat na nazwe pola
  const char* p = json;
  while (*p != '\0') {
    if (*p == '"') {
      const char* start = p;
      p++;
      while (*p != '\0' && *p != '"') {
        if (*p == '\\' && p[1] != '\0') p++;   // znak po ukosniku nalezy do napisu (np. \")
        p++;
      }
      if (*p == '\0') return nullptr;           // niedomkniety napis: tresc ucieta w srodku, nie "zla wartosc"
      p++;
      ostatniNapis = start;
      continue;
    }
    if (*p == ':') {
      const char* w = p + 1;
      while (*w == ' ' || *w == '\t' || *w == '\r' || *w == '\n') w++;
      if (!poczatekWartosciJson(w)) return (ostatniNapis != nullptr) ? ostatniNapis : p;
      p = w;
      continue;
    }
    p++;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Dlawik logu - ten sam blad powtarzany co 2 s nie moze zalac Seriala (to byl
// wspolsprawca "burzy" linii w v1.0.0, patrz FIX-LOG-SPAM w main.cpp).
// ---------------------------------------------------------------------------
struct Dlawik {
  uint32_t ostatnioMs = 0;
  uint32_t pominieto = 0;
  bool bylo = false;

  // true = wolno zalogowac teraz; false = pomin (licznik pominietych rosnie).
  bool pozwol(uint32_t terazMs, uint32_t okresMs) {
    if (!bylo || (uint32_t)(terazMs - ostatnioMs) >= okresMs) {
      bylo = true;
      ostatnioMs = terazMs;
      return true;
    }
    pominieto++;
    return false;
  }

  // Ile wpisow przepadlo od ostatniego zalogowania (i zeruje licznik).
  uint32_t zabierzPominiete() {
    const uint32_t p = pominieto;
    pominieto = 0;
    return p;
  }
};

// ---------------------------------------------------------------------------
// Liczniki do okresowej linii "tag=PANEL" (patrz panelDiagOkresowo() w
// main.cpp) - jedna linia co 30 s zamiast trzech linii NET-DIAG na zapytanie.
// ---------------------------------------------------------------------------
struct Liczniki {
  uint32_t statusOk = 0;
  uint32_t statusBlad = 0;
  uint32_t ustawieniaOk = 0;
  uint32_t ustawieniaBlad = 0;
  uint32_t ekranOdswiezen = 0;   // ile razy swieze dane faktycznie trafily do LVGL

  void zeruj() { *this = Liczniki(); }
};

// ---------------------------------------------------------------------------
// Swiezosc danych na ekranie.
// v1.1.0: po nieudanym odczycie panel zostawial na kafelkach OSTATNIE znane
// wartosci bez zadnego znaku, ze sa stare. Ten straznik mowi petli glownej,
// kiedy przelaczyc kafelki na "--" (raz, w chwili przekroczenia progu), a
// kiedy dane wrocily.
// ---------------------------------------------------------------------------
struct StraznikSwiezosci {
  bool oznaczoneJakoStare = false;

  // Wolane co obrot petli. `stare` = od ostatniego POPRAWNIE odczytanego
  // statusu minelo wiecej niz prog. Zwraca true dokladnie raz na kazdy
  // "zanik" danych - wtedy wolajacy czysci kafelki.
  bool wlasniePrzeterminowane(bool stare) {
    if (stare && !oznaczoneJakoStare) {
      oznaczoneJakoStare = true;
      return true;
    }
    return false;
  }

  // Wolane po kazdym poprawnie odczytanym statusie. Zwraca true, gdy dane
  // wlasnie WROCILY po zaniku (do logu).
  bool odnotujSwiezeDane() {
    const bool wrocily = oznaczoneJakoStare;
    oznaczoneJakoStare = false;
    return wrocily;
  }
};

// ---------------------------------------------------------------------------
// WiFi: ktora centrala?
// ---------------------------------------------------------------------------
// "AA:BB:CC:DD:EE:FF" (17 znakow + terminator -> bufor >= 18).
inline void formatBssid(const uint8_t* b, char* wyjscie, size_t n) {
  if (wyjscie == nullptr || n == 0) return;
  if (b == nullptr) {
    snprintf(wyjscie, n, "--:--:--:--:--:--");
    return;
  }
  snprintf(wyjscie, n, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
}

// Jeden punkt dostepowy o NASZEJ nazwie sieci znaleziony przy skanowaniu.
struct KandydatAp {
  uint8_t bssid[6];
  int32_t kanal;
  int32_t rssi;     // dBm, wieksze (blizej zera) = silniejszy sygnal
};

// Indeks najsilniejszego kandydata albo -1, gdy lista jest pusta.
// Przy remisie wygrywa pierwszy na liscie (wynik jest wtedy powtarzalny).
inline int wybierzNajsilniejszy(const KandydatAp* k, int n) {
  int najlepszy = -1;
  for (int i = 0; i < n; i++) {
    if (najlepszy < 0 || k[i].rssi > k[najlepszy].rssi) najlepszy = i;
  }
  return najlepszy;
}

// ---------------------------------------------------------------------------
// Symulacja czujnikow w centrali
// ---------------------------------------------------------------------------
// Centrala (main_centrala.cpp, [SYMULACJA-PER-CZUJNIK]) dopisuje do
// /api/status pola "symulacja_<czujnik>":true|false. true = wartosc tego
// czujnika jest w tej chwili PODSTAWIONA z panelu WWW centrali, a nie
// zmierzona. Panel zbiera te flagi w maske bitowa: bit i odpowiada
// CZUJNIKI_SYMULOWANE[i].
struct CzujnikSymulowany {
  const char* pole;    // nazwa pola w /api/status
  const char* skrot;   // krotka nazwa do naglowka i logu (jak nazwy kafelkow)
};
static const CzujnikSymulowany CZUJNIKI_SYMULOWANE[] = {
    {"symulacja_zewn", "zewn"},          {"symulacja_pokoj", "pokoj"},
    {"symulacja_ogrz", "piec"},          {"symulacja_bojler", "bojler"},
    {"symulacja_panel", "panel"},        {"symulacja_dym", "dym"},
    {"symulacja_wilgotnosc", "wilg"},    {"symulacja_cisnienie", "cisn"},
    {"symulacja_ogrz_powrot", "powrot"}, {"symulacja_ogrz_trociny", "trociny"},
};
static const int LICZBA_CZUJNIKOW_SYMULOWANYCH = (int)(sizeof(CZUJNIKI_SYMULOWANE) / sizeof(CZUJNIKI_SYMULOWANE[0]));

// Skroty czujnikow z maski, np. "zewn,pokoj". Pusta maska -> "". Najwyzej
// `maksNazw` nazw; reszta jako licznik: "zewn,pokoj +3". Wynik zawsze
// zakonczony zerem i nigdy dluzszy niz n-1 znakow.
inline void listaSymulowanych(uint16_t maska, char* wyjscie, size_t n,
                              int maksNazw = LICZBA_CZUJNIKOW_SYMULOWANYCH, const char* separator = ",") {
  if (wyjscie == nullptr || n == 0) return;
  wyjscie[0] = '\0';
  size_t dl = 0;
  int wypisane = 0, pominiete = 0;
  for (int i = 0; i < LICZBA_CZUJNIKOW_SYMULOWANYCH; i++) {
    if ((maska & (uint16_t)(1u << i)) == 0) continue;
    if (wypisane >= maksNazw) {
      pominiete++;
      continue;
    }
    const int k = snprintf(wyjscie + dl, n - dl, "%s%s", wypisane > 0 ? separator : "", CZUJNIKI_SYMULOWANE[i].skrot);
    if (k < 0 || (size_t)k >= n - dl) return;   // obciete - snprintf juz postawil terminator
    dl += (size_t)k;
    wypisane++;
  }
  if (pominiete > 0) snprintf(wyjscie + dl, n - dl, " +%d", pominiete);
}

// Tekst znacznika na srodku naglowka dashboardu (patrz
// lvgl_dashboard_poc_aktualizuj_centrale() w dashboard_lvgl_poc.h). Sklada
// sie z najwyzej dwoch czesci oddzielonych dwiema spacjami:
//   1) co odpowiada / co jest symulowane:
//      symulator=true    -> "SYMULATOR"              (centrala-testowa-c3)
//      maskaSym != 0     -> "SYMULACJA: zewn, pokoj" (prawdziwa centrala z
//                           symulacja czujnikow; najwyzej 2 nazwy, przy
//                           drugiej czesci 1, reszta jako "+N")
//   2) liczbaAp >= 2     -> "2 CENTRALE (A4:1C)"     (2 ostatnie bajty BSSID;
//                           bez nawiasu, gdy bssid == nullptr)
// Nic z tego -> "" (nic nie pokazuj - zwykla, jedna centrala, bez symulacji).
// Wynik zawsze zakonczony zerem i nigdy dluzszy niz n-1 znakow.
inline void znacznikCentrali(char* wyjscie, size_t n, bool symulator, int liczbaAp, const uint8_t* bssid,
                             uint16_t maskaSym = 0) {
  if (wyjscie == nullptr || n == 0) return;
  wyjscie[0] = '\0';
  char ap[28] = "";
  if (liczbaAp >= 2) {
    if (bssid != nullptr) snprintf(ap, sizeof(ap), "%d CENTRALE (%02X:%02X)", liczbaAp, bssid[4], bssid[5]);
    else                  snprintf(ap, sizeof(ap), "%d CENTRALE", liczbaAp);
  }
  char co[40] = "";
  if (symulator) {
    snprintf(co, sizeof(co), "SYMULATOR");
  } else if (maskaSym != 0) {
    char lista[24];
    listaSymulowanych(maskaSym, lista, sizeof(lista), ap[0] != '\0' ? 1 : 2, ", ");
    snprintf(co, sizeof(co), "SYMULACJA: %s", lista);
  }
  snprintf(wyjscie, n, "%s%s%s", co, (co[0] != '\0' && ap[0] != '\0') ? "  " : "", ap);
}

// ---------------------------------------------------------------------------
// [v1.2.1 RATUNEK] Panel nie ma danych z centrali: dlaczego i co z tym zrobic
// ---------------------------------------------------------------------------
// Zgloszenie z 2026-11-12 (v1.2.0 na plytce): "podczas pracy zresetowalem LCD
// i dopiero wtedy odebral swieze dane". W chwili pisania v1.2.1 przyczyna nie
// byla znana (byl tylko log centrali) - pewne bylo tylko to, ze pomaga
// restart panelu. [v1.2.2: log panelu pokazal brak pamieci RAM - patrz
// PRZYCZYNA_PAMIEC nizej i HISTORIA ZMIAN w main.cpp.] Dlatego:
//   1) panel pokazuje na ekranie i pisze w logu, CZEGO mu brakuje (Przyczyna),
//   2) sam probuje sie ratowac (Ratunek): najpierw ponowne polaczenie z siecia
//      centrali, a gdy to nie pomaga - restart, czyli to samo, co dotad robil
//      uzytkownik reka,
//   3) po takim restarcie zglasza centrali, dlaczego go zrobil
//      (tokenAutorestartu() -> parametr reset_powod w /api/status -> linia
//      "tag=RESET-DIAG" w logu centrali).
// Makro dla testow na PC: ta wersja naglowka ma ponizsze typy.
#define DIAG_MA_RATUNEK 1

// Wartosci liczbowe sa zapisywane w pamieci RTC (PamiecAutorestartu) - nowe
// przyczyny dopisuj NA KONCU.
enum Przyczyna : uint8_t {
  PRZYCZYNA_BRAK = 0,    // dane sa - nic nie brakuje
  PRZYCZYNA_WIFI,        // panel nie jest polaczony z siecia centrali
  PRZYCZYNA_MILCZY,      // WiFi polaczone, ale centrala nie odpowiada (timeout, blad polaczenia)
  PRZYCZYNA_ODPOWIEDZ,   // centrala odpowiada, ale tresci nie da sie odczytac (kod HTTP, JSON)
  // [v1.2.2] Panelowi zabraklo pamieci RAM: odpowiedz centrali przyszla, ale
  // nie bylo jej gdzie rozpakowac (ArduinoJson: NoMemory), albo nie dalo sie
  // nawet otworzyc gniazda / wyslac zapytania (ENOMEM, ENOBUFS, ENFILE). To
  // jest przyczyna z logu z 2026-11-12 - wina lezy w panelu, nie w centrali.
  PRZYCZYNA_PAMIEC,
  // [v1.2.2] Biblioteka ekranu (LVGL) zglosila blad wewnetrzny (assert) -
  // w praktyce: nieudany przydzial pamieci w trakcie rysowania. Panel nie
  // moze wtedy rysowac dalej i restartuje sie od razu (bez czekania 45 s).
  PRZYCZYNA_LVGL
};

// `ostatniBladToTresc`: ostatnia nieudana proba /api/status skonczyla sie
// odpowiedzia, ktorej nie dalo sie odczytac (a nie brakiem odpowiedzi).
// `ostatniBladToPamiec`: ...brakiem pamieci w panelu (patrz PRZYCZYNA_PAMIEC).
inline Przyczyna ustalPrzyczyne(bool wifiPolaczone, bool ostatniBladToTresc, bool ostatniBladToPamiec = false) {
  if (!wifiPolaczone) return PRZYCZYNA_WIFI;
  if (ostatniBladToPamiec) return PRZYCZYNA_PAMIEC;
  return ostatniBladToTresc ? PRZYCZYNA_ODPOWIEDZ : PRZYCZYNA_MILCZY;
}

// Ponizej tylu bajtow wolnego RAM-u panel pisze w logu ostrzezenie
// "tag=PAMIEC ... zdarzenie=malo_pamieci". Jedno odpytanie centrali o status
// potrzebuje chwilowo ok. 6,5 kB na sama odpowiedz i jej rozpakowanie
// (pomiar: testy_host/pomiar_pamieci.sh), do tego bufory WiFi i rysowanie
// ekranu - 16 kB to granica, ponizej ktorej zaczyna brakowac.
static const uint32_t RAM_PROG_OSTRZEZENIA_B = 16000UL;
#define DIAG_MA_PAMIEC 1

// Czy numer bledu gniazda (errno z socket()/connect()/send()/recv()) oznacza,
// ze PANELOWI zabraklo zasobow - pamieci albo wolnych gniazd. Numery jak w
// newlib na ESP32 (takie same na Linuksie, na ktorym biegna testy):
//   12 ENOMEM, 23 ENFILE, 24 EMFILE, 105 ENOBUFS.
inline bool bladGniazdaToBrakPamieci(int numer) {
  return numer == 12 || numer == 23 || numer == 24 || numer == 105;
}

// Napis na ekran (naglowek dashboardu) - tylko wielkie litery ASCII.
inline const char* przyczynaNapis(Przyczyna p) {
  switch (p) {
    case PRZYCZYNA_BRAK:      return "";
    case PRZYCZYNA_WIFI:      return "BRAK WIFI";
    case PRZYCZYNA_MILCZY:    return "CENTRALA MILCZY";
    case PRZYCZYNA_ODPOWIEDZ: return "ZLA ODPOWIEDZ";
    case PRZYCZYNA_PAMIEC:    return "BRAK PAMIECI";
    case PRZYCZYNA_LVGL:      return "BLAD EKRANU";
  }
  return "?";
}

// Krotka nazwa do logu i do tokenu wysylanego centrali (tylko [a-z]).
inline const char* przyczynaSkrot(Przyczyna p) {
  switch (p) {
    case PRZYCZYNA_BRAK:      return "brak";
    case PRZYCZYNA_WIFI:      return "wifi";
    case PRZYCZYNA_MILCZY:    return "milczy";
    case PRZYCZYNA_ODPOWIEDZ: return "odp";
    case PRZYCZYNA_PAMIEC:    return "pam";
    case PRZYCZYNA_LVGL:      return "lvgl";
  }
  return "x";
}

// Nazwa kodu przyczyny rozlaczenia WiFi (wifi_err_reason_t z ESP-IDF, pole
// `reason` zdarzenia STA_DISCONNECTED). Tylko kody spotykane w praktyce;
// reszta jako "?" - w logu i tak jest liczba.
inline const char* nazwaKoduRozlaczenia(uint8_t kod) {
  switch (kod) {
    case 0:   return "brak";
    case 1:   return "UNSPECIFIED";
    case 2:   return "AUTH_EXPIRE";
    case 3:   return "AUTH_LEAVE";
    case 4:   return "ASSOC_EXPIRE";
    case 5:   return "ASSOC_TOOMANY";
    case 6:   return "NOT_AUTHED";
    case 7:   return "NOT_ASSOCED";
    case 8:   return "ASSOC_LEAVE";              // rozlaczenie na wlasne zyczenie (WiFi.disconnect())
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT";   // typowo: zle haslo
    case 16:  return "GROUP_KEY_UPDATE_TIMEOUT";
    case 200: return "BEACON_TIMEOUT";           // centrala przestala nadawac
    case 201: return "NO_AP_FOUND";              // sieci centrali nie ma w eterze
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    case 205: return "CONNECTION_FAIL";
    case 206: return "AP_TSF_RESET";
    case 207: return "ROAMING";
    case 208: return "ASSOC_COMEBACK_TIME_TOO_LONG";
    case 209: return "SA_QUERY_TIMEOUT";
    case 210: return "NO_AP_FOUND_W_COMPATIBLE_SECURITY";
    case 211: return "NO_AP_FOUND_IN_AUTHMODE_THRESHOLD";
    case 212: return "NO_AP_FOUND_IN_RSSI_THRESHOLD";
  }
  return "?";
}

// Po ilu milisekundach BEZ poprawnie odczytanego statusu panel sam sie
// restartuje. `kolejnych` = ile takich restartow juz bylo z rzedu (bez ani
// jednego poprawnego statusu pomiedzy nimi): 45 s, potem 90 s, 3 min i dalej
// co 5 min - zeby przy centrali wylaczonej na dlugo panel nie restartowal sie
// w kolko, a mimo to wrocil do pracy najdalej 5 min po jej powrocie.
inline uint32_t progRestartuMs(uint8_t kolejnych) {
  switch (kolejnych) {
    case 0:  return 45000UL;
    case 1:  return 90000UL;
    case 2:  return 180000UL;
    default: return 300000UL;
  }
}

// Czy samoczynny restart ma sens. Przy nieczytelnych odpowiedziach centrali
// (PRZYCZYNA_ODPOWIEDZ) restart pomaga tylko wtedy, gdy wina lezy w panelu
// (np. brak pamieci). Jesli po dwoch restartach z rzedu odpowiedzi dalej sa
// nieczytelne, wina lezy po stronie centrali i kolejne restarty nic nie
// dadza - a kazdy konczylby sie wpisem w logu centrali i wiadomoscia, bo w
// tym stanie zapytania panelu do centrali DOCIERAJA. Przy braku WiFi i przy
// milczacej centrali ograniczenia nie ma (progRestartuMs() i tak rozrzedza
// restarty do jednego na 5 min).
static const uint8_t MAKS_RESTARTOW_PRZY_ZLEJ_ODPOWIEDZI = 2;
inline bool restartMaSens(Przyczyna p, uint8_t kolejnych) {
  return !(p == PRZYCZYNA_ODPOWIEDZ && kolejnych >= MAKS_RESTARTOW_PRZY_ZLEJ_ODPOWIEDZI);
}

// Licznik "restartow z rzedu" wraca do zera dopiero po tylu ms NIEPRZERWANIE
// poprawnych danych - nie po pierwszym udanym statusie. Inaczej panel, ktory
// po kazdym restarcie dostaje dane tylko na chwile, restartowalby sie co
// minute bez konca.
static const uint32_t CZAS_STABILNYCH_DANYCH_MS = 120000UL;

// "Seria" samoczynnych restartow konczy sie po godzinie nieprzerwanie
// poprawnych danych. Sluzy tylko do tego, KTORE restarty panel zglasza
// centrali. Centrala o kazdym zgloszonym restarcie pisze w logu i wysyla
// wiadomosc - przy lacznosci, ktora rwie sie co kilka minut, zglaszanie
// kazdego oznaczaloby wiadomosc co kilka minut, przez cala noc. Dlatego:
// pierwsze trzy restarty serii sa zglaszane zawsze, nastepne dopiero wtedy,
// gdy numer w serii jest co najmniej dwa razy wiekszy od ostatnio
// zgloszonego (czyli 6., 12., 24....). Numer jest w zgloszeniu ("n12"), wiec
// pominiete restarty widac jako przeskok numeracji. `zgloszonyNr` = 0: w tej
// serii centrala nie dostala jeszcze zadnego zgloszenia (np. byla wylaczona)
// - wtedy zglaszany jest restart o dowolnym numerze.
static const uint32_t CZAS_KONCA_SERII_MS = 3600000UL;
static const uint8_t ZGLOSZEN_BEZ_OGRANICZEN = 3;

inline bool zglosicRestart(uint8_t seria, uint8_t zgloszonyNr) {
  if (seria <= ZGLOSZEN_BEZ_OGRANICZEN || zgloszonyNr == 0) return true;
  return (unsigned)seria >= 2u * (unsigned)zgloszonyNr;
}

// Drabina ratunkowa. krok() wolany w kazdym obrocie petli glownej;
// daneSwieze() po kazdym poprawnie odczytanym statusie.
//
// Czas bez danych jest SUMOWANY z odcinkow miedzy kolejnymi wywolaniami
// krok(), a pojedynczy odcinek liczy sie najwyzej jako KROK_MAKS_MS. Powod:
// panel ma operacje, ktore zatrzymuja petle na wiele sekund (pobieranie
// wykresow, aktualizacja OTA) - po takiej przerwie licznik nie moze od razu
// przekroczyc progu restartu, skoro w jej trakcie panel w ogole nie pytal
// centrali o status.
struct Ratunek {
  enum Akcja : uint8_t {
    NIC = 0,
    POLACZ_PONOWNIE,            // rozlacz i polacz z ta sama centrala (zapamietany BSSID)
    POLACZ_PONOWNIE_BEZ_BSSID,  // rozlacz i szukaj sieci centrali od nowa, na wszystkich kanalach
    RESTART                     // ESP.restart()
  };

  static const uint32_t KROK_MAKS_MS = 1000;
  // WiFi "polaczone", a centrala nie odpowiada od tylu ms -> wymus ponowne
  // polaczenie. 12 s = 6 kolejnych nieudanych odpytan statusu; dluzej niz
  // pojedyncze zaburzenie kanalu AP centrali (jej wlasna proba polaczenia z
  // routerem - patrz wpis z 2026-09-12 w HISTORII ZMIAN main.cpp).
  static const uint32_t PROG_MILCZENIA_MS = 12000;
  static const uint8_t  MAKS_WYMUSZEN = 2;

  uint32_t bezDanychMs;     // ile petla "przezyla" od ostatniego poprawnego statusu
  uint32_t milczyMs;        // ...z tego nieprzerwanie z WiFi polaczonym i centrala milczaca
  uint32_t ostatnioMs;
  uint8_t  wymuszen;        // ile razy w tym zaniku wymuszono ponowne polaczenie
  bool     restartZlecony;
  bool     bylo;

  Ratunek() : bezDanychMs(0), milczyMs(0), ostatnioMs(0), wymuszen(0), restartZlecony(false), bylo(false) {}

  void daneSwieze() {
    bezDanychMs = 0;
    milczyMs = 0;
    wymuszen = 0;
    restartZlecony = false;
  }

  // `polaczoneAleMilczy`: WiFi zglasza polaczenie, a ostatnia proba statusu
  // skonczyla sie brakiem odpowiedzi (nie: nieczytelna odpowiedzia - na to
  // ponowne polaczenie z siecia nic nie poradzi).
  Akcja krok(uint32_t terazMs, bool polaczoneAleMilczy, uint32_t progRestartu) {
    uint32_t dt = 0;
    if (bylo) {
      dt = (uint32_t)(terazMs - ostatnioMs);
      if (dt > KROK_MAKS_MS) dt = KROK_MAKS_MS;
    }
    bylo = true;
    ostatnioMs = terazMs;
    if (bezDanychMs < 0xFFFF0000UL) bezDanychMs += dt;
    if (polaczoneAleMilczy) milczyMs += dt;
    else milczyMs = 0;

    if (bezDanychMs >= progRestartu) {
      if (restartZlecony) return NIC;
      restartZlecony = true;
      return RESTART;
    }
    if (polaczoneAleMilczy && milczyMs >= PROG_MILCZENIA_MS && wymuszen < MAKS_WYMUSZEN) {
      wymuszen++;
      milczyMs = 0;
      return (wymuszen == 1) ? POLACZ_PONOWNIE : POLACZ_PONOWNIE_BEZ_BSSID;
    }
    return NIC;
  }

  // Wolajacy nie wykonal zleconego restartu (restartMaSens() == false):
  // krok() bedzie zglaszal RESTART przy kazdym nastepnym wywolaniu, dopoki
  // danych nie ma - zeby restart nastapil, gdy tylko przyczyna sie zmieni.
  void restartOdrzucony() { restartZlecony = false; }

  // Ile zostalo do restartu (0 = juz).
  uint32_t doRestartuMs(uint32_t progRestartu) const {
    return (bezDanychMs >= progRestartu) ? 0 : (progRestartu - bezDanychMs);
  }
};

// Tekst na srodku naglowka dashboardu, gdy panel NIE MA danych - zamiast
// znacznika centrali (patrz znacznikCentrali() nizej w tym pliku):
//   "CENTRALA MILCZY 14 s"          - czego brakuje i od ilu sekund,
//   "RESTART ZA 7 s: BRAK WIFI"     - ostatnie 10 s przed samoczynnym restartem.
// `doRestartuS` = BEZ_RESTARTU, gdy restartu nie bedzie (restartMaSens()).
// Wynik zawsze zakonczony zerem i nigdy dluzszy niz n-1 znakow.
static const uint32_t BEZ_RESTARTU = 0xFFFFFFFFUL;
inline void znacznikBrakuDanych(char* wyjscie, size_t n, Przyczyna p, uint32_t bezDanychS, uint32_t doRestartuS) {
  if (wyjscie == nullptr || n == 0) return;
  wyjscie[0] = '\0';
  if (p == PRZYCZYNA_BRAK) return;
  if (doRestartuS <= 10) snprintf(wyjscie, n, "RESTART ZA %lu s: %s", (unsigned long)doRestartuS, przyczynaNapis(p));
  else                   snprintf(wyjscie, n, "%s %lu s", przyczynaNapis(p), (unsigned long)bezDanychS);
}

// To, co ma przetrwac samoczynny restart (ESP.restart()). W main.cpp lezy w
// pamieci RTC_NOINIT: restart programowy jej nie rusza, a po wlaczeniu
// zasilania zawiera przypadkowe wartosci - stad `magia` i `suma`.
// UWAGA: struktura MUSI pozostac bez konstruktora i bez domyslnych wartosci
// pol - inaczej program przy starcie wpisalby je do pamieci i skasowal to,
// co mialo przetrwac.
struct PamiecAutorestartu {
  uint32_t magia;
  uint8_t  oczekuje;         // 1 = ten restart zlecil Ratunek (ustawiane tuz przed ESP.restart())
  uint8_t  kolejnych;        // takie restarty z rzedu, bez poprawnego statusu pomiedzy nimi
  uint8_t  przyczyna;        // Przyczyna w chwili restartu
  uint8_t  kodRozlaczenia;   // ostatni kod rozlaczenia WiFi (0 = nie bylo zadnego)
  uint16_t bezDanychS;       // ile sekund panel byl bez danych
  uint16_t heapKb;           // wolny RAM w chwili restartu [kB] - od v1.2.2 tylko ten, z ktorego korzysta malloc()
  uint16_t blokKb;           // najwiekszy ciagly wolny blok tego RAM-u [kB]
  uint16_t lacznie;          // wszystkie takie restarty od wlaczenia zasilania
  uint16_t bladNumer;        // ostatnia nieudana proba statusu: numer bledu gniazda albo kod HTTP (0 = limit czasu)
  uint8_t  bladEtap;         // ...i etap, na ktorym padla: litera jak w NonBlockingHttp::etapBledu(), 'o' = odpowiedz nieczytelna,
                             // 'm' = zabraklo pamieci na odpowiedz (numer 200: przyszla cala, nie bylo jak jej rozpakowac;
                             // numer 12: nie bylo juz gdzie jej odebrac), 'v' = blad wewnetrzny LVGL
  uint8_t  alarmy;           // ostatni znany stan alarmow - patrz spakujAlarmy()
  uint8_t  seria;            // takie restarty w biezacej serii (konczy ja godzina poprawnych danych) - numer "n" w zgloszeniu
  uint8_t  zgloszonyNr;      // numer w serii ostatniego restartu, o ktorym centrala na pewno sie dowiedziala (0 = o zadnym)
  uint32_t suma;

  static const uint32_t MAGIA = 0x41525331UL;   // "ARS1"

  uint32_t policzSume() const {
    uint32_t s = 0x9E3779B9UL ^ magia;
    s = s * 31u + oczekuje;
    s = s * 31u + kolejnych;
    s = s * 31u + przyczyna;
    s = s * 31u + kodRozlaczenia;
    s = s * 31u + bezDanychS;
    s = s * 31u + heapKb;
    s = s * 31u + blokKb;
    s = s * 31u + lacznie;
    s = s * 31u + bladNumer;
    s = s * 31u + bladEtap;
    s = s * 31u + alarmy;
    s = s * 31u + seria;
    s = s * 31u + zgloszonyNr;
    return s;
  }
  bool wazna() const { return magia == MAGIA && suma == policzSume(); }
  void zapieczetuj() { magia = MAGIA; suma = policzSume(); }
  void wyczysc() {
    memset(this, 0, sizeof(*this));
    zapieczetuj();
  }

  // Tuz przed samoczynnym restartem: liczniki o jeden w gore (z nasyceniem).
  // Nie pieczetuje - wolajacy dopisuje jeszcze reszte pol.
  void odnotujRestart() {
    oczekuje = 1;
    if (kolejnych < 250) kolejnych++;
    if (seria < 250) seria++;
    if (lacznie < 65000) lacznie++;
  }

  // Centrala odpowiedziala na zapytanie, w ktorym szlo zgloszenie tego
  // restartu - czyli na pewno je dostala.
  void zgloszenieDotarlo() {
    zgloszonyNr = seria;
    zapieczetuj();
  }

  // Dane sa poprawne nieprzerwanie od `dobreOdMs` milisekund: wyzeruj to, co
  // mialo wygasnac. Zwraca true, gdy cos sie zmienilo (pamiec jest wtedy juz
  // zapieczetowana).
  bool poDobrychDanych(uint32_t dobreOdMs) {
    bool zmiana = false;
    if (kolejnych != 0 && dobreOdMs >= CZAS_STABILNYCH_DANYCH_MS) { kolejnych = 0; zmiana = true; }
    if ((seria != 0 || zgloszonyNr != 0) && dobreOdMs >= CZAS_KONCA_SERII_MS) { seria = 0; zgloszonyNr = 0; zmiana = true; }
    if (zmiana) zapieczetuj();
    return zmiana;
  }
};
// Pilnuje UWAGI powyzej: kompilacja nie przejdzie, jesli ktos doda
// konstruktor albo domyslna wartosc pola.
static_assert(std::is_trivial<PamiecAutorestartu>::value && std::is_standard_layout<PamiecAutorestartu>::value,
              "PamiecAutorestartu lezy w pamieci RTC_NOINIT - nie moze miec konstruktora ani domyslnych wartosci pol");

// Ostatni znany stan alarmow w jednym bajcie (PamiecAutorestartu::alarmy).
// Panel restartuje sie sam wtedy, gdy NIE MA danych - a przy braku danych
// obowiazuje w nim zasada "ostatni znany stan alarmu zostaje". Zeby restart
// jej nie lamal (cichy brzeczyk i znikniety ekran alarmu w trakcie
// przegrzania pieca), stan alarmow jedzie przez restart w pamieci RTC:
//   bit 0 dym/pozar, bit 1 przegrzanie pieca, bit 2 przegrzanie panelu,
//   bit 3 brzeczyk wyciszony przyciskiem WYCISZ, bity 4-5 ktory typ alarmu
//   wyciszono (BrzeczykAlarmTyp w main.cpp, 0-2), bity 6-7 na jakim poziomie
//   zamknieto ekran alarmu przyciskiem ZAMKNIJ (0-2).
inline uint8_t spakujAlarmy(bool dym, bool ogrzewanie, bool panel, bool wyciszony, int wyciszonyTyp, int odrzuconyPoziom) {
  return (uint8_t)((dym ? 0x01 : 0) | (ogrzewanie ? 0x02 : 0) | (panel ? 0x04 : 0) | (wyciszony ? 0x08 : 0) |
                   ((wyciszonyTyp & 0x03) << 4) | ((odrzuconyPoziom & 0x03) << 6));
}
inline bool alarmyDym(uint8_t a)        { return (a & 0x01) != 0; }
inline bool alarmyOgrzewanie(uint8_t a) { return (a & 0x02) != 0; }
inline bool alarmyPanel(uint8_t a)      { return (a & 0x04) != 0; }
inline bool alarmyJakikolwiek(uint8_t a) { return (a & 0x07) != 0; }
inline bool alarmyWyciszony(uint8_t a)  { return (a & 0x08) != 0; }
// Wartosci spoza zakresu 0-2 (bajt z pamieci RTC moze byc dowolny) -> 0.
inline int  alarmyWyciszonyTyp(uint8_t a) { const int t = (a >> 4) & 0x03; return t <= 2 ? t : 0; }
inline int  alarmyOdrzucony(uint8_t a)    { const int p = (a >> 6) & 0x03; return p <= 2 ? p : 0; }

// Litera etapu do tokenu: tylko [a-z], wszystko inne (w tym '-' = brak
// bledu i przypadkowe wartosci) jako 'x'.
inline char etapDoTokenu(uint8_t etap) {
  return (etap >= 'a' && etap <= 'z') ? (char)etap : 'x';
}

// Powod restartu zglaszany centrali po samoczynnym restarcie, np.
// "auto_milczy_r0_n1_t45_h61_b31_el0":
//   auto    - panel zrestartowal sie sam, bo nie mial danych,
//   milczy  - czego brakowalo (przyczynaSkrot()): wifi | milczy | odp | pam
//             (panelowi zabraklo pamieci) | lvgl (blad wewnetrzny biblioteki
//             ekranu - restart od razu, bez czekania),
//   r0      - ostatni kod rozlaczenia WiFi (0 = nie bylo; nazwaKoduRozlaczenia()),
//   n1      - ktory to taki restart w serii (serie konczy godzina poprawnych
//             danych; przeskok numeru = restarty, ktorych panel nie zglaszal
//             - patrz zglosicRestart()),
//   t45     - ile sekund panel byl bez danych,
//   h61,b31 - wolny RAM i najwiekszy wolny blok w kB. Od v1.2.2 to pamiec, z
//             ktorej naprawde korzysta malloc(); v1.2.1 doliczala ok. 39 kB
//             obszaru, ktorego zwykly malloc() na ESP32 nie uzywa (patrz
//             opis przy ramWolny() w main.cpp), wiec jej "h40_b38" znaczylo
//             w rzeczywistosci ok. 1 kB,
//   el0     - ostatnia nieudana proba statusu: litera "e", etap, numer bledu:
//             el0   limit czasu laczenia (centrala nie odpowiada na probe polaczenia),
//             ec0   polaczono, ale odpowiedz nie przyszla w czasie,
//             en118 connect() odrzucony od razu, blad 118 (na ESP32: EHOSTUNREACH
//                   - brak drogi do centrali; en12 - ENOMEM, brak pamieci),
//             eo500 odpowiedz nieczytelna, kod HTTP 500 (eo200 = zly JSON),
//             em200 odpowiedz przyszla (HTTP 200), ale panelowi zabraklo
//                   pamieci na jej odczytanie (em12: juz na jej odebranie),
//             ev0   blad wewnetrzny LVGL (przy przyczynie "lvgl"),
//             ex0   zadnej nieudanej proby nie bylo.
// Tylko znaki [a-z0-9_] - token idzie do adresu zapytania bez kodowania.
// Centrala wpisuje go do swojego logu (tag=RESET-DIAG ... powod=...).
// Najdluzszy mozliwy token ma 50 znakow - bufor co najmniej TOKEN_AUTORESTARTU_MAKS.
static const size_t TOKEN_AUTORESTARTU_MAKS = 56;
inline void tokenAutorestartu(char* wyjscie, size_t n, const PamiecAutorestartu& m) {
  if (wyjscie == nullptr || n == 0) return;
  snprintf(wyjscie, n, "auto_%s_r%u_n%u_t%u_h%u_b%u_e%c%u", przyczynaSkrot((Przyczyna)m.przyczyna),
           (unsigned)m.kodRozlaczenia, (unsigned)m.seria, (unsigned)m.bezDanychS, (unsigned)m.heapKb, (unsigned)m.blokKb,
           etapDoTokenu(m.bladEtap), (unsigned)m.bladNumer);
}

// ---------------------------------------------------------------------------
// Bateria: czy ogniwo w ogole jest podlaczone?
// ---------------------------------------------------------------------------
// Ogniwo 1S z zabezpieczeniem nie schodzi ponizej ok. 2,5-3,0 V. Odczyt duzo
// nizszy (w logu z 2026-11-10: 0,28 V = zero na przetworniku ESP32 po
// kalibracji) oznacza, ze na dzielniku GPIO34 NIE MA napiecia baterii:
// panel pracuje z samego USB albo dzielnik nie jest podlaczony. v1.1.0
// pokazywal wtedy "0%" - tak samo jak przy rozladowanym ogniwie.
static const float BATERIA_PROG_OBECNOSCI_V = 2.5f;

inline bool bateriaObecna(float napiecieV) {
  return napiecieV >= BATERIA_PROG_OBECNOSCI_V;
}

}  // namespace diag
