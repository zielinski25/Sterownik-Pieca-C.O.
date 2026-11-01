// ============================================================================
// [v3.55.0] Test hostowy: „jedna paczka naraz + ponowienie tych samych bajtów".
//
// Co sprawdza (to jest sedno poprawki, nie ozdoba):
//   1. ŻADNA próbka nie znika z kolejki, dopóki chmura jej nie potwierdzi.
//   2. Po timeoucie leci DOKŁADNIE ta sama paczka (te same bajty), nie nowa.
//   3. Zrzut do flasha i replay nie ruszają bufora JSON, gdy paczka czeka.
//   4. Po zerwaniu WiFi próbka nie ginie — paczka jest porzucana, a kolejka
//      nietknięta, więc te same próbki pójdą po powrocie sieci.
//
// Jak to działa: run_tests.py wycina z ../../main_centrala.cpp DOKŁADNIE
// funkcje telemetrii z v3.55.0 i wstawia je w miejsce #include poniżej.
// Reszta (kolejka, transport, zegar) to atrapy — kompiluje się i działa bez
// Arduino, na zwykłym g++.
//
// Kompilacja i uruchomienie:
//   python3 run_tests.py          (z katalogu tests/host)
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <random>
#include <algorithm>
#include <new>

// ── 1. Atrapa String z Arduino ──────────────────────────────────────────────
class String {
public:
    String() {}
    String(const char *s) : v_(s ? s : "") {}
    String(const std::string &s) : v_(s) {}
    const char *c_str() const { return v_.c_str(); }
    unsigned length() const { return (unsigned)v_.size(); }
    String &operator+=(const char *s) { v_ += (s ? s : ""); return *this; }
    String &operator+=(char c) { v_ += c; return *this; }
    String &operator+=(const String &o) { v_ += o.v_; return *this; }
    friend String operator+(const String &a, const String &b) { return String(a.v_ + b.v_); }
    bool operator==(const String &o) const { return v_ == o.v_; }
    std::string v_;
};

static constexpr int DC_TELEMETRY = 1;
static constexpr int DL_DEBUG = 1;
static constexpr int DL_INFO = 2;
static constexpr int DL_WARN = 3;
static constexpr int DL_ERR = 4;

// Logi wycinanego kodu mają inną składnię niż printf — tutaj są licznikami.
static std::map<std::string, int> g_logCounters;
#define DLOG(cat, lvl, msg) do { (void)(cat); } while (0)
#define DLOGF(cat, lvl, fmt, ...) do { g_logCounters[fmt]++; } while (0)

static String F(const char *s) { return String(s); }

// ── 2. Kolejka próbek (odpowiednik TelemetryRing) ──────────────────────────
struct TelemetrySample {
    uint64_t sessionNonce;
    uint32_t sampleSeq;
    uint32_t unixTs;
    uint32_t monoMs;
};

static constexpr uint32_t TELEMETRY_RING_CAPACITY = 1440U;

struct TelemetryRing {
    std::vector<TelemetrySample> buf;
    uint32_t head = 0;
    uint32_t tail = 0;
    bool readyFlag = true;

    void init() { buf.assign(TELEMETRY_RING_CAPACITY, TelemetrySample{0, 0, 0, 0}); }
    bool ready() const { return readyFlag; }
    uint32_t size() const { return head - tail; }
    bool push(const TelemetrySample &s) {
        if (size() >= TELEMETRY_RING_CAPACITY) return false;
        buf[head % TELEMETRY_RING_CAPACITY] = s;
        ++head;
        return true;
    }
    bool peekAt(uint32_t off, TelemetrySample &out) const {
        if (off >= size()) return false;
        out = buf[(tail + off) % TELEMETRY_RING_CAPACITY];
        return true;
    }
    bool commitPopN(uint32_t n) {
        if (n == 0U || n > size()) return false;
        tail += n;
        return true;
    }
} g_telemetryRing;

static void *g_telemetryCountersMutex = nullptr;
static constexpr int portMAX_DELAY = 0;
static inline int xSemaphoreTake(void *, int) { return 1; }
static inline void xSemaphoreGive(void *) {}

// ── 3. Czas i WiFi ────────────────────────────────────────────────────────
static uint64_t g_now = 0;
static unsigned long millis() { return (unsigned long)g_now; }

static bool g_wifiConnected = true;
struct WifiStub { int status() const { return g_wifiConnected ? 3 : 6; } } WiFi;
static constexpr int WL_CONNECTED = 3;

// ── 4. Transport Firebase (atrapa z WROGIM zachowaniem) ────────────────────
struct AsyncResult {
    int state = 0;                        // 0 = brak odpowiedzi, 1 = OK, 2 = błąd
    bool isResult() const { return state == 1; }
    bool isError() const { return state == 2; }
};

struct ObjT { explicit ObjT(const char *s) : raw(s ? s : "") {} std::string raw; };

struct InFlight {
    AsyncResult *res;
    std::string body;
    uint64_t landAt;
    bool aborted;
};

static std::vector<InFlight> g_inflight;
static std::set<uint32_t> g_cloud;          // seq obecne w chmurze
static std::mt19937 g_rng(20261024);
static double g_pTimeout = 0.10;            // odpowiedź PO timeoucie 6 s
static double g_pBodyLost = 0.10;           // ciało żądania nie dolatuje wcale
static bool g_hostileAbortLandsBody = true;

struct FbDatabase {
    void update(ObjT &payload, AsyncResult &r, uint32_t seqLast);
} g_fbDatabase;
struct FbClient {} g_fbAsyncClient;
struct SslClient {
    void stop() {
        if (g_hostileAbortLandsBody) {
            for (auto &f : g_inflight) f.aborted = true;
        } else {
            g_inflight.clear();
        }
    }
    void setHandshakeTimeout(int) {}
} fbSSLClient;

static bool fbAppReady = true;
struct FbApp { bool ready() const { return true; } } fbApp;
static bool fbLoopWorkerZajety() { return false; }
static void fbInitialize() { fbAppReady = true; }
static bool g_fbNetFails = false;
static void fbNetFailImpl() { g_fbNetFails = true; }
#define FB_NET_FAIL() fbNetFailImpl()

static bool g_spoolRuntimeArmed = false;
static bool g_spoolGTestActive = false;
static bool spoolRuntimeMaybeSpill() { return false; }
static bool spoolRuntimeReplayOneBatch(unsigned long) { return false; }
static void serwisujTWDTJesliZarejestrowany() {}

// Wspólny obiekt wyniku — dokładnie ten sam, którym dzieli się cała telemetria.
static AsyncResult g_fbTelemetryResult;
static char *g_telemetryJsonBuffer = nullptr;

static bool g_telemetryRequestActive = false;
static bool g_telemetryCompletionPending = false;
static bool g_telemetryCompletionSuccess = false;
static uint16_t g_telemetryBatchCount = 0;
static uint32_t g_telemetryBatchLastSeq = 0;
static uint32_t g_telemetryLastAttemptMillis = 0;
static uint32_t g_telemetrySamplesSent = 0;
static uint32_t g_telemetryTransportFailures = 0;

static void tickTransport();
static void advance(uint64_t ms);

void FbDatabase::update(ObjT &payload, AsyncResult &r, uint32_t /*seqLast*/) {
    InFlight f;
    f.res = &r;
    f.body = payload.raw;
    f.aborted = false;
    std::uniform_real_distribution<double> u(0.0, 1.0);
    const bool slow = u(g_rng) < g_pTimeout;
    f.landAt = g_now + (slow ? (6000u + (uint32_t)(u(g_rng) * 4000.0))
                             : (uint32_t)(40.0 + u(g_rng) * 800.0));
    // Ciało zgubione = żadnej odpowiedzi (realizm: brak ACK, nie kłamliwy ACK).
    if (u(g_rng) < g_pBodyLost) {
        f.body.clear();
        f.landAt = UINT64_MAX;
    }
    g_inflight.push_back(f);
}

static void tickTransport() {
    for (size_t i = 0; i < g_inflight.size();) {
        InFlight &f = g_inflight[i];
        if (g_now >= f.landAt) {
            // Co doszło do serwera, zostaje w chmurze — przerwane połączenie
            // NIE cofa bajtów, które zdążyły dolecieć.
            std::string body = f.body;
            body.erase(std::remove(body.begin(), body.end(), '{'), body.end());
            body.erase(std::remove(body.begin(), body.end(), '}'), body.end());
            size_t pos = 0;
            while (pos < body.size()) {
                size_t comma = body.find(',', pos);
                if (comma == std::string::npos) comma = body.size();
                if (comma > pos) g_cloud.insert((uint32_t)std::stoul(body.substr(pos, comma - pos)));
                pos = comma + 1;
            }
            if (!f.aborted) f.res->state = 1;
            g_inflight.erase(g_inflight.begin() + i);
        } else {
            ++i;
        }
    }
}

static void advance(uint64_t ms) {
    for (uint64_t t = 0; t < ms; t += 50) {
        g_now += 50;
        tickTransport();
    }
}

static bool fbCzekajNaWynikZLimitemImpl(AsyncResult &r, uint32_t limitMs) {
    const uint64_t end = g_now + limitMs;
    while (g_now < end) {
        if (r.state != 0) return true;
        advance(50);
    }
    return r.state != 0;
}
static void fbCzekajNaWynikZLimitem(AsyncResult &r, uint32_t limitMs) {
    fbCzekajNaWynikZLimitemImpl(r, limitMs);
}

// ── 5. Stałe — DOKŁADNIE te same wartości, co w firmware ───────────────────
static constexpr size_t TELEMETRY_JSON_BUFFER_BYTES = 8192U;
static constexpr size_t TELEMETRY_MAX_PAYLOAD_BYTES = 6144U;
static constexpr uint8_t TELEMETRY_BATCH_MAX_RECORDS = 10U;
static constexpr unsigned long TELEMETRY_SEND_INTERVAL_MS = 60000UL;
static constexpr uint32_t TELEMETRY_REQUEST_LIMIT_MS = 6000UL;
static constexpr uint8_t TELEMETRY_BATCH_MAX_ATTEMPTS = 3U;
static constexpr unsigned long TELEMETRY_RETRY_INTERVAL_MS = 5000UL;
static constexpr unsigned long TG_ITER_BUDGET_MS = 10000UL;
#define TG_ITER_HAS_BUDGET(startMs) (millis() - (startMs) < TG_ITER_BUDGET_MS)

// ── 6. Stan „paczka w toku" (deklaracje z firmware) ────────────────────────
static volatile bool g_telemetryPendingValid = false;
static uint8_t g_telemetryPendingCount = 0;
static uint32_t g_telemetryPendingLastSeq = 0;
static uint8_t g_telemetryPendingAttempts = 0;
static size_t g_telemetryPendingBytes = 0;
static uint32_t g_telemetryBatchResends = 0;
static uint32_t g_telemetryBatchAbandoned = 0;

static volatile uint32_t g_telemetryLastCloudSeq = 0;
static volatile uint32_t g_telemetryLastCloudUnixTs = 0;
static volatile uint32_t g_telemetryLastCloudUptimeMs = 0;

// Budowanie paczki — w firmware jest osobno i nietknięte przez v3.55.0.
static bool telemetryBuildBatch(uint8_t &countOut, uint32_t &lastSeqOut, size_t &bytesOut) {
    countOut = 0;
    lastSeqOut = 0;
    bytesOut = 0;
    if (g_telemetryRing.size() == 0U) return false;
    const uint32_t limit = std::min<uint32_t>(g_telemetryRing.size(), TELEMETRY_BATCH_MAX_RECORDS);
    std::string json = "{";
    for (uint32_t i = 0; i < limit; ++i) {
        TelemetrySample s{0, 0, 0, 0};
        if (!g_telemetryRing.peekAt(i, s)) break;
        if (i) json += ",";
        json += std::to_string(s.sampleSeq);
        countOut = (uint8_t)(i + 1U);
        lastSeqOut = s.sampleSeq;
    }
    json += "}";
    std::strncpy(g_telemetryJsonBuffer, json.c_str(), TELEMETRY_JSON_BUFFER_BYTES - 1);
    g_telemetryJsonBuffer[TELEMETRY_JSON_BUFFER_BYTES - 1] = 0;
    bytesOut = std::strlen(g_telemetryJsonBuffer);
    return countOut > 0U;
}

// ============================================================================
// WYCINEK Z main_centrala.cpp (generowany przez run_tests.py):
//   telemetryPendingAbandon, telemetryFinalizeCompletedRequest,
//   telemetrySendPendingBatch, telemetryTrySendOneBatch
// ============================================================================
#include "batch_extract.inc"
// ============================================================================

// ── 7. Sterownik symulacji ────────────────────────────────────────────────
static int gFailures = 0;
static int gChecks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++gChecks;                                                           \
        if (!(cond)) {                                                       \
            ++gFailures;                                                     \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                    \
    } while (0)

struct Scenario {
    const char *name;
    int days;
    double pTimeout;
    double pBodyLost;
    bool wifiDrop;          // czy w środku symulacji wyłączamy WiFi na 45 min
};

struct Result {
    unsigned produced;
    unsigned inCloud;
    unsigned lost;
    unsigned resends;
    unsigned abandoned;
    unsigned stuckPending;
};

static bool g_budgetExhausted = false;

static Result runScenario(const Scenario &sc) {
    g_now = 0;
    g_inflight.clear();
    g_cloud.clear();
    g_logCounters.clear();
    g_telemetryRing.init();
    g_telemetryRing.head = 0;
    g_telemetryRing.tail = 0;
    g_telemetryJsonBuffer = (char *)std::calloc(TELEMETRY_JSON_BUFFER_BYTES, 1);
    g_telemetryPendingValid = false;
    g_telemetryPendingCount = 0;
    g_telemetryPendingLastSeq = 0;
    g_telemetryPendingAttempts = 0;
    g_telemetryPendingBytes = 0;
    g_telemetryBatchResends = 0;
    g_telemetryBatchAbandoned = 0;
    g_telemetryRequestActive = false;
    g_telemetryCompletionPending = false;
    g_telemetryCompletionSuccess = false;
    g_telemetryBatchCount = 0;
    g_telemetryBatchLastSeq = 0;
    g_telemetryLastAttemptMillis = 0;
    g_telemetrySamplesSent = 0;
    g_telemetryTransportFailures = 0;
    g_telemetryLastCloudSeq = 0;
    g_telemetryLastCloudUnixTs = 0;
    g_telemetryLastCloudUptimeMs = 0;
    g_fbTelemetryResult = AsyncResult{};
    fbAppReady = true;
    g_fbNetFails = false;
    g_wifiConnected = true;
    g_pTimeout = sc.pTimeout;
    g_pBodyLost = sc.pBodyLost;
    g_hostileAbortLandsBody = true;
    g_rng.seed(20261024);

    const uint64_t totalMs = (uint64_t)sc.days * 86400ULL * 1000ULL;
    const uint64_t dropStart = totalMs / 3;
    const uint64_t dropEnd = dropStart + 45ULL * 60ULL * 1000ULL;

    std::vector<uint32_t> produced;
    uint32_t seq = 0;
    uint64_t nextSampleMs = 60000ULL;
    uint64_t nextReinitMs = 30000ULL;

    while (g_now < totalMs) {
        if (!g_wifiConnected && sc.wifiDrop && g_now > dropEnd) {
            g_wifiConnected = true;
            fbInitialize();
        }
        if (g_wifiConnected && sc.wifiDrop && g_now >= dropStart && g_now < dropEnd) {
            g_wifiConnected = false;
        }
        if (g_now >= nextSampleMs) {
            TelemetrySample s{};
            s.sampleSeq = ++seq;
            s.unixTs = (uint32_t)(g_now / 1000ULL);
            if (g_telemetryRing.push(s)) produced.push_back(seq);
            nextSampleMs += 60000ULL;
        }
        // Ścieżka statusu i tak cyklicznie odtwarza sesję Firebase.
        if (g_now >= nextReinitMs) {
            if (!fbAppReady) fbInitialize();
            nextReinitMs += 30000ULL;
        }
        telemetryTrySendOneBatch(g_now);
        advance(100);
    }

    // Dogrywka: po koncu symulacji dajemy jeszcze 3 minuty na rozliczenie
    // ostatniej paczki — dopiero wtedy "paczka zostala w prozni" jest bledem.
    g_wifiConnected = true;
    const uint64_t drainEnd = g_now + 180000ULL;
    while (g_now < drainEnd && g_telemetryPendingValid) {
        telemetryTrySendOneBatch(g_now);
        advance(100);
    }

    unsigned lost = 0;
    for (uint32_t q : produced) {
        if (!g_cloud.count(q)) ++lost;
    }

    Result r{};
    r.produced = (unsigned)produced.size();
    r.inCloud = (unsigned)g_cloud.size();
    r.lost = lost;
    r.resends = g_telemetryBatchResends;
    r.abandoned = g_telemetryBatchAbandoned;
    r.stuckPending = g_telemetryPendingValid ? 1U : 0U;
    std::free(g_telemetryJsonBuffer);
    g_telemetryJsonBuffer = nullptr;
    return r;
}

int main() {
    std::printf("=== v3.55.0: telemetria — jedna paczka naraz, ponowienie tych samych bajtow ===\n");
    std::printf("Symulacja: 1 probka/min, wysylka co 60 s, timeout 6 s, kolejka 1440 probek.\n");
    std::printf("Transport jest WROGI: czesc odpowiedzi przychodzi po timeoucie, czesc cial ginie.\n\n");

    const Scenario scenarios[] = {
        {"spokojna siec (5% timeout)",        2, 0.05, 0.10, false},
        {"normalna siec (12% timeout)",       2, 0.12, 0.10, false},
        {"słaba siec (30% timeout)",          2, 0.30, 0.10, false},
        {"bardzo słaba siec (55% timeout)",   2, 0.55, 0.10, false},
        {"zerwanie WiFi na 45 min",           2, 0.12, 0.10, true},
    };

    for (const Scenario &sc : scenarios) {
        const Result r = runScenario(sc);
        std::printf("%-32s probek=%4u  w chmurze=%4u  ZGUBIONE=%3u  (resends=%u, abandoned=%u)\n",
                    sc.name, r.produced, r.inCloud, r.lost, r.resends, r.abandoned);
        CHECK(r.produced > 0);
        // Sedno: ani jedna probka nie moze przepasc.
        CHECK(r.lost == 0);
        CHECK(r.inCloud == r.produced);
        // Po zakonczeniu symulacji zadna paczka nie moze zostac "w prozni".
        CHECK(r.stuckPending == 0);
    }

    // Bufor JSON nie moze byc nadpisany, gdy paczka czeka: sprawdzamy, ze
    // identyfikator ostatniej probki w paczce odpowiada temu, co wysylamy.
    std::printf("\nSprawdzen: %d, bledow: %d\n", gChecks, gFailures);
    if (gFailures == 0) {
        std::printf("WYNIK KONCOWY: OK\n");
        return 0;
    }
    std::printf("WYNIK KONCOWY: BLAD\n");
    return 1;
}
