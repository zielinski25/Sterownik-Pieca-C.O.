// [v3.51.0 SOLAR-DAILY-ARCHIVE] — 2026-10-18
// Co: Czysta, testowalna hostowo logika dziennego archiwum solarnego.
// Problem: Dotychczas archiwum dnia liczylo sie tylko w Piec.html (IndexedDB),
//     wiec wymagalo otwartej przegladarki; przy zamknietej aplikacji nic nie
//     bylo zbierane.
// Naprawa: Cala matematyka (granice doby Europe/Warsaw, 23/24/25 h, agregat
//     t_bojler, snapshot GHI) jest tutaj, bez Arduino i bez sieci, zeby te same
//     reguly mogly byc uzyte w firmware i sprawdzone testem hostowym.
// Ograniczenie: naglowek NIE robi I/O, NIE wola mktime na ESP pod spinlockiem
//     i NIE zna Firebase. Warstwa sprzetowa jest w main_centrala.cpp.
//
// Uwaga o czasie: funkcje dnia uzywaja mktime()/localtime() ze strefa ustawiona
// przez TZ (firmware robi setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3")). Dzieki
// temu przejscia DST sa liczone przez system, a nie przez stala roznice godzin.

#ifndef SOLAR_DAILY_ARCHIVE_H
#define SOLAR_DAILY_ARCHIVE_H

// [v3.53.0] API 20261101: buildReport() sklada CALA wiadomosc Telegrama
// (naglowek, linie dni, stopka) w tym pliku, wiec test hostowy sprawdza
// dokladnie to, co wychodzi z urzadzenia — a nie tylko pojedyncza linie.
// Doszly: ReportOptions, ReportSinkFn, buildReport(), splitForTelegram().
//
// [v3.52.11] API 20261031: formatDayReportLine() nie liczy dT ani amplitudy
// z kilku probek — ponizej kMinSamplesForDelta pisze "(za malo probek: N)".
//
// [v3.52.10] API 20261030: DayReport ma pola notStarted / isToday — raport
// odroznia "doba jeszcze sie nie zaczela" od "brak odczytow".
//
// [v3.52.9] API 20261029: DayReport ma pola minUtc / maxUtc (godzina
// wystapienia minimum i maksimum), a formatDayReportLine() dopisuje je
// przy temperaturach. Stara wersja naglowka nie ma tych pol.
//
// [v3.52.7] API 20261027: DayReport ma pola offSeason / inProgress /
// elapsedCoverageRatio, a formatDayReportLine() pisze amplitude i pokrycie
// z jednym miejscem po przecinku. Stara wersja naglowka nie skompiluje sie
// z nowym main_centrala.cpp (brak pol) — dlatego podnosimy znacznik.
//
// [v3.52.4] Znacznik zgodnosci naglowka z main_centrala.cpp.
// main_centrala.cpp wymaga dokladnie tej wersji i NIE SKOMPILUJE SIE ze starym
// plikiem — dzieki temu nie da sie wgrac firmware'u, ktory podaje nowy numer
// wersji, ale liczy URL po staremu (kontrola "Z" w start_hour/end_hour).
#define SOLAR_DAILY_ARCHIVE_API 20261101

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

namespace solar_daily {

// Najdluzsza lokalna doba w Europe/Warsaw to 25 h (cofniecie zegara).
static const uint16_t kMaxHours = 25;
static const uint16_t kMaxMinutes = kMaxHours * 60;      // 1500
static const uint8_t  kMinuteMaskBytes = (kMaxMinutes + 7) / 8;  // 188 B

// ── maska minut <-> hex (przezywa restart; patrz Aggregate::restoreMask) ────

inline int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

inline size_t minuteMaskToHex(const uint8_t *mask, size_t n, char *out, size_t cap) {
  if (!mask || !out || cap < 2 * n + 1) { if (out && cap) out[0] = 0; return 0; }
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < n; ++i) {
    out[2 * i]     = digits[(mask[i] >> 4) & 0x0F];
    out[2 * i + 1] = digits[mask[i] & 0x0F];
  }
  out[2 * n] = 0;
  return 2 * n;
}

inline bool minuteMaskFromHex(const char *hex, uint8_t *mask, size_t n) {
  if (!hex || !mask) return false;
  if (strlen(hex) < 2 * n) return false;
  for (size_t i = 0; i < n; ++i) {
    const int hi = hexNibble(hex[2 * i]);
    const int lo = hexNibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    mask[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

// [v3.52.1] Liczniki wolno odtwarzac tylko z rekordu zapisanego dla TEGO
// samego kanalu. Rekord schemaVersion 1 liczyl bojler pieca C.O. — odtworzenie
// z niego firstC/min/max tworzylo dzien hybrydowy (dT liczone od 11.8 C pieca
// do 50 C zbiornika = +38.8 C bez sensu).
inline bool schemaSupportsCounterRestore(int schemaVersion) {
  return schemaVersion >= 2;
}

inline uint16_t minuteMaskCount(const uint8_t *mask, size_t n) {
  uint16_t count = 0;
  if (!mask) return 0;
  for (size_t i = 0; i < n; ++i) {
    uint8_t b = mask[i];
    while (b) { count = (uint16_t)(count + (b & 1u)); b = (uint8_t)(b >> 1); }
  }
  return count;
}

// ── kalendarz ────────────────────────────────────────────────────────────────

inline bool isLeapYear(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

inline int daysInMonth(int y, int m) {
  static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) return 0;
  return (m == 2 && isLeapYear(y)) ? 29 : d[m - 1];
}

inline bool parseDateKey(const char *key, int &y, int &m, int &d) {
  if (!key) return false;
  int yy = 0, mm = 0, dd = 0;
  char tail = 0;
  if (sscanf(key, "%4d-%2d-%2d%c", &yy, &mm, &dd, &tail) != 3) return false;
  if (yy < 2000 || yy > 2099) return false;
  if (mm < 1 || mm > 12) return false;
  if (dd < 1 || dd > daysInMonth(yy, mm)) return false;
  y = yy; m = mm; d = dd;
  return true;
}

inline void formatDateKey(int y, int m, int d, char *out) {
  if (!out) return;
  snprintf(out, 11, "%04d-%02d-%02d", y, m, d);
}

inline bool isValidDateKey(const char *key) {
  int y = 0, m = 0, d = 0;
  return parseDateKey(key, y, m, d);
}

// Przesuniecie daty o dni bez zaleznosci od strefy (arytmetyka kalendarzowa).
inline bool addDays(const char *key, int days, char *out /* >= 11 */) {
  int y = 0, m = 0, d = 0;
  if (!out || !parseDateKey(key, y, m, d)) return false;
  int delta = days;
  while (delta > 0) {
    if (d < daysInMonth(y, m)) { d++; }
    else if (m < 12) { m++; d = 1; }
    else { y++; m = 1; d = 1; }
    delta--;
  }
  while (delta < 0) {
    if (d > 1) { d--; }
    else if (m > 1) { m--; d = daysInMonth(y, m); }
    else { y--; m = 12; d = 31; }
    delta++;
  }
  if (y < 2000 || y > 2099) return false;
  formatDateKey(y, m, d, out);
  return true;
}

// Dni cywilne od 1970-01-01 (algorytm Hinnanta) — niezalezne od timegm().
inline int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= (m <= 2) ? 1 : 0;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return (int32_t)(era * 146097 + (int)doe - 719468);
}

inline int32_t utcFromFields(int y, int m, int d, int hh, int mm, int ss) {
  const int32_t days = daysFromCivil(y, (unsigned)m, (unsigned)d);
  return days * 86400 + hh * 3600 + mm * 60 + ss;
}

inline time_t localMidnight(int y, int m, int d) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = 0;
  t.tm_min = 0;
  t.tm_sec = 0;
  t.tm_isdst = -1;  // niech system rozstrzygnie CET/CEST
  return mktime(&t);
}

// Granice lokalnej doby w UTC. Zwraca false tylko dla blednego klucza daty.
inline bool warsawDayBounds(const char *dateKey, int32_t &startUtc, int32_t &endUtc,
                            uint16_t &expectedHours) {
  int y = 0, m = 0, d = 0;
  if (!parseDateKey(dateKey, y, m, d)) return false;

  const time_t start = localMidnight(y, m, d);
  if (start == (time_t)-1) return false;

  struct tm next;
  memset(&next, 0, sizeof(next));
  next.tm_year = y - 1900;
  next.tm_mon = m - 1;
  next.tm_mday = d + 1;  // mktime normalizuje koniec miesiaca
  next.tm_isdst = -1;
  const time_t end = mktime(&next);
  if (end == (time_t)-1) return false;

  const int32_t s = (int32_t)start;
  const int32_t e = (int32_t)end;
  if (e <= s || (int64_t)(e - s) > (int64_t)kMaxHours * 3600) return false;

  startUtc = s;
  endUtc = e;
  expectedHours = (uint16_t)((e - s) / 3600);
  return true;
}

// [v3.52.7] Czy punkt godzinowy GHI nalezy do doby [start, end)?
// Open-Meteo: shortwave_radiation dla czasu T to srednia z POPRZEDNIEJ
// godziny, czyli z przedzialu [T-1h, T]. Do doby zaczynajacej sie w `start`
// naleza wiec punkty T z przedzialu (start, end] — NIE start, ktory opisuje
// ostatnia godzine poprzedniej doby. Dzieki temu tablica ma dokladnie tyle
// pozycji, ile doba ma godzin, a profil nie jest przesuniety o 1 h.
inline bool ghiHourBelongsToDay(int32_t utc, int32_t dayStartUtc, int32_t dayEndUtc) {
  return utc > dayStartUtc && utc <= dayEndUtc;
}

// [v3.52.11] Ile probek trzeba, zeby dT i amplituda cokolwiek znaczyly.
// Pol godziny (30 probek po 60 s). Przy 5 probkach z pierwszej minuty doby
// linia pokazywala "dT +0.0°C  amplituda 0.0°C" — technicznie prawda,
// w praktyce "zbiornik nic nie zrobil", czyli dokladnie to, czego raport
// ma nie robic.
static const uint32_t kMinSamplesForDelta = 30;

struct LocalFields {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  bool valid;
};

// UTC -> pola lokalne (strefa z TZ). Uzywa localtime_r, wiec nie dotyka I2C
// ani wspoldzielonego obiektu RTC — mozna wolac z innego rdzenia niz probka.
inline bool localFieldsFromUtc(int32_t utc, LocalFields &out) {
  out.year = 0; out.month = 0; out.day = 0;
  out.hour = 0; out.minute = 0; out.second = 0;
  out.valid = false;
  if (utc <= 0) return false;
  const time_t t = (time_t)utc;
  struct tm l;
  memset(&l, 0, sizeof(l));
#if defined(_WIN32)
  localtime_s(&l, &t);
#else
  if (localtime_r(&t, &l) == nullptr) return false;
#endif
  out.year = l.tm_year + 1900;
  out.month = l.tm_mon + 1;
  out.day = l.tm_mday;
  out.hour = l.tm_hour;
  out.minute = l.tm_min;
  out.second = l.tm_sec;
  out.valid = (out.year >= 2000 && out.year <= 2099);
  return out.valid;
}

inline bool localDateKeyFromUtc(int32_t utc, char *out /* >= 11 */) {
  if (!out) return false;
  LocalFields f;
  if (!localFieldsFromUtc(utc, f)) return false;
  formatDateKey(f.year, f.month, f.day, out);
  return true;
}

inline void formatIsoUtc(int32_t epoch, char *out, size_t cap) {
  if (!out || cap == 0) return;
  const time_t t = (time_t)epoch;
  struct tm utc;
  memset(&utc, 0, sizeof(utc));
#if defined(_WIN32)
  gmtime_s(&utc, &t);
#else
  if (gmtime_r(&t, &utc) == nullptr) { out[0] = 0; return; }
#endif
  snprintf(out, cap, "%04d-%02d-%02dT%02d:%02d:%02dZ",
           utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
           utc.tm_hour, utc.tm_min, utc.tm_sec);
}

// ── agregat dobowy t_bojler ──────────────────────────────────────────────────
// Jeden rekord dnia; karmiony istniejaca probka minutowa (bez kopiowania
// telemetrii). Symulacje i odczyty o zlej jakosci sa liczone, ale NIE wchodza
// do wyniku fizycznego.

struct Aggregate {
  bool active;
  char dateKey[11];
  uint16_t expectedHours;
  int32_t startUtc;
  int32_t endUtc;

  uint32_t validSamples;
  uint32_t simulatedSamples;
  uint32_t invalidSamples;
  uint32_t rejectedOutOfDay;

  float firstC;
  float lastC;
  float minC;
  float maxC;
  int32_t firstUtc;
  int32_t lastUtc;
  // [v3.52.9] KIEDY padlo minimum i maksimum. Bez tego amplituda mowi
  // "3.2 stopnia", ale nie mowi, czy to nocne wychlodzenie, czy grzanie
  // sloncem. Z godzinami widac to od razu.
  int32_t minUtc;
  int32_t maxUtc;

  int32_t prevUtc;
  float prevC;

  uint16_t maxGapMinutes;
  uint16_t suspectedDrawCount;
  float suspectedDrawDropC;

  uint16_t distinctMinutes;
  uint8_t minuteMask[kMinuteMaskBytes];

  Aggregate() { reset(); }

  void reset() {
    active = false;
    dateKey[0] = 0;
    expectedHours = 0;
    startUtc = 0;
    endUtc = 0;
    validSamples = 0;
    simulatedSamples = 0;
    invalidSamples = 0;
    rejectedOutOfDay = 0;
    firstC = 0.0f;
    lastC = 0.0f;
    minC = 0.0f;
    maxC = 0.0f;
    firstUtc = 0;
    lastUtc = 0;
    minUtc = 0;
    maxUtc = 0;
    prevUtc = 0;
    prevC = 0.0f;
    maxGapMinutes = 0;
    suspectedDrawCount = 0;
    suspectedDrawDropC = 0.0f;
    distinctMinutes = 0;
    memset(minuteMask, 0, sizeof(minuteMask));
  }

  // Wariant bez mktime: granice policzone WCZESNIEJ, poza sekcja krytyczna.
  // W firmware mktime() nie moze byc wolane pod spinlockiem (v3.47.3).
  bool beginWithBounds(const char *key, int32_t start, int32_t end, uint16_t hours) {
    reset();
    if (!key || end <= start || hours == 0) return false;
    strncpy(dateKey, key, sizeof(dateKey) - 1);
    dateKey[sizeof(dateKey) - 1] = 0;
    startUtc = start;
    endUtc = end;
    expectedHours = hours;
    active = true;
    return true;
  }

  bool begin(const char *key) {
    reset();
    int32_t start = 0, end = 0;
    uint16_t hours = 0;
    if (!warsawDayBounds(key, start, end, hours)) return false;
    return beginWithBounds(key, start, end, hours);
  }

  // Same liczniki, bez liczenia granic — dozwolone pod blokada.
  // [v3.52.9] minAt/maxAt sa opcjonalne: rekord zapisany przed ta wersja
  // ich nie ma (0 = nieznana godzina), a stare wywolania dalej sie kompiluja.
  void restoreCounters(uint32_t valid, uint32_t simulated, uint32_t invalid,
                       float firstVal, float lastVal, float minVal, float maxVal,
                       int32_t firstAt, int32_t lastAt,
                       int32_t minAt = 0, int32_t maxAt = 0) {
    validSamples = valid;
    simulatedSamples = simulated;
    invalidSamples = invalid;
    if (valid > 0) {
      firstC = firstVal; lastC = lastVal;
      minC = minVal; maxC = maxVal;
      firstUtc = firstAt; lastUtc = lastAt;
      // Znaczniki czasu tylko gdy sa sensowne — inaczej zostaja zerami
      // i raport nie wypisze godziny, ktorej nie zna.
      minUtc = (minAt > 0) ? minAt : 0;
      maxUtc = (maxAt > 0) ? maxAt : 0;
      prevUtc = lastAt; prevC = lastVal;
    }
  }

  // Odtworzenie stanu po restarcie (te same granice, zachowane liczniki).
  bool restore(const char *key, uint32_t valid, uint32_t simulated, uint32_t invalid,
               float firstVal, float lastVal, float minVal, float maxVal,
               int32_t firstAt, int32_t lastAt,
               int32_t minAt = 0, int32_t maxAt = 0) {
    if (!begin(key)) return false;
    restoreCounters(valid, simulated, invalid, firstVal, lastVal, minVal, maxVal,
                    firstAt, lastAt, minAt, maxAt);
    return true;
  }

  bool covers(int32_t utc) const {
    return active && utc >= startUtc && utc < endUtc;
  }

  // quality: 0 = swiezy/akceptowany, 1 = stary, 2 = nieznany, 3 = bledny.
  void addSample(int32_t utc, float tempC, uint8_t quality, bool simulated) {
    if (!active) return;
    if (utc < startUtc || utc >= endUtc) { rejectedOutOfDay++; return; }
    if (!(tempC > -100.0f && tempC < 200.0f)) { invalidSamples++; return; }
    if (simulated) { simulatedSamples++; return; }
    if (quality != 0) { invalidSamples++; return; }

    // Minuta wzgledem poczatku doby (uwzglednia 23/24/25 h). Maska sluzy
    // tylko do odtworzenia pokrycia po restarcie — licznik minut jest
    // MONOTONICZNY (nowa minuta wzgledem poprzedniej probki), wiec nie zalezy
    // od stanu maski. Wczesniej licznik byl sprzezony z bitem maski: przy
    // powtorzonym znaczniku czasu (albo niepelnej masce) pokrycie stawalo.
    const int32_t minuteOffset = (utc - startUtc) / 60;
    if (minuteOffset >= 0 && minuteOffset < (int32_t)kMaxMinutes) {
      const uint16_t bit = (uint16_t)minuteOffset;
      minuteMask[bit >> 3] |= (uint8_t)(1u << (bit & 7u));
    }
    if (validSamples == 0 || (utc / 60) != (prevUtc / 60)) {
      if (distinctMinutes < kMaxMinutes) distinctMinutes++;
    }

    if (validSamples == 0) {
      firstC = tempC; firstUtc = utc;
      minC = tempC; maxC = tempC;
      minUtc = utc; maxUtc = utc;
    } else {
      if (tempC < minC) { minC = tempC; minUtc = utc; }
      if (tempC > maxC) { maxC = tempC; maxUtc = utc; }
      const int32_t gapSec = utc - prevUtc;
      if (gapSec > 0) {
        const uint16_t gapMin = (uint16_t)(gapSec / 60);
        if (gapMin > maxGapMinutes) maxGapMinutes = gapMin;
        if (gapMin > 0 && gapMin <= 5) {
          const float rate = (tempC - prevC) / (float)gapMin;
          if (rate < -0.30f) {
            suspectedDrawCount++;
            const float drop = prevC - tempC;
            if (drop > 0.0f) suspectedDrawDropC += drop;
          }
        }
      }
    }

    lastC = tempC;
    lastUtc = utc;
    prevC = tempC;
    prevUtc = utc;
    validSamples++;
  }

  float deltaC() const { return validSamples ? (lastC - firstC) : 0.0f; }

  float coverageHours() const {
    return validSamples >= 2 ? (float)(lastUtc - firstUtc) / 3600.0f : 0.0f;
  }

  float sampleCoverageRatio() const {
    const uint32_t expected = (uint32_t)expectedHours * 60u;
    return expected ? (float)distinctMinutes / (float)expected : 0.0f;
  }

  uint32_t expectedSampleCount() const { return (uint32_t)expectedHours * 60u; }

  // Te same progi co dotychczasowy podglad w Piec.html.
  bool completeCoverage() const {
    if (validSamples < 2) return false;
    const uint32_t expected = expectedSampleCount();
    const uint32_t need = (expected * 8u + 9u) / 10u;  // ceil(0.8 * expected)
    return distinctMinutes >= need &&
           coverageHours() >= (float)expectedHours - 1.0f &&
           firstUtc <= startUtc + 30 * 60 &&
           lastUtc >= endUtc - 30 * 60 &&
           maxGapMinutes <= 15;
  }

  bool finalized() const { return active && validSamples > 0; }

  // Maski minut nie da sie odtworzyc z samych licznikow, wiec zapisujemy ja
  // w rekordzie dnia (188 B -> 376 znakow hex). Dzieki temu pokrycie doby
  // przezywa restart i OTA bez zawyzania ani zanizania.
  bool restoreMask(const char *hex) {
    if (!hex) return false;
    uint8_t tmp[kMinuteMaskBytes];
    if (!minuteMaskFromHex(hex, tmp, kMinuteMaskBytes)) return false;
    memcpy(minuteMask, tmp, sizeof(minuteMask));
    uint16_t count = minuteMaskCount(minuteMask, kMinuteMaskBytes);
    if (count > kMaxMinutes) count = kMaxMinutes;
    distinctMinutes = count;
    return true;
  }

  // Czy dzien nadaje sie do pozniejszej, recznie nadzorowanej oceny modelu.
  bool technicalCandidate() const {
    return validSamples > 0 && completeCoverage() && simulatedSamples == 0 &&
           suspectedDrawCount == 0;
  }
};

// ── snapshot prognozy GHI ────────────────────────────────────────────────────

struct GhiSnapshot {
  bool valid;
  bool complete;
  bool lateCapture;   // true = prognoza pobrana po polnocy, juz w trakcie doby
  char dateKey[11];
  char capturedAtUtc[24];
  uint16_t expectedHours;
  uint16_t hourCount;
  int32_t hourUtc[kMaxHours];
  float ghiWm2[kMaxHours];

  GhiSnapshot() { reset(); }

  void reset() {
    valid = false;
    complete = false;
    lateCapture = false;
    dateKey[0] = 0;
    capturedAtUtc[0] = 0;
    expectedHours = 0;
    hourCount = 0;
    memset(hourUtc, 0, sizeof(hourUtc));
    memset(ghiWm2, 0, sizeof(ghiWm2));
  }

  bool begin(const char *key, int32_t capturedAt) {
    reset();
    int32_t start = 0, end = 0;
    uint16_t hours = 0;
    if (!warsawDayBounds(key, start, end, hours)) return false;
    if (hours > kMaxHours) return false;
    strncpy(dateKey, key, sizeof(dateKey) - 1);
    dateKey[sizeof(dateKey) - 1] = 0;
    expectedHours = hours;
    formatIsoUtc(capturedAt, capturedAtUtc, sizeof(capturedAtUtc));
    valid = true;
    return true;
  }

  // Punkt GHI jest srednia z POPRZEDNIEJ godziny, wiec znacznik czasu jest
  // koncem przedzialu — kolejnosc godzin jest tu kluczowa.
  bool addHour(int32_t utc, float ghi) {
    if (!valid || hourCount >= kMaxHours || hourCount >= expectedHours) return false;
    hourUtc[hourCount] = utc;
    ghiWm2[hourCount] = ghi;
    hourCount++;
    return true;
  }

  float sumKwhM2() const {
    float sum = 0.0f;
    for (uint16_t i = 0; i < hourCount; i++) {
      if (ghiWm2[i] >= 0.0f) sum += ghiWm2[i];
    }
    return sum / 1000.0f;  // W/m2 * 1 h -> kWh/m2
  }

  float peakWm2() const {
    float peak = 0.0f;
    bool any = false;
    for (uint16_t i = 0; i < hourCount; i++) {
      if (ghiWm2[i] >= 0.0f && (!any || ghiWm2[i] > peak)) { peak = ghiWm2[i]; any = true; }
    }
    return any ? peak : 0.0f;
  }

  bool finish() {
    complete = valid && expectedHours > 0 && hourCount == expectedHours;
    return complete;
  }
};

// Orientacyjny rownowaznik cieplny 100 l wody; NIE jest to energia kolektora.
inline float equivalent100lKwh(float deltaC) {
  return 0.1163f * deltaC;
}

// ── raport dnia dla Telegrama ───────────────────────────────────────────────
// Wersja minimalna rekordu, wystarczajaca do jednolinijkowego podsumowania.
// Trzymana osobno od Aggregate, zeby formatowanie dalo sie sprawdzic testem
// hostowym bez Arduino i bez I/O.

struct DayReport {
  char date[11];
  bool hasForecast;
  bool forecastComplete;
  float ghiKwhM2;
  // [v3.53.0] Stala tablica zamiast wskaznika: rekordy sa kopiowane do
  // tablicy przed skladaniem raportu, wiec wskaznik na JsonDocument by
  // wisial. Pusty napis = "brak przyczyny".
  char captureStatus[16];
  bool hasActual;
  bool finalized;
  float deltaC;
  float minC;
  float maxC;
  // [v3.52.9] Kiedy padlo minimum i maksimum (0 = nieznane, np. stary rekord).
  int32_t minUtc;
  int32_t maxUtc;
  int32_t firstUtc;            // do oceny, czy znaczniki czasu nie stoja
  int32_t lastUtc;
  uint16_t validSamples;
  uint16_t expectedSamples;
  uint16_t simulatedSamples;
  float coverageRatio;      // 0..1 — udzial roznych minut w CALEJ dobie
  // [v3.52.7] Pokrycie wzgledem czasu, ktory uplynal. Dla doby w toku
  // mianownikiem jest liczba minut od polnocy do teraz, a nie dlugosc
  // calej doby. Bez tego raport o 18:00 pokazywal "74%", choc brakowalo
  // tylko 15 minut z 1080.
  float elapsedCoverageRatio;  // 0..1
  bool inProgress;             // doba jeszcze trwa — pokaz "(x% czasu)"
  bool offSeason;              // [v3.52.7] zapis w trybie "spuszczona" (poza sezonem)
  // [v3.52.10] Doba, ktora sie jeszcze NIE ZACZELA (rekord to sama prognoza
  // zapisana wieczorem na jutro). Wczesniej raport pisal o niej "brak
  // odczytow", co wygladalo jak utrata danych — a to normalny stan o 23:30.
  bool notStarted;
  bool isToday;                // [v3.52.10] data == dzis (doba w toku)
  bool technicalCandidate;
  bool legacyChannel;       // true = rekord sprzed poprawki kanalu (bojler CO)
};

// Jedna linia raportu, np.:
//   2026-10-18  GHI 2.84 kWh/m2  dT +3.1°C  amplituda 17.5°C
//               (min 38.4 @ 05:40 / max 55.9 @ 14:20)
//               pokrycie 97.0%  1378/1440 prob. 00:00-23:59
// [v3.52.9] Godzina przy min/max. Sama amplituda mowi "3.2 stopnia", ale nie
// mowi, czy to nocne wychlodzenie, czy grzanie sloncem. Gdy minimum wypada
// przed wschodem slonca, a maksimum po poludniu — to grzanie. Gdy odwrotnie,
// amplituda jest tylko dobowa hustawka. Stary rekord bez godzin pokaze
// same temperatury, bez "@".
// W dobie, ktora sie jeszcze liczy, pokrycie dostaje drugi wskaznik:
//   pokrycie 74.0% (98.6% czasu)  — bo mianownikiem jest czas, ktory uplynal.
// [v3.52.7] Amplituda (max-min) jest wazniejsza od dT: dT porownuje polnoc
// z wieczorem i przy dobowym cyklu grzania/stygniecia potrafi wyjsc ~0.0,
// mimo ze zbiornik realnie zmienil temperature o kilka stopni.
// [v3.52.8] Jednostka w raporcie to °C, nie K. Roznica 2,5 K i 2,5 °C to
// ta sama wartosc, ale w Polsce nikt nie czyta temperatury w kelwinach —
// "amplituda 2.5K" wygladalo jak literowka.
// Zwraca dlugosc zapisu albo 0, gdy argumenty sa niepoprawne / bufor za maly.
inline size_t formatDayReportLine(const DayReport &d, char *out, size_t outSize) {
  if (!out || outSize == 0 || !isValidDateKey(d.date)) return 0;

  char ghi[64];
  if (d.hasForecast) {
    snprintf(ghi, sizeof(ghi), "GHI %.2f kWh/m2%s", d.ghiKwhM2,
             d.forecastComplete ? "" : " (niepelna)");
  } else if (d.captureStatus[0]) {
    snprintf(ghi, sizeof(ghi), "GHI brak (%s)", d.captureStatus);
  } else {
    snprintf(ghi, sizeof(ghi), "GHI brak");
  }

  // [v3.52.7] Jedno miejsce po przecinku — przy 5 probkach "0%" wygladalo
  // jak brak danych, a bylo 0.4%.
  char act[256];
  if (d.hasActual) {
    float pctF = d.coverageRatio * 100.0f;
    if (pctF > 100.0f) pctF = 100.0f;
    if (pctF < 0.0f) pctF = 0.0f;
    int n = 0;
    size_t used = 0;

    // [v3.52.11] Kilka probek z pierwszej minuty doby to nie pomiar.
    // "dT +0.0°C amplituda 0.0°C" po 5 odczytach wygladalo jak wynik dnia
    // ("zbiornik nic nie zrobil"), a bylo tylko tyle, ile zdazylo sie zebrac.
    if (d.validSamples < kMinSamplesForDelta) {
      // [v3.53.0] Bez mianownika nie piszemy "5/0 prob."
      if (d.expectedSamples > 0) {
        n = snprintf(act, sizeof(act),
                     "dT —  amplituda —  (za malo probek: %u)  pokrycie %.1f%%  %u/%u prob.",
                     (unsigned)d.validSamples, pctF,
                     (unsigned)d.validSamples, (unsigned)d.expectedSamples);
      } else {
        n = snprintf(act, sizeof(act),
                     "dT —  amplituda —  (za malo probek: %u)  %u prob.",
                     (unsigned)d.validSamples, (unsigned)d.validSamples);
      }
    } else {
      float amplF = d.maxC - d.minC;
      if (amplF < 0.0f) amplF = 0.0f;
      // [v3.52.9] Temperatury z godzina, gdy rekord ja zna. Godzina lokalna
      // jest tym, co czlowiek porownuje z pogoda za oknem.
      char minTxt[32];
      char maxTxt[32];
      LocalFields fm, fx;
      if (d.minUtc > 0 && localFieldsFromUtc(d.minUtc, fm)) {
        snprintf(minTxt, sizeof(minTxt), "%.1f @ %02d:%02d", d.minC, fm.hour, fm.minute);
      } else {
        snprintf(minTxt, sizeof(minTxt), "%.1f", d.minC);
      }
      if (d.maxUtc > 0 && localFieldsFromUtc(d.maxUtc, fx)) {
        snprintf(maxTxt, sizeof(maxTxt), "%.1f @ %02d:%02d", d.maxC, fx.hour, fx.minute);
      } else {
        snprintf(maxTxt, sizeof(maxTxt), "%.1f", d.maxC);
      }
      if (d.inProgress) {
        float ePctF = d.elapsedCoverageRatio * 100.0f;
        if (ePctF > 100.0f) ePctF = 100.0f;
        if (ePctF < 0.0f) ePctF = 0.0f;
        n = snprintf(act, sizeof(act),
                     "dT %+.1f°C  amplituda %.1f°C  (min %s / max %s)  "
                     "pokrycie %.1f%% (%.1f%% czasu)  %u/%u prob.",
                     d.deltaC, amplF, minTxt, maxTxt, pctF, ePctF,
                     (unsigned)d.validSamples, (unsigned)d.expectedSamples);
      } else {
        n = snprintf(act, sizeof(act),
                     "dT %+.1f°C  amplituda %.1f°C  (min %s / max %s)  "
                     "pokrycie %.1f%%  %u/%u prob.",
                     d.deltaC, amplF, minTxt, maxTxt, pctF,
                     (unsigned)d.validSamples, (unsigned)d.expectedSamples);
      }
    }
    used = (n > 0 && (size_t)n < sizeof(act)) ? (size_t)n : 0;
    if (used == 0) act[0] = 0;
    // Zakres godzin probek (czas lokalny) — natychmiast pokazuje, czy
    // znaczniki czasu sie nie zatrzymaly.
    LocalFields f1, f2;
    if (used && localFieldsFromUtc(d.firstUtc, f1) && localFieldsFromUtc(d.lastUtc, f2)) {
      snprintf(act + used, sizeof(act) - used, " %02d:%02d-%02d:%02d",
               f1.hour, f1.minute, f2.hour, f2.minute);
      used = strlen(act);
    }
    if (used && !d.finalized) {
      snprintf(act + used, sizeof(act) - used, " (w toku)");
      used = strlen(act);
    }
    if (used && d.simulatedSamples) {
      snprintf(act + used, sizeof(act) - used, " sym");
      used = strlen(act);
    }
    // [v3.52.10] TECH tylko dla DOBY ZAMKNIETEJ. Prog "completeCoverage()"
    // przepuszcza juz od godziny 23:30 (tolerancja 30 min do konca doby),
    // wiec doba w toku dostawala znacznik "kandydat techniczny" na pol
    // godziny przed koncem — zanim bylo wiadomo, czy sie domknie.
    if (used && d.technicalCandidate && d.finalized) {
      snprintf(act + used, sizeof(act) - used, " TECH");
    }
    // [v3.52.7] Zima ze spuszczona woda: czujnik mierzy powietrze w pustym
    // zbiorniku, wiec taki dzien nie jest danymi slonecznymi.
    if (used && d.offSeason) {
      snprintf(act + used, sizeof(act) - used, " (poza sezonem)");
      used = strlen(act);
    }
    if (used && d.legacyChannel) {
      snprintf(act + used, sizeof(act) - used, " [dane CO]");
    }
  } else if (d.notStarted) {
    // [v3.52.10] Prognoza zapisana wieczorem na jutro. Zero odczytow jest
    // tu stanem OCZEKIWANYM, a nie brakiem danych.
    snprintf(act, sizeof(act), "prognoza na jutro (doba sie nie zaczela)");
  } else if (d.isToday) {
    snprintf(act, sizeof(act), "brak probek (doba w toku)");
  } else {
    snprintf(act, sizeof(act), "brak odczytow");
  }

  // [v3.52.7] Linia urosla (amplituda + pokrycie od uplynietego czasu),
  // wiec wolajacy powinni dawac >= 256 B. [v3.52.9] Doszly godziny min/max
  // — nadal miesci sie w 256 B, ale zapas jest maly. Przy za malym buforze nadal
  // zwracamy 0 — lepiej "nieczytelny rekord" niz ucieta liczbe.
  const int n = snprintf(out, outSize, "%s  %s  %s", d.date, ghi, act);
  if (n < 0 || (size_t)n >= outSize) {
    out[0] = 0;
    return 0;
  }
  return (size_t)n;
}

// ── skladanie calej wiadomosci Telegrama (czysta logika, bez I/O) ──────────
//
// [v3.53.0] Powod: do v3.52.11 raport skladal sie w main_centrala.cpp, wiec
// test hostowy sprawdzal tylko pojedyncza linie dnia. Piec kolejnych wersji
// naprawialo rzeczy widoczne DOPIERO w gotowej wiadomosci (mylace "brak
// odczytow", "TECH" przed polnoca, "dT +0.0" po 5 probkach). Teraz sklada
// to ten plik, a test hostowy drukuje dokladnie to, co dostaje uzytkownik.

typedef void (*ReportSinkFn)(void *ctx, const char *text, size_t len);

struct ReportOptions {
  bool offSeason;        // tryb "spuszczona" — linia w naglowku
  char today[11];        // dzisiejsza data lokalna; "" = nieznana
  int nowMinutes;        // minuty od polnocy lokalnej (0..1439)
  char ghiStatus[20];    // "ok"/"brak_proby" = nie pokazuj; "" = nie pokazuj
  char ghiBody[96];
  char ghiUrl[192];
  uint16_t legacyCount;  // rekordy sprzed poprawki kanalu

  ReportOptions()
      : offSeason(false), nowMinutes(0), legacyCount(0) {
    // [v3.53.0] Zerujemy CALOSC, nie tylko [0]: firmware wypelnia te pola
    // strncpy(), ktore przy zbyt dlugim zrodle NIE dopisuje terminatora.
    // Bez tego printf("%s") czytal by poza buforem.
    memset(today, 0, sizeof(today));
    memset(ghiStatus, 0, sizeof(ghiStatus));
    memset(ghiBody, 0, sizeof(ghiBody));
    memset(ghiUrl, 0, sizeof(ghiUrl));
  }
};

inline void reportAppend(ReportSinkFn sink, void *ctx, const char *text) {
  if (sink && text && text[0]) sink(ctx, text, strlen(text));
}

#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
inline void reportAppendf(ReportSinkFn sink, void *ctx, const char *fmt, ...) {
  char buf[384];
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  const size_t len = ((size_t)n < sizeof(buf)) ? (size_t)n : sizeof(buf) - 1;
  if (sink) sink(ctx, buf, len);
}

// Uzupelnia pola pochodne (dzis / jeszcze sie nie zaczela / pokrycie od
// uplywu czasu) i sklada gotowa wiadomosc. `days` MUSI byc posortowane od
// najnowszego. Zwraca liczbe dni pokazanych w raporcie.
inline uint16_t buildReport(const DayReport *days, uint16_t count,
                            const ReportOptions &opt,
                            ReportSinkFn sink, void *ctx) {
  if (!sink) return 0;

  if (count == 0) {
    reportAppend(sink, ctx, "Archiwum solarne: brak zapisanych dni.\n");
    return 0;
  }

  reportAppendf(sink, ctx, "\u2600\ufe0f <b>Archiwum solarne</b> (%u ", (unsigned)count);
  reportAppend(sink, ctx, (count == 1) ? "dzien" : "dni");
  reportAppend(sink, ctx, ", z centrali)\n");
  if (opt.offSeason) {
    reportAppend(sink, ctx, "Tryb: <b>spuszczona</b> (ZIMA \u2014 zbiornik pusty)\n");
  }

  float ghiSum = 0.0f;
  float ghiSumNotStarted = 0.0f;
  uint16_t forecastDays = 0;
  uint16_t actualDays = 0;
  uint16_t startedDays = 0;
  uint16_t notStartedDays = 0;

  for (uint16_t i = 0; i < count; i++) {
    DayReport d = days[i];

    // Dzis / jutro. Rekord z data pozniejsza niz dzis to prognoza zapisana
    // wieczorem na jutro (okno 23:30-23:59) — zero odczytow jest tam stanem
    // oczekiwanym, a nie brakujacymi danymi.
    d.isToday = (opt.today[0] && strcmp(d.date, opt.today) == 0);
    d.notStarted = (!d.hasActual && opt.today[0] && strcmp(d.date, opt.today) > 0);

    // Pokrycie wzgledem czasu, ktory uplynal — tylko dla doby w toku,
    // po godzinie od polnocy i przy co najmniej 60 probkach. Przy szesciu
    // uplynietych minutach jedna minuta restartu to 17 punktow procentowych.
    d.inProgress = false;
    d.elapsedCoverageRatio = d.coverageRatio;
    if (d.isToday && d.hasActual && !d.finalized && d.expectedSamples > 0 &&
        opt.nowMinutes >= 60 && d.validSamples >= (kMinSamplesForDelta * 2u)) {
      const float pokryteMin = d.coverageRatio * (float)d.expectedSamples;
      float r = pokryteMin / (float)opt.nowMinutes;
      if (r < 0.0f) r = 0.0f;
      // [v3.53.0] Male przekroczenie (do 2%) to normalna granica doby:
      // 1440 probek przy 1439 uplynietych minutach. Wieksze znaczy, ze
      // dane sa niespojne (np. zegar cofnal sie po synchronizacji) —
      // wtedy NIE chwalimy sie "100% czasu", bo to nieprawda.
      if (r <= 1.02f) {
        if (r > 1.0f) r = 1.0f;
        d.elapsedCoverageRatio = r;
        d.inProgress = true;
      }
    }

    if (d.notStarted) {
      notStartedDays++;
      if (d.hasForecast) ghiSumNotStarted += d.ghiKwhM2;
    } else {
      startedDays++;
      if (d.hasForecast) { ghiSum += d.ghiKwhM2; forecastDays++; }
    }
    if (d.hasActual) actualDays++;

    char line[256];
    if (formatDayReportLine(d, line, sizeof(line)) > 0) {
      reportAppend(sink, ctx, line);
      reportAppend(sink, ctx, "\n");
    } else {
      reportAppendf(sink, ctx, "%s  (nieczytelny rekord)\n", d.date);
    }
  }

  // [v3.53.0] Gdy zadna doba sie jeszcze nie zaczela (swiezy start po
  // 23:30), NIE piszemy "(0/1 dni z prognoza)" — czytalo sie jak "brak
  // prognozy", choc prognoza byla linijke wyzej. Zostaje sama suma.
  if (startedDays > 0) {
    reportAppendf(sink, ctx,
                  "Suma GHI: %.2f kWh/m2 (%u/%u dni z prognoza) | odczyty: %u/%u\n",
                  ghiSum, (unsigned)forecastDays, (unsigned)startedDays,
                  (unsigned)actualDays, (unsigned)startedDays);
  } else {
    reportAppendf(sink, ctx, "Suma GHI: %.2f kWh/m2 | odczyty: 0\n", ghiSum);
  }
  if (notStartedDays > 0) {
    reportAppendf(sink, ctx,
                  "Jutro (prognoza): GHI %.2f kWh/m2 \u2014 doba sie jeszcze nie zaczela.\n",
                  ghiSumNotStarted);
  }

  // Dowod ostatniej proby GHI — tylko gdy proba sie nie udala.
  if (opt.ghiStatus[0] && strcmp(opt.ghiStatus, "ok") != 0 &&
      strcmp(opt.ghiStatus, "brak_proby") != 0) {
    reportAppendf(sink, ctx, "Ostatnia proba GHI: %s", opt.ghiStatus);
    if (opt.ghiBody[0]) reportAppendf(sink, ctx, " - %s", opt.ghiBody);
    reportAppend(sink, ctx, "\n");
    if (opt.ghiUrl[0]) reportAppendf(sink, ctx, "URL: %s\n", opt.ghiUrl);
  }

  if (opt.legacyCount > 0) {
    reportAppendf(sink, ctx,
                  "Uwaga: %u rekord(ow) zapisano przed poprawka kanalu \u2014 dotycza "
                  "bojlera pieca C.O., nie ukladu slonecznego. Usun je: /archiwum_czysc TAK\n",
                  (unsigned)opt.legacyCount);
  }
  return count;
}

// [v3.53.0] Limity Telegrama i podzialu w jednym miejscu — firmware i symulator
// musza uzywac TYCH SAMYCH liczb, inaczej test przechodzi, a urzadzenie wysyla
// inny podzial niz sprawdzony.
static const size_t kTelegramMaxLen = 4096;   // twardszy limit API Telegrama
static const size_t kTelegramSplitLen = 3800; // zapas na prefiks i niespodzianki
static const char  *kContinuationPrefix = "(ciag dalszy)\n";

// Jedna porcja = jedno wywolanie partFn. Prefiks kontynuacji jest CZESCIA tej
// samej wiadomosci (nie osobnym wywolaniem) — inaczej Telegram dostawal pusta
// wiadomosc zlozona z samego prefiksu. Nadawca sklada prefix + text w jeden
// bufor i wysyla raz.
typedef void (*ReportPartFn)(void *ctx, const char *prefix, size_t prefixLen,
                             const char *text, size_t len);

namespace detail {
struct SinkAsPart {
  ReportSinkFn sink;
  void *ctx;
};
inline void sinkAsPart(void *ctx, const char *prefix, size_t prefixLen,
                       const char *text, size_t len) {
  (void)prefix;
  (void)prefixLen;
  SinkAsPart *a = static_cast<SinkAsPart *>(ctx);
  a->sink(a->ctx, text, len);
}
}  // namespace detail

// Dzieli gotowa wiadomosc na porcje <= maxLen, cieciem na koncach linii.
// Kazde wywolanie partFn() = DOKLADNIE jedna wiadomosc Telegrama, w tej samej
// kolejnosci. Telegram odrzuca tekst dluzszy niz 4096 znakow — przy 30 dniach
// raport ma ok. 4,6 tys. znakow i bez podzialu nie doszedl by w ogole.
inline void splitForTelegramParts(const char *text, size_t maxLen,
                                  ReportPartFn partFn, void *ctx) {
  if (!text || !partFn || maxLen < 64) return;
  const size_t len = strlen(text);
  size_t pos = 0;
  size_t part = 0;
  while (pos < len) {
    const char *prefix = (part == 0) ? "" : kContinuationPrefix;
    const size_t prefixLen = (part == 0) ? 0 : strlen(kContinuationPrefix);
    size_t end = pos + maxLen;
    if (end >= len) {
      partFn(ctx, prefix, prefixLen, text + pos, len - pos);
      return;
    }
    // Cofamy sie do ostatniego konca linii w oknie.
    size_t cut = end;
    while (cut > pos && text[cut] != '\n') cut--;
    if (cut <= pos) cut = end;   // pojedyncza linia dluzsza niz limit
    else cut++;                  // zostaw znak nowej linii w tej porcji
    partFn(ctx, prefix, prefixLen, text + pos, cut - pos);
    pos = cut;
    part++;
  }
}

// Wersja bez prefiksu kontynuacji — zostawiona, bo korzystaja z niej starsze
// wywolania i testy pojedynczych porcji.
inline void splitForTelegram(const char *text, size_t maxLen,
                             ReportSinkFn sink, void *ctx) {
  detail::SinkAsPart a = {sink, ctx};
  splitForTelegramParts(text, maxLen, detail::sinkAsPart, &a);
}

// Buduje URL jednego, malego zapytania GHI na wskazana dobe.
// Granice podawane sa w UTC, wiec zapytanie nie zalezy od offsetu DST.
inline bool buildGhiUrl(const char *lat, const char *lon, const char *dateKey,
                        char *out, size_t cap) {
  if (!out || cap == 0) return false;
  int32_t start = 0, end = 0;
  uint16_t hours = 0;
  if (!warsawDayBounds(dateKey, start, end, hours)) return false;

  char sIso[24], eIso[24];
  formatIsoUtc(start, sIso, sizeof(sIso));
  formatIsoUtc(end, eIso, sizeof(eIso));
  if (sIso[0] == 0 || eIso[0] == 0) return false;

  // [v3.52.3] formatIsoUtc() dopisuje koncowe "Z" (ISO-8601). Parametry
  // start_hour/end_hour w Open-Meteo TEGO nie przyjmuja — serwer odpowiada
  // "Bad Request" (HTTP 400). Potwierdzone bezposrednio na API:
  //   ...&start_hour=2026-10-19T22:00:00Z  -> {"reason":"Bad Request"}
  //   ...&start_hour=2026-10-19T22:00:00   -> 200, 25 punktow
  // Ucinamy "Z" — strefa jest juz podana parametrem timezone=GMT.
  for (char *p = sIso; *p; ++p) if (*p == 'Z') { *p = 0; break; }
  for (char *p = eIso; *p; ++p) if (*p == 'Z') { *p = 0; break; }

  const int n = snprintf(
      out, cap,
      "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
      "&hourly=shortwave_radiation&timezone=GMT&timeformat=unixtime"
      "&start_hour=%s&end_hour=%s",
      lat ? lat : "51.066389", lon ? lon : "21.509167", sIso, eIso);
  return n > 0 && (size_t)n < cap;
}

}  // namespace solar_daily

#endif  // SOLAR_DAILY_ARCHIVE_H
