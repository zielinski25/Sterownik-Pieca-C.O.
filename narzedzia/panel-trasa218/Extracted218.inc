// WYGENEROWANE przez narzedzia/panel-trasa218/build.py - nie edytowac.
// Zrodlo: main_centrala.cpp v3.56.4 (sha256 f89874f3390009acd79632db1031d04cd90e67435eda68e2007ea7c4b3168e38).
// Dwa doslowne fragmenty z pliku po patchu 218:
//  1) adaptery biletow (losowosc, zegar) i podpiecie ich do trasy,
//  2) trzy rejestracje tras /history* - tyle, ile widzi serwer.

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

inline void zarejestrujTrasy218() {
  wifiWebServer.on(AsyncURIMatcher::exact("/history"), HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);
  });
  wifiWebServer.on(AsyncURIMatcher::exact("/history/long"), HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);
  });
  wifiWebServer.on(AsyncURIMatcher::exact("/history/link"), HTTP_GET, [](AsyncWebServerRequest *request) {
    historiaPobranie.link(request);
  });
}
