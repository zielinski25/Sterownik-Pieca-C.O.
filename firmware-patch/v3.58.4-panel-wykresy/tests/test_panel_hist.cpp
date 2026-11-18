// ============================================================================
// Testy na PC kodu [v3.58.4 PANEL-WYKRESY] centrali (GET /api/history/panel).
//
// Testowany jest tekst WYCIETY z ../src/main_centrala.cpp przez wytnij_blok.py
// (plik panel_hist_wycinek.inc powstaje przy kazdym uruchomieniu testow), a nie
// jego kopia. Ponizej jest tylko minimalne otoczenie, ktorego ten kod wymaga:
// String, spinlock, ps_malloc, DLOGF.
//
// Dwa rodzaje sprawdzen:
//   1. scenariusze z wynikiem policzonym recznie (co MA wyjsc),
//   2. porownanie z NIEZALEZNYM liczeniem "sekunda po sekundzie" na danych
//      losowych (czy rachunek na odcinkach czasu nie myli sie o sekunde ani
//      o kubelek).
// Uruchomienie: ./uruchom_testy.sh
// ============================================================================
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Otoczenie (atrapa tego, co w centrali daje Arduino / ESP-IDF)
// ---------------------------------------------------------------------------
namespace atrapa {
size_t odmowaRezerwyOd = SIZE_MAX;   // String::reserve(n) odmawia dla n >= tej wartosci
long odmowaConcatPo = -1;            // >= 0: concat() odmawia po tylu udanych wywolaniach
long licznikConcat = 0;
bool psramJest = true;               // false: ps_malloc() zwraca nullptr (plytka bez PSRAM)
bool mallocOdmawia = false;          // true: malloc() tez zwraca nullptr
int zywychBlokow = 0;                // przydzialy niezwolnione (wyciek = blad)
int sekcjaKrytyczna = 0;             // glebokosc portENTER_CRITICAL
int przydzialWSekcji = 0;            // przydzialy wykonane pod spinlockiem (ma byc 0)
std::string ostatniLog;
}   // namespace atrapa

class String {
 public:
  String() {}
  String(const char* s) : t(s ? s : "") {}
  String& operator=(const char* s) { t = s ? s : ""; return *this; }
  bool reserve(unsigned int n) {
    if ((size_t)n >= atrapa::odmowaRezerwyOd) return false;
    t.reserve(n);
    return true;
  }
  bool concat(const char* s) {
    if (atrapa::odmowaConcatPo >= 0 && atrapa::licznikConcat++ >= atrapa::odmowaConcatPo) return false;
    t += s;
    return true;
  }
  unsigned int length() const { return (unsigned int)t.size(); }
  const char* c_str() const { return t.c_str(); }
  std::string t;
};

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m), atrapa::sekcjaKrytyczna++)
#define portEXIT_CRITICAL(m) ((void)(m), atrapa::sekcjaKrytyczna--)

static void* atrapaPrzydziel(size_t n, bool zPsram) {
  if (atrapa::sekcjaKrytyczna != 0) atrapa::przydzialWSekcji++;
  if (zPsram && !atrapa::psramJest) return nullptr;
  if (!zPsram && atrapa::mallocOdmawia) return nullptr;
  atrapa::zywychBlokow++;
  return std::malloc(n);
}
static void atrapaZwolnij(void* p) {
  if (p != nullptr) atrapa::zywychBlokow--;
  std::free(p);
}
static void* ps_malloc(size_t n) { return atrapaPrzydziel(n, true); }
#define malloc(n) atrapaPrzydziel((n), false)
#define free(p) atrapaZwolnij(p)

enum { DC_HISTORIA = 1 };
enum { DL_INFO = 2 };
static void atrapaLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void atrapaLog(const char* fmt, ...) {
  char b[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  atrapa::ostatniLog = b;
}
#define DLOGF(kat, poziom, fmt, ...) atrapaLog(fmt, ##__VA_ARGS__)

static bool g_fsWlaczony = true;
static bool historiaZapisNaFsWlaczony() { return g_fsWlaczony; }

// ---------------------------------------------------------------------------
// Kod centrali - wyciety z main_centrala.cpp
// ---------------------------------------------------------------------------
#include "panel_hist_wycinek.inc"

#undef malloc
#undef free

// ---------------------------------------------------------------------------
// Narzedzia testu
// ---------------------------------------------------------------------------
static int g_sprawdzen = 0, g_bledow = 0;
static void sprawdz(bool ok, const char* opis) {
  g_sprawdzen++;
  if (!ok) g_bledow++;
  printf("    [%s] %s\n", ok ? " OK " : "BLAD", opis);
}
static void sprawdzCicho(bool ok, const std::string& opis) {   // do petli z tysiacami porownan
  g_sprawdzen++;
  if (!ok) {
    g_bledow++;
    printf("    [BLAD] %s\n", opis.c_str());
  }
}

static const uint32_t T0 = 1794571200U;   // 2026-11-13 12:00:00 UTC = 13:00 czasu polskiego (CET)

static HistoriaProbka probka(uint32_t czas, float wartosc) {
  HistoriaProbka p;
  memset(&p, 0, sizeof(p));
  snprintf(p.etykieta, sizeof(p.etykieta), "%s", czas ? "xx:xx" : "--:--");
  p.t_zewn = wartosc;
  p.t_bojler = wartosc + 1.0f;
  p.t_ogrz = wartosc + 2.0f;
  p.t_ogrz_sr = wartosc + 3.0f;
  p.t_powrot = wartosc + 4.0f;
  p.t_panel = wartosc + 5.0f;
  p.t_pokoj = wartosc + 6.0f;
  p.pompa = false;
  p.cisnienie = 1.5f;
  p.wilgotnosc = 45.0f;
  p.klapa_proc = 40;
  p.syberko_proc = 60;
  p.czas_unix = czas;
  return p;
}

static HistoriaProbkaDluga probkaDluga(uint32_t czas, float wartosc) {
  HistoriaProbkaDluga p;
  memset(&p, 0, sizeof(p));
  snprintf(p.etykieta, sizeof(p.etykieta), "xx.xx xx:xx");
  p.t_zewn_sr = p.t_zewn_min = p.t_zewn_max = wartosc;
  p.t_bojler_sr = p.t_bojler_min = p.t_bojler_max = wartosc + 1.0f;
  p.t_ogrz_sr = p.t_ogrz_min = p.t_ogrz_max = wartosc + 2.0f;
  p.t_ogrz_sr_sr = p.t_ogrz_sr_min = p.t_ogrz_sr_max = wartosc + 3.0f;
  p.t_powrot_sr = p.t_powrot_min = p.t_powrot_max = wartosc + 4.0f;
  p.t_panel_sr = p.t_panel_min = p.t_panel_max = wartosc + 5.0f;
  p.t_pokoj_sr = p.t_pokoj_min = p.t_pokoj_max = wartosc + 6.0f;
  p.cisnienie_sr = p.cisnienie_min = p.cisnienie_max = 1.5f;
  p.wilgotnosc_sr = p.wilgotnosc_min = p.wilgotnosc_max = 45.0f;
  p.klapa_proc_sr = p.klapa_proc_min = p.klapa_proc_max = 40;
  p.syberko_proc_sr = p.syberko_proc_min = p.syberko_proc_max = 60;
  p.czas_unix = czas;
  p.pompa = 0;
  return p;
}

// Wpisuje probki do bufora kolowego tak, jak zrobilaby to centrala: pelny
// bufor zaczyna sie od dowolnego miejsca (`przesuniecie`), niepelny od zera.
static void ustawKrotki(const std::vector<HistoriaProbka>& w, int przesuniecie = 0) {
  if (!historiaBufor) historiaBufor = (HistoriaProbka*)std::calloc(HISTORIA_MAX_PROBEK, sizeof(HistoriaProbka));
  const int n = (int)w.size();
  if (n > HISTORIA_MAX_PROBEK) { printf("test: za duzo probek\n"); exit(2); }
  const bool pelny = (n == HISTORIA_MAX_PROBEK);
  const int start = pelny ? przesuniecie % HISTORIA_MAX_PROBEK : 0;
  for (int i = 0; i < n; i++) historiaBufor[(start + i) % HISTORIA_MAX_PROBEK] = w[(size_t)i];
  historiaLiczbaProbek = n;
  historiaPelna = pelny;
  historiaGlowa = pelny ? start : n % HISTORIA_MAX_PROBEK;
}
static void ustawDlugi(const std::vector<HistoriaProbkaDluga>& w, int przesuniecie = 0) {
  if (!historiaDlugaBufor) historiaDlugaBufor = (HistoriaProbkaDluga*)std::calloc(HISTORIA_DLUGA_MAX_PROBEK, sizeof(HistoriaProbkaDluga));
  const int n = (int)w.size();
  if (n > HISTORIA_DLUGA_MAX_PROBEK) { printf("test: za duzo probek\n"); exit(2); }
  const bool pelny = (n == HISTORIA_DLUGA_MAX_PROBEK);
  const int start = pelny ? przesuniecie % HISTORIA_DLUGA_MAX_PROBEK : 0;
  for (int i = 0; i < n; i++) historiaDlugaBufor[(start + i) % HISTORIA_DLUGA_MAX_PROBEK] = w[(size_t)i];
  historiaDlugaLiczbaProbek = n;
  historiaDlugaPelna = pelny;
  historiaDlugaGlowa = pelny ? start : n % HISTORIA_DLUGA_MAX_PROBEK;
}

struct Odpowiedz {
  int kod = 0;
  std::string tekst;
  mutable std::map<std::string, std::string> naglowek;   // pola "klucz=wartosc" z pierwszej linii
  std::vector<std::string> wpisy;                // wiersze danych i "#przerwa", w kolejnosci
  int koniec = -1;                               // liczba z linii "#koniec"
  bool poprawnaBudowa = false;                   // naglowek pierwszy, "#koniec" ostatni, nic po nim
  size_t najdluzszaLinia = 0;
};

static std::vector<std::string> podziel(const std::string& s, char znak) {
  std::vector<std::string> w;
  size_t p = 0;
  while (true) {
    const size_t k = s.find(znak, p);
    if (k == std::string::npos) { w.push_back(s.substr(p)); break; }
    w.push_back(s.substr(p, k - p));
    p = k + 1;
  }
  return w;
}

static Odpowiedz wywolaj(const char* tryb, int pkt) {
  Odpowiedz o;
  String out;
  atrapa::licznikConcat = 0;
  o.kod = historiaPanelZbudujCsv(tryb, pkt, out);
  o.tekst = out.t;
  if (o.kod != 200) return o;
  std::vector<std::string> linie = podziel(o.tekst, '\n');
  bool budowa = !linie.empty() && linie.back().empty();   // tekst konczy sie znakiem nowej linii
  if (budowa) linie.pop_back();
  for (size_t i = 0; i < linie.size(); i++) {
    const std::string& l = linie[i];
    o.najdluzszaLinia = std::max(o.najdluzszaLinia, l.size());
    if (i == 0) {
      if (l.rfind("#panel-hist 1 ", 0) != 0) budowa = false;
      for (const std::string& pole : podziel(l, ' ')) {
        const size_t r = pole.find('=');
        if (r != std::string::npos) o.naglowek[pole.substr(0, r)] = pole.substr(r + 1);
      }
    } else if (l.rfind("#koniec ", 0) == 0) {
      o.koniec = atoi(l.c_str() + 8);
      if (i + 1 != linie.size()) budowa = false;
    } else if (l == "#przerwa" || (!l.empty() && l[0] != '#')) {
      o.wpisy.push_back(l);
    } else {
      budowa = false;
    }
  }
  o.poprawnaBudowa = budowa && o.koniec == (int)o.wpisy.size();
  return o;
}

static int ilePrzerw(const Odpowiedz& o) {
  return (int)std::count(o.wpisy.begin(), o.wpisy.end(), std::string("#przerwa"));
}
// Kolumna `k` wiersza danych (0 = etykieta, 1.. = liczby jak w /api/history/long).
static std::string kol(const std::string& wiersz, int k) {
  const std::vector<std::string> c = podziel(wiersz, ',');
  return (k >= 0 && k < (int)c.size()) ? c[(size_t)k] : "(brak)";
}
static int ileKolumn(const std::string& wiersz) { return (int)podziel(wiersz, ',').size(); }

// ---------------------------------------------------------------------------
// Niezalezne liczenie: sekunda po sekundzie
// ---------------------------------------------------------------------------
struct RefWiersz {
  uint32_t czas;
  long sr[11], mn[11], mx[11];
  bool jest[11];
  int pompa;
  bool dlugi;
};

static long refDziesiate(float v, bool& jest) {
  jest = !(std::isnan(v) || v > 9999.9f || v < -9999.9f);
  return jest ? std::lround((double)v * 10.0) : 0;
}

// Wiersze w kolejnosci chronologicznej wedlug zasad opisanych w komentarzu
// [v3.58.4 PANEL-WYKRESY]: w trybie tydz najpierw bufor dlugi (tylko czas
// sprzed bufora krotkiego), potem krotki; probki bez czasu dostaja czas z
// sasiadow.
static std::vector<RefWiersz> refZrodlo(bool zDlugiej, bool& jestCzas) {
  std::vector<HistoriaProbka> k;
  if (historiaBufor) {
    const int start = historiaPelna ? historiaGlowa : 0;
    for (int i = 0; i < historiaLiczbaProbek; i++) k.push_back(historiaBufor[(start + i) % HISTORIA_MAX_PROBEK]);
  }
  for (HistoriaProbka& p : k)
    if (p.czas_unix > 4102444800U) p.czas_unix = 0;
  int pierwszy = -1;
  for (size_t i = 0; i < k.size(); i++)
    if (k[i].czas_unix != 0) { pierwszy = (int)i; break; }
  jestCzas = (pierwszy >= 0);
  // czasy probek krotkich
  std::vector<uint32_t> czasK(k.size(), 0);
  for (size_t i = 0; i < k.size(); i++) {
    if (k[i].czas_unix != 0) czasK[i] = k[i].czas_unix;
    else if (i > 0 && (pierwszy < 0 || (int)i > pierwszy)) czasK[i] = czasK[i - 1] + 60;
    else if (pierwszy >= 0) {
      const uint32_t wstecz = (uint32_t)(pierwszy - (int)i) * 60U;
      czasK[i] = (k[(size_t)pierwszy].czas_unix > wstecz) ? k[(size_t)pierwszy].czas_unix - wstecz : 1U;
    } else czasK[i] = 1U;   // i == 0, brak zegara
  }
  uint32_t granica = 0;
  if (pierwszy >= 0) granica = czasK.empty() ? 0 : czasK[0];

  std::vector<RefWiersz> w;
  if (zDlugiej && historiaDlugaBufor) {
    const int start = historiaDlugaPelna ? historiaDlugaGlowa : 0;
    for (int i = 0; i < historiaDlugaLiczbaProbek; i++) {
      const HistoriaProbkaDluga& p = historiaDlugaBufor[(start + i) % HISTORIA_DLUGA_MAX_PROBEK];
      if (p.czas_unix == 0 || p.czas_unix > 4102444800U) continue;
      if (granica != 0 && p.czas_unix >= granica) continue;
      RefWiersz r;
      memset(&r, 0, sizeof(r));
      r.czas = p.czas_unix;
      r.dlugi = true;
      r.pompa = (p.pompa == 0 || p.pompa == 1) ? p.pompa : -1;
      const float sr[11] = {p.t_zewn_sr, p.t_bojler_sr, p.t_ogrz_sr, p.t_ogrz_sr_sr, p.t_powrot_sr, p.t_panel_sr, p.t_pokoj_sr,
                            p.cisnienie_sr, p.wilgotnosc_sr, (float)p.klapa_proc_sr, (float)p.syberko_proc_sr};
      const float mn[11] = {p.t_zewn_min, p.t_bojler_min, p.t_ogrz_min, p.t_ogrz_sr_min, p.t_powrot_min, p.t_panel_min, p.t_pokoj_min,
                            p.cisnienie_min, p.wilgotnosc_min, (float)p.klapa_proc_min, (float)p.syberko_proc_min};
      const float mx[11] = {p.t_zewn_max, p.t_bojler_max, p.t_ogrz_max, p.t_ogrz_sr_max, p.t_powrot_max, p.t_panel_max, p.t_pokoj_max,
                            p.cisnienie_max, p.wilgotnosc_max, (float)p.klapa_proc_max, (float)p.syberko_proc_max};
      for (int f = 0; f < 11; f++) {
        bool jMn, jMx;
        r.sr[f] = refDziesiate(sr[f], r.jest[f]);
        r.mn[f] = refDziesiate(mn[f], jMn);
        r.mx[f] = refDziesiate(mx[f], jMx);
        if (!jMn || r.mn[f] > r.sr[f]) r.mn[f] = r.sr[f];
        if (!jMx || r.mx[f] < r.sr[f]) r.mx[f] = r.sr[f];
      }
      w.push_back(r);
      jestCzas = true;
    }
  }
  for (size_t i = 0; i < k.size(); i++) {
    const HistoriaProbka& p = k[i];
    RefWiersz r;
    memset(&r, 0, sizeof(r));
    r.czas = czasK[i];
    r.dlugi = false;
    r.pompa = p.pompa ? 1 : 0;
    const float v[11] = {p.t_zewn, p.t_bojler, p.t_ogrz, p.t_ogrz_sr, p.t_powrot, p.t_panel, p.t_pokoj,
                         p.cisnienie, p.wilgotnosc, (float)p.klapa_proc, (float)p.syberko_proc};
    for (int f = 0; f < 11; f++) {
      r.sr[f] = refDziesiate(v[f], r.jest[f]);
      r.mn[f] = r.mx[f] = r.sr[f];
    }
    w.push_back(r);
  }
  return w;
}

static void refLiczba(std::string& s, bool jest, long dziesiate) {
  char b[32];
  if (!jest) snprintf(b, sizeof(b), "nan,");
  else snprintf(b, sizeof(b), "%s%ld.%ld,", dziesiate < 0 ? "-" : "", std::labs(dziesiate) / 10, std::labs(dziesiate) % 10);
  s += b;
}

static std::string refCsv(const char* tryb, int pkt) {
  const std::string t = tryb;
  const uint32_t okno = (t == "godz") ? 3600U : (t == "dzien") ? 86400U : 604800U;
  const bool zDlugiej = (t == "tydz");
  if (pkt <= 0) pkt = 96;
  pkt = std::max(8, std::min(240, pkt));
  bool jestCzas = false;
  const std::vector<RefWiersz> w = refZrodlo(zDlugiej, jestCzas);
  char b[256];
  if (w.empty()) {
    snprintf(b, sizeof(b), "#panel-hist 1 tryb=%s pkt=%d wierszy=0 od=0 do=0 krok_s=0 zrodlo=lokalne fs=%d od_etyk=--:-- do_etyk=--:--\n#koniec 0\n", tryb, pkt, g_fsWlaczony ? 1 : 0);
    return b;
  }
  uint32_t tMin = w[0].czas, tMax = w[0].czas;
  for (const RefWiersz& r : w) { tMin = std::min(tMin, r.czas); tMax = std::max(tMax, r.czas); }
  const uint64_t koniec = (uint64_t)tMax + 1;
  uint64_t poczatek = (koniec > okno) ? koniec - okno : 0;
  if (poczatek < tMin) poczatek = tMin;
  const uint64_t rozp = koniec - poczatek;
  const int n = (rozp < (uint64_t)pkt) ? (int)rozp : pkt;
  std::vector<uint64_t> g((size_t)n + 1);
  for (int k = 0; k <= n; k++) g[(size_t)k] = poczatek + ((uint64_t)k * rozp) / (uint64_t)n;
  g[(size_t)n] = koniec;

  std::vector<long long> suma((size_t)n * 11, 0);
  std::vector<long long> waga((size_t)n * 11, 0), wagaRazem((size_t)n, 0);
  std::vector<long> mn((size_t)n * 11, 0), mx((size_t)n * 11, 0);
  std::vector<int> pompa((size_t)n, -1);
  auto sekunda = [&](uint64_t s, const RefWiersz& r) {
    if (s < poczatek || s >= koniec) return;
    const size_t k = (size_t)(std::upper_bound(g.begin(), g.end(), s) - g.begin()) - 1;   // g[k] <= s < g[k+1]
    for (int f = 0; f < 11; f++) {
      if (!r.jest[f]) continue;
      const size_t i = k * 11 + (size_t)f;
      suma[i] += r.sr[f];
      if (waga[i] == 0 || r.mn[f] < mn[i]) mn[i] = r.mn[f];
      if (waga[i] == 0 || r.mx[f] > mx[i]) mx[i] = r.mx[f];
      waga[i]++;
    }
    wagaRazem[k]++;
    pompa[k] = r.pompa;
  };
  for (size_t i = 0; i < w.size(); i++) {
    uint64_t doKiedy = (uint64_t)w[i].czas + 1;
    if (i + 1 < w.size() && w[i + 1].czas > w[i].czas) {
      const uint32_t prog = (w[i].dlugi || w[i + 1].dlugi) ? 5400U : 360U;
      if (w[i + 1].czas - w[i].czas <= prog) doKiedy = w[i + 1].czas;
    }
    for (uint64_t s = w[i].czas; s < doKiedy; s++) sekunda(s, w[i]);
  }

  std::string out;
  // Poczatek okna i czas najnowszej probki jako tekst (czas lokalny).
  auto refEtyk = [&](uint64_t czasUnix) -> std::string {
    if (!jestCzas) return "--:--";
    const time_t c = (time_t)czasUnix;
    struct tm lok;
    localtime_r(&c, &lok);
    char e[32];
    if (t != "godz") snprintf(e, sizeof(e), "%02d.%02d_%02d:%02d", lok.tm_mday, lok.tm_mon + 1, lok.tm_hour, lok.tm_min);
    else snprintf(e, sizeof(e), "%02d:%02d", lok.tm_hour, lok.tm_min);
    return e;
  };
  snprintf(b, sizeof(b), "#panel-hist 1 tryb=%s pkt=%d wierszy=%zu od=%llu do=%lu krok_s=%llu zrodlo=lokalne fs=%d od_etyk=%s do_etyk=%s\n", tryb, pkt, w.size(),
           jestCzas ? (unsigned long long)poczatek : 0ULL, jestCzas ? (unsigned long)tMax : 0UL, (unsigned long long)(rozp / (uint64_t)n), g_fsWlaczony ? 1 : 0,
           refEtyk(poczatek).c_str(), refEtyk(tMax).c_str());
  out += b;
  for (int k = 0; k < n; k++) {
    if (wagaRazem[(size_t)k] == 0) { out += "#przerwa\n"; continue; }
    const time_t czas = (time_t)(g[(size_t)k + 1] - 1);
    if (!jestCzas) out += "--:--,";
    else {
      struct tm lok;
      localtime_r(&czas, &lok);
      if (zDlugiej) snprintf(b, sizeof(b), "%02d.%02d %02d:%02d,", lok.tm_mday, lok.tm_mon + 1, lok.tm_hour, lok.tm_min);
      else snprintf(b, sizeof(b), "%02d:%02d,", lok.tm_hour, lok.tm_min);
      out += b;
    }
    for (int f = 0; f < 11; f++) {
      const size_t i = (size_t)k * 11 + (size_t)f;
      if (waga[i] == 0) { out += "nan,nan,nan,"; continue; }
      const long long s = suma[i], wg = waga[i];
      const long long sr = (s >= 0) ? (s + wg / 2) / wg : -((-s + wg / 2) / wg);
      refLiczba(out, true, (long)sr);
      refLiczba(out, true, mn[i]);
      refLiczba(out, true, mx[i]);
    }
    snprintf(b, sizeof(b), "%lu,%d\n", jestCzas ? (unsigned long)czas : 0UL, pompa[(size_t)k]);
    out += b;
  }
  snprintf(b, sizeof(b), "#koniec %d\n", n);
  out += b;
  return out;
}

static bool zgodnyZRef(const char* tryb, int pkt, const char* opis) {
  const Odpowiedz o = wywolaj(tryb, pkt);
  const std::string ref = refCsv(tryb, pkt);
  const bool ok = (o.kod == 200 && o.tekst == ref);
  if (!ok) {
    const std::vector<std::string> a = podziel(o.tekst, '\n'), b = podziel(ref, '\n');
    for (size_t i = 0; i < std::max(a.size(), b.size()); i++) {
      const std::string la = i < a.size() ? a[i] : "(brak linii)", lb = i < b.size() ? b[i] : "(brak linii)";
      if (la != lb) {
        printf("      %s: pierwsza roznica w linii %zu\n        centrala: %s\n        liczone po sekundzie: %s\n", opis, i + 1, la.c_str(), lb.c_str());
        break;
      }
    }
  }
  return ok;
}

// Prosty generator liczb pseudolosowych - ten sam wynik przy kazdym uruchomieniu.
static uint32_t g_los = 12345;
static uint32_t los() {
  g_los = g_los * 1664525U + 1013904223U;
  return g_los >> 8;
}
static float losWartosc() { return (float)((int)(los() % 1200) - 300) / 10.0f; }   // -30,0 .. 89,9 co 0,1

static void wyczyscBufory() {
  ustawKrotki({});
  ustawDlugi({});
}

// ---------------------------------------------------------------------------
// Scenariusze
// ---------------------------------------------------------------------------
static void testGodzinaStalaWartosc() {
  printf("  1. godzina pracy, probka co 60 s, stale 21,5 C:\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i <= 60; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 21.5f));
  ustawKrotki(w);
  const Odpowiedz o = wywolaj("godz", 96);
  sprawdz(o.kod == 200 && o.poprawnaBudowa, "kod 200, naglowek na poczatku, \"#koniec\" na koncu z wlasciwa liczba");
  sprawdz(o.naglowek["tryb"] == "godz" && o.naglowek["pkt"] == "96" && o.naglowek["wierszy"] == "61" && o.naglowek["zrodlo"] == "lokalne" &&
              o.naglowek["fs"] == "1",
          "naglowek: tryb=godz pkt=96 wierszy=61 zrodlo=lokalne fs=1");
  sprawdz(o.naglowek["od"] == std::to_string(T0 + 1) && o.naglowek["do"] == std::to_string(T0 + 3600) && o.naglowek["krok_s"] == "37",
          "naglowek: okno = 3600 s do najnowszej probki wlacznie, krok 37 s");
  sprawdz(o.naglowek["od_etyk"] == "13:00" && o.naglowek["do_etyk"] == "14:00",
          "naglowek: od_etyk=13:00 do_etyk=14:00 (czas polski) - podpis osi czasu dla panelu");
  sprawdz(o.wpisy.size() == 96 && ilePrzerw(o) == 0, "96 punktow, zadnej przerwy (wykres na cala szerokosc)");
  bool wszystkie = true, kolumny = true;
  for (const std::string& l : o.wpisy) {
    if (kol(l, 1) != "21.5" || kol(l, 2) != "21.5" || kol(l, 3) != "21.5" || kol(l, 4) != "22.5" || kol(l, 19) != "27.5") wszystkie = false;
    if (ileKolumn(l) != 36) kolumny = false;
  }
  sprawdz(wszystkie, "kazdy punkt: zewn 21.5 (sr, min, max), bojler 22.5, pokoj 27.5");
  sprawdz(kolumny, "kazdy wiersz ma 36 kolumn, jak /api/history/long");
  sprawdz(kol(o.wpisy.back(), 0) == "14:00" && kol(o.wpisy.back(), 34) == std::to_string(T0 + 3600),
          "ostatni punkt: etykieta 14:00 (czas polski), czas = czas najnowszej probki");
  sprawdz(kol(o.wpisy.front(), 0) == "13:00", "pierwszy punkt: etykieta 13:00");
  sprawdz(kol(o.wpisy[0], 22) == "1.5" && kol(o.wpisy[0], 25) == "45.0" && kol(o.wpisy[0], 28) == "40.0" && kol(o.wpisy[0], 31) == "60.0" &&
              kol(o.wpisy[0], 35) == "0",
          "cisnienie 1.5, wilgotnosc 45.0, klapa 40.0, syberek 60.0, pompa 0");
  sprawdz(atrapa::ostatniLog.find("PANEL_WYKRES tryb=godz pkt=96 wierszy=61 punktow=96 przerw=0") == 0, "linia w logu centrali");
  sprawdz(atrapa::zywychBlokow == 0 && atrapa::przydzialWSekcji == 0 && atrapa::sekcjaKrytyczna == 0,
          "pamiec zwolniona, spinlock oddany, zadnego przydzialu pod spinlockiem");
}

static void testSkokWartosci() {
  printf("  2. skok temperatury w polowie godziny (20 C -> 30 C):\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i < 60; i++) w.push_back(probka(T0 + (uint32_t)i * 60, i < 30 ? 20.0f : 30.0f));
  ustawKrotki(w);   // 60 probek: T0 .. T0+3540, okno = 3541 s
  const Odpowiedz o = wywolaj("godz", 60);
  sprawdz(o.poprawnaBudowa && o.wpisy.size() == 60, "60 punktow");
  int n20 = 0, n30 = 0, inne = 0;
  for (const std::string& l : o.wpisy) {
    if (kol(l, 1) == "20.0") n20++;
    else if (kol(l, 1) == "30.0") n30++;
    else inne++;
  }
  sprawdz(n20 >= 29 && n30 >= 29 && inne <= 1, "polowa punktow ma 20.0, polowa 30.0, najwyzej jeden punkt przejsciowy");
  sprawdz(kol(o.wpisy.front(), 1) == "20.0" && kol(o.wpisy.back(), 1) == "30.0", "pierwszy punkt 20.0, ostatni 30.0");
  sprawdz(zgodnyZRef("godz", 60, "skok"), "wynik zgodny z liczeniem sekunda po sekundzie");
}

static void testPrzerwa() {
  printf("  3. centrala wylaczona na 30 minut w srodku godziny:\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i <= 15; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));          // 13:00 - 13:15
  for (int i = 45; i <= 60; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 25.0f));         // 13:45 - 14:00
  ustawKrotki(w);
  const Odpowiedz o = wywolaj("godz", 96);
  const int przerw = ilePrzerw(o);
  sprawdz(o.poprawnaBudowa && o.wpisy.size() == 96, "96 pozycji razem z przerwami");
  sprawdz(przerw >= 46 && przerw <= 49, "ok. polowa pozycji (30 z 60 minut) to \"#przerwa\"");
  bool ciagla = true;
  int pierwsza = -1, ostatnia = -1;
  for (int i = 0; i < (int)o.wpisy.size(); i++)
    if (o.wpisy[(size_t)i] == "#przerwa") { if (pierwsza < 0) pierwsza = i; ostatnia = i; }
  for (int i = pierwsza; i <= ostatnia; i++)
    if (o.wpisy[(size_t)i] != "#przerwa") ciagla = false;
  sprawdz(ciagla && pierwsza > 20 && ostatnia < 75, "przerwa jest jednym ciaglym odcinkiem w srodku wykresu");
  sprawdz(kol(o.wpisy.front(), 1) == "20.0" && kol(o.wpisy.back(), 1) == "25.0", "przed przerwa 20.0, po przerwie 25.0");
  sprawdz(atrapa::ostatniLog.find("przerw=" + std::to_string(przerw)) != std::string::npos, "liczba przerw w logu centrali");
  sprawdz(zgodnyZRef("godz", 96, "przerwa"), "wynik zgodny z liczeniem sekunda po sekundzie");
  printf("  3b. odstep 5 minut (jedna zgubiona probka to nie przerwa - prog 6 minut):\n");
  w.clear();
  for (int i = 0; i <= 10; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  for (int i = 15; i <= 30; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  ustawKrotki(w);
  const Odpowiedz o2 = wywolaj("godz", 96);
  sprawdz(o2.poprawnaBudowa && ilePrzerw(o2) == 0, "zadnej przerwy");
  printf("  3c. odstep 7 minut to juz przerwa:\n");
  w.clear();
  for (int i = 0; i <= 10; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  for (int i = 17; i <= 30; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  ustawKrotki(w);
  const Odpowiedz o3 = wywolaj("godz", 96);
  sprawdz(o3.poprawnaBudowa && ilePrzerw(o3) > 15, "jest przerwa");
}

static void testDzienPelnyBufor() {
  printf("  4. pelny bufor krotki (1440 probek, zaczyna sie w srodku tablicy):\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i < 1440; i++) w.push_back(probka(T0 + (uint32_t)i * 60, (float)(i / 60)));   // co godzine o 1 C wiecej
  ustawKrotki(w, 777);
  const Odpowiedz o = wywolaj("dzien", 96);
  sprawdz(o.poprawnaBudowa && o.wpisy.size() == 96 && ilePrzerw(o) == 0, "96 punktow bez przerw");
  sprawdz(o.naglowek["wierszy"] == "1440" && o.naglowek["krok_s"] == "899", "naglowek: wierszy=1440, krok ok. 15 minut");
  sprawdz(o.naglowek["od_etyk"] == "13.11_13:00" && o.naglowek["do_etyk"] == "14.11_12:59",
          "naglowek: od_etyk=13.11_13:00 do_etyk=14.11_12:59 (doba siega poprzedniego dnia - z data)");
  sprawdz(kol(o.wpisy.front(), 1) == "0.0" && kol(o.wpisy.back(), 1) == "23.0", "pierwszy punkt 0.0, ostatni 23.0");
  // Piaty punkt to sekundy 3597..4495 od poczatku: 3 s jeszcze z wartoscia 0, potem 896 s z wartoscia 1.
  sprawdz(kol(o.wpisy[4], 1) == "1.0" && kol(o.wpisy[4], 2) == "0.0" && kol(o.wpisy[4], 3) == "1.0", "piaty punkt (druga godzina): srednia 1.0, min 0.0, max 1.0");
  sprawdz(zgodnyZRef("dzien", 96, "pelny bufor"), "wynik zgodny z liczeniem sekunda po sekundzie");
  const Odpowiedz g = wywolaj("godz", 96);
  sprawdz(g.poprawnaBudowa && g.naglowek["od"] == std::to_string(T0 + 1439 * 60 + 1 - 3600) && kol(g.wpisy.front(), 1) == "22.0" &&
              kol(g.wpisy.back(), 1) == "23.0",
          "tryb godz z tego samego bufora: tylko ostatnia godzina (22.0 -> 23.0)");
}

static void testTydzien() {
  printf("  5. tydzien: bufor dlugi (zdarzenia) + ostatnia doba z bufora krotkiego:\n");
  wyczyscBufory();
  const uint32_t poczatek = T0 - 9 * 86400;   // 9 dni historii w buforze dlugim
  std::vector<HistoriaProbkaDluga> d;
  for (uint32_t t = poczatek; t < T0; t += 3600) d.push_back(probkaDluga(t, 10.0f));   // co godzine 10 C
  std::vector<HistoriaProbka> k;
  for (int i = 0; i < 1440; i++) k.push_back(probka(T0 - 86400 + (uint32_t)i * 60, 50.0f));   // ostatnia doba: 50 C
  ustawDlugi(d);
  ustawKrotki(k);
  const Odpowiedz o = wywolaj("tydz", 96);
  sprawdz(o.poprawnaBudowa && o.wpisy.size() == 96 && ilePrzerw(o) == 0, "96 punktow bez przerw");
  const uint32_t koniec = T0 - 86400 + 1439 * 60;
  sprawdz(o.naglowek["do"] == std::to_string(koniec) && o.naglowek["od"] == std::to_string(koniec + 1 - 604800) && o.naglowek["krok_s"] == "6300",
          "okno: dokladnie 7 dni do najnowszej probki, krok 105 minut");
  sprawdz(o.naglowek["od_etyk"] == "06.11_12:59" && o.naglowek["do_etyk"] == "13.11_12:59",
          "naglowek: od_etyk=06.11_12:59 do_etyk=13.11_12:59 (z data, bez spacji w srodku)");
  int n10 = 0, n50 = 0, inne = 0;
  for (const std::string& l : o.wpisy) {
    if (kol(l, 1) == "10.0") n10++;
    else if (kol(l, 1) == "50.0") n50++;
    else inne++;
  }
  sprawdz(n10 >= 81 && n10 <= 83 && n50 >= 13 && n50 <= 14 && inne <= 1, "6 dni po 10.0 (dlugi), ostatnia doba 50.0 (krotki), najwyzej 1 punkt przejsciowy");
  sprawdz(kol(o.wpisy.back(), 0).size() == 11 && kol(o.wpisy.back(), 0)[2] == '.' && kol(o.wpisy.back(), 0)[5] == ' ',
          "etykiety z data: \"DD.MM HH:MM\"");
  sprawdz(kol(o.wpisy.back(), 0) == "13.11 12:59", "ostatnia etykieta 13.11 12:59");
  sprawdz(zgodnyZRef("tydz", 96, "tydzien"), "wynik zgodny z liczeniem sekunda po sekundzie");
  printf("  5b. bufor dlugi zdarzeniowy: wartosc obowiazuje do nastepnego wpisu (srednia wazona czasem):\n");
  wyczyscBufory();
  d.clear();
  // 12 godzin: 10 C przez 5 h 50 min, potem wpisy co 30 s z 90 C przez 10 minut, potem znowu 10 C.
  d.push_back(probkaDluga(T0, 10.0f));
  for (uint32_t t = T0 + 3600; t <= T0 + 5 * 3600; t += 3600) d.push_back(probkaDluga(t, 10.0f));
  for (uint32_t t = T0 + 21000; t < T0 + 21600; t += 30) d.push_back(probkaDluga(t, 90.0f));
  for (uint32_t t = T0 + 21600; t <= T0 + 43200; t += 3600) d.push_back(probkaDluga(t, 10.0f));
  ustawDlugi(d);
  const Odpowiedz z = wywolaj("tydz", 8);   // 8 punktow po 90 minut
  sprawdz(z.poprawnaBudowa && z.wpisy.size() == 8 && ilePrzerw(z) == 0, "8 punktow bez przerw");
  // 4. punkt obejmuje 4:30-6:00: 80 minut po 10 C i 10 minut po 90 C -> (80*10 + 10*90)/90 = 18,9
  sprawdz(kol(z.wpisy[3], 1) == "18.9" && kol(z.wpisy[3], 2) == "10.0" && kol(z.wpisy[3], 3) == "90.0",
          "punkt z 10-minutowym szczytem: srednia 18.9 (a nie ok. 80 ze zwyklej sredniej wpisow), min 10.0, max 90.0");
  sprawdz(kol(z.wpisy[2], 1) == "10.0" && kol(z.wpisy[4], 1) == "10.0", "sasiednie punkty: 10.0");
  sprawdz(zgodnyZRef("tydz", 8, "zdarzenia"), "wynik zgodny z liczeniem sekunda po sekundzie");
  printf("  5c. przerwa w buforze dlugim (ponad 90 minut bez wpisu):\n");
  d.clear();
  for (uint32_t t = T0; t <= T0 + 10 * 3600; t += 3600) d.push_back(probkaDluga(t, 10.0f));
  for (uint32_t t = T0 + 20 * 3600; t <= T0 + 30 * 3600; t += 3600) d.push_back(probkaDluga(t, 12.0f));
  ustawDlugi(d);
  const Odpowiedz p = wywolaj("tydz", 96);
  sprawdz(p.poprawnaBudowa && ilePrzerw(p) >= 30 && ilePrzerw(p) <= 33, "ok. 1/3 pozycji to przerwa (10 z 30 godzin)");
  sprawdz(zgodnyZRef("tydz", 96, "przerwa w dlugim"), "wynik zgodny z liczeniem sekunda po sekundzie");
}

static void testBrakDanych() {
  printf("  6. brak danych i dane szczatkowe:\n");
  wyczyscBufory();
  for (const char* tryb : {"godz", "dzien", "tydz"}) {
    const Odpowiedz o = wywolaj(tryb, 96);
    sprawdz(o.kod == 200 && o.poprawnaBudowa && o.wpisy.empty() && o.naglowek["wierszy"] == "0" && o.koniec == 0,
            (std::string("pusta historia, tryb ") + tryb + ": kod 200, wierszy=0, \"#koniec 0\"").c_str());
  }
  HistoriaProbka* schowanyK = historiaBufor;
  HistoriaProbkaDluga* schowanyD = historiaDlugaBufor;
  historiaBufor = nullptr;
  historiaDlugaBufor = nullptr;
  const Odpowiedz bez = wywolaj("tydz", 96);
  sprawdz(bez.kod == 200 && bez.naglowek["wierszy"] == "0", "bufory nieprzydzielone (centrala bez PSRAM): wierszy=0, bez wywrotki");
  historiaBufor = schowanyK;
  historiaDlugaBufor = schowanyD;

  ustawKrotki({probka(T0, 33.3f)});
  const Odpowiedz jedna = wywolaj("dzien", 96);
  sprawdz(jedna.poprawnaBudowa && jedna.wpisy.size() == 1 && kol(jedna.wpisy[0], 1) == "33.3" && kol(jedna.wpisy[0], 34) == std::to_string(T0),
          "jedna probka: jeden punkt z jej wartoscia i czasem");
  ustawKrotki({probka(T0, 10.0f), probka(T0 + 60, 20.0f)});
  const Odpowiedz dwie = wywolaj("dzien", 96);
  sprawdz(dwie.poprawnaBudowa && dwie.wpisy.size() == 61 && kol(dwie.wpisy.front(), 1) == "10.0" && kol(dwie.wpisy.back(), 1) == "20.0",
          "dwie probki co 60 s: 61 punktow (po sekundzie), od 10.0 do 20.0");
  sprawdz(zgodnyZRef("dzien", 96, "dwie probki"), "wynik zgodny z liczeniem sekunda po sekundzie");
}

static void testBezZegara() {
  printf("  7. centrala bez zegara (probki z czasem 0):\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i < 30; i++) w.push_back(probka(0, (float)i));
  ustawKrotki(w);
  const Odpowiedz o = wywolaj("dzien", 96);
  sprawdz(o.poprawnaBudowa && o.wpisy.size() == 96 && ilePrzerw(o) == 0, "96 punktow bez przerw (probki liczone co 60 s)");
  sprawdz(o.naglowek["od"] == "0" && o.naglowek["do"] == "0", "naglowek: od=0 do=0 (czas nieznany)");
  sprawdz(o.naglowek["od_etyk"] == "--:--" && o.naglowek["do_etyk"] == "--:--", "naglowek: od_etyk i do_etyk to \"--:--\"");
  bool etykiety = true;
  for (const std::string& l : o.wpisy)
    if (kol(l, 0) != "--:--" || kol(l, 34) != "0") etykiety = false;
  sprawdz(etykiety, "kazda etykieta \"--:--\", kolumna czasu 0");
  bool rosnie = true;
  for (size_t i = 1; i < o.wpisy.size(); i++)
    if (atof(kol(o.wpisy[i], 1).c_str()) < atof(kol(o.wpisy[i - 1], 1).c_str())) rosnie = false;
  // Najnowsza probka (29) wchodzi do ostatniego punktu jedna sekunda - srednia tego punktu to glownie
  // poprzednia probka (28), a 29 widac w jego maksimum.
  sprawdz(rosnie && kol(o.wpisy.front(), 1) == "0.0" && kol(o.wpisy.back(), 1) == "28.1" && kol(o.wpisy.back(), 3) == "29.0",
          "wartosci rosna od 0.0; ostatni punkt: srednia 28.1, maksimum 29.0 (najnowsza probka)");
  sprawdz(zgodnyZRef("dzien", 96, "bez zegara"), "wynik zgodny z liczeniem sekunda po sekundzie");
  const Odpowiedz t = wywolaj("tydz", 96);
  sprawdz(t.poprawnaBudowa && t.wpisy.size() == 96 && kol(t.wpisy.back(), 0) == "--:--", "tryb tydz bez zegara: te same probki, etykiety \"--:--\"");

  printf("  7b. zegar zlapany po 10 minutach (pierwsze probki bez czasu):\n");
  w.clear();
  for (int i = 0; i < 10; i++) w.push_back(probka(0, 5.0f));
  for (int i = 10; i < 40; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 7.0f));
  ustawKrotki(w);
  const Odpowiedz m = wywolaj("godz", 96);
  sprawdz(m.poprawnaBudowa && ilePrzerw(m) == 0 && m.naglowek["od"] == std::to_string(T0) && kol(m.wpisy.front(), 1) == "5.0" &&
              kol(m.wpisy.back(), 1) == "7.0",
          "probki bez czasu dostaja czas z sasiadow: okno od T0, 5.0 na poczatku, 7.0 na koncu, bez przerw");
  sprawdz(zgodnyZRef("godz", 96, "zegar po 10 min"), "wynik zgodny z liczeniem sekunda po sekundzie");
  printf("  7c. zegar zgubiony w trakcie (ostatnie probki bez czasu):\n");
  w.clear();
  for (int i = 0; i < 20; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 5.0f));
  for (int i = 0; i < 10; i++) w.push_back(probka(0, 9.0f));
  ustawKrotki(w);
  const Odpowiedz u = wywolaj("godz", 96);
  sprawdz(u.poprawnaBudowa && ilePrzerw(u) == 0 && u.naglowek["do"] == std::to_string(T0 + 29 * 60) && kol(u.wpisy.back(), 1) == "9.0",
          "probki bez czasu sa dopisane co 60 s po ostatniej z czasem");
  sprawdz(zgodnyZRef("godz", 96, "zegar zgubiony"), "wynik zgodny z liczeniem sekunda po sekundzie");
}

static void testWartosciNietypowe() {
  printf("  8. wartosci nietypowe:\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i <= 60; i++) {
    HistoriaProbka p = probka(T0 + (uint32_t)i * 60, -12.3f);
    p.t_bojler = NAN;                              // czujnik bez odczytu
    p.t_ogrz = (i < 30) ? NAN : 55.5f;             // czujnik wraca po pol godziny
    p.t_panel = INFINITY;
    p.cisnienie = -0.04f;                          // zaokragla sie do -0.0 -> "0.0"
    p.wilgotnosc = 12345.0f;                       // bez sensu -> nan
    p.pompa = true;
    w.push_back(p);
  }
  ustawKrotki(w);
  const Odpowiedz o = wywolaj("godz", 96);
  sprawdz(o.poprawnaBudowa && o.wpisy.size() == 96, "96 punktow");
  sprawdz(kol(o.wpisy[0], 1) == "-12.3" && kol(o.wpisy[0], 2) == "-12.3" && kol(o.wpisy[0], 3) == "-12.3", "wartosc ujemna: -12.3");
  sprawdz(kol(o.wpisy[50], 4) == "nan" && kol(o.wpisy[50], 5) == "nan" && kol(o.wpisy[50], 6) == "nan", "NaN w czujniku -> \"nan,nan,nan\" tylko w jego kolumnach");
  sprawdz(kol(o.wpisy[0], 7) == "nan" && kol(o.wpisy.back(), 7) == "55.5", "czujnik, ktory wrocil: najpierw nan, potem 55.5");
  sprawdz(kol(o.wpisy[0], 16) == "nan" && kol(o.wpisy[0], 25) == "nan", "nieskonczonosc i wartosc bez sensu -> nan");
  sprawdz(kol(o.wpisy[0], 22) == "0.0", "-0,04 zaokragla sie do \"0.0\" (bez \"-0.0\")");
  sprawdz(kol(o.wpisy[0], 35) == "1", "pompa wlaczona: 1");
  sprawdz(zgodnyZRef("godz", 96, "nietypowe"), "wynik zgodny z liczeniem sekunda po sekundzie");

  // Najdluzsza mozliwa linia: wszystkie pola na granicy zakresu, etykieta z data.
  wyczyscBufory();
  std::vector<HistoriaProbkaDluga> d;
  for (int i = 0; i < 20; i++) {
    HistoriaProbkaDluga p = probkaDluga(T0 + (uint32_t)i * 600, -9999.9f);
    p.t_bojler_sr = p.t_bojler_min = p.t_bojler_max = -9999.9f;
    p.t_ogrz_sr = p.t_ogrz_min = p.t_ogrz_max = -9999.9f;
    p.t_ogrz_sr_sr = p.t_ogrz_sr_min = p.t_ogrz_sr_max = -9999.9f;
    p.t_powrot_sr = p.t_powrot_min = p.t_powrot_max = -9999.9f;
    p.t_panel_sr = p.t_panel_min = p.t_panel_max = -9999.9f;
    p.t_pokoj_sr = p.t_pokoj_min = p.t_pokoj_max = -9999.9f;
    p.cisnienie_sr = p.cisnienie_min = p.cisnienie_max = -9999.9f;
    p.wilgotnosc_sr = p.wilgotnosc_min = p.wilgotnosc_max = -9999.9f;
    p.klapa_proc_sr = p.klapa_proc_min = p.klapa_proc_max = -9999;
    p.syberko_proc_sr = p.syberko_proc_min = p.syberko_proc_max = -9999;
    p.pompa = -1;
    d.push_back(p);
  }
  ustawDlugi(d);
  const Odpowiedz dl = wywolaj("tydz", 96);
  sprawdz(dl.poprawnaBudowa && dl.wpisy.size() == 96 && kol(dl.wpisy[0], 1) == "-9999.9" && kol(dl.wpisy[0], 35) == "-1", "wartosci skrajne przechodza, pompa -1");
  printf("    - najdluzsza linia odpowiedzi: %zu znakow\n", dl.najdluzszaLinia);
  sprawdz(dl.najdluzszaLinia < 300, "najdluzsza mozliwa linia ma mniej niz 300 znakow (bufor linii panelu LCD)");

  // Wiersz dlugi z min > sr albo max < sr (uszkodzony plik) - min/max nie moga byc "odwrocone".
  d.clear();
  for (int i = 0; i < 10; i++) {
    HistoriaProbkaDluga p = probkaDluga(T0 + (uint32_t)i * 600, 20.0f);
    p.t_zewn_min = 25.0f;
    p.t_zewn_max = NAN;
    d.push_back(p);
  }
  ustawDlugi(d);
  const Odpowiedz od = wywolaj("tydz", 8);
  sprawdz(od.poprawnaBudowa && kol(od.wpisy[0], 1) == "20.0" && kol(od.wpisy[0], 2) == "20.0" && kol(od.wpisy[0], 3) == "20.0",
          "wiersz z min > sr i max = NaN: min i max rowne sredniej");
}

static void testParametry() {
  printf("  9. parametry zapytania:\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i < 600; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  ustawKrotki(w);
  sprawdz(wywolaj("godz", 0).wpisy.size() == 96 && wywolaj("godz", -5).wpisy.size() == 96, "pkt=0 albo ujemne: domyslne 96");
  sprawdz(wywolaj("godz", 1).wpisy.size() == 8, "pkt=1: najmniej 8");
  sprawdz(wywolaj("godz", 100000).wpisy.size() == 240, "pkt=100000: najwyzej 240");
  sprawdz(wywolaj("dzien", 37).wpisy.size() == 37 && wywolaj("dzien", 240).poprawnaBudowa, "pkt=37: 37 punktow; pkt=240: poprawna odpowiedz");
  for (const char* zly : {"", "tydzien", "GODZ", "godz ", "x"}) {
    const Odpowiedz o = wywolaj(zly, 96);
    sprawdz(o.kod == 400 && o.tekst == "tryb=godz|dzien|tydz\n", (std::string("tryb \"") + zly + "\": kod 400 z podpowiedzia").c_str());
  }
  String out;
  sprawdz(historiaPanelZbudujCsv(nullptr, 96, out) == 400, "tryb = nullptr: kod 400, bez wywrotki");
  g_fsWlaczony = false;
  sprawdz(wywolaj("godz", 96).naglowek["fs"] == "0", "zapis historii na flash wylaczony: fs=0 w naglowku");
  g_fsWlaczony = true;
}

static void testBrakPamieci() {
  printf("  10. brak pamieci w centrali:\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i < 600; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  ustawKrotki(w);
  atrapa::psramJest = false;
  const Odpowiedz a = wywolaj("dzien", 96);
  sprawdz(a.kod == 200 && a.poprawnaBudowa && a.wpisy.size() == 96 && atrapa::zywychBlokow == 0, "bez PSRAM: kubelki ze zwyklego malloc(), wynik ten sam");
  atrapa::mallocOdmawia = true;
  const Odpowiedz b = wywolaj("dzien", 96);
  sprawdz(b.kod == 503 && b.tekst == "brak pamieci\n" && atrapa::zywychBlokow == 0, "brak pamieci na kubelki: kod 503, nic nie wycieka");
  atrapa::psramJest = true;
  atrapa::mallocOdmawia = false;
  atrapa::odmowaRezerwyOd = 1000;
  const Odpowiedz c = wywolaj("dzien", 96);
  sprawdz(c.kod == 503 && c.tekst == "brak pamieci\n" && atrapa::zywychBlokow == 0, "brak pamieci na tekst odpowiedzi: kod 503, kubelki zwolnione");
  atrapa::odmowaRezerwyOd = SIZE_MAX;
  bool wszystkie = true;
  for (long po = 0; po <= 97; po++) {
    atrapa::odmowaConcatPo = po;
    const Odpowiedz d = wywolaj("dzien", 96);
    atrapa::odmowaConcatPo = -1;
    if (d.kod != 503 || d.tekst != "brak pamieci\n" || atrapa::zywychBlokow != 0) wszystkie = false;
  }
  sprawdz(wszystkie, "odmowa dopisania w dowolnym z 98 miejsc: zawsze kod 503, nigdy ucieta odpowiedz z kodem 200");
  const Odpowiedz e = wywolaj("dzien", 96);
  sprawdz(e.kod == 200 && e.poprawnaBudowa, "po przywroceniu pamieci odpowiedz znow poprawna");
}

static void testZegarSkacze() {
  printf("  11. zegar centrali skacze:\n");
  wyczyscBufory();
  std::vector<HistoriaProbka> w;
  for (int i = 0; i < 30; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  for (int i = 0; i < 30; i++) w.push_back(probka(T0 + 600 + (uint32_t)i * 60, 30.0f));   // zegar cofniety o 20 minut
  ustawKrotki(w);
  const Odpowiedz o = wywolaj("godz", 96);
  sprawdz(o.kod == 200 && o.poprawnaBudowa && o.wpisy.size() == 96, "czas cofniety w trakcie: poprawna odpowiedz, 96 pozycji");
  sprawdz(zgodnyZRef("godz", 96, "czas cofniety"), "wynik zgodny z liczeniem sekunda po sekundzie");
  w.clear();
  for (int i = 0; i < 30; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  w.push_back(probka(4200000000U, 99.0f));   // rok 2103: blad zegara -> probka traktowana jak bez czasu
  w.push_back(probka(0xFFFFFFFFU, 99.0f));
  ustawKrotki(w);
  const Odpowiedz p = wywolaj("godz", 96);
  sprawdz(p.kod == 200 && p.poprawnaBudowa && p.naglowek["do"] == std::to_string(T0 + 31 * 60), "probka z czasem po roku 2100 nie przesuwa okna w przyszlosc");
  sprawdz(zgodnyZRef("godz", 96, "czas z przyszlosci"), "wynik zgodny z liczeniem sekunda po sekundzie");
  std::vector<HistoriaProbkaDluga> d;
  d.push_back(probkaDluga(0xFFFFFFF0U, 1.0f));
  d.push_back(probkaDluga(T0, 2.0f));
  d.push_back(probkaDluga(T0 + 600, 3.0f));
  wyczyscBufory();
  ustawDlugi(d);
  const Odpowiedz q = wywolaj("tydz", 96);
  sprawdz(q.kod == 200 && q.poprawnaBudowa && q.naglowek["wierszy"] == "2", "wpis dlugi z czasem po roku 2100 jest pomijany");
  w.clear();
  w.push_back(probka(946684800U, 1.0f));     // rok 2000: zegar RTC po utracie baterii
  for (int i = 0; i < 30; i++) w.push_back(probka(T0 + (uint32_t)i * 60, 20.0f));
  wyczyscBufory();
  ustawKrotki(w);
  const Odpowiedz r = wywolaj("dzien", 96);
  sprawdz(r.kod == 200 && r.poprawnaBudowa && kol(r.wpisy.back(), 1) == "20.0" && ilePrzerw(r) > 80, "probka z roku 2000 nie psuje wykresu (reszta doby to przerwa)");
  sprawdz(zgodnyZRef("dzien", 96, "rok 2000"), "wynik zgodny z liczeniem sekunda po sekundzie");
}

static void testLosowe() {
  printf("  12. dane losowe, porownanie z liczeniem sekunda po sekundzie:\n");
  int proby = 0, zgodne = 0;
  for (int seria = 0; seria < 400; seria++) {
    wyczyscBufory();
    std::vector<HistoriaProbka> k;
    uint32_t t = T0 - (los() % 200000);
    const int ileK = (seria % 7 == 0) ? 1440 : (int)(los() % 500);
    for (int i = 0; i < ileK; i++) {
      const uint32_t r = los() % 100;
      t += (r < 85) ? 60 : (r < 93) ? (30 + los() % 300) : (r < 97) ? (361 + los() % 4000) : 0;   // zwykle 60 s, czasem nierowno, czasem przerwa, czasem ten sam czas
      HistoriaProbka p = probka((los() % 50 == 0) ? 0 : t, losWartosc());
      p.t_bojler = (los() % 40 == 0) ? NAN : losWartosc();
      p.cisnienie = (float)(los() % 40) / 10.0f;
      p.klapa_proc = (int)(los() % 181);
      p.pompa = (los() % 2) != 0;
      k.push_back(p);
    }
    ustawKrotki(k, (int)(los() % HISTORIA_MAX_PROBEK));
    if (seria % 3 == 0) {
      std::vector<HistoriaProbkaDluga> d;
      uint32_t td = T0 - 900000 - (los() % 100000);
      const int ileD = (int)(los() % 400);
      for (int i = 0; i < ileD; i++) {
        const uint32_t r = los() % 100;
        td += (r < 60) ? 3600 : (r < 90) ? (30 + los() % 900) : (5401 + los() % 30000);
        HistoriaProbkaDluga p = probkaDluga(td, losWartosc());
        p.t_zewn_min = p.t_zewn_sr - (float)(los() % 30) / 10.0f;
        p.t_zewn_max = p.t_zewn_sr + (float)(los() % 30) / 10.0f;
        p.pompa = (int8_t)((int)(los() % 3) - 1);
        d.push_back(p);
      }
      ustawDlugi(d);
    }
    const int pkt = (seria % 5 == 0) ? 96 : (int)(8 + los() % 233);
    for (const char* tryb : {"godz", "dzien"}) {
      proby++;
      const bool ok = zgodnyZRef(tryb, pkt, tryb);
      if (ok) zgodne++;
      sprawdzCicho(ok, std::string("seria ") + std::to_string(seria) + " tryb " + tryb + " pkt " + std::to_string(pkt));
    }
    if (seria % 3 == 0) {
      proby++;
      const bool ok = zgodnyZRef("tydz", pkt, "tydz");
      if (ok) zgodne++;
      sprawdzCicho(ok, std::string("seria ") + std::to_string(seria) + " tryb tydz pkt " + std::to_string(pkt));
    }
    sprawdzCicho(atrapa::zywychBlokow == 0 && atrapa::sekcjaKrytyczna == 0 && atrapa::przydzialWSekcji == 0, "pamiec i spinlock po serii " + std::to_string(seria));
  }
  printf("    [%s] %d z %d losowych zestawow daje dokladnie ten sam tekst odpowiedzi\n", zgodne == proby ? " OK " : "BLAD", zgodne, proby);
}

static void testPelneBufory() {
  printf("  13. oba bufory pelne (8000 zdarzen + 1440 probek) - rozmiar i czas:\n");
  wyczyscBufory();
  std::vector<HistoriaProbkaDluga> d;
  uint32_t t = T0 - 8000U * 75U;   // zdarzenie co 75 s: piec pracujacy bez przerwy, ok. 7 dni
  for (int i = 0; i < 8000; i++, t += 75) d.push_back(probkaDluga(t, 40.0f + (float)(i % 200) / 10.0f));
  std::vector<HistoriaProbka> k;
  for (int i = 0; i < 1440; i++) k.push_back(probka(T0 - 86400 + (uint32_t)i * 60, 50.0f + (float)(i % 100) / 10.0f));
  ustawDlugi(d, 4321);
  ustawKrotki(k, 123);
  const auto start = std::chrono::steady_clock::now();
  Odpowiedz o;
  for (int i = 0; i < 20; i++) o = wywolaj("tydz", 96);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 20.0;
  // To samo po staremu: caly CSV bufora dlugiego (rozmiar liczony z formatu historiaDlugaLiniaCsv()).
  size_t staryCsv = 0;
  for (const HistoriaProbkaDluga& p : d) {
    char b[400];
    staryCsv += (size_t)snprintf(b, sizeof(b), "%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%d,%d,%d,%d,%d,%d,%lu,%d\n",
                                 "13.11 12:00", p.t_zewn_sr, p.t_zewn_min, p.t_zewn_max, p.t_bojler_sr, p.t_bojler_min, p.t_bojler_max, p.t_ogrz_sr, p.t_ogrz_min, p.t_ogrz_max,
                                 p.t_ogrz_sr_sr, p.t_ogrz_sr_min, p.t_ogrz_sr_max, p.t_powrot_sr, p.t_powrot_min, p.t_powrot_max, p.t_panel_sr, p.t_panel_min, p.t_panel_max,
                                 p.t_pokoj_sr, p.t_pokoj_min, p.t_pokoj_max, p.cisnienie_sr, p.cisnienie_min, p.cisnienie_max, p.wilgotnosc_sr, p.wilgotnosc_min, p.wilgotnosc_max,
                                 p.klapa_proc_sr, p.klapa_proc_min, p.klapa_proc_max, p.syberko_proc_sr, p.syberko_proc_min, p.syberko_proc_max, (unsigned long)p.czas_unix, (int)p.pompa);
  }
  sprawdz(o.poprawnaBudowa && o.wpisy.size() == 96 && ilePrzerw(o) == 0, "96 punktow bez przerw");
  printf("    - odpowiedz dla panelu: %zu B; ten sam zakres po staremu (/api/history/long): %zu B, czyli %.0f razy wiecej\n", o.tekst.size(), staryCsv,
         (double)staryCsv / (double)o.tekst.size());
  printf("    - czas liczenia na tym komputerze: %.2f ms (9440 wierszy zrodlowych)\n", ms);
  sprawdz(o.tekst.size() < 24000, "odpowiedz ma mniej niz 24 kB");
  sprawdz(o.naglowek["wierszy"] == std::to_string(8000 - 1152 + 1440), "wierszy: 6848 zdarzen sprzed ostatniej doby + 1440 probek");
  sprawdz(zgodnyZRef("tydz", 96, "pelne bufory"), "wynik zgodny z liczeniem sekunda po sekundzie");
}

// ---------------------------------------------------------------------------
// Wzorce odpowiedzi dla testow panelu LCD (LCD-Centrali/testy_host)
// ---------------------------------------------------------------------------
// Testy panelu nie zgaduja, jak wyglada odpowiedz centrali: dostaja pliki
// zapisane TYM kodem centrali, na danych opisanych ponizej. uruchom_testy.sh
// porownuje je przy kazdym uruchomieniu z kopia w katalogu panelu, wiec
// zmiana formatu po jednej stronie nie przejdzie niezauwazona po drugiej.
static HistoriaProbka probkaPelna(uint32_t czas, float zewn, float bojler, float ogrz, float ogrzSr, float powrot, float panel, float pokoj,
                                  float cisn, float wilg, int klapa, int syberek, bool pompa) {
  HistoriaProbka p = probka(czas, 0.0f);
  p.t_zewn = zewn; p.t_bojler = bojler; p.t_ogrz = ogrz; p.t_ogrz_sr = ogrzSr; p.t_powrot = powrot; p.t_panel = panel; p.t_pokoj = pokoj;
  p.cisnienie = cisn; p.wilgotnosc = wilg; p.klapa_proc = klapa; p.syberko_proc = syberek; p.pompa = pompa;
  return p;
}
// Zdarzenie bufora dlugiego: srednia `zewn`, minimum o `rozrzut` nizej, maksimum o `rozrzut` wyzej.
static HistoriaProbkaDluga probkaDlugaPelna(uint32_t czas, float zewn, float rozrzut, float bojler, float ogrz, float pokoj, float cisn, int pompa) {
  HistoriaProbkaDluga p = probkaDluga(czas, 0.0f);
  p.t_zewn_sr = zewn; p.t_zewn_min = zewn - rozrzut; p.t_zewn_max = zewn + rozrzut;
  p.t_bojler_sr = p.t_bojler_min = p.t_bojler_max = bojler;
  p.t_ogrz_sr = p.t_ogrz_min = p.t_ogrz_max = ogrz;
  p.t_ogrz_sr_sr = p.t_ogrz_sr_min = p.t_ogrz_sr_max = ogrz - 1.0f;
  p.t_powrot_sr = p.t_powrot_min = p.t_powrot_max = ogrz - 15.0f;
  p.t_panel_sr = p.t_panel_min = p.t_panel_max = zewn + 4.0f;
  p.t_pokoj_sr = p.t_pokoj_min = p.t_pokoj_max = pokoj;
  p.cisnienie_sr = p.cisnienie_min = p.cisnienie_max = cisn;
  p.pompa = (int8_t)pompa;
  return p;
}

static bool zapiszWzorzec(const std::string& katalog, const char* nazwa, const char* tryb, const char* opis) {
  String out;
  const int kod = historiaPanelZbudujCsv(tryb, 96, out);
  if (kod != 200) { printf("    [BLAD] wzorzec %s: kod %d\n", nazwa, kod); return false; }
  const std::string sciezka = katalog + "/" + nazwa;
  FILE* f = fopen(sciezka.c_str(), "wb");
  if (!f) { printf("    [BLAD] nie moge zapisac %s\n", sciezka.c_str()); return false; }
  fwrite(out.t.data(), 1, out.t.size(), f);
  fclose(f);
  printf("    %-28s %6zu B  %s\n", nazwa, out.t.size(), opis);
  return true;
}

static int zapiszWzorce(const std::string& katalog) {
  printf("Wzorce odpowiedzi GET /api/history/panel?tryb=...&pkt=96 -> %s\n", katalog.c_str());
  bool ok = true;

  // dzien.txt - pelna doba, probka co 60 s, koniec o 13:00.
  //   ZEWN rosnie rowno od -5,0 do 9,4; BOJLER 45,0; C.O. 60,0 przez pierwsze 12 h, potem 40,0;
  //   POWROT pila 30,0..35,9 co godzine; pompa pracuje w drugiej polowie.
  wyczyscBufory();
  {
    std::vector<HistoriaProbka> k;
    for (int i = 0; i < 1440; i++)
      k.push_back(probkaPelna(T0 - (uint32_t)(1439 - i) * 60, -5.0f + 0.01f * (float)i, 45.0f, i < 720 ? 60.0f : 40.0f, 55.0f,
                              30.0f + 0.1f * (float)(i % 60), 12.0f, 21.5f, 1013.2f, 45.0f, 40, 60, i >= 720));
    ustawKrotki(k, 777);
  }
  ok = zapiszWzorzec(katalog, "dzien.txt", "dzien", "pelna doba, ZEWN rosnie od -5,0 do 9,4") && ok;

  // godz_ta_sama_doba.txt - te same dane, tryb godzinny: ostatnia godzina tej doby.
  ok = zapiszWzorzec(katalog, "godz_ta_sama_doba.txt", "godz", "ostatnia godzina tej samej doby") && ok;

  // tydz_przerwa.txt - tydzien z przerwa w pracy centrali:
  //   dni 7..4 wstecz: zdarzenie co 30 min, ZEWN 2,0 (min 1,5, max 2,5)
  //   dni 4..2,5 wstecz: centrala wylaczona (nic)
  //   dni 2,5..1 wstecz: zdarzenie co 30 min, ZEWN 6,0
  //   ostatnia doba: probka co 60 s, ZEWN 8,0
  wyczyscBufory();
  {
    std::vector<HistoriaProbkaDluga> d;
    for (uint32_t t = T0 - 7 * 86400; t <= T0 - 4 * 86400; t += 1800) d.push_back(probkaDlugaPelna(t, 2.0f, 0.5f, 50.0f, 60.0f, 21.0f, 1010.0f, 1));
    for (uint32_t t = T0 - 216000; t < T0 - 86400; t += 1800) d.push_back(probkaDlugaPelna(t, 6.0f, 0.5f, 52.0f, 62.0f, 21.5f, 1015.0f, 0));
    std::vector<HistoriaProbka> k;
    for (int i = 0; i < 1440; i++)
      k.push_back(probkaPelna(T0 - (uint32_t)(1439 - i) * 60, 8.0f, 54.0f, 64.0f, 63.0f, 49.0f, 12.0f, 22.0f, 1020.0f, 50.0f, 35, 55, true));
    ustawDlugi(d);
    ustawKrotki(k);
  }
  ok = zapiszWzorzec(katalog, "tydz_przerwa.txt", "tydz", "tydzien, centrala wylaczona przez 1,5 doby") && ok;

  // godz_bez_czujnika.txt - godzina; czujnik pokojowy bez odczytu (NaN) przez pierwsze pol godziny.
  wyczyscBufory();
  {
    std::vector<HistoriaProbka> k;
    for (int i = 0; i <= 60; i++)
      k.push_back(probkaPelna(T0 - (uint32_t)(60 - i) * 60, 3.0f, 45.0f, 58.0f, 57.0f, 41.0f, 10.0f, i < 30 ? NAN : 21.5f, 1013.2f, 45.0f, 40, 60, false));
    ustawKrotki(k);
  }
  ok = zapiszWzorzec(katalog, "godz_bez_czujnika.txt", "godz", "czujnik pokojowy bez odczytu przez pierwsze pol godziny") && ok;

  // dzien_pusta.txt - centrala tuz po pierwszym uruchomieniu: historii nie ma.
  wyczyscBufory();
  ok = zapiszWzorzec(katalog, "dzien_pusta.txt", "dzien", "centrala bez historii") && ok;

  // dzien_pusta_bez_fs.txt - to samo, gdy w centrali wylaczony jest zapis historii na flash (fs=0).
  g_fsWlaczony = false;
  ok = zapiszWzorzec(katalog, "dzien_pusta_bez_fs.txt", "dzien", "centrala bez historii, zapis na flash wylaczony") && ok;
  g_fsWlaczony = true;

  // godz_jedna_probka.txt - minute po starcie: jeden pomiar.
  wyczyscBufory();
  ustawKrotki({probkaPelna(T0, 3.0f, 45.0f, 58.0f, 57.0f, 41.0f, 10.0f, 21.5f, 1013.2f, 45.0f, 40, 60, false)});
  ok = zapiszWzorzec(katalog, "godz_jedna_probka.txt", "godz", "jeden pomiar") && ok;

  // dzien_bez_zegara.txt - centrala bez ustawionego zegara: 30 probek z czasem 0, ZEWN = 0, 1, 2, ...
  wyczyscBufory();
  {
    std::vector<HistoriaProbka> k;
    for (int i = 0; i < 30; i++) k.push_back(probkaPelna(0, (float)i, 45.0f, 58.0f, 57.0f, 41.0f, 10.0f, 21.5f, 1013.2f, 45.0f, 40, 60, false));
    ustawKrotki(k);
  }
  ok = zapiszWzorzec(katalog, "dzien_bez_zegara.txt", "dzien", "centrala bez zegara") && ok;

  // tydz_pelne_bufory.txt - oba bufory pelne (8000 zdarzen + 1440 probek), liczby o najdluzszym zapisie.
  wyczyscBufory();
  {
    std::vector<HistoriaProbkaDluga> d;
    const uint32_t poczatek = T0 - 86400 - 8000U * 60U;
    for (int i = 0; i < 8000; i++)
      d.push_back(probkaDlugaPelna(poczatek + (uint32_t)i * 60U, -12.3f - 0.1f * (float)(i % 50), 1.5f, 78.4f, 89.9f, 22.5f, 1013.2f, i % 2));
    std::vector<HistoriaProbka> k;
    for (int i = 0; i < 1440; i++)
      k.push_back(probkaPelna(T0 - (uint32_t)(1439 - i) * 60, -15.5f - 0.1f * (float)(i % 40), 78.4f, 89.9f, 88.8f, 74.4f, -11.1f, 22.5f, 1013.2f, 100.0f, 100, 100, i % 3 == 0));
    ustawDlugi(d, 4321);
    ustawKrotki(k, 123);
  }
  ok = zapiszWzorzec(katalog, "tydz_pelne_bufory.txt", "tydz", "oba bufory pelne, najdluzsze liczby") && ok;

  wyczyscBufory();
  return ok ? 0 : 1;
}

int main(int argc, char** argv) {
  // Ta sama strefa czasowa co w centrali (RTC_NTP_TZ_POLSKA w main_centrala.cpp).
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  if (argc == 3 && std::string(argv[1]) == "--wzorce") return zapiszWzorce(argv[2]);
  printf("Testy kodu [v3.58.4 PANEL-WYKRESY] (GET /api/history/panel)\n");
  testGodzinaStalaWartosc();
  testSkokWartosci();
  testPrzerwa();
  testDzienPelnyBufor();
  testTydzien();
  testBrakDanych();
  testBezZegara();
  testWartosciNietypowe();
  testParametry();
  testBrakPamieci();
  testZegarSkacze();
  testLosowe();
  testPelneBufory();
  printf("%s: %d sprawdzen, %d bledow\n", g_bledow == 0 ? "WSZYSTKO OK" : "SA BLEDY", g_sprawdzen, g_bledow);
  return g_bledow == 0 ? 0 : 1;
}
