// diagnostyka_panelu.h
// ============================================================================
// PANEL LCD - male, CZYSTE funkcje pomocnicze diagnostyki (dodane w v1.2.0)
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
