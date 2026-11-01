// WYGENEROWANE przez narzedzia/od-przycisku216/build.py - nie edytowac.
// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp
//         + main_centrala-v3.56.2-to-v3.56.3.patch (sha256 26a97fe756f1fa95a83ff1c4d8e5ccd35e0ff2911cfa0b5bf7b5fce15d20d091)
// Ponizej: prawdziwy kod z pliku po patchu v3.56.3.

static void historiaLosuj213(std::uint8_t *out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(esp_random() & 0xFFu);
}
static std::uint32_t historiaZegar213() { return static_cast<std::uint32_t>(millis()); }

// bramka i obiekt trasy (212) - jedyne miejsce, ktore siega do stanu historii
struct HistoriaBramka212 : historia212::Bramka {
  history193::Calls &io() override { return historiaSys; }
  bool magazynGotowy() override { return historiaDostepGotowy() && littlefsReady; }
  std::uint64_t potwierdzony(historia212::Strumien s) override {
    return (s == historia212::Strumien::Krotki) ? static_cast<std::uint64_t>(g_histShortSize)
                                                : static_cast<std::uint64_t>(g_histLongSize);
  }
  const char *sciezka(historia212::Strumien s) override {
    return (s == historia212::Strumien::Krotki) ? historia211::kShortVfsPath
                                                : historia211::kLongVfsPath;
  }
  bool zamknij() override {
    if (!logMutex) return false;
    return xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) == pdTRUE;
  }
  void otworz() override { xSemaphoreGive(logMutex); }
};
static HistoriaBramka212 historiaBramka;
static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);

// /history (ANY) - tresc lambdy z main_centrala.cpp
static void obslugaHistory(AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);
}

// /history/long (ANY) - tresc lambdy z main_centrala.cpp
static void obslugaHistoryLong(AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);
}

// /history/link (GET) - tresc lambdy z main_centrala.cpp
static void obslugaHistoryLink(AsyncWebServerRequest *request) {
    historiaPobranie.link(request);
}

// Sciezka -> obsluga, w kolejnosci rejestracji w main_centrala.cpp.
struct Trasa216 {
  const char *sciezka;
  void (*obsluz)(AsyncWebServerRequest *);
};
static const Trasa216 TRASY_OBSLUGI216[] = {
  {"/history", obslugaHistory},
  {"/history/long", obslugaHistoryLong},
  {"/history/link", obslugaHistoryLink},
};
static const int TRASY_OBSLUGI216_N = 3;
