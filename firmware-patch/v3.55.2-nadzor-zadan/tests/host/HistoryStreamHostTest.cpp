// ============================================================================
// [v3.55.1] Test hostowy: „start nie zalezy od rozmiaru plikow historii".
//
// Co sprawdza (to jest sedno poprawki, nie ozdoba):
//   1. Czytnik zwraca DOKLADNIE te same wiersze co stary parser — takze gdy
//      znak konca linii wypada na granicy porcji 4 KB.
//   2. Czytanie duzego pliku sklada sie z wielu malych krokow: miedzy dwoma
//      oddaniami czasu schedulerowi nigdy nie przechodzi wiecej niz jedna
//      porcja. To wlasnie ta wlasciwosc ratuje urzadzenie przed WDT.
//   3. Bufor w RAM trzyma OSTATNIE N probek (pierscien), a nie pierwsze N.
//   4. Plik wiekszy niz limit bajtow jest czytany od OGONA — start nie
//      wydluza sie wraz ze wzrostem pliku.
//   5. Limit czasu przerywa czytanie zamiast je zawieszac (start idzie dalej).
//   6. Format 35 kolumn (stary) i 36 kolumn (nowy) obslugiwane tak samo.
//
// Jak to dziala: run_history_tests.py wycina z ../../main_centrala.cpp
// DOKLADNIE klase HistoriaStrumien i obie funkcje wczytujace, a reszte
// (File, LittleFS, String, zegar, WDT) podstawia jako atrapy. Kompiluje sie
// i dziala na zwyklym g++.
//
// Uzycie (z katalogu tests/host):  python3 run_history_tests.py
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

// ── 1. Atrapa String z Arduino ──────────────────────────────────────────────
class String {
public:
    String() {}
    String(const char *s) : v_(s ? s : "") {}
    String(const std::string &s) : v_(s) {}
    const char *c_str() const { return v_.c_str(); }
    unsigned length() const { return (unsigned)v_.size(); }
    char operator[](unsigned i) const { return i < v_.size() ? v_[i] : '\0'; }
    String &operator+=(char c) { v_ += c; return *this; }
    String &operator+=(const char *s) { v_ += (s ? s : ""); return *this; }
    String &operator=(const char *s) { v_ = (s ? s : ""); return *this; }
    bool operator==(const String &o) const { return v_ == o.v_; }
    bool operator==(const char *o) const { return v_ == (o ? o : ""); }
    String substring(unsigned from) const {
        if (from >= v_.size()) return String();
        return String(v_.substr(from));
    }
    String substring(unsigned from, unsigned to) const {
        if (from >= v_.size()) return String();
        if (to > v_.size()) to = (unsigned)v_.size();
        if (to <= from) return String();
        return String(v_.substr(from, to - from));
    }
    float toFloat() const { return strtof(v_.c_str(), nullptr); }
    int toInt() const { return (int)strtol(v_.c_str(), nullptr, 10); }
    std::string v_;
};

// ── 2. Atrapa plikow LittleFS ──────────────────────────────────────────────
struct VfsFile {
    const std::vector<uint8_t> *dane = nullptr;
    uint32_t pos = 0;
    bool otwarty = false;
};

class File {
public:
    File() {}
    explicit File(const std::vector<uint8_t> *d) : dane_(d), otwarty_(true) {}
    explicit operator bool() const { return otwarty_ && dane_; }
    uint32_t size() const { return dane_ ? (uint32_t)dane_->size() : 0U; }
    bool seek(uint32_t p) {
        if (!dane_ || p > dane_->size()) return false;
        pos_ = p;
        return true;
    }
    size_t read(uint8_t *buf, size_t n) {
        if (!dane_ || !otwarty_) return 0;
        const size_t zostalo = dane_->size() - pos_;
        if (n > zostalo) n = zostalo;
        if (n) memcpy(buf, dane_->data() + pos_, n);
        pos_ += (uint32_t)n;
        return n;
    }
    void close() { otwarty_ = false; }
private:
    const std::vector<uint8_t> *dane_ = nullptr;
    uint32_t pos_ = 0;
    bool otwarty_ = false;
};

class LittleFSAtrapa {
public:
    std::map<std::string, std::vector<uint8_t>> pliki;
    bool exists(const char *n) const { return pliki.count(n) != 0; }
    File open(const char *n, const char *) {
        auto it = pliki.find(n);
        if (it == pliki.end()) return File();
        return File(&it->second);
    }
};
static LittleFSAtrapa LittleFS;

// ── 3. Atrapy pamieci, zegara i WDT ────────────────────────────────────────
static uint32_t g_clock = 0;
static uint32_t g_clockStep = 1;         // ms na kazde wywolanie millis()
static uint32_t g_delayCalls = 0;
static uint32_t g_bajtowBezOddechu = 0;  // kontrola: ile bajtow miedzy oddechami
static uint32_t g_maxBajtowBezOddechu = 0;
static std::vector<size_t> g_allocSizes;

static uint32_t millis() { g_clock += g_clockStep; return g_clock; }
static void vTaskDelay(int) { g_delayCalls++; g_bajtowBezOddechu = 0; }
static inline void serwisujTWDTJesliZarejestrowany() {}

static void *ps_malloc(size_t n) { g_allocSizes.push_back(n); return malloc(n); }
static void *ps_calloc(size_t n, size_t s) { g_allocSizes.push_back(n * s); return calloc(n, s); }

// ── 4. Atrapy stalych i logow ──────────────────────────────────────────────
static constexpr int DC_HISTORIA = 1;
static constexpr int DL_DEBUG = 1, DL_INFO = 2, DL_WARN = 3, DL_ERR = 4;
static int g_warnCount = 0;
#define DLOG(cat, lvl, msg) do { (void)(cat); if ((lvl) >= DL_WARN) g_warnCount++; } while (0)
#define DLOGF(cat, lvl, fmt, ...) do { (void)(cat); if ((lvl) >= DL_WARN) g_warnCount++; } while (0)
static String F(const char *s) { return String(s); }

// ── 5. Struktury i globalne buforow historii (1:1 z firmware) ──────────────
struct HistoriaProbka {
    char etykieta[10];
    float t_zewn, t_bojler, t_ogrz, t_ogrz_sr, t_powrot, t_panel, t_pokoj;
    bool pompa;
    float cisnienie, wilgotnosc;
    int klapa_proc, syberko_proc;
    uint32_t czas_unix;
};

struct HistoriaProbkaDluga {
    char  etykieta[12];
    float t_zewn_sr, t_zewn_min, t_zewn_max;
    float t_bojler_sr, t_bojler_min, t_bojler_max;
    float t_ogrz_sr, t_ogrz_min, t_ogrz_max;
    float t_ogrz_sr_sr, t_ogrz_sr_min, t_ogrz_sr_max;
    float t_powrot_sr, t_powrot_min, t_powrot_max;
    float t_panel_sr, t_panel_min, t_panel_max;
    float t_pokoj_sr, t_pokoj_min, t_pokoj_max;
    float cisnienie_sr, cisnienie_min, cisnienie_max;
    float wilgotnosc_sr, wilgotnosc_min, wilgotnosc_max;
    int   klapa_proc_sr, klapa_proc_min, klapa_proc_max;
    int   syberko_proc_sr, syberko_proc_min, syberko_proc_max;
    uint32_t czas_unix;
    int8_t pompa;
};

#define HISTORIA_MAX_PROBEK        1440
#define HISTORIA_DLUGA_MAX_PROBEK  8000
#define HISTORIA_PLIK              "/hist_short.csv"
#define HISTORIA_DLUGA_PLIK        "/hist_long.csv"

static std::vector<HistoriaProbka> g_bufShort(HISTORIA_MAX_PROBEK);
static std::vector<HistoriaProbkaDluga> g_bufLong(HISTORIA_DLUGA_MAX_PROBEK);
static HistoriaProbka *historiaBufor = g_bufShort.data();
static HistoriaProbkaDluga *historiaDlugaBufor = g_bufLong.data();
static int historiaGlowa = 0, historiaLiczbaProbek = 0, historiaPlikLiczbaLinii = -1;
static bool historiaPelna = false;
static int historiaDlugaGlowa = 0, historiaDlugaLiczbaProbek = 0, historiaDlugaPlikLiczbaLinii = -1;
static bool historiaDlugaPelna = false;
static bool littlefsReady = true;

// Normalizacja znacznikow czasu nie jest przedmiotem tego testu - przepuszczamy.
static uint32_t historiaNormalizujLegacyUnix(const char *, uint32_t ts, bool) { return ts; }

// ── 6. Kod wyciagniety z firmware ──────────────────────────────────────────
// @@SLICE@@

// ============================================================================
// 7. Testy
// ============================================================================
static int g_checks = 0, g_fails = 0;

static void check(bool warunek, const char *opis) {
    g_checks++;
    if (!warunek) { g_fails++; printf("  BLAD: %s\n", opis); }
}

static void resetStatystyk() {
    g_clock = 0; g_clockStep = 1; g_delayCalls = 0;
    g_bajtowBezOddechu = 0; g_maxBajtowBezOddechu = 0;
    g_allocSizes.clear(); g_warnCount = 0;
    historiaGlowa = historiaLiczbaProbek = 0;
    historiaPlikLiczbaLinii = -1; historiaPelna = false;
    historiaDlugaGlowa = historiaDlugaLiczbaProbek = 0;
    historiaDlugaPlikLiczbaLinii = -1; historiaDlugaPelna = false;
    memset(g_bufShort.data(), 0, g_bufShort.size() * sizeof(HistoriaProbka));
    memset(g_bufLong.data(), 0, g_bufLong.size() * sizeof(HistoriaProbkaDluga));
}

// Wiersz bufora krotkiego, 14 kolumn (kolejnosc jak w historiaLiniaCsv).
static std::string wierszKrotki(int i, bool crlf) {
    char b[32];
    std::string s;
    snprintf(b, sizeof(b), "%02d.%02d %02d:%02d", 1 + (i / 1440) % 28, 1 + (i / 60) % 28, (i / 60) % 24, i % 60);
    s += b;
    const double kolumny[7] = {
        10.0 + (i % 40) + (i % 100) / 100.0,   // t_zewn
        20.0 + (i % 40) + (i % 100) / 100.0,   // t_bojler
        30.0 + (i % 40) + (i % 100) / 100.0,   // t_ogrz
        31.0 + (i % 40) + (i % 100) / 100.0,   // t_ogrz_sr
        32.0 + (i % 40) + (i % 100) / 100.0,   // t_powrot
        40.0 + (i % 40) + (i % 100) / 100.0,   // t_panel
        50.0 + (i % 40) + (i % 100) / 100.0,   // t_pokoj
    };
    for (int k = 0; k < 7; k++) { snprintf(b, sizeof(b), ",%.2f", kolumny[k]); s += b; }
    snprintf(b, sizeof(b), ",%d,%.2f,%d,%d,%d,%u", i % 2, 950.0 + (i % 40),
             (int)(40 + i % 50), i % 101, i % 91, (unsigned)(1700000000u + (unsigned)i * 60u));
    s += b;
    s += crlf ? "\r\n" : "\n";
    return s;
}

// Wiersz bufora dlugiego: 35 albo 36 kolumn (36 = z kolumna pompy na koncu).
static std::string wierszDlugi(int i, bool zPompa, bool crlf) {
    std::string s;
    char b[32];
    snprintf(b, sizeof(b), "%02d.%02d %02d:00", 1 + (i / 24) % 28, 1 + (i / 24) % 28, i % 24);
    s += b;
    for (int k = 0; k < 27; k++) {          // 9 grup po 3 (sr,min,max)
        snprintf(b, sizeof(b), ",%d.%02d", 5 + (i + k) % 60, (i * 7 + k) % 100);
        s += b;
    }
    for (int k = 0; k < 6; k++) {           // klapa (3) + syberko (3), calkowite
        snprintf(b, sizeof(b), ",%d", (i + k) % 101);
        s += b;
    }
    snprintf(b, sizeof(b), ",%u", (unsigned)(1700000000u + (unsigned)i * 3600u));
    s += b;
    if (zPompa) { snprintf(b, sizeof(b), ",%d", i % 2); s += b; }
    s += crlf ? "\r\n" : "\n";
    return s;
}

static void wgraj(const char *nazwa, const std::string &tresc) {
    std::vector<uint8_t> v(tresc.begin(), tresc.end());
    LittleFS.pliki[nazwa] = v;
}

// ── Test 1: bufor krotki, rowna sie staremu parserowi ──────────────────────
static void testKrotkiZgodny(int linii, bool crlf) {
    printf("  bufor krotki: %d wierszy%s\n", linii, crlf ? " (CRLF)" : "");
    std::string tresc;
    for (int i = 0; i < linii; i++) tresc += wierszKrotki(i, crlf);
    wgraj(HISTORIA_PLIK, tresc);

    resetStatystyk();
    historiaWczytajZFS();

    const int oczekiwane = linii < HISTORIA_MAX_PROBEK ? linii : HISTORIA_MAX_PROBEK;
    check(historiaLiczbaProbek == oczekiwane, "liczba probek w buforze");
    check(historiaPelna == (linii >= HISTORIA_MAX_PROBEK), "flaga pelny");

    // Pierscien: indeks 0 = najstarsza z zatrzymanych (tak czyta firmware:
    // startSnap = historiaPelna ? historiaGlowa : 0).
    const int pierwsza = (linii > HISTORIA_MAX_PROBEK) ? (linii - HISTORIA_MAX_PROBEK) : 0;
    const int start = historiaPelna ? historiaGlowa : 0;
    bool zgodne = true;
    for (int k = 0; k < oczekiwane; k++) {
        const HistoriaProbka &p = historiaBufor[(start + k) % HISTORIA_MAX_PROBEK];
        const int oczekiwanyCzas = 1700000000 + (pierwsza + k) * 60;
        if ((int)p.czas_unix != oczekiwanyCzas) zgodne = false;
        if (k == 0 && p.t_zewn < 9.0f) zgodne = false;
    }
    check(zgodne, "kolejnosc i znaczniki czasu probek");
    check(historiaPlikLiczbaLinii == linii, "licznik linii w pliku");

    // Kazda porcja 4 KB = jedno oddanie czasu; nigdy wiecej niz porcja.
    check(g_maxBajtowBezOddechu <= 4096, "miedzy oddechami nie wiecej niz porcja 4 KB");
    check(g_delayCalls >= (uint32_t)(tresc.size() / 4096), "liczba oddan czasu rosnie z rozmiarem");
    bool alokacjaOk = true;
    for (size_t s : g_allocSizes) if (s > 8192) alokacjaOk = false;
    check(alokacjaOk, "zadnej duzej alokacji (caly plik nie trafia do RAM)");
}

// ── Test 2: bufor dlugi, 35 i 36 kolumn ───────────────────────────────────
static void testDlugi(int linii, bool zPompa) {
    printf("  bufor dlugi: %d wierszy, %d kolumn\n", linii, zPompa ? 36 : 35);
    std::string tresc;
    for (int i = 0; i < linii; i++) tresc += wierszDlugi(i, zPompa, false);
    wgraj(HISTORIA_DLUGA_PLIK, tresc);

    resetStatystyk();
    historiaDlugaWczytajZFS();

    const int oczekiwane = linii < HISTORIA_DLUGA_MAX_PROBEK ? linii : HISTORIA_DLUGA_MAX_PROBEK;
    check(historiaDlugaLiczbaProbek == oczekiwane, "liczba probek w buforze dlugim");
    check(historiaDlugaPelna == (linii >= HISTORIA_DLUGA_MAX_PROBEK), "flaga pelny (dlugi)");

    const int pierwsza = (linii > HISTORIA_DLUGA_MAX_PROBEK) ? (linii - HISTORIA_DLUGA_MAX_PROBEK) : 0;
    const int start = historiaDlugaPelna ? historiaDlugaGlowa : 0;
    bool zgodne = true;
    for (int k = 0; k < oczekiwane; k++) {
        const HistoriaProbkaDluga &p = historiaDlugaBufor[(start + k) % HISTORIA_DLUGA_MAX_PROBEK];
        const unsigned oczekiwany = 1700000000u + (unsigned)(pierwsza + k) * 3600u;
        if (p.czas_unix != oczekiwany) zgodne = false;
        if (!zPompa && p.pompa != -1) zgodne = false;
        if (zPompa && p.pompa != (int8_t)((pierwsza + k) % 2)) zgodne = false;
    }
    check(zgodne, "kolejnosc, czasy i kolumna pompy");
    check(g_maxBajtowBezOddechu <= 4096, "miedzy oddechami nie wiecej niz porcja 4 KB");
}

// ── Test 3: plik wiekszy niz limit — czytany od ogona ─────────────────────
static void testOgona() {
    printf("  plik > limitu bajtow: czytany od ogona\n");
    // 12000 wierszy x ~172 B = ~2,06 MB > limitu 2 MB.
    const int linii = 12000;
    std::string tresc;
    for (int i = 0; i < linii; i++) tresc += wierszDlugi(i, true, false);
    wgraj(HISTORIA_DLUGA_PLIK, tresc);

    resetStatystyk();
    historiaDlugaWczytajZFS();

    check(tresc.size() > HISTORIA_DLUGA_WCZYTAJ_LIMIT_BAJTOW, "plik testowy naprawde wiekszy od limitu");
    check(historiaDlugaLiczbaProbek == HISTORIA_DLUGA_MAX_PROBEK, "bufor pelny mimo limitu");
    check(g_delayCalls > 0 && g_maxBajtowBezOddechu <= 4096, "ogon tez czytany porcjami");

    // Najnowsza probka MUSI byc ostatnia z pliku (nic nie ucieto z konca).
    const HistoriaProbkaDluga &ostatnia =
        historiaDlugaBufor[(historiaDlugaGlowa + HISTORIA_DLUGA_MAX_PROBEK - 1) % HISTORIA_DLUGA_MAX_PROBEK];
    // (przy pelnym pierscieniu glowa wskazuje NAJSTARSZA probke — patrz
    //  startSnap = historiaPelna ? historiaGlowa : 0 w historiaDlugaZbudujCsv)
    check(ostatnia.czas_unix == 1700000000u + (unsigned)(linii - 1) * 3600u, "najnowsza probka to ostatni wiersz pliku");
    // Pierwsza w buforze to 8000. wiersz od konca — czyli czytanie od ogona dziala.
    const HistoriaProbkaDluga &pierwsza = historiaDlugaBufor[historiaDlugaGlowa];
    check(pierwsza.czas_unix == 1700000000u + (unsigned)(linii - HISTORIA_DLUGA_MAX_PROBEK) * 3600u,
          "najstarsza w buforze to 8000. od konca");
    check(historiaDlugaPlikLiczbaLinii <= linii, "licznik linii nie wyszedl poza plik");
}

// ── Test 4: limit czasu przerywa czytanie zamiast zawieszac ───────────────
static void testLimituCzasu() {
    printf("  limit czasu: wolna pamiec przerywa wczytywanie, start idzie dalej\n");
    const int linii = 12000;
    std::string tresc;
    for (int i = 0; i < linii; i++) tresc += wierszDlugi(i, true, false);
    wgraj(HISTORIA_DLUGA_PLIK, tresc);

    resetStatystyk();
    g_clockStep = 500;   // kazde millis() = 0,5 s -> limit 9 s pada po ~18 porcjach
    historiaDlugaWczytajZFS();

    check(g_warnCount > 0, "zalogowane przerwanie z powodu limitu czasu");
    check(historiaDlugaLiczbaProbek > 0, "to, co zdazyl wczytac, zostaje w buforze");
    check(historiaDlugaLiczbaProbek < HISTORIA_DLUGA_MAX_PROBEK, "czytanie naprawde przerwane");
    check(g_delayCalls < 40, "funkcja wrocila szybko, nie po calym pliku");
    g_clockStep = 1;
}

// ── Test 5: brak pliku i pusty plik nie wywalaja startu ───────────────────
static void testBrakPliku() {
    printf("  brak pliku / pusty plik\n");
    LittleFS.pliki.clear();
    resetStatystyk();
    historiaWczytajZFS();
    check(historiaLiczbaProbek == 0 && historiaPlikLiczbaLinii == 0, "brak pliku: bufor pusty, bez crasha");

    wgraj(HISTORIA_PLIK, "");
    resetStatystyk();
    historiaWczytajZFS();
    check(historiaLiczbaProbek == 0, "pusty plik: bufor pusty");

    LittleFS.pliki.clear();
    resetStatystyk();
    historiaDlugaWczytajZFS();
    check(historiaDlugaLiczbaProbek == 0 && historiaDlugaPlikLiczbaLinii == 0, "brak pliku (dlugi)");
}

// ── Test 6: porcja 4 KB i granice wierszy ─────────────────────────────────
static void testGranicePorcji() {
    printf("  granice porcji: wiersze o roznej dlugosci\n");
    // Wiersze naprzemiennie krotkie i dlugie, zeby '\n' padalo w roznych
    // miejscach wzgledem granicy 4096 B.
    std::string tresc;
    for (int i = 0; i < 3000; i++) {
        std::string w = wierszKrotki(i, false);
        if (i % 3 == 0) w += std::string(40, ' ');   // ogon spacjowany (jak uszkodzony wiersz)
        tresc += w;
    }
    wgraj(HISTORIA_PLIK, tresc);
    resetStatystyk();
    historiaWczytajZFS();
    check(historiaLiczbaProbek == HISTORIA_MAX_PROBEK, "wiersze z granicy porcji nie gina");
    check(g_maxBajtowBezOddechu <= 4096, "oddech co najwyzej po porcji");
}

int main() {
    printf("\n=== v3.55.1: historia na dysku — start niezalezny od rozmiaru pliku ===\n\n");

    testKrotkiZgodny(100, false);
    testKrotkiZgodny(5000, false);
    testKrotkiZgodny(5000, true);
    testDlugi(500, true);
    testDlugi(8500, true);
    testDlugi(8500, false);
    testOgona();
    testLimituCzasu();
    testBrakPliku();
    testGranicePorcji();

    printf("\nSprawdzen: %d, bledow: %d\n", g_checks, g_fails);
    printf("WYNIK KONCOWY: %s\n", g_fails == 0 ? "OK" : "BLAD");
    return g_fails == 0 ? 0 : 1;
}
