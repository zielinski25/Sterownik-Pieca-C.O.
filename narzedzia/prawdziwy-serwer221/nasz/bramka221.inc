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

// [v3.56.1 HISTORIA-BILETY] Zrodlo losowosci i zegar dla biletow. esp_random()
// to sprzetowy generator ESP32; zegar to millis() (32 bity, przewija sie co
// ~49 dni - porownania w Biletach sa na roznicy, wiec przewiniecie nie szkodzi).
static void historiaLosuj213(std::uint8_t *out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(esp_random() & 0xFFu);
}
static std::uint32_t historiaZegar213() { return static_cast<std::uint32_t>(millis()); }
static historia213::Bilety historiaBilety(historiaLosuj213, historiaZegar213);

// Podpiecie biletow do trasy robi konstruktor ponizszego obiektu - w tej samej
// jednostce kompilacji kolejnosc inicjalizacji jest z gory na dol, wiec
// historiaPobranie (wyzej) jest juz zbudowane.
struct HistoriaBiletyPodpiecie213 {
  HistoriaBiletyPodpiecie213() { historiaPobranie.ustawBilety(&historiaBilety); }
};
static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;
