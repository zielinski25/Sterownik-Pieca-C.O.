// main.cpp
// ============================================================================
// CENTRALA TESTOWA (SYMULATOR) NA ESP32-C3 — warstwa Arduino
// ============================================================================
// PO CO TO JEST:
//   Prawdziwa centrala (Cenatrala Pieca.zip -> src/main_centrala.cpp) jest
//   napisana pod ESP32-S3 N16R8: 16 MB flash, 8 MB PSRAM (36 miejsc alokacji
//   w PSRAM), dwa rdzenie, ~28 endpointow HTTP, 9 bibliotek (Firebase,
//   AsyncWebServer, RTClib, BMP280, DS18B20...) i 1,65 MB jednego pliku
//   zrodlowego. Na ESP32-C3 (4 MB flash, BRAK PSRAM, jeden rdzen RISC-V) tego
//   sie NIE uruchomi — to nie kwestia "przekompilowania", brakuje pamieci.
//
//   Ten firmware to CENTRALA-ZASTEPNIK: wystawia DOKLADNIE ten sam kontrakt
//   HTTP, ktory czyta panel LCD (main.cpp panelu):
//     GET  /api/status?panel_fw=...&reset_powod=...   (co 2 s)
//     GET  /api/ustawienia-get                        (co 5 s)
//     POST /api/ustawienia-set   pole=..&wartosc=..
//     POST /api/servo-manual     os=klapa|syberek&procent=..
//     POST /api/pompa-manual     stan=auto|wl|wyl
//     GET  /api/history          CSV 14 kolumn (wykresy: Godziny/Dzien)
//     GET  /api/history/long     CSV 35 kolumn (wykresy: Dzien/Tydzien)
//     GET  /panel-ota.bin        404 "brak" (panel nie probuje OTA)
//   ...i podklada pod nie SYMULOWANE dane (patrz symulacja.cpp). Dzieki temu
//   panel da sie przetestowac bez prawdziwej centrali i bez czujnikow.
//
// CZEGO TU CELOWO NIE MA: przekaźnikow, PWM, DS18B20, DHT22, BMP280, RTC I2C,
// Firebase, Telegrama, WebSocket-terminala, OTA. To nie jest centrala
// produkcyjna i nie steruje niczym realnym.
//
// SIEĆ: C3 stawia SoftAP o TYCH SAMYCH parametrach, ktore ma zapisane panel
// (WIFI_SSID/WIFI_PASS w main.cpp panelu): "PiecCO_AP" / "KociolCO2026",
// brama 192.168.4.1 — panel laczy sie bez zadnych zmian po swojej stronie.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

#include "wspolne.h"
#include "ustawienia.h"
#include "symulacja.h"
#include "strona_www.h"

// ===========================================================================
// KONFIGURACJA
// ===========================================================================
// [MUSI BYC IDENTYCZNE z panelem LCD] main.cpp panelu, sekcja WiFi:
//   static const char* WIFI_SSID = "PiecCO_AP";
//   static const char* WIFI_PASS = "KociolCO2026";
//   static const char* CENTRALA_HOST = "192.168.4.1";
static const char* AP_SSID = "PiecCO_AP";
static const char* AP_HASLO = "KociolCO2026";
static const uint8_t AP_KANAL = 6;
static const uint8_t AP_MAX_STACJI = 4;

static const uint16_t PORT_HTTP = 80;
static const char* WERSJA_SYMULATORA = "v0.1.0";

// Strefa czasowa Polski (CET/CEST) — panel pokazuje godzine z /api/status,
// wiec zegar musi byc lokalny, tak jak DateTime RTC w prawdziwej centrali.
static const char* STREFA_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

// ===========================================================================
// STAN GLOBALNY
// ===========================================================================
static WiFiServer serwer(PORT_HTTP);
static Preferences nvs;
static ustaw::Stan ustawienia;

// Bufor JSON-a — CELOWO statyczny (nie na stosie): na C3 stos zadania loop()
// ma ~8 KB, a /api/status z polami symulacji to ~1,4 KB tekstu.
static char buforJson[2400];

// Odpowiedzi "statyczne" (bez JSON-a)
static const char* JSON_OK = "{\"ok\":true}";

// Diagnostyka panelu — to, co panel zglasza przy kazdym /api/status.
static char g_panelFw[24] = "";
static char g_panelResetPowod[24] = "";
static uint32_t g_liczbaZapytan = 0;
static uint32_t g_liczbaStatusow = 0;
static uint32_t g_ostatnieZapytanieMs = 0;

// ===========================================================================
// ZEGAR
// ===========================================================================
// Brak RTC I2C na tej plytce, wiec zegar jest "programowy": unix + uptime.
// Po restarcie startujemy z ostatnio zapisanego czasu (Jak RTC z bateria, z ta
// roznica, ze po dluzszym wylaczeniu zasilania czas bedzie spozniony — do
// poprawienia jednym klikiem w panelu WWW albo przez /api/rtc-sync).
static time_t g_czasBaza = 0;
static uint32_t g_czasBazaMs = 0;
static bool g_czasUstawiony = false;
static uint32_t g_ostatniZapisCzasuS = 0;

static uint32_t unixTeraz() {
  if (!g_czasUstawiony) return 0;
  return (uint32_t)(g_czasBaza + (uint32_t)((millis() - g_czasBazaMs) / 1000UL));
}

static void ustawCzasUnix(uint32_t unixSek) {
  g_czasBaza = (time_t)unixSek;
  g_czasBazaMs = millis();
  g_czasUstawiony = (unixSek > 1600000000u);
}

static wsp::Czas czasLokalny() {
  wsp::Czas c;
  if (!g_czasUstawiony) return c;  // ok=false -> panel pokaze "--:--"
  const uint32_t unixSek = unixTeraz();
  c.unix = unixSek;
  time_t t = (time_t)unixSek;
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  c.rok = tmLocal.tm_year + 1900;
  c.miesiac = tmLocal.tm_mon + 1;
  c.dzien = tmLocal.tm_mday;
  c.godzina = tmLocal.tm_hour;
  c.minuta = tmLocal.tm_min;
  c.sekunda = tmLocal.tm_sec;
  c.ok = true;
  return c;
}

// Zapis czasu do NVS raz na godzine (oszczedzanie komorek flash).
static void zapiszCzasDoNvsJesliTrzeba() {
  if (!g_czasUstawiony) return;
  const uint32_t unixSek = unixTeraz();
  if (unixSek - g_ostatniZapisCzasuS < 3600u) return;
  g_ostatniZapisCzasuS = unixSek;
  nvs.putUInt("czasUnix", unixSek);
}

// ===========================================================================
// POMOCNICZE: URL-decode i parametry
// ===========================================================================
static int hexWartosc(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void urlDekoduj(const char* src, size_t dlugosc, char* out, size_t outLen) {
  size_t i = 0, o = 0;
  while (i < dlugosc && o + 1 < outLen) {
    if (src[i] == '%' && i + 2 < dlugosc) {
      int h = hexWartosc(src[i + 1]);
      int l = hexWartosc(src[i + 2]);
      if (h >= 0 && l >= 0) {
        out[o++] = (char)((h << 4) | l);
        i += 3;
        continue;
      }
    }
    out[o++] = (src[i] == '+') ? ' ' : src[i];
    i++;
  }
  out[o] = '\0';
}

// Wyszukiwanie naglowka HTTP bez wzgledu na wielkosc liter (bez strcasestr,
// ktory jest rozszerzeniem GNU — na ESP32 bywa dostepny, ale nie chcemy na tym
// polegac).
static const char* znajdzNaglowek(const char* naglowki, const char* nazwa) {
  const size_t dl = strlen(nazwa);
  for (const char* p = naglowki; p && *p; ++p) {
    bool pasuje = true;
    for (size_t i = 0; i < dl; i++) {
      const char a = p[i];
      const char b = nazwa[i];
      const char al = (a >= 'A' && a <= 'Z') ? (char)(a + 32) : a;
      const char bl = (b >= 'A' && b <= 'Z') ? (char)(b + 32) : b;
      if (al != bl) { pasuje = false; break; }
    }
    if (pasuje) return p;
    p = strchr(p, '\n');
    if (!p) return nullptr;
  }
  return nullptr;
}

// Wyciaga parametr `nazwa` z tekstu "a=1&b=2". Zwraca false gdy brak.
static bool parametr(const char* zrodlo, const char* nazwa, char* out, size_t outLen) {
  if (!zrodlo || !*zrodlo) return false;
  const size_t dNazwy = strlen(nazwa);
  const char* p = zrodlo;
  while (p && *p) {
    const bool naPoczatku = (p == zrodlo) || (*(p - 1) == '&');
    if (naPoczatku && strncmp(p, nazwa, dNazwy) == 0 && p[dNazwy] == '=') {
      const char* wart = p + dNazwy + 1;
      const char* koniec = strchr(wart, '&');
      const size_t dl = koniec ? (size_t)(koniec - wart) : strlen(wart);
      urlDekoduj(wart, dl, out, outLen);
      return true;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
  return false;
}

// ===========================================================================
// MINI-SERWER HTTP (wlasny, na gołych gniazdach)
// ===========================================================================
// CZEMU NIE WebServer z Arduino? Bo panel czyta odpowiedz surowym gniazdem
// BSD i konczy dopiero na ZAMKNIECIU polaczenia (patrz NonBlockingHttp::poll()
// w siec_nieblokujaca_LCD.h: zamyka dopiero przy recv() == 0). Synchroniczny
// WebServer z rdzenia Arduino-ESP32 potrafi trzymac gniazdo otwarte do ~2 s po
// wyslaniu odpowiedzi, a panel ma CALKOWITY_TIMEOUT_MS = 1500 ms — czyli
// zgłaszalby blad przy KAZDYM odczycie. Tutaj robimy dokladnie to, co robi
// ESPAsyncWebServer w prawdziwej centrali: wysylamy Content-Length i NATYCHMIAST
// zamykamy gniazdo. Efekt: recv() == 0 od razu, panel konczy odczyt w ms.
// ===========================================================================
struct Zadanie {
  char metoda[8];
  char sciezka[80];      // sama sciezka, bez query
  char zapytanie[192];   // czesc po '?'
  char cialo[320];       // body POST (form-urlencoded)
  bool maCialo;
};

static void wyslijNaglowki(WiFiClient& k, int kod, const char* typ, size_t dlugosc) {
  const char* opis = (kod == 200) ? "OK"
                     : (kod == 204) ? "No Content"
                     : (kod == 400) ? "Bad Request"
                     : (kod == 401) ? "Unauthorized"
                     : (kod == 404) ? "Not Found"
                                    : "Error";
  k.print("HTTP/1.1 ");
  k.print(kod);
  k.print(" ");
  k.print(opis);
  k.print("\r\nContent-Type: ");
  k.print(typ);
  k.print("\r\nContent-Length: ");
  k.print((unsigned)dlugosc);
  // Bez "Connection: close" klient HTTP/1.1 moglby czekac na kolejna
  // odpowiedz — panel tego nie robi (HTTP/1.0 + zamkniecie), ale przegladarka
  // tak, wiec jawnie konczymy polaczenie.
  k.print("\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n");
}

static void odpowiedzTekst(WiFiClient& k, int kod, const char* typ, const char* tresc,
                           size_t dlugosc) {
  wyslijNaglowki(k, kod, typ, dlugosc);
  if (dlugosc > 0) k.write((const uint8_t*)tresc, dlugosc);
  k.flush();
  k.stop();  // <-- to jest ten EOF, na ktory czeka panel
}

static void odpowiedzJson(WiFiClient& k, int kod, const char* json) {
  odpowiedzTekst(k, kod, "application/json", json, strlen(json));
}

// Pisarz dla historii: wersja "na sucho" (liczy bajty) i wersja wysylajaca.
struct KontekstCsv {
  WiFiClient* klient;
  size_t licznik;
};

static void pisarzLicznik(void* ctx, const char* dane, size_t dlugosc) {
  (void)dane;
  KontekstCsv* c = (KontekstCsv*)ctx;
  c->licznik += dlugosc;
}

static void pisarzKlient(void* ctx, const char* dane, size_t dlugosc) {
  KontekstCsv* c = (KontekstCsv*)ctx;
  if (c->klient) c->klient->write((const uint8_t*)dane, dlugosc);
}

// ===========================================================================
// HANDLERY
// ===========================================================================
static void obsluzStatus(WiFiClient& k, const Zadanie& z) {
  // Panel zglasza swoja wersje i powod ostatniego restartu przy KAZDYM
  // /api/status — w prawdziwej centrali trafia to do logu/Telegrama. Tu
  // zapisujemy to do diagnostyki (widoczne na stronie WWW i w /api/diag).
  char tmp[24];
  if (parametr(z.zapytanie, "panel_fw", tmp, sizeof(tmp))) {
    if (strcmp(tmp, g_panelFw) != 0) {
      snprintf(g_panelFw, sizeof(g_panelFw), "%s", tmp);
      Serial.printf("[PANEL] zgloszona wersja: %s\n", g_panelFw);
    }
  }
  if (parametr(z.zapytanie, "reset_powod", tmp, sizeof(tmp))) {
    if (strcmp(tmp, g_panelResetPowod) != 0) {
      snprintf(g_panelResetPowod, sizeof(g_panelResetPowod), "%s", tmp);
      Serial.printf("[PANEL] powod restartu: %s\n", g_panelResetPowod);
    }
  }

  sym::Kontekst ktx;
  static char ipBuf[16];
  snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.softAPIP().toString().c_str());
  ktx.ip = ipBuf;
  ktx.rssi = 0;  // w trybie AP centrala nie ma wlasnego lacza STA -> 0 (jak na produkcie)
  ktx.internet = false;
  ktx.wifiMode = "AP";
  ktx.wifiSsid = "";
  ktx.panelOtaDostepna = false;  // brak OTA panelu w symulatorze
  ktx.panelOtaWersja = "";
  ktx.panelFwZgloszony = g_panelFw;
  ktx.panelResetPowod = g_panelResetPowod;
  ktx.uptimeS = millis() / 1000UL;
  ktx.heapFree = (unsigned long)ESP.getFreeHeap();
  ktx.stacje = WiFi.softAPgetStationNum();
  ktx.terazMs = millis();

  sym::zbudujStatus(buforJson, sizeof(buforJson), ktx, ustawienia, czasLokalny());
  g_liczbaStatusow++;
  odpowiedzJson(k, 200, buforJson);
}

static void obsluzUstawieniaGet(WiFiClient& k) {
  sym::zbudujUstawienia(buforJson, sizeof(buforJson), ustawienia);
  odpowiedzJson(k, 200, buforJson);
}

static void obsluzUstawieniaSet(WiFiClient& k, const Zadanie& z) {
  char pole[32] = "";
  char wartosc[16] = "";
  // Panel wysyla form-urlencoded w ciele POST; przegladarka moze wyslac query.
  bool maPole = parametr(z.maCialo ? z.cialo : z.zapytanie, "pole", pole, sizeof(pole));
  bool maWart = parametr(z.maCialo ? z.cialo : z.zapytanie, "wartosc", wartosc, sizeof(wartosc));
  if (!maPole || !maWart) {
    if (parametr(z.zapytanie, "pole", pole, sizeof(pole))) maPole = true;
    if (parametr(z.zapytanie, "wartosc", wartosc, sizeof(wartosc))) maWart = true;
  }
  if (!maPole || !maWart) {
    odpowiedzJson(k, 400, "{\"ok\":false,\"error\":\"brak pole/wartosc\"}");
    return;
  }

  const long w = strtol(wartosc, nullptr, 10);
  char err[160] = "";
  if (ustaw::ustaw(ustawienia, pole, w, err, sizeof(err))) {
    const char* klucz = ustaw::kluczNvs(pole);
    if (klucz) nvs.putInt(klucz, (int32_t)ustaw::wartosc(ustawienia, pole));
    Serial.printf("[USTAWIENIA] %s = %ld (NVS: %s)\n", pole, w, klucz ? klucz : "-");
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":true,\"wartosc\":%ld}", w);
    odpowiedzJson(k, 200, buforJson);
  } else {
    Serial.printf("[USTAWIENIA] odrzucono %s=%ld: %s\n", pole, w, err);
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":false,\"error\":\"%s\"}", err);
    odpowiedzJson(k, 400, buforJson);
  }
}

static void obsluzServoManual(WiFiClient& k, const Zadanie& z) {
  const char* zrodlo = z.maCialo ? z.cialo : z.zapytanie;
  char os[16] = "", procentTxt[8] = "";
  bool maOs = parametr(zrodlo, "os", os, sizeof(os));
  bool maProc = parametr(zrodlo, "procent", procentTxt, sizeof(procentTxt));
  if (!maOs && !maProc) {  // fallback na query
    maOs = parametr(z.zapytanie, "os", os, sizeof(os));
    maProc = parametr(z.zapytanie, "procent", procentTxt, sizeof(procentTxt));
  }
  if (!maOs || !maProc) {
    odpowiedzJson(k, 400, "{\"ok\":false,\"error\":\"brak os/procent\"}");
    return;
  }
  const int procent = wsp::ogranicz((int)strtol(procentTxt, nullptr, 10), 0, 100);
  char err[80] = "";
  if (sym::zlecSerwo(os, procent, ustawienia, millis(), err, sizeof(err))) {
    // trybSerwa mogl zostac zmieniony przez uzytkownika — zapisz go
    nvs.putInt("trybSerwa", ustawienia.trybSerwa);
    Serial.printf("[SERWO] %s -> %d%%\n", os, procent);
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":true,\"os\":\"%s\",\"procent\":%d}", os, procent);
    odpowiedzJson(k, 200, buforJson);
  } else {
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":false,\"error\":\"%s\"}", err);
    odpowiedzJson(k, 400, buforJson);
  }
}

static void obsluzPompaManual(WiFiClient& k, const Zadanie& z) {
  char stan[12] = "";
  const char* zrodlo = z.maCialo ? z.cialo : z.zapytanie;
  if (!parametr(zrodlo, "stan", stan, sizeof(stan)) && !parametr(z.zapytanie, "stan", stan, sizeof(stan))) {
    odpowiedzJson(k, 400, "{\"ok\":false,\"error\":\"brak stan\"}");
    return;
  }
  char err[80] = "";
  if (sym::zlecPompe(stan, millis(), err, sizeof(err))) {
    Serial.printf("[POMPA] %s\n", stan);
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":true,\"tryb\":\"%s\"}", stan);
    odpowiedzJson(k, 200, buforJson);
  } else {
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":false,\"error\":\"%s\"}", err);
    odpowiedzJson(k, 400, buforJson);
  }
}

static void obsluzMieszadloManual(WiFiClient& k, const Zadanie& z) {
  char stan[12] = "";
  const char* zrodlo = z.maCialo ? z.cialo : z.zapytanie;
  if (!parametr(zrodlo, "stan", stan, sizeof(stan)) && !parametr(z.zapytanie, "stan", stan, sizeof(stan))) {
    odpowiedzJson(k, 400, "{\"ok\":false,\"error\":\"brak stan\"}");
    return;
  }
  char err[80] = "";
  if (sym::zlecMieszadlo(stan, millis(), err, sizeof(err))) {
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":true,\"tryb\":\"%s\"}", stan);
    odpowiedzJson(k, 200, buforJson);
  } else {
    snprintf(buforJson, sizeof(buforJson), "{\"ok\":false,\"error\":\"%s\"}", err);
    odpowiedzJson(k, 400, buforJson);
  }
}

static void obsluzHistorie(WiFiClient& k, bool dlugi) {
  // Dwa przebiegi: 1) policz dlugosc CSV, 2) wyslij z poprawnym Content-Length.
  // Dlaczego nie chunked/bez Content-Length: panel (wykresyFetchWykonaj())
  // ustawia useHTTP10(true) i czyta strumien do zamkniecia gniazda, ale
  // Content-Length jest twardsza gwarancja przejscia przez HTTPClient.
  KontekstCsv licz = {nullptr, 0};
  sym::historiaCsv(dlugi, pisarzLicznik, &licz);
  wyslijNaglowki(k, 200, "text/plain; charset=utf-8", licz.licznik);
  KontekstCsv wys = {&k, 0};
  sym::historiaCsv(dlugi, pisarzKlient, &wys);
  k.flush();
  k.stop();
  Serial.printf("[HISTORIA] %s: %u B\n", dlugi ? "dluga" : "krotka", (unsigned)wys.licznik);
}

static void obsluzRtcSync(WiFiClient& k, const Zadanie& z) {
  // Ten sam kontrakt co /api/rtc-sync w centrali: rok/miesiac/dzien/godzina/
  // minuta/sekunda czasu LOKALNEGO (w przegladarce), walidacja zakresow.
  const char* zrodlo = z.maCialo ? z.cialo : z.zapytanie;
  char rokTxt[8] = "", miesTxt[8] = "", dzienTxt[8] = "", godzTxt[8] = "", minTxt[8] = "", sekTxt[8] = "";
  auto pobierz = [&](const char* nazwa, char* out, size_t n) -> bool {
    if (parametr(zrodlo, nazwa, out, n)) return true;
    return parametr(z.zapytanie, nazwa, out, n);
  };
  if (!pobierz("rok", rokTxt, sizeof(rokTxt)) || !pobierz("miesiac", miesTxt, sizeof(miesTxt)) ||
      !pobierz("dzien", dzienTxt, sizeof(dzienTxt)) || !pobierz("godzina", godzTxt, sizeof(godzTxt)) ||
      !pobierz("minuta", minTxt, sizeof(minTxt)) || !pobierz("sekunda", sekTxt, sizeof(sekTxt))) {
    odpowiedzJson(k, 400, "{\"ok\":false,\"blad\":\"brak_parametru\"}");
    return;
  }
  const int rok = atoi(rokTxt), miesiac = atoi(miesTxt), dzien = atoi(dzienTxt);
  const int godzina = atoi(godzTxt), minuta = atoi(minTxt), sekunda = atoi(sekTxt);
  const bool zakresOk = (rok >= 2024 && rok <= 2099) && (miesiac >= 1 && miesiac <= 12) &&
                        (dzien >= 1 && dzien <= 31) && (godzina >= 0 && godzina <= 23) &&
                        (minuta >= 0 && minuta <= 59) && (sekunda >= 0 && sekunda <= 59);
  if (!zakresOk) {
    odpowiedzJson(k, 400, "{\"ok\":false,\"blad\":\"zakres_poza_granicami\"}");
    return;
  }
  struct tm tmLocal = {};
  tmLocal.tm_year = rok - 1900;
  tmLocal.tm_mon = miesiac - 1;
  tmLocal.tm_mday = dzien;
  tmLocal.tm_hour = godzina;
  tmLocal.tm_min = minuta;
  tmLocal.tm_sec = sekunda;
  tmLocal.tm_isdst = -1;  // niech newlib sam rozstrzygnie CET/CEST
  const time_t unixSek = mktime(&tmLocal);
  if (unixSek <= 0) {
    odpowiedzJson(k, 400, "{\"ok\":false,\"blad\":\"czas_niepoliczalny\"}");
    return;
  }
  ustawCzasUnix((uint32_t)unixSek);
  g_ostatniZapisCzasuS = (uint32_t)unixSek;
  nvs.putUInt("czasUnix", (uint32_t)unixSek);
  Serial.printf("[CZAS] ustawiony z przegladarki: %04d-%02d-%02d %02d:%02d:%02d\n",
                rok, miesiac, dzien, godzina, minuta, sekunda);
  odpowiedzJson(k, 200, "{\"ok\":true}");
}

static void obsluzTest(WiFiClient& k, const Zadanie& z) {
  // Wstrzykiwanie awarii, zeby sprawdzic ekrany alarmowe panelu bez czekania
  // na realne przegrzanie / dym. Wylacznie do testow.
  char typ[16] = "", stan[8] = "";
  parametr(z.zapytanie, "typ", typ, sizeof(typ));
  if (!parametr(z.zapytanie, "stan", stan, sizeof(stan))) {
    snprintf(stan, sizeof(stan), "1");
  }
  const bool wl = (strcmp(stan, "0") != 0);
  if (!strcmp(typ, "dym")) {
    sym::st.injDym = wl;
  } else if (!strcmp(typ, "co")) {
    sym::st.injAlarmOgrz = wl;
  } else if (!strcmp(typ, "panel")) {
    sym::st.injAlarmPanel = wl;
  } else if (!strcmp(typ, "deszcz")) {
    sym::st.deszczStan = atoi(stan);
    sym::st.deszczMin = (sym::st.deszczStan == 0) ? -1 : atoi(stan) + 25;
  } else if (!strcmp(typ, "klapa")) {
    sym::st.klapaDeg = wsp::ogranicz(atoi(stan), 0, 180);
  } else if (!strcmp(typ, "syberka")) {
    sym::st.syberkaDeg = wsp::ogranicz(atoi(stan), 0, 90);
  } else if (!strcmp(typ, "temp")) {
    // Ręczne ustawienie temperatury pieca (do testu progow alarmowych).
    sym::st.tOgrz = (float)atof(stan);
    sym::st.tOgrzSr = sym::st.tOgrz;
  } else if (!strcmp(typ, "historia")) {
    sym::wyczyscHistorie();
  } else {
    odpowiedzJson(k, 400, "{\"ok\":false,\"error\":\"typ: dym|co|panel|deszcz|klapa|syberka|temp|historia\"}");
    return;
  }
  Serial.printf("[TEST] %s = %s\n", typ, stan);
  odpowiedzJson(k, 200, JSON_OK);
}

static void obsluzDiag(WiFiClient& k) {
  snprintf(buforJson, sizeof(buforJson),
           "{\"ok\":true,\"wersja\":\"%s\",\"uptime_s\":%lu,\"heap_wolny\":%u,"
           "\"heap_min\":%u,\"stacje_ap\":%d,\"zapytania\":%lu,\"statusy\":%lu,"
           "\"panel_fw\":\"%s\",\"panel_reset\":\"%s\","
           "\"czas_unix\":%lu,\"czas_ok\":%s,"
           "\"t_ogrz\":%d,\"t_ogrz_sr\":%d,\"klapa\":%d,\"syberka\":%d,"
           "\"pompa\":%s,\"alarm_dym\":%s,\"hist_krotka\":%d,\"hist_dluga\":%d}",
           WERSJA_SYMULATORA, (unsigned long)(millis() / 1000UL),
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
           WiFi.softAPgetStationNum(), (unsigned long)g_liczbaZapytan,
           (unsigned long)g_liczbaStatusow, g_panelFw, g_panelResetPowod,
           (unsigned long)unixTeraz(), g_czasUstawiony ? "true" : "false",
           wsp::roundInt(sym::st.tOgrz), wsp::roundInt(sym::st.tOgrzSr),
           sym::st.klapaDeg, sym::st.syberkaDeg, sym::st.pompaOn ? "true" : "false",
           sym::st.alarmDym ? "true" : "false", sym::histKrotkaLiczba,
           sym::histDlugaLiczba);
  odpowiedzJson(k, 200, buforJson);
}

// ===========================================================================
// ROZPROWADZANIE ZADAN
// ===========================================================================
static void obsluzZadanie(WiFiClient& klient, const Zadanie& z) {
  g_liczbaZapytan++;
  g_ostatnieZapytanieMs = millis();

  const bool get = (strcmp(z.metoda, "GET") == 0);
  const bool post = (strcmp(z.metoda, "POST") == 0);

  if (get && (!strcmp(z.sciezka, "/") || !strcmp(z.sciezka, "/index.html"))) {
    odpowiedzTekst(klient, 200, "text/html; charset=utf-8", STRONA_HTML, strlen(STRONA_HTML));
    return;
  }
  if (get && !strcmp(z.sciezka, "/api/status")) return obsluzStatus(klient, z);
  if (get && !strcmp(z.sciezka, "/api/ustawienia-get")) return obsluzUstawieniaGet(klient);
  if (post && !strcmp(z.sciezka, "/api/ustawienia-set")) return obsluzUstawieniaSet(klient, z);
  if (post && !strcmp(z.sciezka, "/api/servo-manual")) return obsluzServoManual(klient, z);
  if (post && !strcmp(z.sciezka, "/api/pompa-manual")) return obsluzPompaManual(klient, z);
  if (post && !strcmp(z.sciezka, "/api/mieszadlo-manual")) return obsluzMieszadloManual(klient, z);
  if (get && !strcmp(z.sciezka, "/api/history")) return obsluzHistorie(klient, false);
  if (get && !strcmp(z.sciezka, "/api/history/long")) return obsluzHistorie(klient, true);
  if (get && !strcmp(z.sciezka, "/api/rtc-sync")) return obsluzRtcSync(klient, z);
  if (post && !strcmp(z.sciezka, "/api/rtc-sync")) return obsluzRtcSync(klient, z);
  if (get && !strcmp(z.sciezka, "/api/diag")) return obsluzDiag(klient);
  if (get && !strcmp(z.sciezka, "/api/test")) return obsluzTest(klient, z);

  // OTA panelu — w prawdziwej centrali serwuje plik z LittleFS. Tu celowo 404:
  // panel sprawdza panel_ota_dostepna=false i nigdy tego nie wola, ale jesli
  // ktos wymusi, dostanie czysty blad zamiast zawieszenia.
  if (get && !strcmp(z.sciezka, "/panel-ota.bin")) {
    odpowiedzTekst(klient, 404, "text/plain", "Brak OTA w symulatorze", 23);
    return;
  }
  snprintf(buforJson, sizeof(buforJson),
           "{\"ok\":false,\"error\":\"nieznana sciezka\",\"sciezka\":\"%s\"}", z.sciezka);
  odpowiedzJson(klient, 404, buforJson);
}

// ===========================================================================
// PARSOWANIE ZADANIA HTTP
// ===========================================================================
static bool przeczytajZadanie(WiFiClient& klient, Zadanie& z) {
  static char surowe[1400];
  size_t n = 0;
  const unsigned long start = millis();
  unsigned long limit = 300UL;  // na pierwszy bajt — w LAN przychodzi w ms

  // 1) Naglowki — do "\r\n\r\n". Panel ma CALKOWITY_TIMEOUT_MS = 1500 ms na
  //    CALA wymiane, wiec nie mozemy sobie pozwolic na dlugie czekanie na
  //    "martwe" polaczenie (np. skaner portow): 300 ms na pierwszy bajt,
  //    potem 700 ms na reszte naglowkow.
  while (millis() - start < limit) {
    while (klient.available() && n < sizeof(surowe) - 1) {
      surowe[n++] = (char)klient.read();
    }
    if (n >= 4 && surowe[n - 4] == '\r' && surowe[n - 3] == '\n' &&
        surowe[n - 2] == '\r' && surowe[n - 1] == '\n') {
      break;
    }
    if (n > 0) limit = 700UL;  // pierwszy bajt juz jest — dociagamy naglowki
    if (!klient.connected() && !klient.available()) break;
    delay(1);
  }
  if (n == 0) return false;
  surowe[n] = '\0';

  // 2) Pierwsza linia: METODA SP SCIEZKA?QUERY SP HTTP/x.y
  char* koniecLinii = strstr(surowe, "\r\n");
  if (!koniecLinii) return false;
  *koniecLinii = '\0';
  char* sp1 = strchr(surowe, ' ');
  if (!sp1) return false;
  *sp1 = '\0';
  snprintf(z.metoda, sizeof(z.metoda), "%s", surowe);
  char* cel = sp1 + 1;
  char* sp2 = strchr(cel, ' ');
  if (sp2) *sp2 = '\0';

  char* pytajnik = strchr(cel, '?');
  if (pytajnik) {
    *pytajnik = '\0';
    snprintf(z.zapytanie, sizeof(z.zapytanie), "%s", pytajnik + 1);
  } else {
    z.zapytanie[0] = '\0';
  }
  snprintf(z.sciezka, sizeof(z.sciezka), "%s", cel);

  // 3) Content-Length (POST-y panelu maja male cialo, < 100 B).
  z.maCialo = false;
  z.cialo[0] = '\0';
  long dlugoscCiala = 0;
  {
    const char* p = znajdzNaglowek(koniecLinii + 1, "content-length:");
    if (p) dlugoscCiala = strtol(p + 15, nullptr, 10);
  }
  if (dlugoscCiala > 0 && dlugoscCiala < (long)sizeof(z.cialo)) {
    // Cialo zaczyna sie 4 znaki za poczatkiem "\r\n\r\n" (koniecLinii
    // wskazuje na pierwszy z tych znakow, nadpisany potem przez '\0').
    const size_t pozycjaCiala = (size_t)(koniecLinii - surowe) + 4;
    size_t dostepne = (pozycjaCiala <= n) ? (n - pozycjaCiala) : 0;
    size_t zebrane = 0;
    while (zebrane < dostepne && zebrane < (size_t)dlugoscCiala) {
      z.cialo[zebrane] = surowe[pozycjaCiala + zebrane];
      zebrane++;
    }
    const unsigned long startCiala = millis();
    while (zebrane < (size_t)dlugoscCiala && millis() - startCiala < 600UL) {
      while (klient.available() && zebrane < (size_t)dlugoscCiala) {
        z.cialo[zebrane++] = (char)klient.read();
      }
      delay(1);
    }
    z.cialo[zebrane] = '\0';
    z.maCialo = true;
  }
  return true;
}

// ===========================================================================
// KONSOLA SERIAL (diagnostyka bez przegladarki)
// ===========================================================================
static void obsluzSerial() {
  static char linia[96];
  static size_t dlugosc = 0;
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (dlugosc == 0) continue;
      linia[dlugosc] = '\0';
      dlugosc = 0;
      if (!strcmp(linia, "help") || !strcmp(linia, "pomoc")) {
        Serial.println(F("Komendy: ip | czas | czas RRRR-MM-DD GG:MM:SS | dym 0/1 | co 0/1 |"
                         " panel 0/1 | deszcz 0/1/2 | diag | reset"));
      } else if (!strcmp(linia, "ip")) {
        Serial.printf("AP: %s  SSID: %s  stacje: %d\n",
                      WiFi.softAPIP().toString().c_str(), AP_SSID, WiFi.softAPgetStationNum());
      } else if (!strcmp(linia, "diag")) {
        Serial.printf("heap=%u min=%u uptime=%lus stacje=%d zapytania=%lu statusy=%lu fw_panelu=%s\n",
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                      (unsigned long)(millis() / 1000UL), WiFi.softAPgetStationNum(),
                      (unsigned long)g_liczbaZapytan, (unsigned long)g_liczbaStatusow, g_panelFw);
      } else if (!strcmp(linia, "reset")) {
        Serial.println(F("Restart..."));
        delay(100);
        ESP.restart();
      } else if (strncmp(linia, "czas ", 5) == 0) {
        int r, m, d, g, mi, s;
        if (sscanf(linia + 5, "%d-%d-%d %d:%d:%d", &r, &m, &d, &g, &mi, &s) == 6) {
          struct tm tmLocal = {};
          tmLocal.tm_year = r - 1900; tmLocal.tm_mon = m - 1; tmLocal.tm_mday = d;
          tmLocal.tm_hour = g; tmLocal.tm_min = mi; tmLocal.tm_sec = s; tmLocal.tm_isdst = -1;
          const time_t u = mktime(&tmLocal);
          if (u > 0) {
            ustawCzasUnix((uint32_t)u);
            nvs.putUInt("czasUnix", (uint32_t)u);
            Serial.println(F("Czas ustawiony."));
          } else {
            Serial.println(F("Nie udalo sie przeliczyc czasu."));
          }
        } else {
          Serial.println(F("Format: czas 2026-11-05 14:30:00"));
        }
      } else if (!strcmp(linia, "czas")) {
        const wsp::Czas c = czasLokalny();
        if (c.ok) Serial.printf("%04d-%02d-%02d %02d:%02d:%02d (unix %lu)\n", c.rok, c.miesiac,
                                c.dzien, c.godzina, c.minuta, c.sekunda, (unsigned long)c.unix);
        else Serial.println(F("Czas nieustawiony (panel pokaze --:--)"));
      } else if (strncmp(linia, "dym ", 4) == 0) {
        sym::st.injDym = (atoi(linia + 4) != 0);
        Serial.printf("symulacja dymu: %s\n", sym::st.injDym ? "WL" : "WYL");
      } else if (strncmp(linia, "co ", 3) == 0) {
        sym::st.injAlarmOgrz = (atoi(linia + 3) != 0);
        Serial.printf("alarm CO: %s\n", sym::st.injAlarmOgrz ? "WL" : "WYL");
      } else if (strncmp(linia, "panel ", 6) == 0) {
        sym::st.injAlarmPanel = (atoi(linia + 6) != 0);
        Serial.printf("alarm panelu: %s\n", sym::st.injAlarmPanel ? "WL" : "WYL");
      } else if (strncmp(linia, "deszcz ", 7) == 0) {
        sym::st.deszczStan = atoi(linia + 7);
        sym::st.deszczMin = (sym::st.deszczStan == 0) ? -1 : 25;
        Serial.printf("deszcz_stan=%d deszcz_min=%d\n", sym::st.deszczStan, sym::st.deszczMin);
      } else {
        Serial.println(F("Nieznana komenda. Wpisz: pomoc"));
      }
    } else if (dlugosc < sizeof(linia) - 1) {
      linia[dlugosc++] = c;
    }
  }
}

// ===========================================================================
// USTAWIENIA — ODCZYT Z NVS
// ===========================================================================
static void wczytajUstawienia() {
  // UWAGA: nvs.begin("piec") jest juz wywolane w setup() PRZED tym — jedno
  // otwarcie namespace'u na caly program (tak samo jak w main_centrala.cpp).
  const char* pola[] = {"wybor", "trybSerwa", "czasOn", "czasOff", "tempOn", "tempOff",
                        "antystopWlaczony", "antystopDni", "mieszadloWlaczony",
                        "mieszadloCzasOn", "mieszadloCzasOff", "tempZadServo",
                        "histerServo", "skokKlapy", "skokSyberka", "odchylTemp",
                        "mnoznik", "progAlarmTemp", "progAlarmDym", "dymCzasOn",
                        "dymCzasOff", "dymCzasStabilizacji", "dymProgTemp",
                        "dymTrybPracy"};
  for (const char* pole : pola) {
    const char* klucz = ustaw::kluczNvs(pole);
    if (!klucz) continue;
    if (!nvs.isKey(klucz)) continue;
    const int32_t zapisane = nvs.getInt(klucz, (int32_t)ustaw::wartosc(ustawienia, pole));
    char err[160] = "";
    // Walidujemy tak samo jak przy zapisie z API — jesli w NVS lezy cos spoza
    // zakresu (np. po recznych zmianach), zostaje wartosc domyslna.
    if (!ustaw::ustaw(ustawienia, pole, (long)zapisane, err, sizeof(err))) {
      Serial.printf("[NVS] odrzucona wartosc %s=%ld (%s)\n", pole, (long)zapisane, err);
    }
  }
  Serial.printf("[NVS] wczytano: wybor=%d trybSerwa=%d progAlarmTemp=%d progAlarmDym=%d\n",
                ustawienia.wybor, ustawienia.trybSerwa, ustawienia.progAlarmTemp,
                ustawienia.progAlarmDym);
}

// ===========================================================================
// SETUP / LOOP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("============================================================"));
  // Uklad bierzemy z ESP, a nie na sztywno: ten sam kod chodzi na C3, klasycznym
  // ESP32 i S3 (patrz platformio.ini), wiec banner nie moze klamac.
  Serial.printf(" CENTRALA TESTOWA (SYMULATOR) — %s, %u rdzeni, %lu MHz\n",
                ESP.getChipModel(), (unsigned)ESP.getChipCores(),
                (unsigned long)ESP.getCpuFreqMHz());
  Serial.println(F(" Zastepnik prawdziwej centrali dla testow panelu LCD."));
  Serial.println(F("============================================================"));

  // Strefa czasowa (bez tego localtime_r zwracalby UTC).
  setenv("TZ", STREFA_TZ, 1);
  tzset();

  // Zegar: odtworz ostatni zapisany czas (jak RTC z bateria).
  nvs.begin("piec", false);
  {
    const uint32_t zapisany = nvs.getUInt("czasUnix", 0);
    if (zapisany > 1600000000u) {
      ustawCzasUnix(zapisany);
      g_ostatniZapisCzasuS = zapisany;
      Serial.printf("[CZAS] odtworzony z NVS: unix=%lu (po dlugim wylaczeniu moze byc spozniony)\n",
                    (unsigned long)zapisany);
    } else {
      Serial.println(F("[CZAS] brak zapisanego czasu — panel pokaze \"--:--\"."));
      Serial.println(F("       Ustaw go z panelu WWW (http://192.168.4.1) albo komenda: czas 2026-11-05 14:30:00"));
    }
  }
  wczytajUstawienia();

  sym::reset();

  // --- SoftAP o parametrach, ktore ma zapisane panel LCD ---
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP);
  IPAddress ip(192, 168, 4, 1), brama(192, 168, 4, 1), maska(255, 255, 255, 0);
  WiFi.softAPConfig(ip, brama, maska);
  const bool apOk = WiFi.softAP(AP_SSID, AP_HASLO, AP_KANAL, false, AP_MAX_STACJI);
  Serial.printf("[AP] %s  SSID=\"%s\"  IP=%s  kanal=%d\n",
                apOk ? "wstalo" : "BLAD STARTOWANIA", AP_SSID,
                WiFi.softAPIP().toString().c_str(), AP_KANAL);

  serwer.begin();
  serwer.setNoDelay(true);
  Serial.printf("[HTTP] nasluchuje na %s:%u\n", WiFi.softAPIP().toString().c_str(), PORT_HTTP);
  Serial.println(F("[HTTP] Panel LCD laczy sie do AP powyzej i pyta /api/status co 2 s."));
  Serial.println(F("[WWW]  Panel testowy: http://192.168.4.1 (z telefonu/PC w sieci PiecCO_AP)"));

  // Historia startuje z pierwsza probka od razu, zeby wykresy nie byly puste.
  sym::tick(millis(), czasLokalny(), ustawienia);

  // Panel/PC moze wolno czytac CSV (~25 KB) — nie pozwalamy WDT ubic loop().
  disableLoopWDT();

  Serial.println(F("[GOTOWE] Wpisz \"pomoc\" w konsoli Serial."));
}

void loop() {
  // 1) Symulacja + probka historii co 1 s.
  static unsigned long ostatniTick = 0;
  static int zapamietanyTrybSerwa = -1;
  if (millis() - ostatniTick >= 1000UL) {
    ostatniTick = millis();
    sym::tick(ostatniTick, czasLokalny(), ustawienia);
    zapiszCzasDoNvsJesliTrzeba();
    // Tryb serwa moze zmienic sie SAM (auto-wygaśniecie trybu Recznego po
    // 60 min — patrz obslugaWygasanSymulacjiElementow w centrali) — wtedy
    // trzeba go zapisac, bo nikt nie wyslal /api/ustawienia-set.
    if (zapamietanyTrybSerwa < 0) {
      zapamietanyTrybSerwa = ustawienia.trybSerwa;
    } else if (zapamietanyTrybSerwa != ustawienia.trybSerwa) {
      zapamietanyTrybSerwa = ustawienia.trybSerwa;
      nvs.putInt("trybSerwa", ustawienia.trybSerwa);
      Serial.printf("[SERWO] tryb zmieniony automatycznie -> %d\n", ustawienia.trybSerwa);
    }
  }

  // 2) Jedno zadanie HTTP na raz (mini-serwer). Kolejne polaczenia czekaja w
  //    kolejce gniazda — panel wysyla najwyzej 2-3 rownolegle, a odpowiedzi
  //    sa male (poza historia, ktora i tak leci tylko z ekranu Wykresy).
  WiFiClient klient = serwer.available();
  if (klient) {
    Zadanie z;
    memset(&z, 0, sizeof(z));
    if (przeczytajZadanie(klient, z)) {
      obsluzZadanie(klient, z);
    } else {
      klient.stop();
    }
  }

  // 3) Konsola Serial.
  obsluzSerial();

  // 4) Diagnostyka: ostrzezenie, jesli od 5 minut nikt nie pytal o status
  //    (panel nie jest podlaczony / nie laczy sie z AP).
  static unsigned long ostatniaDiagnostyka = 0;
  if (millis() - ostatniaDiagnostyka > 300000UL) {
    ostatniaDiagnostyka = millis();
    if (g_ostatnieZapytanieMs != 0 && millis() - g_ostatnieZapytanieMs > 300000UL) {
      Serial.printf("[DIAG] Brak zapytan od %lu s (panel podlaczony do AP? stacje=%d)\n",
                    (unsigned long)((millis() - g_ostatnieZapytanieMs) / 1000UL),
                    WiFi.softAPgetStationNum());
    }
  }
}
