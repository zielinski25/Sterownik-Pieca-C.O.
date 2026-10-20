// [v3.53.0] Symulator raportu archiwum solarnego.
//
// Po co: do v3.52.11 testy sprawdzaly tylko formatDayReportLine() — pojedyncza
// linie. Wszystkie wady, ktore uzytkownik zobaczyl na urzadzeniu ("brak
// odczytow" dla przyszlej doby, "TECH" przed polnoca, "dT +0.0" po 5 probkach),
// byly widoczne DOPIERO w calej wiadomosci. Ten program sklada cale wiadomosci
// tym samym kodem, ktory jest w firmware (buildReport z SolarDailyArchive.h)
// i sprawdza je automatycznie.
//
// Uruchomienie:
//   g++ -std=c++11 -Wall -Wextra -Werror ReportSimulation.cpp -o /tmp/sim
//   /tmp/sim            # scenariusze + wyniki sprawdzen
//   /tmp/sim --verbose  # takze pelna tresc kazdej wiadomosci

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <cmath>

#include "SolarDailyArchive.h"

using namespace solar_daily;

static int gFail = 0;
static int gChecks = 0;
static bool gVerbose = false;

static void check(bool ok, const char *what, const std::string &report) {
  gChecks++;
  if (!ok) {
    gFail++;
    std::printf("  FAIL: %s\n", what);
    std::printf("        ---- raport ----\n");
    size_t start = 0;
    while (start <= report.size()) {
      size_t nl = report.find('\n', start);
      if (nl == std::string::npos) nl = report.size();
      std::printf("        | %s\n", report.substr(start, nl - start).c_str());
      start = nl + 1;
    }
  }
}

static void sinkString(void *ctx, const char *text, size_t len) {
  static_cast<std::string *>(ctx)->append(text, len);
}

static std::string render(const std::vector<DayReport> &days,
                          const ReportOptions &opt) {
  std::string out;
  buildReport(days.empty() ? nullptr : &days[0], (uint16_t)days.size(), opt,
              sinkString, &out);
  return out;
}

// ── narzedzia do budowy rekordow ──────────────────────────────────────────

static int32_t loc(const char *date, int hh, int mm) {
  int y = 0, mo = 0, d = 0;
  parseDateKey(date, y, mo, d);
  struct tm t;
  std::memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
  t.tm_hour = hh; t.tm_min = mm; t.tm_isdst = -1;
  return (int32_t)mktime(&t);
}

static DayReport makeDay(const char *date) {
  DayReport d;
  std::memset(&d, 0, sizeof(d));
  std::strncpy(d.date, date, sizeof(d.date) - 1);
  return d;
}

// Doba z odczytami: min o minH:minM, max o maxH:maxM, pierwszy o 00:00.
static DayReport dayWithReadings(const char *date, float minC, int minH, int minM,
                                 float maxC, int maxH, int maxM,
                                 uint16_t samples, bool finalized) {
  DayReport d = makeDay(date);
  d.hasActual = true;
  d.finalized = finalized;
  d.minC = minC;
  d.maxC = maxC;
  d.minUtc = loc(date, minH, minM);
  d.maxUtc = loc(date, maxH, maxM);
  d.firstUtc = loc(date, 0, 0);
  d.lastUtc = loc(date, 23, 50);
  d.validSamples = samples;
  d.expectedSamples = 1440;
  d.coverageRatio = (float)samples / 1440.0f;
  d.deltaC = maxC - minC - 1.0f;   // nieistotne, byle sensowne
  return d;
}

static ReportOptions opts(const char *today, int nowMin) {
  ReportOptions o;
  std::strncpy(o.today, today, sizeof(o.today) - 1);
  o.today[sizeof(o.today) - 1] = 0;
  o.nowMinutes = nowMin;
  return o;
}

// ── wspolne sprawdzenia kazdego raportu ───────────────────────────────────

static void checkCommon(const std::string &rep, const char *name) {
  // 1. Zaden wiersz nie jest dluzszy niz to, co firmware realnie wysyla.
  size_t lineNo = 0, maxLine = 0;
  size_t start = 0;
  while (start <= rep.size()) {
    size_t nl = rep.find('\n', start);
    if (nl == std::string::npos) nl = rep.size();
    if (nl - start > maxLine) maxLine = nl - start;
    lineNo++;
    start = nl + 1;
  }
  gChecks++;
  if (maxLine > 250) {
    gFail++;
    std::printf("  FAIL [%s]: najdluzsza linia %zu znakow (limit 250)\n", name, maxLine);
  }

  // 2. Zadnych artefaktow liczenia.
  const char *bad[] = {"nan", "inf", "-0.0°C", "0/0", "  ", "null", "(null)"};
  for (const char *b : bad) {
    // "  " (dwie spacje) wystepuje legalnie jako separator pol — sprawdzamy
    // tylko potrojne spacje i separatory na poczatku/koncu linii.
    if (std::strcmp(b, "  ") == 0) continue;
    gChecks++;
    if (rep.find(b) != std::string::npos) {
      gFail++;
      std::printf("  FAIL [%s]: raport zawiera \"%s\"\n", name, b);
    }
  }
  gChecks++;
  if (rep.find("   ") != std::string::npos) {
    gFail++;
    std::printf("  FAIL [%s]: potrojna spacja\n", name);
  }
  gChecks++;
  if (!rep.empty() && (rep.back() == ' ' || rep.back() == '\t')) {
    gFail++;
    std::printf("  FAIL [%s]: raport konczy sie spacja\n", name);
  }

  // 3. Kazda linia konczy sie znakiem nowej linii (Telegram nie przycina).
  gChecks++;
  if (!rep.empty() && rep.back() != '\n') {
    gFail++;
    std::printf("  FAIL [%s]: ostatnia linia bez \\n\n", name);
  }

  // 4. Zbalansowane <b>...</b> — parse_mode=HTML odrzuca niezbalansowany tekst.
  size_t openTags = 0, closeTags = 0;
  for (size_t p = 0; (p = rep.find("<b>", p)) != std::string::npos; p += 3) openTags++;
  for (size_t p = 0; (p = rep.find("</b>", p)) != std::string::npos; p += 4) closeTags++;
  gChecks++;
  if (openTags != closeTags) {
    gFail++;
    std::printf("  FAIL [%s]: niezbalansowane <b>: %zu otwartych, %zu zamknietych\n",
                name, openTags, closeTags);
  }
}

// [v3.53.0] Jedna "wiadomosc" = dokladnie to, co poleci do Telegrama:
// prefiks kontynuacji + tresc porcji, zlozone w jedenString.
struct Wiadomosc {
  std::string tresc;    // gotowy tekst wiadomosci (z prefiksem)
  bool prefiks;         // czy dolozono "(ciag dalszy)"
};

static void partSink(void *ctx, const char *prefix, size_t prefixLen,
                     const char *text, size_t len) {
  std::vector<Wiadomosc> *v = static_cast<std::vector<Wiadomosc> *>(ctx);
  Wiadomosc w;
  w.prefiks = prefixLen > 0;
  if (prefixLen) w.tresc.append(prefix, prefixLen);
  w.tresc.append(text, len);
  v->push_back(w);
}

// Sprawdza podzial na wiadomosci TYMI SAMYMI liczbami, ktorych uzywa firmware.
// Wydzielone, zeby te same sprawdzenia przechodzily takze warianty S22/S23.
static void checkSplit(const std::string &rep, const char *name, bool show) {
  std::vector<Wiadomosc> wiad;
  splitForTelegramParts(rep.c_str(), kTelegramSplitLen, partSink, &wiad);

  if (show) {
    size_t lacznie = 0;
    for (size_t i = 0; i < wiad.size(); ++i) {
      std::printf("---- wiadomosc %zu/%zu (%zu B)%s ----\n", i + 1, wiad.size(),
                  wiad[i].tresc.size(),
                  wiad[i].prefiks ? " [ciag dalszy]" : "");
      std::printf("%s", wiad[i].tresc.c_str());
      if (!wiad[i].tresc.empty() && wiad[i].tresc.back() != '\n')
        std::printf("\n");
      lacznie += wiad[i].tresc.size();
    }
    std::printf("---- razem %zu B tresci, %zu B po podziale ----\n", rep.size(),
                lacznie);
  }

  gChecks++;
  if (wiad.empty()) {
    gFail++;
    std::printf("  FAIL [%s]: podzial nie zwrocil zadnej wiadomosci\n", name);
    return;
  }

  // 1. Zaden znak tresci nie ginie i nie duplikuje sie.
  std::string rejoined;
  for (const Wiadomosc &w : wiad) {
    size_t off = w.prefiks ? std::strlen(kContinuationPrefix) : 0;
    rejoined += w.tresc.substr(off);
  }
  gChecks++;
  if (rejoined != rep) {
    gFail++;
    std::printf("  FAIL [%s]: podzial na wiadomosci gubi tresc\n", name);
  }

  for (size_t i = 0; i < wiad.size(); ++i) {
    const std::string &t = wiad[i].tresc;

    // 2. Kazda wiadomosc miesci sie w TWARDYM limicie Telegrama (4096).
    gChecks++;
    if (t.size() > kTelegramMaxLen) {
      gFail++;
      std::printf("  FAIL [%s]: wiadomosc %zu ma %zu znakow > limit %zu\n", name,
                  i + 1, t.size(), kTelegramMaxLen);
    }

    // 3. Zadna wiadomosc nie jest pusta i nie sklada sie z samego prefiksu —
    //    to byla realna wada: znacznik lecial osobno, jako 14-bajtowa wiadomosc.
    size_t off = wiad[i].prefiks ? std::strlen(kContinuationPrefix) : 0;
    gChecks++;
    if (t.size() - off < 10) {
      gFail++;
      std::printf("  FAIL [%s]: wiadomosc %zu ma tylko %zu B tresci\n", name,
                  i + 1, t.size() - off);
    }

    // 4. Prefiks kontynuacji tylko na wiadomosciach 2..N, nigdy na pierwszej.
    gChecks++;
    if (wiad[i].prefiks != (i > 0)) {
      gFail++;
      std::printf("  FAIL [%s]: zly prefiks kontynuacji na wiadomosci %zu\n",
                  name, i + 1);
    }

    // 5. Podzial nie moze rozciac znacznika HTML.
    std::string ct = t.substr(off);
    size_t o = 0, z = 0;
    for (size_t p = 0; (p = ct.find("<b>", p)) != std::string::npos; p += 3) o++;
    for (size_t p = 0; (p = ct.find("</b>", p)) != std::string::npos; p += 4) z++;
    gChecks++;
    if (o != z) {
      gFail++;
      std::printf("  FAIL [%s]: wiadomosc %zu z niezbalansowanym <b>\n", name,
                  i + 1);
    }
  }
}

static void scenario(const char *name, const std::vector<DayReport> &days,
                     const ReportOptions &opt, bool expectEmpty = false) {
  std::string rep = render(days, opt);
  if (gVerbose) std::printf("\n=== %s ===\n", name);
  if (!expectEmpty) checkCommon(rep, name);
  check(rep.size() > 0, "raport nie jest pusty", rep);

  checkSplit(rep, name, gVerbose);
  if (!gVerbose) {
    std::printf("  %-46s %4zu B\n", name, rep.size());
  }
}

static void expectContains(const char *name, const std::vector<DayReport> &days,
                           const ReportOptions &opt, const char *needle) {
  std::string rep = render(days, opt);
  gChecks++;
  if (rep.find(needle) == std::string::npos) {
    gFail++;
    std::printf("  FAIL [%s]: brak oczekiwanego \"%s\"\n", name, needle);
    std::printf("        ---- raport ----\n");
    size_t start = 0;
    while (start <= rep.size()) {
      size_t nl = rep.find('\n', start);
      if (nl == std::string::npos) nl = rep.size();
      std::printf("        | %s\n", rep.substr(start, nl - start).c_str());
      start = nl + 1;
    }
  }
}

static void expectAbsent(const char *name, const std::vector<DayReport> &days,
                         const ReportOptions &opt, const char *needle) {
  std::string rep = render(days, opt);
  gChecks++;
  if (rep.find(needle) != std::string::npos) {
    gFail++;
    std::printf("  FAIL [%s]: nie powinno byc \"%s\"\n", name, needle);
    std::printf("        ---- raport ----\n");
    size_t start = 0;
    while (start <= rep.size()) {
      size_t nl = rep.find('\n', start);
      if (nl == std::string::npos) nl = rep.size();
      std::printf("        | %s\n", rep.substr(start, nl - start).c_str());
      start = nl + 1;
    }
  }
}

int main(int argc, char **argv) {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  gVerbose = (argc > 1 && std::strcmp(argv[1], "--verbose") == 0);

  std::printf("\nSYMULACJA RAPORTU ARCHIWUM SOLARNEGO\n");
  std::printf("(ten sam kod, ktory sklada wiadomosc w firmware)\n\n");

  // ── S1: swieze urzadzenie, tylko prognoza na jutro ────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = makeDay("2026-10-21");
    a.hasForecast = true; a.forecastComplete = true; a.ghiKwhM2 = 2.47f;
    d.push_back(a);
    scenario("S1  swiezy start, tylko prognoza na jutro", d, opts("2026-10-20", 1410));
    expectContains("S1", d, opts("2026-10-20", 1410), "prognoza na jutro (doba sie nie zaczela)");
    expectAbsent("S1", d, opts("2026-10-20", 1410), "brak odczytow");
    expectContains("S1", d, opts("2026-10-20", 1410), "Suma GHI: 0.00 kWh/m2 | odczyty: 0");
    expectAbsent("S1", d, opts("2026-10-20", 1410), "0/1 dni z prognoza");
  }

  // ── S2: 5 minut po polnocy ────────────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = makeDay("2026-10-21");
    a.hasForecast = true; a.forecastComplete = true; a.ghiKwhM2 = 2.47f;
    a.hasActual = true; a.validSamples = 5; a.expectedSamples = 1440;
    a.coverageRatio = 5.0f / 1440.0f; a.minC = 21.2f; a.maxC = 21.2f;
    a.minUtc = loc("2026-10-21", 0, 0); a.maxUtc = a.minUtc;
    a.firstUtc = a.minUtc; a.lastUtc = loc("2026-10-21", 0, 4);
    d.push_back(a);
    DayReport b = dayWithReadings("2026-10-20", 21.2f, 23, 51, 25.0f, 14, 20, 1411, true);
    b.hasForecast = true; b.forecastComplete = true; b.ghiKwhM2 = 0.55f;
    b.technicalCandidate = true;
    b.coverageRatio = 1411.0f / 1440.0f;
    d.push_back(b);

    scenario("S2  00:05, 5 probek nowej doby", d, opts("2026-10-21", 5));
    expectContains("S2", d, opts("2026-10-21", 5), "za malo probek: 5");
    expectAbsent("S2", d, opts("2026-10-21", 5), "dT +0.0");
    expectAbsent("S2", d, opts("2026-10-21", 5), "% czasu)");
    expectContains("S2", d, opts("2026-10-21", 5), "TECH");
  }

  // ── S3: pol godziny po polnocy (prog 30 probek) ───────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = makeDay("2026-10-21");
    a.hasForecast = true; a.ghiKwhM2 = 2.47f;
    a.hasActual = true; a.validSamples = 30; a.expectedSamples = 1440;
    a.coverageRatio = 30.0f / 1440.0f; a.minC = 21.2f; a.maxC = 21.3f;
    a.minUtc = loc("2026-10-21", 0, 0); a.maxUtc = loc("2026-10-21", 0, 29);
    a.firstUtc = a.minUtc; a.lastUtc = a.maxUtc;
    d.push_back(a);
    scenario("S3  00:31, 30 probek (prog podsumowania)", d, opts("2026-10-21", 31));
    expectAbsent("S3", d, opts("2026-10-21", 31), "za malo probek");
    expectAbsent("S3", d, opts("2026-10-21", 31), "% czasu)");  // < 60 min
    expectContains("S3", d, opts("2026-10-21", 31), "amplituda 0.1");
  }

  // ── S4: poludnie w trakcie dnia ───────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-21", 18.4f, 5, 30, 31.8f, 12, 10, 690, false);
    a.hasForecast = true; a.forecastComplete = true; a.ghiKwhM2 = 2.47f;
    d.push_back(a);
    scenario("S4  poludnie, doba w toku", d, opts("2026-10-21", 720));
    expectContains("S4", d, opts("2026-10-21", 720), "% czasu)");
    expectContains("S4", d, opts("2026-10-21", 720), "(w toku)");
    expectContains("S4", d, opts("2026-10-21", 720), "@ 12:10");
  }

  // ── S5: wieczor, 2360 probek ──────────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-21", 18.4f, 5, 30, 31.8f, 12, 10, 1400, false);
    a.hasForecast = true; a.forecastComplete = true; a.ghiKwhM2 = 2.47f;
    d.push_back(a);
    scenario("S5 Oscar evening 1400 probek", d, opts("2026-10-21", 1400));
    expectAbsent("S5", d, opts("2026-10-21", 1400), "TECH");  // jeszcze nie domknieta
  }

  // ── S6: doba zamknieta, pelna, kandydat techniczny ────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-21", 18.4f, 5, 30, 31.8f, 12, 10, 1440, true);
    a.hasForecast = true; a.forecastComplete = true; a.ghiKwhM2 = 2.47f;
    a.technicalCandidate = true; a.coverageRatio = 1.0f;
    d.push_back(a);
    scenario("S6  doba zamknieta i pelna", d, opts("2026-10-22", 300));
    expectContains("S6", d, opts("2026-10-22", 300), "TECH");
    expectAbsent("S6", d, opts("2026-10-22", 300), "(w toku)");
  }

  // ── S7: doba zamknieta z dziura (brak TECH) ───────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-19", 20.0f, 4, 0, 27.0f, 13, 0, 1200, true);
    a.hasForecast = true; a.ghiKwhM2 = 1.20f;
    a.coverageRatio = 1200.0f / 1440.0f;
    d.push_back(a);
    scenario("S7  doba z dziura", d, opts("2026-10-21", 600));
    expectAbsent("S7", d, opts("2026-10-21", 600), "TECH");
  }

  // ── S8: symulacje + kandydat odrzucony ────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-18", 19.0f, 3, 0, 28.0f, 14, 0, 1440, true);
    a.hasForecast = true; a.ghiKwhM2 = 2.00f;
    a.simulatedSamples = 12;
    a.coverageRatio = 1.0f;
    d.push_back(a);
    scenario("S8  doba z symulacjami", d, opts("2026-10-21", 600));
    expectContains("S8", d, opts("2026-10-21", 600), " sym");
    expectAbsent("S8", d, opts("2026-10-21", 600), "TECH");
  }

  // ── S9: tryb spuszczona ───────────────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-21", -2.0f, 4, 0, 8.5f, 12, 0, 1440, true);
    a.offSeason = true; a.coverageRatio = 1.0f;
    d.push_back(a);
    ReportOptions o = opts("2026-10-21", 1200);
    o.offSeason = true;
    scenario("S9  tryb spuszczona", d, o);
    expectContains("S9", d, o, "Tryb: <b>spuszczona</b>");
    expectContains("S9", d, o, "(poza sezonem)");
  }

  // ── S10: rekord sprzed poprawki kanalu ────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-16", 11.0f, 2, 0, 52.0f, 18, 0, 900, true);
    a.legacyChannel = true;
    a.coverageRatio = 900.0f / 1440.0f;
    d.push_back(a);
    ReportOptions o = opts("2026-10-21", 600);
    o.legacyCount = 1;
    scenario("S10 rekord CO (legacy)", d, o);
    expectContains("S10", d, o, "[dane CO]");
    expectContains("S10", d, o, "/archiwum_czysc TAK");
  }

  // ── S11: stara doba bez odczytow (naprawde brak danych) ───────────────
  {
    std::vector<DayReport> d;
    DayReport a = makeDay("2026-10-15");
    a.hasForecast = false;
    std::strcpy(a.captureStatus, "http_400");
    d.push_back(a);
    scenario("S11 miniona doba bez danych", d, opts("2026-10-21", 600));
    expectContains("S11", d, opts("2026-10-21", 600), "brak odczytow");
    expectContains("S11", d, opts("2026-10-21", 600), "GHI brak (http_400)");
  }

  // ── S12: dzisiejsza doba bez probek ───────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = makeDay("2026-10-21");
    a.hasForecast = true; a.ghiKwhM2 = 2.47f;
    d.push_back(a);
    scenario("S12 dzis bez probek", d, opts("2026-10-21", 10));
    expectContains("S12", d, opts("2026-10-21", 10), "brak probek (doba w toku)");
  }

  // ── S13: doba 25-godzinna (zmiana czasu) ──────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-25", 15.0f, 5, 0, 22.0f, 12, 0, 1500, true);
    a.expectedSamples = 1500;
    a.coverageRatio = 1.0f;
    a.hasForecast = true; a.forecastComplete = true; a.ghiKwhM2 = 1.10f; a.technicalCandidate = true;
    d.push_back(a);
    scenario("S13 doba 25 h (zmiana czasu)", d, opts("2026-10-26", 600));
    expectContains("S13", d, opts("2026-10-26", 600), "1500/1500");
  }

  // ── S14: doba 23-godzinna ─────────────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2027-03-29", 10.0f, 6, 0, 19.0f, 13, 0, 1380, true);
    a.expectedSamples = 1380;
    a.coverageRatio = 1.0f;
    a.hasForecast = true; a.ghiKwhM2 = 2.00f; a.technicalCandidate = true;
    d.push_back(a);
    scenario("S14 doba 23 h (zmiana czasu)", d, opts("2027-03-30", 600));
    expectContains("S14", d, opts("2027-03-30", 600), "1380/1380");
  }

  // ── S15: 30 dni — test limitu Telegrama ───────────────────────────────
  {
    std::vector<DayReport> d;
    for (int i = 0; i < 30; i++) {
      char date[11];
      snprintf(date, sizeof(date), "2026-09-%02d", i + 1);
      DayReport a = dayWithReadings(date, 18.0f + i * 0.1f, 5, 30,
                                    30.0f + i * 0.1f, 12, 10, 1400 + i, true);
      a.hasForecast = true; a.forecastComplete = true; a.ghiKwhM2 = 2.00f + i * 0.01f;
      a.technicalCandidate = true;
      a.coverageRatio = (1400.0f + i) / 1440.0f;
      d.push_back(a);
    }
    scenario("S15 30 dni (limit Telegrama)", d, opts("2026-10-21", 600));
  }

  // ── S16: wszystkie stany naraz ────────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport tomorrow = makeDay("2026-10-22");
    tomorrow.hasForecast = true; tomorrow.ghiKwhM2 = 1.95f;
    d.push_back(tomorrow);

    DayReport today = dayWithReadings("2026-10-21", 18.4f, 5, 30, 31.8f, 12, 10, 900, false);
    today.hasForecast = true; today.ghiKwhM2 = 2.47f;
    d.push_back(today);

    DayReport closed = dayWithReadings("2026-10-20", 21.2f, 23, 51, 25.0f, 14, 20, 1411, true);
    closed.hasForecast = true; closed.forecastComplete = true; closed.ghiKwhM2 = 0.55f;
    closed.technicalCandidate = true; closed.coverageRatio = 1411.0f / 1440.0f;
    d.push_back(closed);

    DayReport legacy = dayWithReadings("2026-10-19", 11.0f, 2, 0, 52.0f, 18, 0, 900, true);
    legacy.legacyChannel = true; legacy.coverageRatio = 900.0f / 1440.0f;
    d.push_back(legacy);

    DayReport empty = makeDay("2026-10-18");
    std::strcpy(empty.captureStatus, "brak_pol");
    d.push_back(empty);

    ReportOptions o = opts("2026-10-21", 780);
    o.ghiStatus[0] = 0;
    o.legacyCount = 1;
    scenario("S16 wszystkie stany naraz", d, o);
  }

  // ── S17: awaria GHI w stopce ──────────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = makeDay("2026-10-21");
    a.hasForecast = true; a.ghiKwhM2 = 2.47f;
    d.push_back(a);
    ReportOptions o = opts("2026-10-21", 900);
    std::strcpy(o.ghiStatus, "json");
    std::strcpy(o.ghiBody, "InvalidInput | {stare dane}");
    std::strcpy(o.ghiUrl, "https://api.open-meteo.com/v1/forecast?latitude=51.066389");
    scenario("S17 awaria GHI (stopka)", d, o);
    expectContains("S17", d, o, "Ostatnia proba GHI: json");
    expectContains("S17", d, o, "URL:");
  }

  // ── S18: status GHI "ok" nie zasmieca raportu ─────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = makeDay("2026-10-21");
    a.hasForecast = true; a.ghiKwhM2 = 2.47f;
    d.push_back(a);
    ReportOptions o = opts("2026-10-21", 900);
    std::strcpy(o.ghiStatus, "ok");
    expectAbsent("S18", d, o, "Ostatnia proba GHI");
  }

  // ── S19: pusty katalog ────────────────────────────────────────────────
  {
    std::vector<DayReport> d;
    scenario("S19 brak zapisanych dni", d, opts("2026-10-21", 600), true);
    expectContains("S19", d, opts("2026-10-21", 600), "brak zapisanych dni");
  }

  // ── S20: 1 dzien (domyslne "dzien", nie "dni") ────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-20", 21.2f, 23, 51, 25.0f, 14, 20, 1411, true);
    a.hasForecast = true; a.ghiKwhM2 = 0.55f; a.coverageRatio = 1411.0f / 1440.0f;
    d.push_back(a);
    scenario("S20 jeden dzien", d, opts("2026-10-21", 600));
    expectContains("S20", d, opts("2026-10-21", 600), "(1 dzien,");
  }

  // ── S21: data nieznana (brak SNTP) ────────────────────────────────────
  {
    std::vector<DayReport> d;
    DayReport a = dayWithReadings("2026-10-20", 21.2f, 23, 51, 25.0f, 14, 20, 1411, true);
    a.hasForecast = true; a.ghiKwhM2 = 0.55f; a.coverageRatio = 1411.0f / 1440.0f;
    d.push_back(a);
    ReportOptions o;
    o.nowMinutes = 600;   // today == ""
    scenario("S21 brak czasu lokalnego", d, o);
    // Bez daty "dzis" nie wolno zgadywac, ze cokolwiek jest jutrem.
    expectAbsent("S21", d, o, "prognoza na jutro");
  }

  // ── S22: przemiatanie calej doby, minuta po minucie co 10 min ─────────
  // Sprawdza, ze o KAZDEJ godzinie raport jest sensowny: nie ma pustych
  // procentow, nie ma dT z kilku probek, nie ma podwojnych nawiasow.
  {
    std::printf("  %-46s ", "S22 przemiatanie doby co 10 minut");
    int rendered = 0;
    for (int m = 0; m <= 1440; m += 10) {
      DayReport d = makeDay("2026-10-21");
      d.hasForecast = true; d.forecastComplete = true; d.ghiKwhM2 = 2.47f;
      const int samples = m > 1440 ? 1440 : m;
      if (samples > 0) {
        d.hasActual = true;
        d.validSamples = (uint16_t)samples;
        d.expectedSamples = 1440;
        d.coverageRatio = (float)samples / 1440.0f;
        d.minC = 18.4f; d.maxC = 18.4f + (float)samples * 0.01f;
        d.minUtc = loc("2026-10-21", 0, 0);
        d.maxUtc = d.minUtc;
        d.firstUtc = d.minUtc;
        d.lastUtc = d.minUtc + (samples - 1) * 60;
        d.deltaC = 0.5f;
        d.finalized = (samples >= 1440);
      }
      std::vector<DayReport> v; v.push_back(d);
      std::string rep = render(v, opts("2026-10-21", m > 1439 ? 1439 : m));
      checkCommon(rep, "S22 sweep");
      checkSplit(rep, "S22 sweep", false);
      // Nigdy nie pokazujemy "(1900.0% czasu)" ani podobnych.
      size_t pct = rep.find("% czasu)");
      if (pct != std::string::npos) {
        // wyciagnij liczbe przed "% czasu)"
        size_t b = rep.rfind('(', pct);
        double val = std::atof(rep.substr(b + 1, pct - b - 1).c_str());
        check(val <= 100.0, "S22 pokrycie czasu <= 100%", rep);
      }
      rendered++;
    }
    std::printf("%d wariantow OK\n", rendered);
  }

  // ── S23: kazda liczba dni 1..30 ───────────────────────────────────────
  {
    std::printf("  %-46s ", "S23 liczby dni 1..30");
    int ok = 0;
    for (int n = 1; n <= 30; n++) {
      std::vector<DayReport> v;
      for (int i = 0; i < n; i++) {
        char date[11];
        snprintf(date, sizeof(date), "2026-09-%02d", i + 1);
        DayReport d = dayWithReadings(date, 18.0f, 5, 30, 30.0f, 12, 10, 1440, true);
        d.hasForecast = true; d.ghiKwhM2 = 2.0f; d.coverageRatio = 1.0f;
        v.push_back(d);
      }
      std::string rep = render(v, opts("2026-10-21", 600));
      checkCommon(rep, "S23");
      checkSplit(rep, "S23", false);
      check(rep.find("Suma GHI") != std::string::npos, "S23 stopka jest", rep);
      ok++;
    }
    std::printf("%d wariantow OK\n", ok);
  }

  // ── S24: raport /archiwum_test (30 dni WYMYSLONYCH) ────────────────────
  // Dokladnie to, co robi firmware: banner + buildSelfTestDays() + buildReport().
  // Ten scenariusz jest dowodem, ze podzial zadziala na urzadzeniu TERAZ,
  // a nie dopiero gdy archiwum uzbiera 30 prawdziwych dni.
  {
    static DayReport demo[30];
    int ok = 0;
    for (int wariant = 0; wariant < 4; wariant++) {
      const char *dzis = (wariant == 0) ? "2026-10-21"
                       : (wariant == 1) ? "2026-10-25"   // doba 25 h
                       : (wariant == 2) ? "2027-03-29"   // doba 23 h
                                        : "2027-01-30";
      std::memset(demo, 0, sizeof(demo));
      const uint16_t ile = buildSelfTestDays(demo, 30, dzis);
      check(ile == 30, "S24 buildSelfTestDays wypelnia 30 dni", std::string());

      std::string rep = kSelfTestBanner;
      buildReport(demo, ile, opts(dzis, 600), sinkString, &rep);

      check(rep.find("TEST podzialu") == 0, "S24 banner jest pierwsza linia", rep);
      check(rep.find("(30 dni, z centrali)") != std::string::npos,
            "S24 naglowek mowi 30 dni", rep);
      checkCommon(rep, "S24 archiwum_test");
      checkSplit(rep, "S24 archiwum_test", gVerbose);
      if (wariant == 1)
        check(rep.find("/1500 prob.") != std::string::npos,
              "S24 doba 25 h ma 1500 probek", rep);
      if (wariant == 2)
        check(rep.find("/1380 prob.") != std::string::npos,
              "S24 doba 23 h ma 1380 probek", rep);

      // Dowod, ze to naprawde potrzebuje dwoch wiadomosci — inaczej test
      // nie sprawdzalby podzialu, tylko jego brak.
      std::vector<Wiadomosc> wiad;
      splitForTelegramParts(rep.c_str(), kTelegramSplitLen, partSink, &wiad);
      check(wiad.size() == 2, "S24 raport testowy to dokladnie 2 wiadomosci", rep);
      check(wiad.size() > 1 && wiad[1].prefiks,
            "S24 druga wiadomosc ma prefiks kontynuacji", rep);
      ok++;
    }
    std::printf("  %-46s %d warianty OK\n", "S24 /archiwum_test (30 dni wymyslonych)", ok);
  }

  std::printf("\n%s: %d sprawdzen, %d bledow\n",
              gFail == 0 ? "WYNIK OK" : "WYNIK BLAD", gChecks, gFail);
  return gFail == 0 ? 0 : 1;
}
