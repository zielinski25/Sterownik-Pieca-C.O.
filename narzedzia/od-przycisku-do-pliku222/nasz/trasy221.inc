  wifiWebServer.on(AsyncURIMatcher::exact("/history"), HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);
  });
  wifiWebServer.on(AsyncURIMatcher::exact("/history/long"), HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);
  });

  // [v3.56.1 HISTORIA-BILETY, v3.56.4 exact()] Panel prosi o bilet, dostaje
  // 32 znaki hex i sam otwiera trase swojego strumienia: /history?k=<bilet>
  // (krotka) albo /history/long?k=<bilet> (dluga). Tu NIE ma tokenu z
  // secrets.h - tylko bilet.
  wifiWebServer.on(AsyncURIMatcher::exact("/history/link"), HTTP_GET, [](AsyncWebServerRequest *request) {
    historiaPobranie.link(request);
  });
