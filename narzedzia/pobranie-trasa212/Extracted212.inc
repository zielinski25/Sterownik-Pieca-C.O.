// WYGENEROWANE przez narzedzia/pobranie-trasa212/build.py - nie edytowac.
// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp
//         + main_centrala-v3.55.8-to-v3.56.0.patch (sha256 e9937cf7e892db4b1c12b671c50ffa55627db0b7c3e6ffeca47d6a5ff39e11a0)
// To jest dokladny tekst bramki i obiektow trasy z pliku PO patchu.

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
