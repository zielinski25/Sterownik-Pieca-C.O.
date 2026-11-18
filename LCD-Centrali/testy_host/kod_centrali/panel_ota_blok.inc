// WYCINEK z main_centrala.cpp v3.58.6 - wygenerowany przez wytnij_blok.py --ota-blok.
// NIE EDYTOWAC RECZNIE: zmiany rob w main_centrala.cpp i wytnij ponownie.
// Zawiera: decyzje posrednictwa aktualizacji panelu LCD [v3.58.6 PANEL-OTA].
// Wymaga tylko <stdint.h>, <stddef.h>, <string.h>, <stdio.h>.

// [v3.58.6 PANEL-OTA-BEGIN]
// ============================================================================
// Posrednictwo aktualizacji panelu LCD - DECYZJE (bez skutkow ubocznych).
//
// Czysty kod: bez klasy String, bez przydzialow pamieci, bez czekania i bez
// wywolan Arduino. Dzieki temu mozna go wolac w krotkiej sekcji krytycznej
// (g_panelOtaMux) i uruchomic na PC - testy: tests/test_panel_ota.cpp w paczce
// 3.58.6, a symulator panelu LCD (LCD-Centrali/testy_host) wlacza DOKLADNIE
// ten tekst jako "centrale". Skutki decyzji (NVS, log, Telegram) wykonuje kod
// klejacy [v3.58.6 PANEL-OTA-KLEJ] nizej w tym pliku.
//
// Co sie zmienia wzgledem 3.58.5:
//   1. Potwierdzenie nie zalezy od numeru wersji. Plik panelu ma na koncu
//      sume SHA-256 (dopisuje ja esptool przy budowaniu). Centrala bierze z
//      niej identyfikator pliku ("obraz", 16 znakow) i podaje go panelowi.
//      Panel v1.4.0+ zglasza identyfikator obrazu, z ktorego pracuje
//      (panel_obraz=...). Rowne = panel dziala na tym pliku.
//   2. Oferta nie czeka bez konca: konczy ja potwierdzenie, odmowa panelu,
//      powrot starszego panelu po pobraniu, limit pobran, limit czasu albo
//      komenda /update_panel_anuluj.
//   3. W czasie alarmu panel nie widzi oferty i nie dostanie pliku.
// Starszy panel (do v1.3.0) nie zglasza identyfikatora - dla niego zostaje
// stara regula (wersja rowna tagowi wydania) i nowa: gdy po pobraniu pliku
// zglosi sie ponownie, oferta znika (inaczej wgrywalby plik przy kazdym starcie).
// ============================================================================
static const uint32_t PANEL_OTA_WAZNOSC_MS      = 24UL * 60UL * 60UL * 1000UL;  // tyle oferta czeka na panel
static const uint32_t PANEL_OTA_TRANSFER_MAX_MS = 5UL * 60UL * 1000UL;          // dluzej "trwajace" pobieranie uznajemy za zerwane
static const uint8_t  PANEL_OTA_MAX_POBRAN      = 4;                            // tyle razy panel moze ZACZAC pobierac jedna oferte
static const size_t   PANEL_OTA_NAGLOWEK_B      = 24;                           // naglowek obrazu programu ESP32
static const size_t   PANEL_OTA_SUMA_B          = 32;                           // suma SHA-256 na koncu obrazu
static const uint32_t PANEL_OTA_PRZYPOMNIENIE_MS = 5UL * 60UL * 1000UL;         // po tylu minutach bez ruchu: jedna wiadomosc "dlaczego nic sie nie dzieje"
static const uint32_t PANEL_OTA_KONTAKT_MS      = 60UL * 1000UL;                // panel "jest w kontakcie", gdy pytal o status nie dawniej niz tyle temu

enum PanelOtaWynik : uint8_t {
  PANEL_OTA_BEZ_ZMIAN = 0,          // oferta (jesli jest) czeka dalej
  PANEL_OTA_POTWIERDZONO_OBRAZ,     // panel pracuje z oferowanego pliku
  PANEL_OTA_POTWIERDZONO_WERSJA,    // starszy panel zglosil wersje rowna tagowi wydania
  PANEL_OTA_ODMOWA_PANELU,          // panel zglosil, ze tego pliku nie wgra
  PANEL_OTA_STARY_PANEL_WROCIL,     // starszy panel pobieral plik i zglosil sie ponownie
  PANEL_OTA_ZA_DUZO_POBRAN,         // panel zaczynal pobierac PANEL_OTA_MAX_POBRAN razy
  PANEL_OTA_MINAL_CZAS,             // PANEL_OTA_WAZNOSC_MS bez wyniku
  PANEL_OTA_ANULOWANO               // komenda /update_panel_anuluj
};

struct PanelOtaStan {
  bool     aktywna;            // oferta czeka na panel
  uint32_t nr;                 // numer oferty: rosnie przy kazdym /update_panel (trwaly, NVS)
  char     tag[32];            // tag wydania na GitHubie, z ktorego jest plik
  char     obraz[17];          // identyfikator pliku: 8 pierwszych bajtow jego sumy SHA-256, szesnastkowo
  uint32_t rozmiar;            // rozmiar pliku w bajtach
  uint32_t odMs;               // millis() z chwili wystawienia oferty (po restarcie centrali: od nowa)
  uint8_t  pobranZaczetych;    // ile razy panel zaczal pobierac ten plik
  uint8_t  transferow;         // ile polaczen wlasnie wysyla plik panelowi (zwykle 0 albo 1)
  uint32_t transferOdMs;       // millis() poczatku ostatniego z nich
  bool     probaZgloszona;     // panel zglosil, ze startuje z tego pliku, ale jeszcze go nie zatwierdzil
  bool     przypomniano;       // wiadomosc "panel jeszcze nie zaczal" juz poszla (jedna na oferte)
  char     wersjaPrzed[24];    // wersja, ktora panel zglaszal, gdy oferta powstala
};

// To, co panel podal w zapytaniu /api/status. Puste pola = "" (nigdy nullptr).
struct PanelOtaZgloszenie {
  const char *wersja;          // panel_fw
  const char *obraz;           // panel_obraz: obraz zatwierdzony (panel v1.4.0+)
  const char *obrazProba;      // panel_obraz_proba: pierwszy start nowego obrazu, jeszcze nie zatwierdzony
  bool        jestOdmowa;      // panel_ota_odmowa=<nr>-<powod> odczytane poprawnie
  uint32_t    odmowaNr;
  const char *czeka;           // panel_ota_czeka: dlaczego panel jeszcze nie zaczal pobierac (krotkie slowo)
};

// To, co centrala wie o samym panelu - niezaleznie od ofert. Sluzy do
// wiadomosci "dlaczego nic sie nie dzieje" i do tego, zeby nie oferowac pliku,
// z ktorego panel juz pracuje.
struct PanelOtaPanel {
  bool     bylKontakt;         // panel pytal o status przynajmniej raz od startu centrali
  uint32_t kontaktMs;          // millis() ostatniego zapytania
  bool     podajeObraz;        // ostatnie zapytanie mialo panel_obraz albo panel_obraz_proba (firmware v1.4.0+)
  char     obraz[17];          // ostatnio zgloszony obraz ZATWIERDZONY ("" = brak)
  char     czeka[16];          // ostatnio zgloszony powod czekania ("" = brak)
  char     wersja[24];         // ostatnio zgloszona wersja (panel_fw), po oczyszczeniu ze znakow specjalnych
};

// Pierwsze 24 i ostatnie 32 bajty pliku, zbierane w trakcie pobierania z GitHuba.
struct PanelOtaSkan {
  uint8_t  naglowek[PANEL_OTA_NAGLOWEK_B];
  uint8_t  ogon[PANEL_OTA_SUMA_B];
  uint32_t lacznie;
};

// Kopia tekstu z obcieciem i zawsze z zerem na koncu. Znaki spoza zwyklego
// ASCII oraz < > & " (psuja wiadomosc Telegrama w trybie HTML i JSON statusu)
// sa zamieniane na '?'.
static void panelOtaKopiujTekst(char *cel, size_t rozmiarCelu, const char *zrodlo) {
  if (cel == nullptr || rozmiarCelu == 0) return;
  size_t i = 0;
  if (zrodlo != nullptr) {
    for (; zrodlo[i] != '\0' && i + 1 < rozmiarCelu; i++) {
      const char c = zrodlo[i];
      const bool zwykly = (c >= 0x20 && c < 0x7F && c != '<' && c != '>' && c != '&' && c != '"' && c != '\\');
      cel[i] = zwykly ? c : '?';
    }
  }
  cel[i] = '\0';
}

// Identyfikator pliku: 8 pierwszych bajtow sumy jako 16 malych znakow szesnastkowych.
static void panelOtaObrazZSumy(const uint8_t *suma, char *wyjscie17) {
  static const char ZNAKI[] = "0123456789abcdef";
  for (int i = 0; i < 8; i++) {
    wyjscie17[2 * i]     = ZNAKI[(suma[i] >> 4) & 0x0F];
    wyjscie17[2 * i + 1] = ZNAKI[suma[i] & 0x0F];
  }
  wyjscie17[16] = '\0';
}

// true = dokladnie 16 znakow 0-9 a-f (tylko taki tekst przyjmujemy od panelu).
static bool panelOtaObrazPoprawny(const char *tekst) {
  if (tekst == nullptr) return false;
  for (int i = 0; i < 16; i++) {
    const char c = tekst[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return tekst[16] == '\0';
}

static void panelOtaSkanStart(PanelOtaSkan &s) {
  memset(&s, 0, sizeof(s));
}

// Kolejny kawalek pobieranego pliku (dowolnej dlugosci, takze 1 bajt).
static void panelOtaSkanDodaj(PanelOtaSkan &s, const uint8_t *dane, size_t n) {
  if (dane == nullptr || n == 0) return;
  for (size_t i = 0; i < n && (size_t)s.lacznie + i < PANEL_OTA_NAGLOWEK_B; i++) {
    s.naglowek[(size_t)s.lacznie + i] = dane[i];
  }
  if (n >= PANEL_OTA_SUMA_B) {
    memcpy(s.ogon, dane + (n - PANEL_OTA_SUMA_B), PANEL_OTA_SUMA_B);
  } else {
    memmove(s.ogon, s.ogon + n, PANEL_OTA_SUMA_B - n);
    memcpy(s.ogon + (PANEL_OTA_SUMA_B - n), dane, n);
  }
  s.lacznie += (uint32_t)n;
}

// nullptr = plik wyglada na obraz programu dla ESP32 (panel) z suma na koncu;
// inaczej powod odrzucenia - tekst dla uzytkownika.
static const char *panelOtaSkanOcen(const PanelOtaSkan &s) {
  if (s.lacznie < PANEL_OTA_NAGLOWEK_B + PANEL_OTA_SUMA_B + 1) return "plik jest za krotki na program ESP32";
  if (s.naglowek[0] != 0xE9) return "to nie jest program ESP32 (zly pierwszy bajt) - zly plik w wydaniu";
  const unsigned uklad = (unsigned)s.naglowek[12] | ((unsigned)s.naglowek[13] << 8);
  if (uklad == 0x0009) return "to program dla ESP32-S3 (centrala), a panel to ESP32 - zly plik w wydaniu";
  if (uklad != 0x0000) return "to program dla innego ukladu niz ESP32 - zly plik w wydaniu";
  if (s.naglowek[23] != 1) return "plik nie ma sumy SHA-256 na koncu - nie da sie potwierdzic, co panel wgral";
  return nullptr;
}

static void panelOtaSkanObraz(const PanelOtaSkan &s, char *wyjscie17) {
  panelOtaObrazZSumy(s.ogon, wyjscie17);
}

// "12-pobieranie" -> nr = 12, powod = "pobieranie". false = tekst ma inna postac.
// Powod: 1..15 znakow a-z 0-9 _ (nic innego nie trafia potem do wiadomosci).
static bool panelOtaCzytajOdmowe(const char *tekst, uint32_t *nr, char *powod, size_t rozmiarPowodu) {
  if (tekst == nullptr || nr == nullptr || powod == nullptr || rozmiarPowodu < 2) return false;
  uint32_t liczba = 0;
  size_t i = 0;
  for (; tekst[i] >= '0' && tekst[i] <= '9'; i++) {
    if (i >= 9) return false;                       // najwyzej 9 cyfr - miesci sie w uint32_t
    liczba = liczba * 10U + (uint32_t)(tekst[i] - '0');
  }
  if (i == 0 || tekst[i] != '-') return false;
  i++;
  size_t dl = 0;
  for (; tekst[i] != '\0'; i++, dl++) {
    const char c = tekst[i];
    const bool dozwolony = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!dozwolony || dl + 1 >= rozmiarPowodu || dl >= 15) return false;
    powod[dl] = c;
  }
  if (dl == 0) return false;
  powod[dl] = '\0';
  *nr = liczba;
  return true;
}

// Krotkie slowo od panelu (powod czekania): 1..15 znakow a-z 0-9 _. Inne -> "".
static void panelOtaCzytajSlowo(const char *tekst, char *wyjscie, size_t rozmiar) {
  if (wyjscie == nullptr || rozmiar == 0) return;
  wyjscie[0] = '\0';
  if (tekst == nullptr) return;
  size_t dl = 0;
  for (; tekst[dl] != '\0'; dl++) {
    const char c = tekst[dl];
    const bool dozwolony = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!dozwolony || dl + 1 >= rozmiar || dl >= 15) return;
  }
  memcpy(wyjscie, tekst, dl);
  wyjscie[dl] = '\0';
}

// Kazde zapytanie panelu o status (takze gdy nie ma oferty).
static void panelOtaPanelZglosil(PanelOtaPanel &p, const PanelOtaZgloszenie &z, uint32_t nowMs) {
  p.bylKontakt = true;
  p.kontaktMs = nowMs;
  p.podajeObraz = (z.obraz[0] != '\0' || z.obrazProba[0] != '\0');
  size_t i = 0;
  for (; z.obraz[i] != '\0' && i + 1 < sizeof(p.obraz); i++) p.obraz[i] = z.obraz[i];
  p.obraz[i] = '\0';
  panelOtaCzytajSlowo(z.czeka, p.czeka, sizeof(p.czeka));
  panelOtaKopiujTekst(p.wersja, sizeof(p.wersja), z.wersja);
}

static bool panelOtaPanelWKontakcie(const PanelOtaPanel &p, uint32_t nowMs) {
  return p.bylKontakt && (uint32_t)(nowMs - p.kontaktMs) < PANEL_OTA_KONTAKT_MS;
}

// true = panel (w kontakcie) pracuje juz z pliku o tym identyfikatorze - nie ma czego oferowac.
static bool panelOtaPanelMaObraz(const PanelOtaPanel &p, const char *obraz, uint32_t nowMs) {
  return panelOtaPanelWKontakcie(p, nowMs) && obraz != nullptr && obraz[0] != '\0' && strcmp(p.obraz, obraz) == 0;
}

// Koniec oferty. Licznik polaczen zostaje: polaczenie, ktorym wlasnie idzie
// plik, moze jeszcze chwile trwac, a dopoki trwa, pliku nie wolno podmienic.
static void panelOtaWycofaj(PanelOtaStan &s) {
  s.aktywna = false;
  s.probaZgloszona = false;
}

// Nowa oferta (takze w miejsce poprzedniej).
static void panelOtaWystaw(PanelOtaStan &s, uint32_t nr, const char *tag, const char *obraz, uint32_t rozmiar,
                           const char *wersjaPanelu, uint32_t nowMs) {
  memset(&s, 0, sizeof(s));
  s.aktywna = true;
  s.nr = nr;
  panelOtaKopiujTekst(s.tag, sizeof(s.tag), tag);
  panelOtaKopiujTekst(s.obraz, sizeof(s.obraz), obraz);
  s.rozmiar = rozmiar;
  s.odMs = nowMs;
  panelOtaKopiujTekst(s.wersjaPrzed, sizeof(s.wersjaPrzed), wersjaPanelu);
}

// Czy panel ma zobaczyc oferte w /api/status. W czasie alarmu - nie.
static bool panelOtaFlagaDlaPanelu(const PanelOtaStan &s, bool alarm) {
  return s.aktywna && !alarm;
}

// true = plik jest wlasnie wysylany panelowi (takze tuz po wycofaniu oferty).
// Polaczenie starsze niz PANEL_OTA_TRANSFER_MAX_MS uznajemy za zerwane.
static bool panelOtaTransferTrwa(const PanelOtaStan &s, uint32_t nowMs) {
  return s.transferow > 0 && (uint32_t)(nowMs - s.transferOdMs) < PANEL_OTA_TRANSFER_MAX_MS;
}

// GET /panel-ota.bin (haslo juz sprawdzone). Zwraca kod HTTP: 200 = wyslij plik.
static int panelOtaStartPobierania(PanelOtaStan &s, uint32_t nowMs, bool alarm, PanelOtaWynik *wynik) {
  *wynik = PANEL_OTA_BEZ_ZMIAN;
  if (!s.aktywna) return 404;        // nie ma oferty (wycofana albo nigdy jej nie bylo)
  if (alarm) return 503;             // alarm: panel ma pokazywac alarm, a nie wgrywac firmware
  if (s.pobranZaczetych >= PANEL_OTA_MAX_POBRAN) {
    panelOtaWycofaj(s);
    *wynik = PANEL_OTA_ZA_DUZO_POBRAN;
    return 409;
  }
  s.pobranZaczetych++;
  if (s.transferow < 255) s.transferow++;
  s.transferOdMs = nowMs;
  return 200;
}

// Polaczenie, ktorym szedl plik oferty `nr`, zostalo zamkniete (albo plik nie
// dal sie otworzyc). Numer chroni przed pomylka, gdy w miedzyczasie powstala
// nowa oferta.
static void panelOtaKoniecPobierania(PanelOtaStan &s, uint32_t nr) {
  if (s.nr == nr && s.transferow > 0) s.transferow--;
}

// Panel zglosil sie z /api/status?panel_fw=...
static PanelOtaWynik panelOtaPoZgloszeniu(PanelOtaStan &s, const PanelOtaZgloszenie &z) {
  if (!s.aktywna) return PANEL_OTA_BEZ_ZMIAN;
  const bool maObraz = (z.obraz[0] != '\0');
  const bool maProbe = (z.obrazProba[0] != '\0');
  if (maObraz && strcmp(z.obraz, s.obraz) == 0) {
    panelOtaWycofaj(s);
    return PANEL_OTA_POTWIERDZONO_OBRAZ;
  }
  if (z.jestOdmowa && z.odmowaNr == s.nr) {
    panelOtaWycofaj(s);
    return PANEL_OTA_ODMOWA_PANELU;
  }
  if (maProbe) {
    // Pierwszy start nowego obrazu. Panel zatwierdza go dopiero po narysowaniu
    // ekranu i odebraniu danych; gdyby sie przedtem zawiesil, wroci poprzedni
    // firmware. Dlatego to jeszcze nie jest potwierdzenie.
    if (strcmp(z.obrazProba, s.obraz) == 0) s.probaZgloszona = true;
    return PANEL_OTA_BEZ_ZMIAN;
  }
  if (maObraz) return PANEL_OTA_BEZ_ZMIAN;   // panel v1.4.0+ na innym pliku: sam zacznie, gdy bedzie mogl
  // Dalej: panel, ktory nie podaje identyfikatora obrazu (firmware do v1.3.0).
  if (strcmp(z.wersja, s.tag) == 0) {
    panelOtaWycofaj(s);
    return PANEL_OTA_POTWIERDZONO_WERSJA;
  }
  if (s.pobranZaczetych > 0) {
    // Taki panel pobiera plik tylko przy starcie i w tym czasie o nic nie pyta.
    // Skoro pyta znowu, proba sie skonczyla - udana albo nie. Dalsze
    // oferowanie kazaloby mu wgrywac ten sam plik przy kazdym starcie.
    panelOtaWycofaj(s);
    return PANEL_OTA_STARY_PANEL_WROCIL;
  }
  return PANEL_OTA_BEZ_ZMIAN;
}

// Wolane okresowo (zadanie Telegrama).
static PanelOtaWynik panelOtaPoCzasie(PanelOtaStan &s, uint32_t nowMs) {
  if (!s.aktywna) return PANEL_OTA_BEZ_ZMIAN;
  if (panelOtaTransferTrwa(s, nowMs)) return PANEL_OTA_BEZ_ZMIAN;
  if ((uint32_t)(nowMs - s.odMs) < PANEL_OTA_WAZNOSC_MS) return PANEL_OTA_BEZ_ZMIAN;
  panelOtaWycofaj(s);
  return PANEL_OTA_MINAL_CZAS;
}

static PanelOtaWynik panelOtaAnuluj(PanelOtaStan &s) {
  if (!s.aktywna) return PANEL_OTA_BEZ_ZMIAN;
  panelOtaWycofaj(s);
  return PANEL_OTA_ANULOWANO;
}

// true = pora na wiadomosc "panel jeszcze nie zaczal" (jedna na oferte). Nie
// przypominamy, gdy panel juz pobieral albo zglosil start z nowego pliku -
// wtedy wynik przyjdzie osobno.
static bool panelOtaCzasNaPrzypomnienie(PanelOtaStan &s, uint32_t nowMs) {
  if (!s.aktywna || s.przypomniano) return false;
  if (s.pobranZaczetych > 0 || s.probaZgloszona) return false;
  if ((uint32_t)(nowMs - s.odMs) < PANEL_OTA_PRZYPOMNIENIE_MS) return false;
  s.przypomniano = true;
  return true;
}

// Co znaczy powod czekania podany przez panel (panel_ota_czeka=...).
static const char *panelOtaOpisCzekania(const char *powod) {
  if (powod == nullptr || powod[0] == '\0') return "panel nie podal powodu";
  if (strcmp(powod, "alarm") == 0)       return "trwa alarm";
  if (strcmp(powod, "brak_danych") == 0) return "panel nie ma swiezych danych z centrali";
  if (strcmp(powod, "po_starcie") == 0)  return "panel dopiero wystartowal";
  if (strcmp(powod, "odstep") == 0)      return "przerwa po nieudanej probie pobrania";
  if (strcmp(powod, "siec_zajeta") == 0) return "panel pobiera dane do wykresu albo wysyla polecenie";
  if (strcmp(powod, "dotyk") == 0)       return "ktos uzywa ekranu panelu";
  if (strcmp(powod, "bateria") == 0)     return "bateria panelu jest slaba, a panel nie jest podlaczony do zasilania USB";
  if (strcmp(powod, "pamiec") == 0)      return "panel ma za malo wolnej pamieci RAM (pomaga wyjscie z ekranu Wykresy albo wylaczenie i wlaczenie panelu)";
  return "powod podany przez panel jest w logu centrali";
}

// Tresc wiadomosci "panel jeszcze nie zaczal". `s` i `p` to KOPIE stanu
// zrobione w chwili, gdy panelOtaCzasNaPrzypomnienie() zwrocilo true.
static void panelOtaTekstPrzypomnienia(const PanelOtaStan &s, const PanelOtaPanel &p, uint32_t nowMs, bool alarm,
                                       const char *wersjaPanelu, char *wyjscie, size_t rozmiar) {
  if (wyjscie == nullptr || rozmiar == 0) return;
  char wersja[24];
  panelOtaKopiujTekst(wersja, sizeof(wersja), (wersjaPanelu != nullptr && wersjaPanelu[0] != '\0') ? wersjaPanelu : "nieznana");
  const unsigned minut = (unsigned)(PANEL_OTA_PRZYPOMNIENIE_MS / 60000UL);
  const unsigned godzin = (unsigned)(PANEL_OTA_WAZNOSC_MS / 3600000UL);
  if (!panelOtaPanelWKontakcie(p, nowMs)) {
    snprintf(wyjscie, rozmiar,
             "Minelo %u minut, a panel nie zaczal pobierac pliku z wydania %s: panel nie zglasza sie do centrali "
             "(wylaczony albo poza zasiegiem). Plik czeka na niego do %u h od wystawienia. Anulowanie: /update_panel_anuluj",
             minut, s.tag, godzin);
  } else if (!p.podajeObraz) {
    snprintf(wyjscie, rozmiar,
             "Minelo %u minut, a panel nie zaczal pobierac pliku z wydania %s. Panel ma starszy firmware (%s), "
             "ktory sprawdza aktualizacje tylko przy starcie: wylacz i wlacz panel, a pobierze plik sam "
             "(ekran bedzie wtedy pusty przez okolo minute). Plik czeka do %u h od wystawienia. Anulowanie: /update_panel_anuluj",
             minut, s.tag, wersja, godzin);
  } else if (alarm) {
    snprintf(wyjscie, rozmiar,
             "Minelo %u minut, a panel nie zaczal pobierac pliku z wydania %s: trwa alarm, a w czasie alarmu centrala "
             "nie podaje panelowi aktualizacji. Panel zacznie sam po ustaniu alarmu. Plik czeka do %u h od wystawienia. "
             "Anulowanie: /update_panel_anuluj",
             minut, s.tag, godzin);
  } else {
    snprintf(wyjscie, rozmiar,
             "Minelo %u minut, a panel nie zaczal pobierac pliku z wydania %s. Powod podany przez panel: %s. "
             "Panel zacznie sam, gdy przeszkoda zniknie. Plik czeka do %u h od wystawienia. Anulowanie: /update_panel_anuluj",
             minut, s.tag, panelOtaOpisCzekania(p.czeka), godzin);
  }
}

// Nazwa zdarzenia do logu (tag=OTA-PANEL zdarzenie=...).
static const char *panelOtaWynikNazwa(PanelOtaWynik w) {
  switch (w) {
    case PANEL_OTA_POTWIERDZONO_OBRAZ:  return "potwierdzono_obraz";
    case PANEL_OTA_POTWIERDZONO_WERSJA: return "potwierdzono_aktualizacje";
    case PANEL_OTA_ODMOWA_PANELU:       return "odmowa_panelu";
    case PANEL_OTA_STARY_PANEL_WROCIL:  return "starszy_panel_wrocil";
    case PANEL_OTA_ZA_DUZO_POBRAN:      return "za_duzo_pobran";
    case PANEL_OTA_MINAL_CZAS:          return "minal_czas";
    case PANEL_OTA_ANULOWANO:           return "anulowano";
    default:                            return "bez_zmian";
  }
}

// Co znaczy powod odmowy podany przez panel.
static const char *panelOtaOpisOdmowy(const char *powod) {
  if (powod == nullptr) return "panel nie podal powodu";
  if (strcmp(powod, "pobieranie") == 0) return "trzy proby pobrania pliku z centrali sie nie udaly";
  if (strcmp(powod, "obraz") == 0)      return "plik nie przeszedl sprawdzenia w panelu";
  if (strcmp(powod, "cofnieta") == 0)   return "nowy firmware nie wystartowal i panel wrocil do poprzedniego";
  if (strcmp(powod, "za_duzy") == 0)    return "plik nie miesci sie w pamieci panelu";
  if (strcmp(powod, "haslo") == 0)      return "centrala odrzucila haslo OTA panelu (rozne hasla w secrets.h obu urzadzen)";
  if (strcmp(powod, "zapis") == 0)      return "panel nie zdolal zapisac pliku w swojej pamieci";
  return "powod podany przez panel jest w logu centrali";
}

// Wiadomosc dla uzytkownika (Telegram) po zakonczeniu oferty. `przed` to stan
// SPRZED decyzji; `wersjaTeraz` to wersja, ktora panel zglasza teraz ("" gdy
// nieznana); `powodOdmowy` tylko dla PANEL_OTA_ODMOWA_PANELU.
static void panelOtaKomunikat(PanelOtaWynik w, const PanelOtaStan &przed, const char *wersjaTeraz,
                              const char *powodOdmowy, char *wyjscie, size_t rozmiar) {
  if (wyjscie == nullptr || rozmiar == 0) return;
  char wersja[24];
  panelOtaKopiujTekst(wersja, sizeof(wersja), (wersjaTeraz != nullptr && wersjaTeraz[0] != '\0') ? wersjaTeraz : "nieznana");
  const char *przedtem = (przed.wersjaPrzed[0] != '\0') ? przed.wersjaPrzed : "nieznana";
  switch (w) {
    case PANEL_OTA_POTWIERDZONO_OBRAZ:
      snprintf(wyjscie, rozmiar, "Panel zaktualizowany: pracuje na pliku z wydania %s (zglasza wersje %s, przedtem %s).",
               przed.tag, wersja, przedtem);
      break;
    case PANEL_OTA_POTWIERDZONO_WERSJA:
      snprintf(wyjscie, rozmiar, "Panel zaktualizowany do %s.", wersja);
      break;
    case PANEL_OTA_ODMOWA_PANELU:
      snprintf(wyjscie, rozmiar,
               "Panel nie wgral pliku z wydania %s: %s. Aktualizacja wycofana, panel pracuje na dotychczasowym firmware (%s).",
               przed.tag, panelOtaOpisOdmowy(powodOdmowy), wersja);
      break;
    case PANEL_OTA_STARY_PANEL_WROCIL:
      if (strcmp(wersja, przedtem) != 0) {
        snprintf(wyjscie, rozmiar,
                 "Panel pobral plik z wydania %s i zglasza teraz wersje %s (przedtem %s) - aktualizacja wykonana. "
                 "Ten firmware panelu nie podaje, ktory plik wgral, dlatego potwierdzam po zmianie wersji.",
                 przed.tag, wersja, przedtem);
      } else {
        snprintf(wyjscie, rozmiar,
                 "Panel pobieral plik z wydania %s, ale nadal zglasza wersje %s. Albo plik zawiera te sama wersje, "
                 "albo pobieranie sie nie udalo, albo nowy firmware nie wystartowal i panel wrocil do poprzedniego. "
                 "Aktualizacja wycofana, zeby panel nie wgrywal pliku przy kazdym starcie.",
                 przed.tag, wersja);
      }
      break;
    case PANEL_OTA_ZA_DUZO_POBRAN:
      snprintf(wyjscie, rozmiar,
               "Panel zaczynal pobierac plik z wydania %s %u razy i ani razu nie potwierdzil. Aktualizacja wycofana.",
               przed.tag, (unsigned)PANEL_OTA_MAX_POBRAN);
      break;
    case PANEL_OTA_MINAL_CZAS:
      snprintf(wyjscie, rozmiar,
               "Panel nie wgral pliku z wydania %s w ciagu %u h (wylaczony, poza zasiegiem albo ma starszy firmware, "
               "ktory zaczyna aktualizacje dopiero po wylaczeniu i wlaczeniu). Aktualizacja wycofana - "
               "wyslij /update_panel ponownie, gdy panel bedzie gotowy.",
               przed.tag, (unsigned)(PANEL_OTA_WAZNOSC_MS / 3600000UL));
      break;
    case PANEL_OTA_ANULOWANO:
      snprintf(wyjscie, rozmiar, "Aktualizacja panelu (plik z wydania %s) anulowana.", przed.tag);
      break;
    default:
      wyjscie[0] = '\0';
      break;
  }
}
// [v3.58.6 PANEL-OTA-END]
