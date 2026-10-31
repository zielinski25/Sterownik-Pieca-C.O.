// WYGENEROWANE przez narzedzia/brama-api-esp215/kompiluj.py - nie edytowac.
// Dokladny kod dodany przez etapy 212-214, wyciagniety z pliku po patchu.

// sekcja tokenu (212)
#ifndef SECRET_HISTORIA_TOKEN
  #define SECRET_HISTORIA_TOKEN ""
#endif
static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;


// adaptery losowosci i zegara (213)
static void historiaLosuj213(std::uint8_t *out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(esp_random() & 0xFFu);
}
static std::uint32_t historiaZegar213() { return static_cast<std::uint32_t>(millis()); }

// bramka trasy (212)
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

// obiekty trasy i biletow (212/213)
static HistoriaBramka212 historiaBramka;
static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);

static historia213::Bilety historiaBilety(historiaLosuj213, historiaZegar213);

struct HistoriaBiletyPodpiecie213 {
  HistoriaBiletyPodpiecie213() { historiaPobranie.ustawBilety(&historiaBilety); }
};
static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;

// rejestracje tras (212/213) - w pliku leza w ciele funkcji,
// wiec tutaj tez musza byc w funkcji, inaczej to nie ten sam kod.
static void rejestracja215() {
wifiWebServer.on("/history", HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);
  });
wifiWebServer.on("/history/long", HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);
  });
wifiWebServer.on("/history/link", HTTP_GET, [](AsyncWebServerRequest *request) {
    historiaPobranie.link(request);
  });
}
