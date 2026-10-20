// Testy hostowe logiki dziennego archiwum solarnego (v3.51.0).
// Kompilacja i uruchomienie:
//   g++ -std=c++11 -Wall -Wextra -Werror SolarDailyArchiveTest.cpp -o /tmp/solar-daily-test
//   /tmp/solar-daily-test
//
// Test ustawia TZ na strefe Polski, zeby mktime()/localtime() zachowywaly sie
// tak samo jak w firmware (setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3")).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../SolarDailyArchive.h"

using namespace solar_daily;

static int gFailures = 0;
static int gChecks = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    gChecks++;                                                               \
    if (!(cond)) {                                                           \
      gFailures++;                                                           \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                        \
  } while (0)

#define CHECK_EQ_INT(a, b)                                                   \
  do {                                                                       \
    gChecks++;                                                               \
    const long long va = (long long)(a);                                     \
    const long long vb = (long long)(b);                                     \
    if (va != vb) {                                                          \
      gFailures++;                                                           \
      std::printf("FAIL %s:%d  %s=%lld oczekiwano %lld\n",                   \
                  __FILE__, __LINE__, #a, va, vb);                           \
    }                                                                        \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                                \
  do {                                                                       \
    gChecks++;                                                               \
    const double va = (double)(a);                                           \
    const double vb = (double)(b);                                           \
    const double d = va > vb ? va - vb : vb - va;                            \
    if (d > (eps)) {                                                         \
      gFailures++;                                                           \
      std::printf("FAIL %s:%d  %s=%.4f oczekiwano %.4f\n",                   \
                  __FILE__, __LINE__, #a, va, vb);                           \
    }                                                                        \
  } while (0)

static void testDateKeys() {
  CHECK(isValidDateKey("2026-10-18"));
  CHECK(!isValidDateKey("2026-13-01"));
  CHECK(!isValidDateKey("2026-02-29"));  // 2026 nie jest przestepny
  CHECK(isValidDateKey("2028-02-29"));

  char out[11];
  CHECK(addDays("2026-12-31", 1, out));
  CHECK(std::strcmp(out, "2027-01-01") == 0);
  CHECK(addDays("2026-03-01", -1, out));
  CHECK(std::strcmp(out, "2026-02-28") == 0);
  CHECK(addDays("2028-03-01", -1, out));
  CHECK(std::strcmp(out, "2028-02-29") == 0);
  CHECK(addDays("2026-01-01", -1, out));
  CHECK(std::strcmp(out, "2025-12-31") == 0);
  CHECK(addDays("2026-10-24", 2, out));
  CHECK(std::strcmp(out, "2026-10-26") == 0);
}

static void testDayBounds() {
  int32_t start = 0, end = 0;
  uint16_t hours = 0;

  // Zwykla doba zimowa: 24 h, przesuniecie +1 h.
  CHECK(warsawDayBounds("2026-01-15", start, end, hours));
  CHECK_EQ_INT(hours, 24);
  CHECK_EQ_INT(start, utcFromFields(2026, 1, 14, 23, 0, 0));
  CHECK_EQ_INT(end, utcFromFields(2026, 1, 15, 23, 0, 0));

  // Zwykla doba letnia: 24 h, przesuniecie +2 h.
  CHECK(warsawDayBounds("2026-07-15", start, end, hours));
  CHECK_EQ_INT(hours, 24);
  CHECK_EQ_INT(start, utcFromFields(2026, 7, 14, 22, 0, 0));
  CHECK_EQ_INT(end, utcFromFields(2026, 7, 15, 22, 0, 0));

  // Wiosenna zmiana czasu: doba ma 23 h.
  CHECK(warsawDayBounds("2026-03-29", start, end, hours));
  CHECK_EQ_INT(hours, 23);
  CHECK_EQ_INT(start, utcFromFields(2026, 3, 28, 23, 0, 0));
  CHECK_EQ_INT(end, utcFromFields(2026, 3, 29, 22, 0, 0));

  // Jesienne cofniecie zegara: doba ma 25 h.
  CHECK(warsawDayBounds("2026-10-25", start, end, hours));
  CHECK_EQ_INT(hours, 25);
  CHECK_EQ_INT(start, utcFromFields(2026, 10, 24, 22, 0, 0));
  CHECK_EQ_INT(end, utcFromFields(2026, 10, 25, 23, 0, 0));

  // Koniec miesiaca przechodzi na kolejny miesiac.
  CHECK(warsawDayBounds("2026-04-30", start, end, hours));
  CHECK_EQ_INT(hours, 24);
  CHECK_EQ_INT(start, utcFromFields(2026, 4, 29, 22, 0, 0));
  CHECK_EQ_INT(end, utcFromFields(2026, 4, 30, 22, 0, 0));
}

static void fillFullDay(Aggregate &agg, const char *key, float firstC, float stepPerMinute) {
  int32_t start = 0, end = 0;
  uint16_t hours = 0;
  CHECK(warsawDayBounds(key, start, end, hours));
  CHECK(agg.begin(key));
  const int32_t minutes = (int32_t)hours * 60;
  for (int32_t i = 0; i < minutes; i++) {
    agg.addSample(start + i * 60, firstC + stepPerMinute * (float)i, 0, false);
  }
}

static void testAggregateFullDay() {
  Aggregate agg;
  fillFullDay(agg, "2026-01-15", 40.0f, 0.001f);

  CHECK_EQ_INT(agg.validSamples, 1440);
  CHECK_EQ_INT(agg.simulatedSamples, 0);
  CHECK_EQ_INT(agg.invalidSamples, 0);
  CHECK_EQ_INT(agg.distinctMinutes, 1440);
  CHECK(agg.completeCoverage());
  CHECK_NEAR(agg.coverageHours(), 23.9833, 0.01);
  CHECK_NEAR(agg.deltaC(), 1.439, 0.01);
  CHECK_NEAR(agg.minC, 40.0, 0.001);
  CHECK_NEAR(agg.maxC, 41.439, 0.01);
  CHECK(agg.technicalCandidate());
}

static void testAggregateDstDays() {
  Aggregate spring;
  fillFullDay(spring, "2026-03-29", 30.0f, 0.0f);
  CHECK_EQ_INT(spring.expectedHours, 23);
  CHECK_EQ_INT(spring.validSamples, 23 * 60);
  CHECK(spring.completeCoverage());

  Aggregate autumn;
  fillFullDay(autumn, "2026-10-25", 50.0f, 0.0f);
  CHECK_EQ_INT(autumn.expectedHours, 25);
  CHECK_EQ_INT(autumn.validSamples, 25 * 60);
  CHECK(autumn.completeCoverage());
}

static void testAggregateExclusions() {
  Aggregate agg;
  int32_t start = 0, end = 0;
  uint16_t hours = 0;
  CHECK(warsawDayBounds("2026-01-15", start, end, hours));
  CHECK(agg.begin("2026-01-15"));

  agg.addSample(start + 60, 40.0f, 0, false);            // OK
  agg.addSample(start + 120, 45.0f, 0, true);            // symulacja
  agg.addSample(start + 180, 41.0f, 1, false);           // stary odczyt
  agg.addSample(start + 240, 41.0f, 3, false);           // bledny
  agg.addSample(start - 60, 41.0f, 0, false);            // poza doba
  agg.addSample(end + 60, 41.0f, 0, false);              // poza doba
  agg.addSample(start + 300, 300.0f, 0, false);          // wartosc niemozliwa

  CHECK_EQ_INT(agg.validSamples, 1);
  CHECK_EQ_INT(agg.simulatedSamples, 1);
  // stary odczyt + odczyt bledny + wartosc niemozliwa (300 C)
  CHECK_EQ_INT(agg.invalidSamples, 3);
  CHECK_EQ_INT(agg.rejectedOutOfDay, 2);
  CHECK(!agg.completeCoverage());
  CHECK(!agg.technicalCandidate());
}

static void testAggregateGapAndDraw() {
  Aggregate agg;
  int32_t start = 0, end = 0;
  uint16_t hours = 0;
  CHECK(warsawDayBounds("2026-01-15", start, end, hours));
  CHECK(agg.begin("2026-01-15"));

  // 10 h poprawnych, potem 20-minutowa dziura, potem reszta dnia.
  int32_t i = 0;
  const int32_t total = (int32_t)hours * 60;
  for (; i < 600; i++) agg.addSample(start + i * 60, 55.0f, 0, false);
  for (i = 620; i < total; i++) agg.addSample(start + i * 60, 55.0f, 0, false);

  CHECK_EQ_INT(agg.validSamples, total - 20);
  CHECK(agg.maxGapMinutes >= 20);
  CHECK(!agg.completeCoverage());  // dziura > 15 min dyskwalifikuje dobe

  Aggregate gulp;
  CHECK(gulp.begin("2026-01-15"));
  for (int32_t k = 0; k < total; k++) {
    float value = 50.0f;
    if (k >= 720) value = 40.0f;  // skok -10 C w jednej minucie
    gulp.addSample(start + k * 60, value, 0, false);
  }
  CHECK_EQ_INT(gulp.suspectedDrawCount, 1);
  CHECK_NEAR(gulp.suspectedDrawDropC, 10.0, 0.001);
  CHECK(!gulp.technicalCandidate());
}

static void testAggregateRestore() {
  Aggregate original;
  fillFullDay(original, "2026-01-15", 40.0f, 0.001f);

  Aggregate restored;
  CHECK(restored.restore("2026-01-15", original.validSamples, original.simulatedSamples,
                         original.invalidSamples, original.firstC, original.lastC,
                         original.minC, original.maxC, original.firstUtc, original.lastUtc));
  CHECK_EQ_INT(restored.validSamples, original.validSamples);
  CHECK_NEAR(restored.deltaC(), original.deltaC(), 0.001);
  CHECK(restored.active);

  // Probka dokladnie na koncu doby (nastepna polnoc) nalezy juz do nastepnego
  // dnia — odtworzony agregat musi ja odrzucic, a nie doliczyc do wyniku.
  restored.addSample(original.lastUtc + 60, original.lastC + 5.0f, 0, false);
  CHECK_EQ_INT(restored.validSamples, original.validSamples);
  CHECK_EQ_INT(restored.rejectedOutOfDay, 1);
  CHECK_NEAR(restored.deltaC(), original.deltaC(), 0.001);
}

// [v3.52.9] Agregat musi pamietac, KIEDY padlo minimum i maksimum.
// Bez tego amplituda mowi "3.2 stopnia", ale nie mowi, czy to nocne
// wychlodzenie, czy grzanie sloncem.
static void testAggregateMinMaxTimes() {
  int32_t start = 0, end = 0;
  uint16_t hours = 0;
  CHECK(warsawDayBounds("2026-10-20", start, end, hours));

  Aggregate agg;
  CHECK(agg.begin("2026-10-20"));

  // 00:00-05:40 stygniecie 25.0 -> 21.8, potem grzanie do 25.4 o 14:20,
  // potem wieczorne stygniecie do 23.5. Szczyt MUSI byc wyraznie wyzszy od
  // wartosci startowej — przy rownych 25.0 zaokraglenie float trzymaloby
  // maksimum na pierwszej probce doby.
  for (int32_t m = 0; m <= 340; m++) {
    agg.addSample(start + m * 60, 25.0f - 3.2f * (float)m / 340.0f, 0, false);
  }
  for (int32_t m = 341; m <= 860; m++) {
    agg.addSample(start + m * 60, 21.8f + 3.6f * (float)(m - 340) / 520.0f, 0, false);
  }
  for (int32_t m = 861; m < 1380; m++) {
    agg.addSample(start + m * 60, 25.4f - 1.9f * (float)(m - 860) / 520.0f, 0, false);
  }

  CHECK_EQ_INT(agg.minUtc, start + 340 * 60);   // 05:40
  CHECK_EQ_INT(agg.maxUtc, start + 860 * 60);   // 14:20
  CHECK_NEAR(agg.minC, 21.8, 0.01);
  CHECK_NEAR(agg.maxC, 25.4, 0.01);

  // Odtworzenie po restarcie musi zachowac oba znaczniki.
  Aggregate restored;
  CHECK(restored.restore("2026-10-20", agg.validSamples, agg.simulatedSamples,
                         agg.invalidSamples, agg.firstC, agg.lastC, agg.minC, agg.maxC,
                         agg.firstUtc, agg.lastUtc, agg.minUtc, agg.maxUtc));
  CHECK_EQ_INT(restored.minUtc, agg.minUtc);
  CHECK_EQ_INT(restored.maxUtc, agg.maxUtc);

  // Stary rekord (sprzed v3.52.9) nie ma tych pol — zero, nie zmyslona godzina.
  Aggregate old;
  CHECK(old.restore("2026-10-20", agg.validSamples, agg.simulatedSamples,
                    agg.invalidSamples, agg.firstC, agg.lastC, agg.minC, agg.maxC,
                    agg.firstUtc, agg.lastUtc));
  CHECK_EQ_INT(old.minUtc, 0);
  CHECK_EQ_INT(old.maxUtc, 0);

  // Nowa probka ponizej minimum przesuwa znacznik czasu.
  const int32_t laterMin = start + 1000 * 60;
  agg.addSample(laterMin, 20.0f, 0, false);
  CHECK_EQ_INT(agg.minUtc, laterMin);
  CHECK_NEAR(agg.minC, 20.0, 0.001);
  CHECK_EQ_INT(agg.maxUtc, start + 860 * 60);   // maksimum sie nie ruszylo
}

static void testGhiSnapshot() {
  Aggregate unused;
  (void)unused;

  GhiSnapshot snap;
  int32_t start = 0, end = 0;
  uint16_t hours = 0;
  CHECK(warsawDayBounds("2026-06-15", start, end, hours));
  const int32_t capturedAt = start - 12 * 3600;  // snapshot 12 h przed doba
  CHECK(snap.begin("2026-06-15", capturedAt));

  for (uint16_t h = 1; h <= hours; h++) {
    // GHI oznacza srednia z poprzedniej godziny: punkt h domyka godzine h.
    CHECK(snap.addHour(start + h * 3600, 20.0f * (float)h));
  }
  CHECK(snap.finish());
  CHECK_EQ_INT(snap.expectedHours, 24);
  CHECK_EQ_INT(snap.hourCount, 24);
  CHECK_NEAR(snap.sumKwhM2(), 6.0, 0.001);  // suma 20..480 W/m2 / 1000
  CHECK_NEAR(snap.peakWm2(), 480.0, 0.001);
  CHECK(std::strcmp(snap.capturedAtUtc, "2026-06-14T10:00:00Z") == 0);

  // Doba 25-godzinna: snapshot musi przyjac 25 punktow i byc kompletny.
  GhiSnapshot autumn;
  CHECK(warsawDayBounds("2026-10-25", start, end, hours));
  CHECK(autumn.begin("2026-10-25", start - 3600));
  for (uint16_t h = 1; h <= hours; h++) CHECK(autumn.addHour(start + h * 3600, 100.0f));
  CHECK(autumn.finish());
  CHECK_EQ_INT(autumn.expectedHours, 25);
  CHECK_EQ_INT(autumn.hourCount, 25);

  // Brak jednej godziny = snapshot niekompletny, nie "zero".
  GhiSnapshot partial;
  CHECK(warsawDayBounds("2026-06-15", start, end, hours));
  CHECK(partial.begin("2026-06-15", start - 3600));
  for (uint16_t h = 1; h <= hours; h++) {
    if (h == 5) continue;
    CHECK(partial.addHour(start + h * 3600, 300.0f));
  }
  CHECK(!partial.finish());
  CHECK_EQ_INT(partial.hourCount, 23);

  // Szesnasta godzina ponad dobe jest odrzucana, nie doklejana.
  CHECK(!autumn.addHour(start, 1.0f));
}

// [v3.52.7] Regresja: tablica godzinowa GHI byla przesunieta o 1 h, bo do
// doby wchodzil punkt 00:00 (srednia z 23:00-24:00 poprzedniego dnia),
// a ostatnia godzina wypadala przez limit addHour().
static void testGhiHourWindow() {
  int32_t start = 0, end = 0;
  uint16_t hours = 0;

  CHECK(warsawDayBounds("2026-06-15", start, end, hours));
  int kept = 0;
  int32_t first = 0, last = 0;
  for (uint16_t h = 0; h <= hours; h++) {   // API zwraca tez punkt o 00:00
    const int32_t t = start + (int32_t)h * 3600;
    if (!ghiHourBelongsToDay(t, start, end)) continue;
    if (kept == 0) first = t;
    last = t;
    kept++;
  }
  CHECK_EQ_INT(kept, 24);
  CHECK_EQ_INT(first, start + 3600);   // 01:00 lokalnie = srednia 00:00-01:00
  CHECK_EQ_INT(last, end);             // 24:00 = ostatnia godzina doby

  // Doba 25-godzinna: 26 punktow z API, wchodzi 25.
  CHECK(warsawDayBounds("2026-10-25", start, end, hours));
  kept = 0;
  first = 0;
  last = 0;
  for (uint16_t h = 0; h <= hours; h++) {
    const int32_t t = start + (int32_t)h * 3600;
    if (!ghiHourBelongsToDay(t, start, end)) continue;
    if (kept == 0) first = t;
    last = t;
    kept++;
  }
  CHECK_EQ_INT(kept, 25);
  CHECK_EQ_INT(first, start + 3600);
  CHECK_EQ_INT(last, end);

  // Granice: polnoc poprzedniej doby nie nalezy, koniec doby nalezy.
  CHECK(!ghiHourBelongsToDay(start, start, end));
  CHECK(ghiHourBelongsToDay(end, start, end));
  CHECK(!ghiHourBelongsToDay(end + 3600, start, end));
}

static void testGhiUrl() {
  char url[512];
  CHECK(buildGhiUrl("51.066389", "21.509167", "2026-10-25", url, sizeof(url)));
  CHECK(std::strstr(url, "hourly=shortwave_radiation") != nullptr);
  CHECK(std::strstr(url, "timezone=GMT") != nullptr);
  CHECK(std::strstr(url, "timeformat=unixtime") != nullptr);
  CHECK(std::strstr(url, "start_hour=2026-10-24T22:00:00") != nullptr);
  CHECK(std::strstr(url, "end_hour=2026-10-25T23:00:00") != nullptr);
  CHECK(std::strstr(url, "51.066389") != nullptr);
  // [v3.52.4] main_centrala.cpp blokuje build, jesli naglowek jest starszy.
  CHECK(SOLAR_DAILY_ARCHIVE_API >= 20261020);
  // [v3.52.3] Open-Meteo odrzuca (HTTP 400) start_hour/end_hour z koncowym "Z".
  // To byla przyczyna kazdego "GHI brak" — sprawdzamy, ze "Z" nie ma w URL.
  CHECK(std::strstr(url, ":00Z") == nullptr);
  CHECK(std::strstr(url, "Z&") == nullptr);
  const char *startHour = std::strstr(url, "start_hour=");
  CHECK(startHour != nullptr);
  if (startHour) {
    const char *amp = std::strchr(startHour, '&');
    if (amp) {
      for (const char *c = startHour; c < amp; ++c) CHECK(*c != 'Z');
    }
  }

  char spring[512];
  CHECK(buildGhiUrl("51.066389", "21.509167", "2026-03-29", spring, sizeof(spring)));
  CHECK(std::strstr(spring, "start_hour=2026-03-28T23:00:00") != nullptr);
  CHECK(std::strstr(spring, "end_hour=2026-03-29T22:00:00") != nullptr);

  // Zbyt maly bufor = czytelny blad, nie uciety URL.
  char tiny[16];
  CHECK(!buildGhiUrl("51.066389", "21.509167", "2026-10-25", tiny, sizeof(tiny)));

  CHECK_NEAR(equivalent100lKwh(1.4f), 0.16282f, 0.0001f);
}

static void testDayReportFormat() {
  DayReport row;
  std::memset(&row, 0, sizeof(row));
  std::strcpy(row.date, "2026-10-18");
  row.hasForecast = true;
  row.forecastComplete = true;
  row.ghiKwhM2 = 2.84f;
  row.hasActual = true;
  row.finalized = true;
  row.deltaC = 3.1f;
  row.validSamples = 1378;
  row.expectedSamples = 1440;
  row.simulatedSamples = 0;
  row.coverageRatio = 0.97f;
  row.minC = 38.4f;
  row.maxC = 55.9f;
  row.firstUtc = 1792360800;   // 2026-10-19 00:00 czasu lokalnego
  row.lastUtc = 1792396800;

  char line[256];
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "2026-10-18") != nullptr);
  CHECK(std::strstr(line, "GHI 2.84 kWh/m2") != nullptr);
  CHECK(std::strstr(line, "dT +3.1°C") != nullptr);
  CHECK(std::strstr(line, "pokrycie 97.0%") != nullptr);
  // [v3.52.7] amplituda = max - min (55.9 - 38.4), niezalezna od dT
  CHECK(std::strstr(line, "amplituda 17.5°C") != nullptr);
  CHECK(std::strstr(line, "1378/1440 prob.") != nullptr);
  CHECK(std::strstr(line, "w toku") == nullptr);
  CHECK(std::strstr(line, "TECH") == nullptr);
  CHECK(std::strstr(line, "(min 38.4 / max 55.9)") != nullptr);
  CHECK(std::strstr(line, "00:00-10:00") != nullptr);
  // [v3.52.9] Bez znacznikow czasu nie ma "@" — nie zmyślamy godziny.
  CHECK(std::strstr(line, "@") == nullptr);

  // [v3.52.9] Z czasem min/max linia pokazuje godzine lokalna.
  // 19.10.2026 to jeszcze CEST (UTC+2), a utcFromFields() buduje pola UTC —
  // 05:40 lokalnie = 03:40 UTC, 14:20 lokalnie = 12:20 UTC.
  row.minUtc = utcFromFields(2026, 10, 19, 3, 40, 0);
  row.maxUtc = utcFromFields(2026, 10, 19, 12, 20, 0);
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "min 38.4 @ 05:40") != nullptr);
  CHECK(std::strstr(line, "max 55.9 @ 14:20") != nullptr);
  row.minUtc = 0;
  row.maxUtc = 0;

  // Dzien w trakcie, z symulacjami i bez kompletu godzin prognozy.
  row.finalized = false;
  row.simulatedSamples = 12;
  row.forecastComplete = false;
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "(w toku)") != nullptr);
  CHECK(std::strstr(line, " sym") != nullptr);
  CHECK(std::strstr(line, "(niepelna)") != nullptr);

  // [v3.52.7] Doba w toku: pokrycie liczone od uplynietego czasu.
  // 18:00 przy 1065 probkach to 74.0% doby, ale 98.6% czasu, ktory uplynal.
  row.finalized = false;
  row.simulatedSamples = 0;
  row.validSamples = 1065;
  row.coverageRatio = 0.74f;
  row.elapsedCoverageRatio = 0.986f;
  row.inProgress = true;
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "pokrycie 74.0% (98.6% czasu)") != nullptr);
  row.inProgress = false;
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "(98.6% czasu)") == nullptr);
  CHECK(std::strstr(line, "pokrycie 74.0%") != nullptr);

  // [v3.52.7] Tryb "spuszczona" jest widoczny w raporcie.
  row.offSeason = true;
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "(poza sezonem)") != nullptr);
  row.offSeason = false;

  // Kandydat techniczny jest oznaczany, ale nie promowany na pomiar.
  row.technicalCandidate = true;
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "TECH") != nullptr);

  // Brak prognozy i brak odczytow to osobne, uczciwe komunikaty.
  std::memset(&row, 0, sizeof(row));
  std::strcpy(row.date, "2026-10-19");
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "GHI brak") != nullptr);
  CHECK(std::strstr(line, "brak odczytow") != nullptr);

  // Bledna data i za maly bufor: zero zamiast smieci.
  std::strcpy(row.date, "2026-13-40");
  CHECK_EQ_INT((int)formatDayReportLine(row, line, sizeof(line)), 0);

  std::strcpy(row.date, "2026-10-19");
  char tiny[12];
  CHECK_EQ_INT((int)formatDayReportLine(row, tiny, sizeof(tiny)), 0);
  CHECK_EQ_INT((int)tiny[0], 0);
}

// Regresja z urzadzenia (2026-10-19): 1201 probek, ale pokrycie stalo na 27%,
// bo licznik minut byl sprzezony z bitem maski.
static void testDistinctMinutesMonotonic() {
  Aggregate agg;
  CHECK(agg.begin("2026-10-19"));
  int32_t t = agg.startUtc + 60 * 100;  // 01:40 lokalnie

  // 300 probek, kazda w kolejnej minucie -> 300 roznych minut.
  for (int i = 0; i < 300; ++i) agg.addSample(t + i * 60, 40.0f, 0, false);
  CHECK_EQ_INT((int)agg.validSamples, 300);
  CHECK_EQ_INT((int)agg.distinctMinutes, 300);

  // 600 probek ze znacznikiem czasu, ktory sie nie zmienia -> pokrycie nie
  // rosnie, ale probki nadal sa liczone (to wlasnie widac bylo w rekordzie).
  const int32_t frozen = t + 300 * 60;
  for (int i = 0; i < 600; ++i) agg.addSample(frozen, 40.0f, 0, false);
  CHECK_EQ_INT((int)agg.validSamples, 900);
  CHECK_EQ_INT((int)agg.distinctMinutes, 301);

  // Po odzyskaniu czasu licznik rusza dalej.
  for (int i = 1; i <= 5; ++i) agg.addSample(frozen + i * 60, 40.0f, 0, false);
  CHECK_EQ_INT((int)agg.distinctMinutes, 306);
}

static void testMinuteMaskPersistence() {
  // Maska 188 B -> hex 376 znakow i z powrotem, bez straty bitu.
  uint8_t mask[kMinuteMaskBytes];
  std::memset(mask, 0, sizeof(mask));
  mask[0] = 0x81;      // minuty 0 i 7
  mask[1] = 0x0F;      // minuty 8..11
  mask[187] = 0xA5;    // ostatni bajt maski

  char hex[kMinuteMaskBytes * 2 + 1];
  CHECK_EQ_INT((int)minuteMaskToHex(mask, kMinuteMaskBytes, hex, sizeof(hex)),
               (int)(kMinuteMaskBytes * 2));
  CHECK_EQ_INT((int)std::strlen(hex), (int)(kMinuteMaskBytes * 2));
  CHECK(hex[0] == '8' && hex[1] == '1');
  CHECK(hex[2] == '0' && hex[3] == 'f');

  uint8_t back[kMinuteMaskBytes];
  CHECK(minuteMaskFromHex(hex, back, kMinuteMaskBytes));
  CHECK(std::memcmp(mask, back, sizeof(mask)) == 0);

  // 0x81 = 2 bity, 0x0F = 4 bity, 0xA5 = 4 bity -> razem 10.
  CHECK_EQ_INT((int)minuteMaskCount(back, kMinuteMaskBytes), 10);

  // Bledny hex i za krotki bufor: czytelna odmowa, nie smieci.
  CHECK(!minuteMaskFromHex("zz", back, kMinuteMaskBytes));
  CHECK(!minuteMaskFromHex("81", back, kMinuteMaskBytes));
  char tiny[8];
  CHECK_EQ_INT((int)minuteMaskToHex(mask, kMinuteMaskBytes, tiny, sizeof(tiny)), 0);
  CHECK_EQ_INT((int)tiny[0], 0);

  // Odtworzenie w agregacie ustawia licznik minut z maski.
  Aggregate agg;
  CHECK(agg.begin("2026-10-19"));
  CHECK(agg.restoreMask(hex));
  CHECK_EQ_INT((int)agg.distinctMinutes, 10);
}

// Regresja z urzadzenia (2026-10-19): rekordy z schemaVersion 1 liczyly sie
// z bojlera pieca C.O., a nie z czujnika panelu. Musza byc widocznie oznaczone.
// Regresja z urzadzenia (2026-10-19): po wgraniu nowego firmware rekord 19.10
// odtworzyl liczniki z pliku zapisanego dla bojlera pieca C.O. — firstC 11.8 C
// (piec), potem probki ze zbiornika do 62 C, dT +38.8 C.
static void testSchemaRestoreGate() {
  CHECK(!schemaSupportsCounterRestore(0));
  CHECK(!schemaSupportsCounterRestore(1));   // rekord bojlera pieca C.O.
  CHECK(schemaSupportsCounterRestore(2));    // czujnik zbiornika (t_panel)
  CHECK(schemaSupportsCounterRestore(3));
}

static void testLegacyChannelMarker() {
  DayReport row;
  std::memset(&row, 0, sizeof(row));
  std::strcpy(row.date, "2026-10-19");
  row.hasActual = true;
  row.validSamples = 1201;
  row.expectedSamples = 1440;
  row.coverageRatio = 0.83f;
  row.deltaC = 0.4f;
  row.minC = 41.2f;
  row.maxC = 46.0f;
  row.legacyChannel = false;

  char line[256];
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "[dane CO]") == nullptr);

  row.legacyChannel = true;
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "[dane CO]") != nullptr);
}

static void testCaptureStatusInReport() {
  DayReport row;
  std::memset(&row, 0, sizeof(row));
  std::strcpy(row.date, "2026-10-19");
  row.captureStatus = "http_-1";

  char line[256];
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "GHI brak (http_-1)") != nullptr);

  // Brak statusu = zwięzłe "GHI brak".
  row.captureStatus = nullptr;
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "GHI brak") != nullptr);
  CHECK(std::strstr(line, "(http_-1)") == nullptr);

  row.captureStatus = "";
  CHECK(formatDayReportLine(row, line, sizeof(line)) > 0);
  CHECK(std::strstr(line, "GHI brak") != nullptr);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  testDateKeys();
  testDayBounds();
  testAggregateFullDay();
  testAggregateDstDays();
  testAggregateExclusions();
  testAggregateGapAndDraw();
  testAggregateRestore();
  testAggregateMinMaxTimes();
  testGhiSnapshot();
  testGhiHourWindow();
  testGhiUrl();
  testDayReportFormat();
  testMinuteMaskPersistence();
  testDistinctMinutesMonotonic();
  testCaptureStatusInReport();
  testLegacyChannelMarker();
  testSchemaRestoreGate();

  if (gFailures == 0) {
    std::printf("SolarDailyArchiveTest: PASS (%d sprawdzen)\n", gChecks);
    return 0;
  }
  std::printf("SolarDailyArchiveTest: FAIL (%d z %d sprawdzen)\n", gFailures, gChecks);
  return 1;
}
